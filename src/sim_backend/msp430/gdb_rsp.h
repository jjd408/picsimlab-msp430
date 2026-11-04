/* A GDB remote serial protocol server for the MSP430 simulator: lets msp430-elf-gdb (or anything that speaks
 * the protocol) set breakpoints and watchpoints, step, continue, interrupt, and read and write registers and
 * memory, over TCP.
 *
 * The server is polled from the simulation thread, between runs of the simulation, so it never touches the
 * chip while it runs. It speaks to the simulator only through GdbServer::Target.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef GDB_RSP_H
#define GDB_RSP_H

#include <stdint.h>
#include <string>

class GdbServer {
public:
    // What the server needs from the simulator.
    class Target {
    public:
        virtual ~Target() {}
        // 16 registers, R0 (PC) to R15, 20 bits each
        virtual void GetRegs(uint32_t regs[16]) = 0;
        virtual void SetReg(int n, uint32_t value) = 0;
        virtual uint8_t Peek(uint32_t addr) = 0;
        virtual void Poke(uint32_t addr, uint8_t value) = 0;
        // Stops the program where it is (a Ctrl-C from the debugger, or one connecting).
        virtual void Halt(void) = 0;
        // Runs on from a stop, or for one instruction.
        virtual void Resume(int step) = 0;
        // Why the program is stopped: 0 if it's running, else a stop reply for GDB ("T05", "T05watch:2000;").
        virtual std::string StopReply(void) = 0;
        // type: 0/1 breakpoint, 2 write, 3 read, 4 access watchpoint. Return false if not supported.
        virtual bool SetBreak(int type, uint32_t addr, uint32_t len, bool set) = 0;
        virtual void ClearBreaks(void) = 0;
        // "monitor" commands typed in GDB; returns the text to show.
        virtual std::string Monitor(const std::string& cmd) = 0;
    };

    explicit GdbServer(Target* target) : target(target) {}
    ~GdbServer() { Close(); }

    // Starts listening on a TCP port, on this machine only (127.0.0.1). Returns false, with error set, if not.
    bool Listen(unsigned short port);
    // Accepts a connection, answers what the debugger has sent, and reports a stop it was waiting for.
    // While the program is stopped it keeps answering for up to about wait_ms, so that the debugger's bursts
    // of requests after a stop don't each wait for the next poll.
    void Poll(int wait_ms);
    void Close(void);
    bool Connected(void) const { return client >= 0; }
    unsigned short Port(void) const { return port; }

    std::string error;

private:
    void Accept(void);
    void Disconnect(void);
    bool ReadInput(int timeout_ms);
    void Process(void);
    void Handle(const std::string& packet);
    void Send(const std::string& payload);
    void SendRaw(const std::string& data);

    std::string ReadMemory(uint32_t addr, uint32_t len);
    std::string Registers(void);

    Target* target;
    int listener = -1;
    int client = -1;
    unsigned short port = 0;
    std::string input;
    bool no_ack = false;
    bool waiting = false;  // the debugger has resumed the program and waits for it to stop
};

#endif
