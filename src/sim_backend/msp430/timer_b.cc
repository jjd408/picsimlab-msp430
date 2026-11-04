/* Timer_B for the MSP430FR2355 model; see timer_b.h. */
#include "timer_b.h"

#include <string.h>

// Register offsets from TBxCTL
enum { O_CTL = 0x00, O_CCTL0 = 0x02, O_R = 0x10, O_CCR0 = 0x12, O_EX0 = 0x20, O_IV = 0x2E };
// TBxCTL
enum { CNTL = 0x1800, TBSSEL = 0x0300, ID = 0x00C0, TBCLR = 0x0004, TBIE = 0x0002, TBIFG = 0x0001 };
// TBxCCTLn
enum { CM = 0xC000, CCIS = 0x3000, CLLD = 0x0600, CAP = 0x0100, OUTMOD = 0x00E0, CCI = 0x0008, OUT = 0x0004,
       COV = 0x0002 };
enum { MC_STOP, MC_UP, MC_CONTINUOUS, MC_UPDOWN };

void TimerB::Init(uint16_t base_, int ccrs_) {
    base = base_;
    ccrs = ccrs_;
    Reset();
}

void TimerB::Reset(void) {
    ctl = ex0 = 0;
    memset(cctl, 0, sizeof(cctl));
    memset(ccr, 0, sizeof(ccr));
    memset(cl, 0, sizeof(cl));
    memset(out, 0, sizeof(out));
    memset(cci, 0, sizeof(cci));
    r = 0;
    down = 0;
    prescale = 0;
    out_changed = 1;
}

uint32_t TimerB::Max(void) const {
    static const uint32_t max[4] = {0xFFFF, 0x0FFF, 0x03FF, 0x00FF};
    return max[(ctl & CNTL) >> 11];
}

/* ------------------------------------------------------------------------------------------------ */
/* Registers                                                                                         */

uint16_t TimerB::Read(uint16_t offset, bool peek) {
    if (offset == O_CTL)
        return ctl;
    if (offset == O_R)
        return r;
    if (offset == O_EX0)
        return ex0;
    if (offset >= O_CCTL0 && offset < O_CCTL0 + 2 * ccrs) {
        int n = (offset - O_CCTL0) / 2;
        return (cctl[n] & ~CCI) | (cci[n] ? CCI : 0);
    }
    if (offset >= O_CCR0 && offset < O_CCR0 + 2 * ccrs)
        return ccr[(offset - O_CCR0) / 2];
    if (offset == O_IV) {
        // Highest-priority pending, enabled source: CCR1 (2) ... CCR6 (0x0C), then TBIFG (0x0E). Reading clears
        // it (unless a debugger is only peeking).
        for (int n = 1; n < ccrs; n++) {
            if ((cctl[n] & CCIE) && (cctl[n] & CCIFG)) {
                if (!peek)
                    cctl[n] &= ~CCIFG;
                return 2 * n;
            }
        }
        if ((ctl & TBIE) && (ctl & TBIFG)) {
            if (!peek)
                ctl &= ~TBIFG;
            return 0x0E;
        }
        return 0;
    }
    return 0;
}

void TimerB::Write(uint16_t offset, uint16_t value) {
    if (offset == O_CTL) {
        ctl = value & ~TBCLR;
        if (value & TBCLR) {
            // TBCLR clears the counter, the divider and the count direction
            r = 0;
            prescale = 0;
            down = 0;
        }
    } else if (offset == O_R) {
        r = value & Max();
    } else if (offset == O_EX0) {
        ex0 = value & 7;
    } else if (offset >= O_CCTL0 && offset < O_CCTL0 + 2 * ccrs) {
        int n = (offset - O_CCTL0) / 2;
        cctl[n] = value;
        if ((value & OUTMOD) == 0 && out[n] != ((value & OUT) != 0)) {
            out[n] = (value & OUT) != 0;  // output mode 0: OUT drives the output directly
            out_changed = 1;
        }
        // CCIS = GND or VCC: software capture by switching between them
        int ccis = (value & CCIS) >> 12;
        if (ccis >= 2) {
            int level = ccis == 3;
            int was = cci[n];
            cci[n] = level;
            if ((value & CAP) && level != was) {
                int cm = (value & CM) >> 14;
                if ((cm & 1 && level) || (cm & 2 && !level))
                    Capture(n);
            }
        }
    } else if (offset >= O_CCR0 && offset < O_CCR0 + 2 * ccrs) {
        int n = (offset - O_CCR0) / 2;
        ccr[n] = value;
        if (!(cctl[n] & CLLD))
            cl[n] = value;  // CLLD = 0: the compare latch loads at once
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Counting                                                                                          */

void TimerB::Advance(double aclk_ticks, double smclk_ticks) {
    if (Mode() == MC_STOP)
        return;
    double ticks;
    switch ((ctl & TBSSEL) >> 8) {
        case 1:
            ticks = aclk_ticks;
            break;
        case 2:
            ticks = smclk_ticks;
            break;
        default:
            return;  // TBxCLK and INCLK aren't modeled
    }
    if (ticks <= 0)
        return;
    prescale += ticks;
    double div = (1 << ((ctl & ID) >> 6)) * (ex0 + 1);
    if (prescale < div)
        return;
    uint32_t steps = (uint32_t)(prescale / div);
    prescale -= steps * div;
    Count(steps);
}

// Moves the counter on by steps counts, stopping at each value where something happens.
void TimerB::Count(uint32_t steps) {
    int mode = Mode();
    uint32_t top = (mode == MC_CONTINUOUS) ? Max() : cl[0];
    if (mode != MC_CONTINUOUS && top == 0)
        return;  // up and up/down modes stop while TBxCL0 is 0

    while (steps) {
        // Distance to the next value where a compare or boundary happens
        uint32_t next;
        if (!down) {
            if (r >= top) {
                next = 1;  // the step after the top: wrap to 0, or turn round in up/down mode
            } else {
                next = top - r;
                for (int n = 0; n < ccrs; n++) {
                    if (!(cctl[n] & CAP) && cl[n] > r && cl[n] - r < next)
                        next = cl[n] - r;
                }
            }
        } else {
            next = r;  // down to 0
            for (int n = 0; n < ccrs; n++) {
                if (!(cctl[n] & CAP) && cl[n] < r && r - cl[n] < next)
                    next = r - cl[n];
            }
        }
        if (steps < next) {
            r = down ? r - steps : r + steps;
            return;
        }
        steps -= next;

        int at_zero = 0;
        if (!down && r >= top) {
            if (mode == MC_UPDOWN) {
                down = 1;
                r = top - 1;
            } else {
                r = 0;
                at_zero = 1;
            }
        } else {
            r = down ? r - next : r + next;
            if (down && r == 0) {
                down = 0;
                at_zero = 1;
            }
        }
        Event(at_zero);
        if (mode == MC_UPDOWN && !down && r == top)
            LoadLatches(0, 1);
        top = (mode == MC_CONTINUOUS) ? Max() : cl[0];
        if (mode != MC_CONTINUOUS && top == 0)
            return;
    }
}

// The counter has just reached r.
void TimerB::Event(int at_zero) {
    if (at_zero) {
        ctl |= TBIFG;  // the counter rolled over to, or counted down to, zero
        LoadLatches(1, 0);
    }
    if (!(cctl[0] & CAP) && r == cl[0])
        Compare(0);
    for (int n = 1; n < ccrs; n++) {
        if (!(cctl[n] & CAP) && r == cl[n])
            Compare(n);
    }
}

static void apply(uint8_t& o, int action, int& changed) {
    // action: 0 none, 1 set, 2 reset, 3 toggle
    uint8_t before = o;
    if (action == 1)
        o = 1;
    else if (action == 2)
        o = 0;
    else if (action == 3)
        o ^= 1;
    if (o != before)
        changed = 1;
}

// TBxR counted to TBxCLn: flag, output action for EQUn, and EQU0 for the other outputs when n is 0.
void TimerB::Compare(int n) {
    cctl[n] |= CCIFG;
    // Output action at EQUn for each OUTMOD (0 output, 1 set, 2 toggle/reset, 3 set/reset, 4 toggle, 5 reset,
    // 6 toggle/set, 7 reset/set)
    static const int at_equn[8] = {0, 1, 3, 1, 3, 2, 3, 2};
    apply(out[n], at_equn[(cctl[n] & OUTMOD) >> 5], out_changed);
    if ((cctl[n] & CLLD) == 0x0600)
        cl[n] = ccr[n];  // CLLD = 3: load when TBxR counts to TBxCLn
    if (n == 0)
        Equ0();
}

void TimerB::Equ0(void) {
    static const int at_equ0[8] = {0, 0, 2, 2, 0, 0, 1, 1};
    for (int m = 1; m < ccrs; m++) {
        if (!(cctl[m] & CAP))
            apply(out[m], at_equ0[(cctl[m] & OUTMOD) >> 5], out_changed);
    }
}

void TimerB::LoadLatches(int at_zero, int at_cl0) {
    for (int n = 0; n < ccrs; n++) {
        int clld = (cctl[n] & CLLD) >> 9;
        if ((clld == 1 && at_zero) || (clld == 2 && (at_zero || at_cl0)))
            cl[n] = ccr[n];
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Capture                                                                                           */

void TimerB::Capture(int n) {
    ccr[n] = r;
    if (cctl[n] & CCIFG)
        cctl[n] |= COV;  // the previous capture wasn't read
    cctl[n] |= CCIFG;
}

void TimerB::SetCaptureInput(int n, int level) {
    if (n >= ccrs || (cctl[n] & CCIS) != 0)  // only CCIxA comes from a pin
        return;
    level = level != 0;
    if (level == cci[n])
        return;
    cci[n] = level;
    if (cctl[n] & CAP) {
        int cm = (cctl[n] & CM) >> 14;
        if ((cm & 1 && level) || (cm & 2 && !level))
            Capture(n);
    }
}

int TimerB::SharedPending(void) const {
    for (int n = 1; n < ccrs; n++) {
        if ((cctl[n] & CCIE) && (cctl[n] & CCIFG))
            return 1;
    }
    return (ctl & TBIE) && (ctl & TBIFG);
}
