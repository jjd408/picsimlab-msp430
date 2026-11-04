/* Firmware for the GDB server test (gdbtest.gdb): a loop that calls a function, writes a variable a watchpoint
 * can catch, and toggles P1.0. */
#include <msp430.h>

volatile unsigned counter;
volatile unsigned last;

__attribute__((noinline)) unsigned add3(unsigned x) {
    return x + 3;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    PM5CTL0 &= ~LOCKLPM5;
    P1DIR |= BIT0;
    for (;;) {
        counter = add3(counter);
        last = counter;
        P1OUT ^= BIT0;
    }
}
