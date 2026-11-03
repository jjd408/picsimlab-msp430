/* eUSCI_B in SPI master mode for the MSP430FR2355 model; see eusci_b.h. */
#include "eusci_b.h"

#include <stdio.h>

// Register offsets from UCBxCTLW0
enum { O_CTLW0 = 0x00, O_CTLW1 = 0x02, O_BRW = 0x06, O_STATW = 0x08, O_RXBUF = 0x0C, O_TXBUF = 0x0E, O_IE = 0x2A,
       O_IFG = 0x2C, O_IV = 0x2E };
// UCBxCTLW0
enum { UCCKPH = 0x8000, UCCKPL = 0x4000, UCMSB = 0x2000, UC7BIT = 0x1000, UCMST = 0x0800, UCMODE = 0x0600,
       UCSSEL = 0x00C0, UCSTEM = 0x0002, UCSWRST = 0x0001 };
// UCBxSTATW
enum { UCBUSY = 0x0001, UCOE = 0x0020, UCLISTEN = 0x0080 };
// UCBxIE / UCBxIFG
enum { UCRXIFG = 0x01, UCTXIFG = 0x02 };

void EusciB::Init(uint16_t base_, const char* name_) {
    base = base_;
    name = name_;
    warned = 0;
    Reset();
}

void EusciB::Reset(void) {
    ctlw0 = 0x01C1;  // UCSWRST, UCSYNC, UCSSEL = SMCLK
    ctlw1 = brw = statw = ie = 0;
    ifg = UCTXIFG;
    txbuf = rxbuf = 0;
    tx_full = shifting = 0;
    ste_release = 0;
    setup_tick = 0;
    tx_shift = rx_shift = 0;
    edge = 0;
    phase = 0;
    clk = simo = ste = somi = 0;
    out_changed = 1;
}

int EusciB::Bits(void) const {
    return (ctlw0 & UC7BIT) ? 7 : 8;
}

int EusciB::BitAt(int i) const {
    return (ctlw0 & UCMSB) ? (tx_shift >> (Bits() - 1 - i)) & 1 : (tx_shift >> i) & 1;
}

int EusciB::DrivesSte(void) const {
    int mode = (ctlw0 & UCMODE) >> 9;
    return (ctlw0 & UCMST) && (ctlw0 & UCSTEM) && (mode == 1 || mode == 2);
}

void EusciB::SetPin(uint8_t& pin, int level) {
    level = level != 0;
    if (pin != level) {
        pin = level;
        out_changed = 1;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Registers                                                                                         */

uint16_t EusciB::Read(uint16_t offset) {
    switch (offset) {
        case O_CTLW0:
            return ctlw0;
        case O_CTLW1:
            return ctlw1;
        case O_BRW:
            return brw;
        case O_STATW:
            return statw;
        case O_RXBUF:
            ifg &= ~UCRXIFG;  // reading RXBUF clears UCRXIFG and UCOE
            statw &= ~UCOE;
            return rxbuf;
        case O_TXBUF:
            return txbuf;
        case O_IE:
            return ie;
        case O_IFG:
            return ifg;
        case O_IV:
            // UCRXIFG (2) before UCTXIFG (4); reading clears the flag reported
            if (ie & ifg & UCRXIFG) {
                ifg &= ~UCRXIFG;
                return 0x02;
            }
            if (ie & ifg & UCTXIFG) {
                ifg &= ~UCTXIFG;
                return 0x04;
            }
            return 0;
    }
    return 0;
}

void EusciB::Write(uint16_t offset, uint16_t value) {
    switch (offset) {
        case O_CTLW0:
            ctlw0 = value;
            if (value & UCSWRST) {
                // Held in reset: interrupts off, flags as after reset, any transfer abandoned
                ie &= ~(UCRXIFG | UCTXIFG);
                ifg = UCTXIFG;
                statw &= ~(UCBUSY | UCOE);
                shifting = tx_full = 0;
                SetPin(ste, (ctlw0 & UCMODE) == 0x0400);  // STE inactive (active-low mode: high)
            }
            if (!shifting)
                SetPin(clk, (ctlw0 & UCCKPL) != 0);  // idle clock level
            break;
        case O_CTLW1:
            ctlw1 = value;
            break;
        case O_BRW:
            brw = value;
            break;
        case O_STATW:
            statw = (statw & UCBUSY) | (value & ~UCBUSY);
            break;
        case O_TXBUF:
            if (ctlw0 & UCSWRST)
                break;
            txbuf = value;
            ifg &= ~UCTXIFG;
            tx_full = 1;
            if (!shifting) {
                StartByte();
                // From idle, the first clock edge waits for the next tick, so STE and the first data bit
                // are set up before it (back-to-back characters follow without a gap)
                setup_tick = 1;
            }
            break;
        case O_IE:
            ie = value & (UCRXIFG | UCTXIFG);
            break;
        case O_IFG:
            ifg = value & (UCRXIFG | UCTXIFG);
            break;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Shifting                                                                                          */

// Moves TXBUF into the shift register and starts the character.
void EusciB::StartByte(void) {
    tx_shift = txbuf;
    tx_full = 0;
    ifg |= UCTXIFG;  // TXBUF is free again
    shifting = 1;
    statw |= UCBUSY;
    edge = 0;
    rx_shift = 0;
    if (DrivesSte())
        SetPin(ste, (ctlw0 & UCMODE) == 0x0200);  // active-high mode: high
    if (ctlw0 & UCCKPH)
        SetPin(simo, BitAt(0));  // UCCKPH = 1: the first bit is out before the first edge
}

void EusciB::Edge(void) {
    int bits = Bits();
    int i = edge / 2;
    int leading = (edge % 2) == 0;
    int ckph = (ctlw0 & UCCKPH) != 0;
    int pol = (ctlw0 & UCCKPL) != 0;

    SetPin(clk, leading ? !pol : pol);
    // UCCKPH = 1: capture on the leading edge, change on the trailing one; UCCKPH = 0: the other way round
    if (leading == ckph) {
        int in = (statw & UCLISTEN) ? simo : somi;
        int pos = (ctlw0 & UCMSB) ? bits - 1 - i : i;
        if (in)
            rx_shift |= 1 << pos;
    } else {
        int next = ckph ? i + 1 : i;
        if (next < bits)
            SetPin(simo, BitAt(next));
    }

    if (++edge < 2 * bits)
        return;

    // Character done
    if (ifg & UCRXIFG)
        statw |= UCOE;  // the last character wasn't read
    rxbuf = rx_shift;
    ifg |= UCRXIFG;
    shifting = 0;
    if (tx_full) {
        StartByte();
    } else {
        statw &= ~UCBUSY;
        // STE goes inactive a tick after the last clock edge, so a device sees that edge with STE still
        // active (chip-select hold time)
        ste_release = DrivesSte();
    }
}

void EusciB::Tick(double aclk_ticks, double smclk_ticks) {
    if (ste_release) {
        ste_release = 0;
        if (!shifting)
            SetPin(ste, (ctlw0 & UCMODE) == 0x0400);
    }
    if (!shifting || (ctlw0 & UCSWRST) || !(ctlw0 & UCMST))
        return;
    if (setup_tick) {
        setup_tick = 0;
        return;
    }
    double brclk;
    switch ((ctlw0 & UCSSEL) >> 6) {
        case 1:
            brclk = aclk_ticks;
            break;
        case 2:
        case 3:
            brclk = smclk_ticks;
            break;
        default:
            return;  // UCLK from outside: slave mode, not modeled
    }
    double half = (brw ? brw : 1) / 2.0;  // BRCLK periods per clock edge
    phase += brclk;
    if (phase < half)
        return;
    Edge();
    phase -= half;
    if (phase >= half) {
        // More than one edge was due in this tick: keep to one so every edge reaches the pins
        if (!warned) {
            fprintf(stderr, "msp430: %s SPI clock is faster than half the simulation clock; transfers slow down "
                            "to it (raise PICSimLab's Clk setting for full speed)\n", name);
            warned = 1;
        }
        phase = 0;
    }
}
