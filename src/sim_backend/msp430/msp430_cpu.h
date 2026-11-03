/* MSP430X CPU core for PICSimLab.
 *
 * The instruction execution code is taken from MSPDebug's simulator (drivers/sim.c),
 * Copyright (C) 2009, 2010, 2020 Daniel Beer and (C) 2020 Bruce G. Burns, GPL v2 or later.
 * Changes for PICSimLab: memory goes through bus callbacks at the access's real width (so peripheral
 * registers see byte and word accesses as the chip would), 20-bit store addresses, and a fix for RETI on
 * CPUX falling through into CALLA.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef MSP430_CPU_H
#define MSP430_CPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSP430_REG_PC 0
#define MSP430_REG_SP 1
#define MSP430_REG_SR 2
#define MSP430_REG_R3 3

#define MSP430_SR_V 0x0100
#define MSP430_SR_SCG1 0x0080
#define MSP430_SR_SCG0 0x0040
#define MSP430_SR_OSCOFF 0x0020
#define MSP430_SR_CPUOFF 0x0010
#define MSP430_SR_GIE 0x0008
#define MSP430_SR_N 0x0004
#define MSP430_SR_Z 0x0002
#define MSP430_SR_C 0x0001

struct msp430_cpu {
    uint32_t regs[16];
    uint32_t current_insn;  // address of the instruction being executed
    int cpux;               // 1 for MSP430X (all FR2xx parts)

    // The chip's memory bus. addr is 20 bits; word accesses are to even addresses.
    void* bus;
    uint8_t (*read8)(void* bus, uint32_t addr);
    uint16_t (*read16)(void* bus, uint32_t addr);
    void (*write8)(void* bus, uint32_t addr, uint8_t value);
    void (*write16)(void* bus, uint32_t addr, uint16_t value);
    void (*error)(void* bus, const char* message);
};

// Executes one instruction. Returns the cycles it took, or -1 on an invalid opcode or PC (the PC is left
// on the failing instruction).
int msp430_cpu_step(struct msp430_cpu* cpu);

// Enters an interrupt as the MSP430X does: pushes PC[15:0], then SR with PC[19:16] in bits 15:12, clears
// SR (keeping SCG0) and jumps through the vector at vector_addr. Returns the cycles it took.
int msp430_cpu_interrupt(struct msp430_cpu* cpu, uint32_t vector_addr);

#ifdef __cplusplus
}
#endif

#endif
