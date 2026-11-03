/* Timer_B for the MSP430FR2355 model (SLAU445 chapter 14).
 *
 * Modeled: up, continuous and up/down modes; 16/12/10/8-bit counter lengths; ACLK and SMCLK sources with the
 * ID and TBIDEX dividers; compare with all eight output modes and the CLLD compare latch; capture from the
 * CCIxA pin or by software (CCIS = GND/VCC); CCIFG, COV, TBIFG and TBxIV. The TBxCLK/INCLK pin sources,
 * TBxCCR grouping (TBCLGRP), synchronized capture (SCS) and the internal CCIxB signals aren't modeled.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef TIMER_B_H
#define TIMER_B_H

#include <stdint.h>

class TimerB {
public:
    static const int MAX_CCR = 7;

    // base: the timer's register block (TBxCTL); ccrs: 3 for TB0-TB2, 7 for TB3.
    void Init(uint16_t base, int ccrs);
    void Reset(void);

    uint16_t Read(uint16_t offset);
    void Write(uint16_t offset, uint16_t value);

    // Advances by the given ACLK and SMCLK clock periods (fractional), as gated by the low-power mode.
    void Advance(double aclk_ticks, double smclk_ticks);
    // The level on a CCIxA capture pin.
    void SetCaptureInput(int ccr, int level);

    // Interrupt requests: CCR0's own vector, and the shared vector for CCR1..n and TBIFG.
    int Ccr0Pending(void) const { return (cctl[0] & CCIE) && (cctl[0] & CCIFG); };
    int SharedPending(void) const;
    void AcknowledgeCcr0(void) { cctl[0] &= ~CCIFG; };

    int Out(int ccr) const { return out[ccr]; };
    // Set when an output changed since the last call; the chip then updates its pins.
    int TakeOutputChanged(void) {
        int c = out_changed;
        out_changed = 0;
        return c;
    };

    uint16_t base;
    int ccrs;

    enum { CCIE = 0x0010, CCIFG = 0x0001 };

private:
    void Count(uint32_t steps);
    void Event(int at_zero_moment);
    void Compare(int n);
    void Equ0(void);
    void LoadLatches(int at_zero, int at_cl0);
    void Capture(int n);
    uint32_t Max(void) const;
    int Mode(void) const { return (ctl >> 4) & 3; };

    uint16_t ctl, ex0;
    uint16_t cctl[MAX_CCR];
    uint16_t ccr[MAX_CCR];  // TBxCCRn as written
    uint16_t cl[MAX_CCR];   // TBxCLn, the compare latches the counter is compared against
    uint8_t out[MAX_CCR];   // output signal OUTn
    uint8_t cci[MAX_CCR];   // capture input level
    uint32_t r;             // TBxR
    int down;               // counting down in up/down mode
    double prescale;        // input clock periods not yet counted
    int out_changed;
};

#endif
