/* MSP430FR2355 chip model for PICSimLab; see fr2355.h. */
#include "fr2355.h"

#include <stdio.h>
#include <string.h>

// Peripheral registers (msp430fr2355.h / msp430fr2355_symbols.ld)
enum {
    SFRIE1 = 0x0100,
    SFRIFG1 = 0x0102,
    SFRRPCR = 0x0104,
    PMMCTL0 = 0x0120,
    PMMIFG = 0x012A,
    PM5CTL0 = 0x0130,
    SYSRSTIV = 0x015E,
    SYSCFG0 = 0x0160,
    FRCTL0 = 0x01A0,
    WDTCTL = 0x01CC,
    CSCTL0 = 0x0180,
    CSCTL1 = 0x0182,
    CSCTL2 = 0x0184,
    CSCTL3 = 0x0186,
    CSCTL4 = 0x0188,
    CSCTL5 = 0x018A,
    CSCTL6 = 0x018C,
    CSCTL7 = 0x018E,
    CSCTL8 = 0x0190,
    MPY = 0x04C0,  // the 32-bit hardware multiplier, MPY32CTL0 at 0x04EC
    MPY32CTL0 = 0x04EC,
};

// SFR bits
enum { WDTIE = 0x01, WDTIFG = 0x01, OFIFG = 0x02 };
// Clock system bits
enum {
    DCORSEL = 0x000E,  // CSCTL1
    FLLD = 0x7000,     // CSCTL2
    FLLN = 0x03FF,
    SELREF = 0x0030,   // CSCTL3
    FLLREFDIV = 0x0007,
    SELA = 0x0300,     // CSCTL4
    SELMS = 0x0007,
    SMCLKOFF = 0x0100, // CSCTL5
    DIVS = 0x0030,
    DIVM = 0x0007,
    DCOFFG = 0x0001,   // CSCTL7
    XT1OFFG = 0x0002,
    FLLULIFG = 0x0010,
    FLLUNLOCK0 = 0x0100,
    FLLUNLOCK1 = 0x0200,
};
// WDTCTL bits
enum { WDTHOLD = 0x80, WDTSSEL = 0x60, WDTTMSEL = 0x10, WDTCNTCL = 0x08, WDTIS = 0x07 };
// PMMCTL0 / PMMIFG bits
enum { PMMSWBOR = 0x04, PMMSWPOR = 0x08, PMMREGOFF = 0x10, PMMBORIFG = 0x0100, PMMLPM5IFG = 0x8000 };
// SYSCFG0 bits
enum { PFWP = 0x01, DFWP = 0x02 };
// PM5CTL0 bits
enum { LOCKLPM5 = 0x01 };

// SYSRSTIV values, lowest = highest priority (SLAU445 table "SYSRSTIV")
enum {
    RST_BOR = 0x02,
    RST_RSTNMI = 0x04,
    RST_PMMSWBOR = 0x06,
    RST_LPM5WU = 0x08,
    RST_PMMSWPOR = 0x14,
    RST_WDTIFG = 0x16,
    RST_WDTPW = 0x18,
    RST_FRCTLPW = 0x1A,
};

// Interrupt vector numbers (msp430fr2355.h); the vector table entry is at 0xFFA2 + 2 * number.
enum {
    VEC_PORT4 = 22,
    VEC_PORT3 = 23,
    VEC_PORT2 = 24,
    VEC_PORT1 = 25,
    VEC_WDT = 34,
};

// Port register offsets from the pair's base (PxIN at +0 for odd ports, +1 for even ports)
enum { P_IN = 0x00, P_OUT = 0x02, P_DIR = 0x04, P_REN = 0x06, P_SEL0 = 0x0A, P_SEL1 = 0x0C, P_IES = 0x18,
       P_IE = 0x1A, P_IFG = 0x1C };

// TSSOP-38 pinout, from Figure 4-3 of the MSP430FR2355 datasheet (SLASEC4D).
const Fr2355::Pin Fr2355::pins[PIN_COUNT] = {
    {"P3.2", 3, 2}, {"P3.1", 3, 1}, {"P3.0", 3, 0}, {"P1.3", 1, 3}, {"P1.2", 1, 2}, {"P1.1", 1, 1},
    {"P1.0", 1, 0}, {"TEST", 0, 0}, {"/RST", 0, 0}, {"DVCC", 0, 0}, {"DVSS", 0, 0}, {"P2.7", 2, 7},
    {"P2.6", 2, 6}, {"P2.5", 2, 5}, {"P2.4", 2, 4}, {"P4.7", 4, 7}, {"P4.6", 4, 6}, {"P4.5", 4, 5},
    {"P4.4", 4, 4}, {"P4.3", 4, 3}, {"P4.2", 4, 2}, {"P4.1", 4, 1}, {"P4.0", 4, 0}, {"P2.3", 2, 3},
    {"P2.2", 2, 2}, {"P2.1", 2, 1}, {"P2.0", 2, 0}, {"P1.7", 1, 7}, {"P1.6", 1, 6}, {"P1.5", 1, 5},
    {"P1.4", 1, 4}, {"P3.7", 3, 7}, {"P3.6", 3, 6}, {"P3.5", 3, 5}, {"P3.4", 3, 4}, {"P5.1", 5, 1},
    {"P5.0", 5, 0}, {"P3.3", 3, 3},
};
static const int PIN_RST = 9, PIN_DVCC = 10;

Fr2355::Fr2355(void) {
    memset(&cpu, 0, sizeof(cpu));
    cpu.cpux = 1;
    cpu.bus = this;
    cpu.read8 = BusRead8;
    cpu.read16 = BusRead16;
    cpu.write8 = BusWrite8;
    cpu.write16 = BusWrite16;
    cpu.error = BusError;
    memset(ram, 0, sizeof(ram));
    memset(fram, 0xFF, sizeof(fram));
    memset(info, 0xFF, sizeof(info));
    memset(pin_ext, 0, sizeof(pin_ext));
    memset(pin_driven, 0, sizeof(pin_driven));
    tick_hz = 8e6;
    reset_pin = 1;
    PowerOnReset();
}

/* ------------------------------------------------------------------------------------------------ */
/* Reset                                                                                             */

void Fr2355::PowerOnReset(void) {
    memset(regs, 0, sizeof(regs));
    memset(pin_level, 0, sizeof(pin_level));
    memset(pin_out, 0, sizeof(pin_out));
    memset(port_in_last, 0, sizeof(port_in_last));
    SetReg16(PM5CTL0, LOCKLPM5);
    SetReg16(SFRRPCR, 0x000C);  // RST pin pull-up enabled, RST function
    SetReg16(PMMIFG, PMMBORIFG);
    rst_pending = 0;
    lpm5_active = 0;
    latch_valid = 0;
    Puc(RST_BOR);
}

// Power-up clear: the reset all reset sources end in. reset_cause is a SYSRSTIV value.
void Fr2355::Puc(uint16_t reset_cause) {
    rst_pending |= 1u << (reset_cause / 2);

    SetReg16(WDTCTL, 0x6904);  // watchdog running, SMCLK / 32768
    wdt_count = 0;
    SetReg16(SYSCFG0, PFWP | DFWP);
    SetReg16(FRCTL0, 0);
    pmm_unlocked = 0;
    frctl_unlocked = 0;
    nwaits_warned = 0;
    SetReg16(SFRIE1, 0);
    SetReg16(SFRIFG1, regs[SFRIFG1] & ~WDTIFG);
    for (int port = 1; port <= 6; port++) {
        uint16_t base = PortBase(port);
        int off = PortOffset(port);
        regs[base + P_DIR + off] = 0;
        regs[base + P_REN + off] = 0;
        regs[base + P_SEL0 + off] = 0;
        regs[base + P_SEL1 + off] = 0;
        regs[base + P_IE + off] = 0;
        regs[base + P_IFG + off] = 0;
    }

    // Clock system reset values (SLAU445 section 3.3): FLL locked to the 32768 Hz reference with FLLN = 31 and
    // FLLD = /2, so DCOCLK = 2.097 MHz and MCLK = SMCLK = DCOCLKDIV = 1.049 MHz; ACLK = XT1CLK, which falls
    // back to REFO because this board has no crystal.
    SetReg16(CSCTL0, 0x0100);
    SetReg16(CSCTL1, 0x0033);
    SetReg16(CSCTL2, 0x101F);
    SetReg16(CSCTL3, 0x0000);
    SetReg16(CSCTL4, 0x0000);
    SetReg16(CSCTL5, 0x0000);
    SetReg16(CSCTL6, 0x01C1);
    SetReg16(CSCTL7, XT1OFFG | DCOFFG);
    SetReg16(CSCTL8, 0x0007);
    dco_hz = 32 * 32768.0 * 2;
    vlo_hz = 10000;
    mclk_owed = 0;
    clk_sr = 0;

    // Hardware multiplier
    for (int a = MPY; a <= MPY32CTL0 + 1; a++)
        regs[a] = 0;
    mpy_res = 0;
    mpy_op1 = 0;
    mpy_op1_32 = 0;
    mpy_op2l = 0;
    mpy_mode = 0;

    memset(cpu.regs, 0, sizeof(cpu.regs));
    cpu.regs[MSP430_REG_PC] = fram[0xFFFE - 0x8000] | (fram[0xFFFF - 0x8000] << 8);
    UpdateClocks();
    halted = 0;
    error.clear();
    // Let the pins settle, then clear the edge flags that settling set: PxIFG is 0 after reset.
    UpdatePins();
    for (int port = 1; port <= 4; port++)
        regs[PortBase(port) + P_IFG + PortOffset(port)] = 0;
    io_updated = 1;
}

void Fr2355::SetResetPin(int level) {
    if (reset_pin && !level) {
        reset_pin = 0;
    } else if (!reset_pin && level) {
        reset_pin = 1;
        Puc(RST_RSTNMI);  // the RST pin causes a BOR on FR2xx parts
        SetReg16(PM5CTL0, LOCKLPM5);
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Memory bus                                                                                        */

uint8_t Fr2355::BusRead8(void* bus, uint32_t addr) {
    Fr2355* chip = (Fr2355*)bus;
    if (addr < 0x1000)
        return chip->ReadReg8(addr);
    if (addr >= 0x2000 && addr < 0x3000)
        return chip->ram[addr - 0x2000];
    if (addr >= 0x8000 && addr < 0x10000)
        return chip->fram[addr - 0x8000];
    if (addr >= 0x1800 && addr < 0x1A00)
        return chip->info[addr - 0x1800];
    return 0xFF;
}

uint16_t Fr2355::BusRead16(void* bus, uint32_t addr) {
    Fr2355* chip = (Fr2355*)bus;
    if (addr < 0x1000)
        return chip->ReadReg16(addr);
    return BusRead8(bus, addr) | (BusRead8(bus, addr + 1) << 8);
}

void Fr2355::BusWrite8(void* bus, uint32_t addr, uint8_t value) {
    Fr2355* chip = (Fr2355*)bus;
    if (addr < 0x1000) {
        chip->WriteReg8(addr, value);
    } else if (addr >= 0x2000 && addr < 0x3000) {
        chip->ram[addr - 0x2000] = value;
    } else if (addr >= 0x8000 && addr < 0x10000) {
        if (!(chip->regs[SYSCFG0] & PFWP))
            chip->fram[addr - 0x8000] = value;
    } else if (addr >= 0x1800 && addr < 0x1A00) {
        if (!(chip->regs[SYSCFG0] & DFWP))
            chip->info[addr - 0x1800] = value;
    }
}

void Fr2355::BusWrite16(void* bus, uint32_t addr, uint16_t value) {
    Fr2355* chip = (Fr2355*)bus;
    if (addr < 0x1000) {
        chip->WriteReg16(addr, value);
    } else {
        BusWrite8(bus, addr, value);
        BusWrite8(bus, addr + 1, value >> 8);
    }
}

void Fr2355::BusError(void* bus, const char* message) {
    Fr2355* chip = (Fr2355*)bus;
    chip->error = message;
}

/* ------------------------------------------------------------------------------------------------ */
/* Peripheral registers                                                                              */

uint16_t Fr2355::ReadReg16(uint16_t addr) {
    if (addr >= MPY && addr <= MPY32CTL0)
        return MpyRead(addr);
    switch (addr) {
        case WDTCTL:
            return (Reg16(WDTCTL) & 0x00FF) | 0x6900;
        case PMMCTL0:
        case SYSCFG0:
        case FRCTL0:
            return (Reg16(addr) & 0x00FF) | 0x9600;
        case SYSRSTIV: {
            for (int n = 1; n < 32; n++) {
                if (rst_pending & (1u << n)) {
                    rst_pending &= ~(1u << n);
                    return n * 2;
                }
            }
            return 0;
        }
    }
    for (int port = 1; port <= 4; port++) {
        if (addr == PortBase(port) + 0x0E + (port % 2 ? 0 : 0x10))
            return PortIv(port);
    }
    return Reg16(addr);
}

// PMMCTL0's low byte, written with the PMM unlocked
void Fr2355::PmmCtl0Low(uint8_t value) {
    if (value & PMMSWBOR) {
        Puc(RST_PMMSWBOR);
        SetReg16(PM5CTL0, LOCKLPM5);
        return;
    }
    if (value & PMMSWPOR) {
        Puc(RST_PMMSWPOR);
        return;
    }
    regs[PMMCTL0] = value;
}

uint8_t Fr2355::ReadReg8(uint16_t addr) {
    uint16_t word = ReadReg16(addr & ~1);
    return (addr & 1) ? word >> 8 : word & 0xFF;
}

void Fr2355::WriteReg16(uint16_t addr, uint16_t value) {
    switch (addr) {
        case WDTCTL:
            if ((value >> 8) != 0x5A) {
                Puc(RST_WDTPW);
                return;
            }
            if (value & WDTCNTCL)
                wdt_count = 0;
            SetReg16(WDTCTL, value & ~WDTCNTCL & 0xFF);
            return;
        case PMMCTL0:
            // A word write carries the password (PMMPW, 0xA5) in its high byte
            pmm_unlocked = (value >> 8) == 0xA5;
            if (pmm_unlocked)
                PmmCtl0Low(value & 0xFF);
            return;
        case SYSCFG0:
            // The high byte is the write password FRWPPW (0xA5); the low byte also holds other bits
            if ((value >> 8) == 0xA5)
                SetReg16(SYSCFG0, value & 0xFF);
            return;
        case FRCTL0:
            if ((value >> 8) != 0xA5) {
                Puc(RST_FRCTLPW);
                return;
            }
            frctl_unlocked = 1;
            SetReg16(FRCTL0, value & 0xFF);
            return;
        case PM5CTL0:
            // LOCKLPM5 can only be cleared by software; clearing it releases pins held since LPMx.5
            SetReg16(PM5CTL0, Reg16(PM5CTL0) & value);
            if (!(regs[PM5CTL0] & LOCKLPM5))
                latch_valid = 0;
            UpdatePins();
            return;
        case SYSRSTIV:
            return;
        case CSCTL7:
            // Fault flags are cleared by writing 0; UpdateClocks sets XT1OFFG again while XT1 is wanted
            SetReg16(CSCTL7, value);
            UpdateClocks();
            return;
    }
    if (addr >= MPY && addr <= MPY32CTL0) {
        MpyWrite(addr, value, 0);
        return;
    }
    SetReg16(addr, value);
    if (addr >= 0x0200 && addr < 0x0260)
        UpdatePins();
    else if ((addr >= CSCTL0 && addr <= CSCTL8) || addr == SFRIFG1)
        UpdateClocks();
}

void Fr2355::WriteReg8(uint16_t addr, uint8_t value) {
    uint16_t word = addr & ~1;
    switch (word) {
        case WDTCTL:
            // The watchdog only takes word writes with its password; anything else is a violation
            Puc(RST_WDTPW);
            return;
        case PMMCTL0:
            // Writing the password to PMMCTL0_H unlocks the PMM registers for byte writes until a wrong
            // password or a PUC
            if (addr & 1)
                pmm_unlocked = value == 0xA5;
            else if (pmm_unlocked)
                PmmCtl0Low(value);
            return;
        case FRCTL0:
            // Likewise FRCTL0_H; a wrong password, or writing FRCTL0_L while locked, is a violation
            if (addr & 1) {
                if (value != 0xA5) {
                    Puc(RST_FRCTLPW);
                    return;
                }
                frctl_unlocked = 1;
            } else if (frctl_unlocked) {
                regs[FRCTL0] = value;
            } else {
                Puc(RST_FRCTLPW);
            }
            return;
        case SYSCFG0:
        case PM5CTL0:
        case SYSRSTIV:
            if (addr & 1)
                return;
            WriteReg16(word, (ReadReg16(word) & 0xFF00) | value);
            return;
    }
    if (word >= MPY && word <= MPY32CTL0) {
        if (addr & 1)  // high byte: merge into the word without the operand extension byte writes get
            MpyWrite(word, (Reg16(word) & 0x00FF) | (value << 8), 0);
        else
            MpyWrite(word, value, 1);
        return;
    }
    regs[addr] = value;
    if (addr >= 0x0200 && addr < 0x0260)
        UpdatePins();
    else if ((word >= CSCTL0 && word <= CSCTL8) || word == SFRIFG1)
        UpdateClocks();
}

/* ------------------------------------------------------------------------------------------------ */
/* Ports                                                                                             */

// P1/P2 at 0x200, P3/P4 at 0x220, P5/P6 at 0x240
uint16_t Fr2355::PortBase(int port) {
    return 0x0200 + ((port - 1) / 2) * 0x20;
}

int Fr2355::PortOffset(int port) {
    return (port % 2) ? 0 : 1;
}

// PxIV: 2 * (n + 1) for the lowest pending, enabled bit n, which reading clears.
uint16_t Fr2355::PortIv(int port) {
    uint16_t ifg_addr = PortBase(port) + P_IFG + PortOffset(port);
    uint8_t pending = regs[ifg_addr] & PortReg(port, P_IE);
    for (int n = 0; n < 8; n++) {
        if (pending & (1 << n)) {
            regs[ifg_addr] &= ~(1 << n);
            return 2 * (n + 1);
        }
    }
    return 0;
}

void Fr2355::UpdatePins(void) {
    int locked = regs[PM5CTL0] & LOCKLPM5;

    for (int i = 0; i < PIN_COUNT; i++) {
        const Pin& p = pins[i];
        uint8_t level, out;
        if (p.port == 0) {
            out = (i + 1 == PIN_DVCC) || (i + 1 == PIN_DVCC + 1);
            level = (i + 1 == PIN_DVCC) ? 1 : (i + 1 == PIN_RST) ? reset_pin : (i + 1 == PIN_DVCC + 1) ? 0 : pin_ext[i];
        } else {
            uint8_t mask = 1 << p.bit;
            int sel = (PortReg(p.port, P_SEL0) | PortReg(p.port, P_SEL1)) & mask;
            int dir = PortReg(p.port, P_DIR) & mask;
            if (locked && latch_valid) {
                // Held as they were when the chip entered LPMx.5
                out = latched_out[i];
                if (out)
                    level = latched_level[i];
                else if (pin_driven[i])
                    level = pin_ext[i];
                else
                    level = latched_pull[i] > 0;
            } else if (dir && !sel && !locked) {
                out = 1;
                level = (PortReg(p.port, P_OUT) & mask) != 0;
            } else if (pin_driven[i]) {
                out = 0;
                level = pin_ext[i];
            } else {
                // Nothing drives the pin: with PxREN set it follows the pull PxOUT selects, else it floats
                // (read as 0).
                out = 0;
                level = (PortReg(p.port, P_REN) & mask) ? (PortReg(p.port, P_OUT) & mask) != 0 : 0;
            }
        }
        if (level != pin_level[i] || out != pin_out[i]) {
            pin_level[i] = level;
            pin_out[i] = out;
            io_updated = 1;
        }
    }

    // PxIN, and pin interrupt flags on the edge PxIES selects (0: rising, 1: falling)
    for (int port = 1; port <= 6; port++) {
        uint8_t in = 0;
        for (int i = 0; i < PIN_COUNT; i++) {
            if (pins[i].port == port && pin_level[i])
                in |= 1 << pins[i].bit;
        }
        uint16_t base = PortBase(port);
        int off = PortOffset(port);
        regs[base + P_IN + off] = in;
        if (port <= 4) {
            uint8_t last = port_in_last[port - 1];
            uint8_t ies = regs[base + P_IES + off];
            uint8_t rising = in & ~last & ~ies;
            uint8_t falling = ~in & last & ies;
            regs[base + P_IFG + off] |= rising | falling;
        }
        port_in_last[port - 1] = in;
    }
}

int Fr2355::GetPinLevel(int pin) const {
    if (pin < 1 || pin > PIN_COUNT)
        return 0;
    return pin_level[pin - 1];
}

int Fr2355::GetPinIsOutput(int pin) const {
    if (pin < 1 || pin > PIN_COUNT)
        return 0;
    return pin_out[pin - 1];
}

void Fr2355::SetPinInput(int pin, int level) {
    if (pin < 1 || pin > PIN_COUNT)
        return;
    if (pin == PIN_RST) {
        SetResetPin(level);
        return;
    }
    if (!pin_driven[pin - 1] || pin_ext[pin - 1] != (level != 0)) {
        pin_driven[pin - 1] = 1;
        pin_ext[pin - 1] = level != 0;
        UpdatePins();
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Watchdog                                                                                          */

void Fr2355::WdtTick(double smclk_ticks, double aclk_ticks, double vlo_ticks) {
    uint8_t ctl = regs[WDTCTL];
    if (ctl & WDTHOLD)
        return;
    switch (ctl & WDTSSEL) {
        case 0x00:
            wdt_count += smclk_ticks;
            break;
        case 0x20:
            wdt_count += aclk_ticks;
            break;
        default:
            wdt_count += vlo_ticks;
            break;
    }
    static const double interval[8] = {2147483648.0, 134217728.0, 8388608.0, 524288.0, 32768.0, 8192.0, 512.0, 64.0};
    if (wdt_count >= interval[ctl & WDTIS]) {
        wdt_count = 0;
        if (ctl & WDTTMSEL)
            regs[SFRIFG1] |= WDTIFG;  // interval mode: interrupt flag
        else
            Puc(RST_WDTIFG);  // watchdog mode: reset
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Clock system (SLAU445 chapter 3)                                                                  */

// Derives DCOCLK, MCLK, SMCLK and ACLK from the CS registers. The FLL locks instantly while it runs (SR.SCG0
// clear); with SCG0 set the DCO keeps its last frequency. The board has no crystal, so XT1 never starts:
// whenever something asks for XT1CLK, XT1OFFG stays set and the request falls back to REFO, as the chip does.
void Fr2355::UpdateClocks(void) {
    const double REFO = 32768.0;
    uint16_t ctl1 = Reg16(CSCTL1), ctl2 = Reg16(CSCTL2), ctl3 = Reg16(CSCTL3), ctl4 = Reg16(CSCTL4),
             ctl5 = Reg16(CSCTL5), ctl7 = Reg16(CSCTL7);

    int xt1_wanted = (ctl4 & SELA) == 0x0000 || (ctl3 & SELREF) == 0x0000 || (ctl4 & SELMS) == 2;
    if (xt1_wanted)
        ctl7 |= XT1OFFG;

    static const double refdiv[8] = {1, 32, 64, 128, 256, 512, 640, 768};
    static const double flld[8] = {1, 2, 4, 8, 16, 32, 32, 32};
    double d = flld[(ctl2 & FLLD) >> 12];
    if (!(cpu.regs[MSP430_REG_SR] & MSP430_SR_SCG0)) {
        // FLL running: DCOCLK = D * (FLLN + 1) * fFLLREFCLK / FLLREFDIV, within the DCORSEL range's reach
        double target = d * ((ctl2 & FLLN) + 1) * REFO / refdiv[ctl3 & FLLREFDIV];
        static const double range_mhz[8] = {1, 2, 4, 8, 12, 16, 20, 24};
        double nominal = range_mhz[(ctl1 & DCORSEL) >> 1] * 1e6;
        double lo = nominal * 0.5, hi = nominal * 1.5;
        ctl7 &= ~(FLLUNLOCK0 | FLLUNLOCK1);
        if (target < lo) {
            dco_hz = lo;
            ctl7 |= FLLUNLOCK0 | FLLULIFG;  // DCO too fast for the target, at the bottom of its range
        } else if (target > hi) {
            dco_hz = hi;
            ctl7 |= FLLUNLOCK1 | FLLULIFG;  // DCO too slow for the target, at the top of its range
        } else {
            dco_hz = target;
        }
    }
    SetReg16(CSCTL7, ctl7);

    if (ctl7 & (XT1OFFG | DCOFFG))
        regs[SFRIFG1] |= OFIFG;

    double dcoclkdiv = dco_hz / d;
    double ms_source;
    switch (ctl4 & SELMS) {
        case 0:
            ms_source = dcoclkdiv;
            break;
        case 3:
            ms_source = vlo_hz;
            break;
        default:  // REFO, or XT1 falling back to REFO
            ms_source = REFO;
            break;
    }
    mclk_hz = ms_source / (1 << (ctl5 & DIVM));
    smclk_hz = (ctl5 & SMCLKOFF) ? 0 : mclk_hz / (1 << ((ctl5 & DIVS) >> 4));
    aclk_hz = ((ctl4 & SELA) == 0x0200) ? vlo_hz : REFO;
    clk_sr = cpu.regs[MSP430_REG_SR] & MSP430_SR_SCG0;
}

/* ------------------------------------------------------------------------------------------------ */
/* 32-bit hardware multiplier, MPY32 (SLAU445 chapter 5)                                             */
/* Saturation (MPYSAT) and fractional (MPYFRAC) modes and delayed writes aren't modeled.            */

enum { MPYOP1_32 = 0x0040, MPYOP2_32 = 0x0080, MPYC = 0x0001 };
enum { MODE_MPY, MODE_MPYS, MODE_MAC, MODE_MACS };

uint16_t Fr2355::MpyRead(uint16_t addr) {
    switch (addr - MPY) {
        case 0x0A:  // RESLO
        case 0x24:  // RES0
            return mpy_res;
        case 0x0C:  // RESHI
        case 0x26:  // RES1
            return mpy_res >> 16;
        case 0x28:  // RES2
            return mpy_res >> 32;
        case 0x2A:  // RES3
            return mpy_res >> 48;
    }
    return Reg16(addr);
}

// byte: the CPU wrote just the low byte, which extends to 16 bits (signed in the signed modes).
void Fr2355::MpyWrite(uint16_t addr, uint16_t value, int byte) {
    int reg = addr - MPY;
    int signed_mode = (mpy_mode == MODE_MPYS || mpy_mode == MODE_MACS);
    SetReg16(addr, value);

    if (reg <= 0x06 || (reg >= 0x10 && reg <= 0x1E)) {
        // First operand. 0x00-0x06: MPY, MPYS, MAC, MACS (16 bit). 0x10-0x1E: their 32-bit L/H pairs.
        int is32 = reg >= 0x10;
        mpy_mode = is32 ? (reg - 0x10) / 4 : reg / 2;
        signed_mode = (mpy_mode == MODE_MPYS || mpy_mode == MODE_MACS);
        if (byte)
            value = signed_mode ? (uint16_t)(int16_t)(int8_t)value : (value & 0xFF);
        if (!is32) {
            mpy_op1 = value;
            mpy_op1_32 = 0;
        } else if ((reg & 2) == 0) {
            mpy_op1 = (mpy_op1 & 0xFFFF0000u) | value;  // low word
            mpy_op1_32 = 1;
        } else {
            mpy_op1 = (mpy_op1 & 0x0000FFFFu) | ((uint32_t)value << 16);  // high word
            mpy_op1_32 = 1;
        }
    } else if (reg == 0x08) {  // OP2: 16-bit second operand, starts the multiplication
        if (byte)
            value = signed_mode ? (uint16_t)(int16_t)(int8_t)value : (value & 0xFF);
        MpyCompute(value, 0);
    } else if (reg == 0x20) {  // OP2L
        mpy_op2l = value;
    } else if (reg == 0x22) {  // OP2H: 32-bit second operand complete, starts the multiplication
        MpyCompute(((uint32_t)value << 16) | mpy_op2l, 1);
    } else if (reg == 0x0A || reg == 0x24) {  // RESLO / RES0
        mpy_res = (mpy_res & ~0xFFFFull) | value;
    } else if (reg == 0x0C || reg == 0x26) {  // RESHI / RES1
        mpy_res = (mpy_res & ~(0xFFFFull << 16)) | ((uint64_t)value << 16);
    } else if (reg == 0x28) {  // RES2
        mpy_res = (mpy_res & ~(0xFFFFull << 32)) | ((uint64_t)value << 32);
    } else if (reg == 0x2A) {  // RES3
        mpy_res = (mpy_res & ~(0xFFFFull << 48)) | ((uint64_t)value << 48);
    }
}

void Fr2355::MpyCompute(uint32_t op2, int op2_32) {
    int signed_mode = (mpy_mode == MODE_MPYS || mpy_mode == MODE_MACS);
    int accumulate = (mpy_mode == MODE_MAC || mpy_mode == MODE_MACS);
    int wide = mpy_op1_32 || op2_32;  // any 32-bit operand gives a 64-bit result
    uint64_t product;
    if (signed_mode) {
        int64_t a = mpy_op1_32 ? (int64_t)(int32_t)mpy_op1 : (int64_t)(int16_t)mpy_op1;
        int64_t b = op2_32 ? (int64_t)(int32_t)op2 : (int64_t)(int16_t)op2;
        product = (uint64_t)(a * b);
    } else {
        uint64_t a = mpy_op1_32 ? mpy_op1 : (mpy_op1 & 0xFFFF);
        uint64_t b = op2_32 ? op2 : (op2 & 0xFFFF);
        product = a * b;
    }

    uint64_t mask = wide ? ~0ull : 0xFFFFFFFFull;
    uint64_t result;
    int carry = 0;
    if (accumulate) {
        uint64_t old = mpy_res & mask;
        result = (old + (product & mask)) & mask;
        carry = result < old || (!wide && ((old + (product & mask)) >> 32));
    } else {
        result = product & mask;
    }

    int negative = wide ? (int64_t)result < 0 : (int32_t)result < 0;
    uint16_t sumext;
    if (signed_mode)
        sumext = negative ? 0xFFFF : 0;
    else
        sumext = carry;
    if (!wide)  // 16 x 16: RES2/RES3 follow the 32-bit result's extension
        result |= (signed_mode && negative) ? 0xFFFFFFFF00000000ull : 0;
    mpy_res = result;
    SetReg16(MPY + 0x0E, sumext);  // SUMEXT
    uint16_t ctl = Reg16(MPY32CTL0) & ~(MPYOP1_32 | MPYOP2_32 | MPYC);
    if (mpy_op1_32)
        ctl |= MPYOP1_32;
    if (op2_32)
        ctl |= MPYOP2_32;
    if (carry)
        ctl |= MPYC;
    SetReg16(MPY32CTL0, ctl);
}

/* ------------------------------------------------------------------------------------------------ */
/* LPM3.5 / LPM4.5                                                                                    */

// Entered when the CPU sleeps in LPM3 or LPM4 with PMMREGOFF set: the core and RAM lose power, pins hold
// their state, and only a port interrupt (P1-P4, with PxIE set) can wake the chip, through a BOR.
void Fr2355::EnterLpm5(void) {
    lpm5_active = 1;
    latch_valid = 1;
    for (int i = 0; i < PIN_COUNT; i++) {
        const Pin& p = pins[i];
        latched_out[i] = pin_out[i];
        latched_level[i] = pin_level[i];
        latched_pull[i] = -1;
        if (p.port && (PortReg(p.port, P_REN) & (1 << p.bit)))
            latched_pull[i] = (PortReg(p.port, P_OUT) >> p.bit) & 1;
    }
}

void Fr2355::WakeLpm5(void) {
    // The wake-up flags and port interrupt settings survive; everything else starts from reset.
    uint8_t keep[4][3];
    for (int port = 1; port <= 4; port++) {
        keep[port - 1][0] = PortReg(port, P_IES);
        keep[port - 1][1] = PortReg(port, P_IE);
        keep[port - 1][2] = PortReg(port, P_IFG);
    }
    // RAM didn't keep power: fill it with a pattern so firmware that relies on it shows up
    for (unsigned i = 0; i < sizeof(ram); i++)
        ram[i] = (uint8_t)(i * 151 + 0x5A);
    lpm5_active = 0;
    SetReg16(PM5CTL0, LOCKLPM5);
    SetReg16(PMMCTL0, 0);
    SetReg16(PMMIFG, Reg16(PMMIFG) | PMMLPM5IFG);
    Puc(RST_LPM5WU);
    for (int port = 1; port <= 4; port++) {
        uint16_t base = PortBase(port);
        int off = PortOffset(port);
        regs[base + P_IES + off] = keep[port - 1][0];
        regs[base + P_IE + off] = keep[port - 1][1];
        regs[base + P_IFG + off] = keep[port - 1][2];
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Interrupts and execution                                                                          */

// Highest-priority pending, enabled maskable interrupt vector number, or -1.
int Fr2355::PendingInterrupt(void) {
    if ((regs[SFRIFG1] & WDTIFG) && (regs[SFRIE1] & WDTIE))
        return VEC_WDT;
    for (int port = 1; port <= 4; port++) {
        if (PortReg(port, P_IFG) & PortReg(port, P_IE))
            return VEC_PORT1 + 1 - port;
    }
    return -1;
}

// Clears the flags of single-source interrupts when the CPU takes them.
void Fr2355::AcknowledgeInterrupt(int vector) {
    if (vector == VEC_WDT)
        regs[SFRIFG1] &= ~WDTIFG;
}

void Fr2355::RunCpu(void) {
    // FRAM can't be read faster than 8 MHz without a wait state (datasheet: "FRAM access" table). The chip
    // then misreads code; the model runs on but says so, once per reset.
    if (mclk_hz > 8.1e6 && !(regs[FRCTL0] & 0x70) && !nwaits_warned) {
        fprintf(stderr, "msp430: MCLK is %.1f MHz but FRCTL0.NWAITS is 0: the real chip needs "
                        "FRCTL0 = FRCTLPW | NWAITS_1 above 8 MHz (PC = 0x%05X)\n",
                mclk_hz / 1e6, cpu.regs[MSP430_REG_PC]);
        nwaits_warned = 1;
    }
    mclk_owed += mclk_hz / tick_hz;
    while (mclk_owed > 0) {
        int vector = (cpu.regs[MSP430_REG_SR] & MSP430_SR_GIE) ? PendingInterrupt() : -1;
        int cycles;
        if (vector >= 0) {
            cycles = msp430_cpu_interrupt(&cpu, 0xFFA2 + 2 * vector);
            AcknowledgeInterrupt(vector);
        } else if (cpu.regs[MSP430_REG_SR] & MSP430_SR_CPUOFF) {
            mclk_owed = 0;  // in a low-power mode: nothing to run until an interrupt
            uint16_t lpm34 = MSP430_SR_CPUOFF | MSP430_SR_SCG0 | MSP430_SR_SCG1;
            if ((regs[PMMCTL0] & PMMREGOFF) && (cpu.regs[MSP430_REG_SR] & lpm34) == lpm34)
                EnterLpm5();
            break;
        } else {
            cycles = msp430_cpu_step(&cpu);
            if (cycles < 0) {
                halted = 1;
                fprintf(stderr, "msp430: %s\n", error.c_str());
                return;
            }
        }
        mclk_owed -= cycles;
        cycle_count += cycles;
    }
}

void Fr2355::Tick(void) {
    if (!reset_pin)
        return;
    if (lpm5_active) {
        // Only a port interrupt flag with its enable set wakes the chip from LPMx.5
        for (int port = 1; port <= 4; port++) {
            if (PortReg(port, P_IFG) & PortReg(port, P_IE)) {
                WakeLpm5();
                break;
            }
        }
        return;
    }
    if (!halted)
        RunCpu();
    uint16_t sr = cpu.regs[MSP430_REG_SR];
    if ((sr & MSP430_SR_SCG0) != clk_sr)
        UpdateClocks();  // the FLL was switched on or off
    // SCG1 (LPM2-LPM4) stops SMCLK; OSCOFF (LPM4) stops ACLK
    double smclk = (sr & MSP430_SR_SCG1) ? 0 : smclk_hz;
    double aclk = (sr & MSP430_SR_OSCOFF) ? 0 : aclk_hz;
    WdtTick(smclk / tick_hz, aclk / tick_hz, vlo_hz / tick_hz);
}

/* ------------------------------------------------------------------------------------------------ */
/* Firmware images                                                                                   */

void Fr2355::EraseFram(void) {
    memset(fram, 0xFF, sizeof(fram));
    memset(info, 0xFF, sizeof(info));
}

static int hexval(const char* s, int n) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= c - '0';
        else if (c >= 'A' && c <= 'F')
            v |= c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            v |= c - 'a' + 10;
        else
            return -1;
    }
    return v;
}

int Fr2355::LoadHex(const char* fname) {
    FILE* f = fopen(fname, "r");
    if (!f) {
        error = std::string("can't open ") + fname;
        return -1;
    }
    char line[600];
    uint32_t upper = 0;
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        if (line[0] != ':')
            continue;
        int count = hexval(line + 1, 2);
        int addr = hexval(line + 3, 4);
        int type = hexval(line + 7, 2);
        if (count < 0 || addr < 0 || type < 0) {
            error = "bad record on line " + std::to_string(lineno) + " of " + fname;
            fclose(f);
            return -1;
        }
        int sum = count + (addr >> 8) + (addr & 0xFF) + type;
        uint8_t data[256];
        for (int i = 0; i <= count; i++) {
            int b = hexval(line + 9 + 2 * i, 2);
            if (b < 0) {
                error = "bad record on line " + std::to_string(lineno) + " of " + fname;
                fclose(f);
                return -1;
            }
            if (i < count)
                data[i] = b;
            sum += b;
        }
        if (sum & 0xFF) {
            error = "checksum error on line " + std::to_string(lineno) + " of " + fname;
            fclose(f);
            return -1;
        }
        if (type == 0x00) {
            for (int i = 0; i < count; i++) {
                uint32_t a = upper + addr + i;
                if (a >= 0x8000 && a < 0x10000)
                    fram[a - 0x8000] = data[i];
                else if (a >= 0x1800 && a < 0x1A00)
                    info[a - 0x1800] = data[i];
                else if (a >= 0x2000 && a < 0x3000)
                    ram[a - 0x2000] = data[i];
            }
        } else if (type == 0x01) {
            break;
        } else if (type == 0x02) {
            upper = ((data[0] << 8) | data[1]) << 4;
        } else if (type == 0x04) {
            upper = ((data[0] << 8) | data[1]) << 16;
        }
    }
    fclose(f);
    return 0;
}

static void hex_record(FILE* f, int type, uint16_t addr, const uint8_t* data, int count) {
    int sum = count + (addr >> 8) + (addr & 0xFF) + type;
    fprintf(f, ":%02X%04X%02X", count, addr, type);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%02X", data[i]);
        sum += data[i];
    }
    fprintf(f, "%02X\n", (-sum) & 0xFF);
}

int Fr2355::SaveHex(const char* fname) {
    FILE* f = fopen(fname, "w");
    if (!f) {
        error = std::string("can't write ") + fname;
        return -1;
    }
    struct {
        uint32_t base;
        const uint8_t* mem;
        uint32_t size;
    } areas[] = {{0x1800, info, sizeof(info)}, {0x8000, fram, sizeof(fram)}};
    for (auto& area : areas) {
        for (uint32_t off = 0; off < area.size; off += 16) {
            int blank = 1;
            for (int i = 0; i < 16; i++)
                blank &= area.mem[off + i] == 0xFF;
            if (!blank)
                hex_record(f, 0x00, area.base + off, area.mem + off, 16);
        }
    }
    hex_record(f, 0x01, 0, NULL, 0);
    fclose(f);
    return 0;
}
