/* MSP430FR2355 chip model for PICSimLab: memory map, peripherals and interrupts around the MSP430X core.
 *
 * Register addresses and bit layouts are from TI's MSP430FR2355 header (msp430fr2355.h) and the
 * MSP430FR4xx/FR2xx family user's guide (SLAU445). Only the peripherals the solitaire handheld uses are
 * modeled; other registers read back what was last written.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef FR2355_H
#define FR2355_H

#include <stdint.h>
#include <string>

#include "msp430_cpu.h"
#include "timer_b.h"

class Fr2355 {
public:
    static const int PIN_COUNT = 38;  // TSSOP-38 (MSP430FR2355TDBT)

    // A package pin as PICSimLab sees it.
    struct Pin {
        const char* name;
        int port;  // 1-5, or 0 for power/TEST/RST
        int bit;
    };
    static const Pin pins[PIN_COUNT];

    Fr2355(void);

    // Loads an Intel HEX file into FRAM/info memory. Returns 0, or -1 with error set.
    int LoadHex(const char* fname);
    // Writes FRAM and info memory as Intel HEX. Returns 0, or -1 with error set.
    int SaveHex(const char* fname);
    void EraseFram(void);

    // Power-on reset (BOR): everything to its reset state, then PUC.
    void PowerOnReset(void);

    // Advances the chip by one simulation tick of 1/tick_hz seconds.
    void Tick(void);
    void SetTickHz(double hz) { tick_hz = hz; };

    // Pin interface, using package pin numbers 1..PIN_COUNT.
    // Level the chip drives or sees on the pin (1/0).
    int GetPinLevel(int pin) const;
    // 1 if the chip drives the pin as an output.
    int GetPinIsOutput(int pin) const;
    // Level driven onto the pin from outside (a part); ignored while the chip drives it.
    void SetPinInput(int pin, int level);
    // Holds the chip in reset while the RST pin is low.
    void SetResetPin(int level);

    // Set when a pin's level or direction changed since the last call to ClearIoUpdated().
    int IoUpdated(void) const { return io_updated; };
    void ClearIoUpdated(void) { io_updated = 0; };

    struct msp430_cpu cpu;
    std::string error;       // last load/save or CPU error
    int halted;              // set when the CPU hit an invalid opcode; cleared by reset
    uint64_t cycle_count;    // MCLK cycles executed since power-on
    double mclk_hz;          // current MCLK frequency
    double smclk_hz, aclk_hz;
    int InLpm5(void) const { return lpm5_active; };

    uint8_t ram[0x1000];     // 0x2000-0x2FFF
    uint8_t fram[0x8000];    // 0x8000-0xFFFF
    uint8_t info[0x200];     // 0x1800-0x19FF
    uint8_t regs[0x1000];    // 0x0000-0x0FFF peripheral register file

private:
    // Memory bus, called by the CPU core.
    static uint8_t BusRead8(void* bus, uint32_t addr);
    static uint16_t BusRead16(void* bus, uint32_t addr);
    static void BusWrite8(void* bus, uint32_t addr, uint8_t value);
    static void BusWrite16(void* bus, uint32_t addr, uint16_t value);
    static void BusError(void* bus, const char* message);

    uint16_t ReadReg16(uint16_t addr);
    void WriteReg16(uint16_t addr, uint16_t value);
    uint8_t ReadReg8(uint16_t addr);
    void PmmCtl0Low(uint8_t value);
    int pmm_unlocked, frctl_unlocked;
    int nwaits_warned;
    void WriteReg8(uint16_t addr, uint8_t value);
    uint16_t Reg16(uint16_t addr) const { return regs[addr] | (regs[addr + 1] << 8); };
    void SetReg16(uint16_t addr, uint16_t value) {
        regs[addr] = value;
        regs[addr + 1] = value >> 8;
    };

    void Puc(uint16_t reset_cause);
    void RunCpu(void);
    int PendingInterrupt(void);
    void AcknowledgeInterrupt(int vector);

    // Ports
    static uint16_t PortBase(int port);
    static int PortOffset(int port);  // 0 for odd-numbered ports (low byte), 1 for even
    uint8_t PortReg(int port, int reg) const { return regs[PortBase(port) + reg + PortOffset(port)]; };
    void UpdatePins(void);
    int PeripheralOutput(int port, int bit, uint8_t* level);
    uint16_t PortIv(int port);

    TimerB tb[4];

    // Clock system
    void UpdateClocks(void);
    double dco_hz;
    uint16_t clk_sr;  // SR.SCG0 when the clocks were last derived

    // Hardware multiplier
    uint16_t MpyRead(uint16_t addr);
    void MpyWrite(uint16_t addr, uint16_t value, int byte);
    void MpyCompute(uint32_t op2, int op2_32);
    uint64_t mpy_res;
    uint32_t mpy_op1;
    int mpy_op1_32;
    uint16_t mpy_op2l;
    int mpy_mode;

    // LPM3.5 / LPM4.5
    void EnterLpm5(void);
    void WakeLpm5(void);
    int lpm5_active;
    int latch_valid;  // pins held at their LPMx.5 state until LOCKLPM5 is cleared
    uint8_t latched_out[PIN_COUNT];
    uint8_t latched_level[PIN_COUNT];
    int8_t latched_pull[PIN_COUNT];

    // Watchdog
    void WdtTick(double smclk_ticks, double aclk_ticks, double vlo_ticks);
    double wdt_count;

    uint32_t rst_pending;  // a bit per pending SYSRSTIV value (value / 2)

    double tick_hz;
    double mclk_owed;        // MCLK cycles still to run this tick (negative: the CPU is ahead)
    double vlo_hz;

    uint8_t pin_ext[PIN_COUNT];   // level driven by parts
    uint8_t pin_driven[PIN_COUNT];  // 1 once a part has driven the pin
    uint8_t pin_level[PIN_COUNT]; // resolved level
    uint8_t pin_out[PIN_COUNT];   // 1 if the chip drives the pin
    uint8_t port_in_last[6];      // last PxIN for edge detection
    int reset_pin;
    int io_updated;
};

#endif
