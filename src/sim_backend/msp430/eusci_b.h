/* eUSCI_B in SPI master mode, for the MSP430FR2355 model (SLAU445 chapter 23).
 *
 * Modeled: master mode with ACLK or SMCLK and the UCBRx divider; clock phase and polarity; MSB or LSB first;
 * 8- and 7-bit characters; the TX buffer and shift register, RX buffer and overrun (UCOE); UCBUSY;
 * loopback (UCLISTEN); a 4-pin STE output (UCSTEM = 1); UCTXIFG/UCRXIFG interrupts and UCBxIV.
 * Not modeled: slave mode and I2C.
 *
 * Clock edges appear on the pins at most once per simulation tick, so parts sampling the pins see every edge;
 * a bit clock faster than half the tick rate slows down to it (with a warning).
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef EUSCI_B_H
#define EUSCI_B_H

#include <stdint.h>

class EusciB {
public:
    void Init(uint16_t base, const char* name);
    void Reset(void);

    uint16_t Read(uint16_t offset, bool peek = false);
    void Write(uint16_t offset, uint16_t value);

    // Advances by the given ACLK and SMCLK periods (fractional) within one simulation tick.
    void Tick(double aclk_ticks, double smclk_ticks);

    // Pin levels the module drives while its pins are selected
    int Clk(void) const { return clk; };
    int Simo(void) const { return simo; };
    int Ste(void) const { return ste; };
    int DrivesSte(void) const;
    void SetSomi(int level) { somi = level != 0; };

    int Pending(void) const { return (ie & ifg & 3) != 0; };
    int TakeOutputChanged(void) {
        int c = out_changed;
        out_changed = 0;
        return c;
    };

    uint16_t base;

private:
    void StartByte(void);
    void Edge(void);
    void SetPin(uint8_t& pin, int level);
    int Bits(void) const;
    int BitAt(int i) const;  // bit i of the transmit byte, in transmission order

    const char* name;
    uint16_t ctlw0, ctlw1, brw, statw, ie, ifg;
    uint8_t txbuf, rxbuf;
    int tx_full;        // TXBUF holds a byte not yet moved to the shift register
    int shifting;       // a character is being shifted
    int ste_release;    // STE goes inactive at the next tick
    int setup_tick;     // a character started from idle this tick: no clock edge until the next
    uint8_t tx_shift, rx_shift;
    int edge;           // edges done in this character (two per bit)
    double phase;       // BRCLK periods since the last edge
    uint8_t clk, simo, ste, somi;
    int out_changed;
    int warned;
};

#endif
