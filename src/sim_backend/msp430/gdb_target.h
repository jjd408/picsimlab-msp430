/* The MSP430FR2355 chip model as a GDB server's target: registers, memory without read side effects,
 * breakpoints, watchpoints, stepping and "monitor" commands. Used by PICSimLab's MSP430 backend and by the
 * command-line test runner.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef GDB_TARGET_H
#define GDB_TARGET_H

#include <functional>

#include "fr2355.h"
#include "gdb_rsp.h"

class Fr2355Target : public GdbServer::Target {
public:
    Fr2355Target(void) {}
    void SetChip(Fr2355* chip_) { chip = chip_; }
    // Called when the debugger changed something the pins show (a write, a reset).
    std::function<void(void)> changed;

    void GetRegs(uint32_t regs[16]) override;
    void SetReg(int n, uint32_t value) override;
    uint8_t Peek(uint32_t addr) override;
    void Poke(uint32_t addr, uint8_t value) override;
    void Halt(void) override;
    void Resume(int step) override;
    std::string StopReply(void) override;
    bool SetBreak(int type, uint32_t addr, uint32_t len, bool set) override;
    void ClearBreaks(void) override;
    std::string Monitor(const std::string& cmd) override;

private:
    Fr2355* chip = nullptr;
};

#endif
