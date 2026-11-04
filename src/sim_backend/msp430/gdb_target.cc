/* The MSP430FR2355 chip model as a GDB server's target: registers, memory without read side effects,
 * breakpoints, watchpoints, stepping and "monitor" commands. Used by PICSimLab's MSP430 backend and by the
 * command-line test runner.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#include "gdb_target.h"

#include <stdio.h>

void Fr2355Target::GetRegs(uint32_t regs[16]) {
    for (int i = 0; i < 16; i++)
        regs[i] = chip ? chip->cpu.regs[i] : 0;
}

void Fr2355Target::SetReg(int n, uint32_t value) {
    if (chip && n >= 0 && n < 16)
        chip->cpu.regs[n] = value & 0xFFFFF;
}

uint8_t Fr2355Target::Peek(uint32_t addr) {
    return chip ? chip->Peek8(addr) : 0;
}

void Fr2355Target::Poke(uint32_t addr, uint8_t value) {
    if (chip) {
        chip->Poke8(addr, value);
        if (changed)
            changed();
    }
}

void Fr2355Target::Halt(void) {
    if (chip && !chip->debug_stop)
        chip->debug_stop = Fr2355::STOP_HALT;
}

void Fr2355Target::Resume(int step) {
    if (chip && chip->debug_stop)
        chip->DebugResume(step);
}

std::string Fr2355Target::StopReply(void) {
    if (!chip)
        return "";
    if (chip->halted)
        return "T04";  // SIGILL: an invalid instruction or address stopped the CPU
    switch (chip->debug_stop) {
        case Fr2355::STOP_NONE:
            return "";
        case Fr2355::STOP_HALT:
            return "T02";  // SIGINT
        case Fr2355::STOP_WATCH: {
            const char* kind = "awatch";
            for (const Fr2355::Watch& w : chip->watches) {
                if (w.addr == chip->watch_addr) {
                    kind = w.kind == Fr2355::WATCH_WRITE  ? "watch"
                           : w.kind == Fr2355::WATCH_READ ? "rwatch"
                                                          : "awatch";
                    break;
                }
            }
            char s[48];
            snprintf(s, sizeof(s), "T05%s:%x;", kind, chip->watch_addr);
            return s;
        }
        default:
            return "T05";  // SIGTRAP: a breakpoint or a step
    }
}

bool Fr2355Target::SetBreak(int type, uint32_t addr, uint32_t len, bool set) {
    if (!chip || type < 0 || type > 4)
        return false;
    if (type <= 1) {
        auto& b = chip->breakpoints;
        for (size_t i = 0; i < b.size(); i++) {
            if (b[i] == addr) {
                if (!set)
                    b.erase(b.begin() + i);
                return true;
            }
        }
        if (set)
            b.push_back(addr);
        return true;
    }
    int kind = type == 2 ? Fr2355::WATCH_WRITE : type == 3 ? Fr2355::WATCH_READ
                                                           : Fr2355::WATCH_WRITE | Fr2355::WATCH_READ;
    auto& w = chip->watches;
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i].addr == addr && w[i].len == len && w[i].kind == kind) {
            if (!set)
                w.erase(w.begin() + i);
            return true;
        }
    }
    if (set)
        w.push_back({addr, len, kind});
    return true;
}

void Fr2355Target::ClearBreaks(void) {
    if (chip) {
        chip->breakpoints.clear();
        chip->watches.clear();
    }
}

std::string Fr2355Target::Monitor(const std::string& cmd) {
    if (!chip)
        return "No chip.\n";
    if (cmd == "reset") {
        chip->PowerOnReset();
        if (changed)
            changed();
        chip->debug_stop = Fr2355::STOP_HALT;
        return "Reset (power-on); stopped at the reset vector.\n";
    }
    if (cmd == "cycles") {
        char s[96];
        snprintf(s, sizeof(s), "%llu MCLK cycles since power-on; MCLK %.3f MHz\n",
                 (unsigned long long)chip->cycle_count, chip->mclk_hz / 1e6);
        return s;
    }
    return "monitor commands: reset (power-on reset, stays stopped), cycles (MCLK cycles run)\n";
}
