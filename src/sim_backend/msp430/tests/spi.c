/* eUSCI_B1 SPI master test. P4.5 = UCB1CLK, P4.6 = UCB1SIMO, P4.7 = UCB1SOMI, P4.4 = UCB1STE (4-pin,
 * active high, driven by the module: UCSTEM). SMCLK / 2.
 * Build with CFG set to the clock/bit-order bits under test (default: UCCKPH | UCMSB, SPI mode 0, MSB first).
 *   1. polled: "MSP430"; the runner holds SOMI high, so each byte received is 0xFF
 *   2. loopback (UCLISTEN): 0x5A, 0xC3 come back as received
 *   3. interrupt-driven from UCTXIFG via UCB1IV while main sleeps in LPM0: 0x11 0x22 0x33 0x44
 * results: rx[0..7] the bytes received in 1 and 2, sent3 the bytes the interrupt sent, done = 0x600D */
#include <msp430.h>
#include <stdint.h>

#ifndef CFG
#define CFG (UCCKPH | UCMSB)
#endif

volatile uint8_t rx[8];
volatile uint16_t sent3, done;
static const uint8_t burst[4] = {0x11, 0x22, 0x33, 0x44};

static uint8_t xfer(uint8_t b) {
    while (!(UCB1IFG & UCTXIFG))
        ;
    UCB1TXBUF = b;
    while (!(UCB1IFG & UCRXIFG))
        ;
    return UCB1RXBUF;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    P4SEL0 |= BIT4 | BIT5 | BIT6 | BIT7;
    PM5CTL0 &= ~LOCKLPM5;

    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= CFG | UCMST | UCSYNC | UCMODE_1 | UCSTEM | UCSSEL__SMCLK;
    UCB1BRW = 2;
    UCB1CTLW0 &= ~UCSWRST;

    const char* msg = "MSP430";
    for (int i = 0; i < 6; i++)
        rx[i] = xfer(msg[i]);

    UCB1CTLW0 |= UCSWRST;
    UCB1STATW |= UCLISTEN;
    UCB1CTLW0 &= ~UCSWRST;
    rx[6] = xfer(0x5A);
    rx[7] = xfer(0xC3);
    while (UCB1STATW & UCBUSY)
        ;

    UCB1IE = UCTXIE;
    __bis_SR_register(LPM0_bits | GIE);
    while (UCB1STATW & UCBUSY)
        ;
    done = 0x600D;
    for (;;) ;
}

__attribute__((interrupt(EUSCI_B1_VECTOR))) void ucb1_isr(void) {
    switch (UCB1IV) {
        case USCI_SPI_UCTXIFG:
            if (sent3 < 4) {
                UCB1TXBUF = burst[sent3++];
            } else {
                UCB1IE &= ~UCTXIE;
                __bic_SR_register_on_exit(LPM0_bits);
            }
            break;
    }
}
