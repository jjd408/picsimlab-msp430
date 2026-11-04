/* GDB remote serial protocol server for the MSP430 simulator; see gdb_rsp.h.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#include "gdb_rsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN_
#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static const char HEX[] = "0123456789abcdef";

static std::string ToHex(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        out += HEX[c >> 4];
        out += HEX[c & 15];
    }
    return out;
}

static int HexDigit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static std::string FromHex(const std::string& s) {
    std::string out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out += (char)(HexDigit(s[i]) * 16 + HexDigit(s[i + 1]));
    return out;
}

// A register as GDB's MSP430X target lays it out: 4 bytes, little-endian.
static std::string RegHex(uint32_t v) {
    std::string out;
    for (int i = 0; i < 4; i++) {
        unsigned char b = (v >> (8 * i)) & 0xFF;
        out += HEX[b >> 4];
        out += HEX[b & 15];
    }
    return out;
}

static uint32_t RegFromHex(const std::string& s) {
    uint32_t v = 0;
    for (int i = 0; i < 4 && 2 * i + 1 < (int)s.size(); i++)
        v |= (uint32_t)(HexDigit(s[2 * i]) * 16 + HexDigit(s[2 * i + 1])) << (8 * i);
    return v;
}

#ifdef _WIN_

bool GdbServer::Listen(unsigned short) {
    error = "the GDB server isn't available on Windows yet";
    return false;
}
void GdbServer::Poll(int) {}
void GdbServer::Close(void) {}

#else

bool GdbServer::Listen(unsigned short port_) {
    if (listener >= 0 && port == port_)
        return true;
    Close();
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        error = strerror(errno);
        return false;
    }
    int one = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // this machine only
    addr.sin_port = htons(port_);
    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(listener, 1) < 0) {
        error = "port " + std::to_string(port_) + ": " + strerror(errno);
        close(listener);
        listener = -1;
        return false;
    }
    fcntl(listener, F_SETFL, O_NONBLOCK);
    port = port_;
    return true;
}

void GdbServer::Close(void) {
    Disconnect();
    if (listener >= 0)
        close(listener);
    listener = -1;
}

void GdbServer::Accept(void) {
    int c = accept(listener, NULL, NULL);
    if (c < 0)
        return;
    int one = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    fcntl(c, F_SETFL, O_NONBLOCK);
    client = c;
    input.clear();
    no_ack = false;
    waiting = false;
    target->Halt();  // a debugger finds the program stopped when it connects
}

void GdbServer::Disconnect(void) {
    if (client < 0)
        return;
    close(client);
    client = -1;
    // Leave the program running as it would without a debugger
    target->ClearBreaks();
    if (!target->StopReply().empty())
        target->Resume(0);
}

bool GdbServer::ReadInput(int timeout_ms) {
    struct pollfd p = {client, POLLIN, 0};
    if (poll(&p, 1, timeout_ms) <= 0)
        return false;
    char buf[4096];
    ssize_t n = recv(client, buf, sizeof(buf), 0);
    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            Disconnect();
        return false;
    }
    input.append(buf, n);
    return true;
}

void GdbServer::SendRaw(const std::string& data) {
    size_t done = 0;
    while (client >= 0 && done < data.size()) {
        ssize_t n = send(client, data.data() + done, data.size() - done, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd p = {client, POLLOUT, 0};
                poll(&p, 1, 100);
                continue;
            }
            Disconnect();
            return;
        }
        done += n;
    }
}

void GdbServer::Poll(int wait_ms) {
    if (listener < 0)
        return;
    if (client < 0)
        Accept();
    if (client < 0)
        return;
    auto start = std::chrono::steady_clock::now();
    for (;;) {
        ReadInput(0);
        Process();
        if (client < 0)
            return;
        std::string stop = target->StopReply();
        if (waiting && !stop.empty()) {
            Send(stop);
            waiting = false;
        }
        if (stop.empty())
            return;  // running: back to the simulation
        int left = wait_ms - (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - start)
                                  .count();
        if (left <= 0 || !ReadInput(left))
            return;
        Process();
    }
}

#endif

void GdbServer::Process(void) {
    while (!input.empty() && client >= 0) {
        char c = input[0];
        if (c == '$') {
            size_t hash = input.find('#');
            if (hash == std::string::npos || hash + 2 >= input.size())
                return;  // not all here yet
            std::string payload = input.substr(1, hash - 1);
            int sum = HexDigit(input[hash + 1]) * 16 + HexDigit(input[hash + 2]);
            input.erase(0, hash + 3);
            unsigned char check = 0;
            for (unsigned char ch : payload)
                check += ch;
            if (!no_ack) {
                if (check != sum) {
                    SendRaw("-");
                    continue;
                }
                SendRaw("+");
            }
            Handle(payload);
        } else {
            if (c == 0x03)
                target->Halt();  // Ctrl-C; the stop is reported to the waiting debugger
            input.erase(0, 1);  // '+', '-' and anything between packets
        }
    }
}

void GdbServer::Send(const std::string& payload) {
    unsigned char sum = 0;
    for (unsigned char c : payload)
        sum += c;
    char tail[4];
    snprintf(tail, sizeof(tail), "#%02x", sum);
    SendRaw("$" + payload + tail);
}

std::string GdbServer::Registers(void) {
    uint32_t regs[16];
    target->GetRegs(regs);
    std::string out;
    for (int i = 0; i < 16; i++)
        out += RegHex(regs[i]);
    return out;
}

std::string GdbServer::ReadMemory(uint32_t addr, uint32_t len) {
    std::string out;
    for (uint32_t i = 0; i < len; i++) {
        unsigned char b = target->Peek((addr + i) & 0xFFFFF);
        out += HEX[b >> 4];
        out += HEX[b & 15];
    }
    return out;
}

void GdbServer::Handle(const std::string& p) {
    if (p.empty()) {
        Send("");
        return;
    }
    char cmd = p[0];
    std::string arg = p.substr(1);
    switch (cmd) {
        case '?': {
            std::string stop = target->StopReply();
            Send(stop.empty() ? "S05" : stop);
            return;
        }
        case 'g':
            Send(Registers());
            return;
        case 'G':
            for (int i = 0; i < 16 && 8 * i + 8 <= (int)arg.size(); i++)
                target->SetReg(i, RegFromHex(arg.substr(8 * i, 8)));
            Send("OK");
            return;
        case 'p': {
            int n = strtol(arg.c_str(), NULL, 16);
            if (n < 0 || n >= 16) {
                Send("E01");
                return;
            }
            uint32_t regs[16];
            target->GetRegs(regs);
            Send(RegHex(regs[n]));
            return;
        }
        case 'P': {
            size_t eq = arg.find('=');
            int n = strtol(arg.c_str(), NULL, 16);
            if (eq == std::string::npos || n < 0 || n >= 16) {
                Send("E01");
                return;
            }
            target->SetReg(n, RegFromHex(arg.substr(eq + 1)));
            Send("OK");
            return;
        }
        case 'm': {
            unsigned long addr = 0, len = 0;
            if (sscanf(arg.c_str(), "%lx,%lx", &addr, &len) != 2 || len > 0x1000) {
                Send("E01");
                return;
            }
            Send(ReadMemory(addr, len));
            return;
        }
        case 'M':
        case 'X': {
            unsigned long addr = 0, len = 0;
            size_t colon = arg.find(':');
            if (sscanf(arg.c_str(), "%lx,%lx", &addr, &len) != 2 || colon == std::string::npos) {
                Send("E01");
                return;
            }
            std::string data = arg.substr(colon + 1);
            std::string bytes;
            if (cmd == 'M') {
                bytes = FromHex(data);
            } else {
                for (size_t i = 0; i < data.size(); i++)  // binary, with '}' escaping
                    bytes += (data[i] == '}' && i + 1 < data.size()) ? (char)(data[++i] ^ 0x20) : data[i];
            }
            for (unsigned long i = 0; i < len && i < bytes.size(); i++)
                target->Poke((addr + i) & 0xFFFFF, bytes[i]);
            Send("OK");
            return;
        }
        case 'Z':
        case 'z': {
            int type = 0;
            unsigned long addr = 0, len = 0;
            if (sscanf(arg.c_str(), "%d,%lx,%lx", &type, &addr, &len) < 2) {
                Send("E01");
                return;
            }
            Send(target->SetBreak(type, addr, len ? len : 1, cmd == 'Z') ? "OK" : "");
            return;
        }
        case 'c':
        case 's':
            if (!arg.empty())
                target->SetReg(0, strtoul(arg.c_str(), NULL, 16));
            target->Resume(cmd == 's');
            waiting = true;
            return;
        case 'D':
            Send("OK");
            Disconnect();
            return;
        case 'k':
            Disconnect();
            return;
        case 'H':
        case 'T':
            Send("OK");
            return;
        case 'v':
            if (p == "vCont?") {
                Send("vCont;c;C;s;S;t");
            } else if (p.compare(0, 6, "vCont;") == 0) {
                char action = p.size() > 6 ? p[6] : 'c';
                if (action == 't') {
                    target->Halt();
                    Send(target->StopReply());
                } else {
                    target->Resume(action == 's' || action == 'S');
                    waiting = true;
                }
            } else {
                Send("");  // vMustReplyEmpty and anything else we don't do
            }
            return;
        case 'q':
            if (p.compare(0, 10, "qSupported") == 0)
                Send("PacketSize=4000;QStartNoAckMode+;swbreak+;hwbreak+");
            else if (p == "qAttached")
                Send("1");
            else if (p == "qC")
                Send("QC1");
            else if (p == "qfThreadInfo")
                Send("m1");
            else if (p == "qsThreadInfo")
                Send("l");
            else if (p == "qOffsets")
                Send("Text=0;Data=0;Bss=0");
            else if (p.compare(0, 7, "qSymbol") == 0)
                Send("OK");
            else if (p.compare(0, 6, "qRcmd,") == 0) {
                std::string out = target->Monitor(FromHex(p.substr(6)));
                if (!out.empty())
                    Send("O" + ToHex(out));
                Send("OK");
            } else
                Send("");
            return;
        case 'Q':
            if (p == "QStartNoAckMode") {
                Send("OK");
                no_ack = true;
            } else {
                Send("");
            }
            return;
    }
    Send("");
}
