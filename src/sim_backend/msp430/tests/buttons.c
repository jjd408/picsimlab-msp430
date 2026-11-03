/* Whole-handheld test for the Solitaire board: the display shows one box per button, filled while it's
 * held. Buttons switch to ground with the MCU's pull-ups; main sleeps in LPM3 (EXTCOMIN running on ACLK) and
 * wakes on any button edge (the edge select flips after each one), then redraws the panel.
 * Box order on screen: row 1 SW1-SW3, row 2 stacks 1-9, row 3 suits 1-4, as in the schematic. */
#include <msp430.h>
#include <stdint.h>
#include <string.h>

#define W 144
#define H 168
static uint8_t fb[H][W / 8];
volatile uint16_t presses;  // button-down edges seen

// Port and bit of each button: SW1-SW3 (P3.0-P3.2), stacks 1-9 (P1.0-P1.6, P2.0, P2.1), suits 1-4 (P2.2-P2.5)
static const uint8_t port_of[16] = {3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2};
static const uint8_t bit_of[16] = {0, 1, 2, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5};
#define P1_BUTTONS 0x7F
#define P2_BUTTONS 0x3F
#define P3_BUTTONS 0x07

static void pixel(int x, int y, int black) {
    if (black)
        fb[y][x / 8] &= ~(1 << (x % 8));
    else
        fb[y][x / 8] |= 1 << (x % 8);
}

static void box(int x0, int y0, int size, int filled) {
    for (int y = y0; y < y0 + size; y++)
        for (int x = x0; x < x0 + size; x++)
            pixel(x, y, filled || x == x0 || x == x0 + size - 1 || y == y0 || y == y0 + size - 1);
}

static void spi(uint8_t b) {
    while (!(UCB1IFG & UCTXIFG))
        ;
    UCB1TXBUF = b;
}

static void frame_begin(uint8_t mode) {
    P4OUT |= BIT7;
    __delay_cycles(64);
    spi(mode);
}

static void frame_end(void) {
    while (UCB1STATW & UCBUSY)
        ;
    __delay_cycles(24);
    P4OUT &= ~BIT7;
    __delay_cycles(64);
}

static void send_frame(void) {
    frame_begin(0x01);
    for (int y = 0; y < H; y++) {
        spi(y + 1);
        for (int i = 0; i < W / 8; i++)
            spi(fb[y][i]);
        spi(0x00);
    }
    spi(0x00);
    frame_end();
}

static int is_down(int b) {
    uint8_t in = port_of[b] == 1 ? P1IN : port_of[b] == 2 ? P2IN : P3IN;
    return !(in & (1 << bit_of[b]));
}

static void draw(void) {
    memset(fb, 0xFF, sizeof(fb));
    for (int b = 0; b < 16; b++) {
        int row = b < 3 ? 0 : b < 12 ? 1 : 2;
        int col = b < 3 ? b : b < 12 ? b - 3 : b - 12;
        box(4 + col * 15, 20 + row * 40, 13, is_down(b));
    }
    send_frame();
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    __bis_SR_register(SCG0);
    CSCTL3 = SELREF__REFOCLK;
    CSCTL1 = DCORSEL_3;
    CSCTL2 = FLLD_0 + 243;
    __delay_cycles(3);
    __bic_SR_register(SCG0);
    while (CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1))
        ;
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;

    // Buttons: inputs with pull-ups, interrupt on the falling edge (press) to start with
    P1DIR &= ~P1_BUTTONS; P1REN |= P1_BUTTONS; P1OUT |= P1_BUTTONS; P1IES |= P1_BUTTONS;
    P2DIR &= ~P2_BUTTONS; P2REN |= P2_BUTTONS; P2OUT |= P2_BUTTONS; P2IES |= P2_BUTTONS;
    P3DIR &= ~P3_BUTTONS; P3REN |= P3_BUTTONS; P3OUT |= P3_BUTTONS; P3IES |= P3_BUTTONS;

    P4SEL0 |= BIT5 | BIT6;
    P4OUT &= ~BIT7;
    P4DIR |= BIT7;
    P5SEL0 |= BIT0;
    P5DIR |= BIT0 | BIT1;
    P5OUT &= ~BIT1;
    PM5CTL0 &= ~LOCKLPM5;
    P1IFG = 0; P2IFG = 0; P3IFG = 0;
    P1IE |= P1_BUTTONS; P2IE |= P2_BUTTONS; P3IE |= P3_BUTTONS;

    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= UCCKPH | UCMST | UCSYNC | UCSSEL__SMCLK;
    UCB1BRW = 8;
    UCB1CTLW0 &= ~UCSWRST;

    TB2CCR0 = 272;
    TB2CCR1 = 100;
    TB2CCTL1 = OUTMOD_4;
    TB2CTL = TBSSEL__ACLK | MC__UP | TBCLR;

    frame_begin(0x04);  // all clear
    spi(0x00);
    frame_end();
    P5OUT |= BIT1;      // DISP on

    for (;;) {
        draw();
        __bis_SR_register(LPM3_bits | GIE);
    }
}

// Each edge: count presses, flip that pin's edge select to catch the opposite edge next, wake main.
static void edge(volatile uint8_t* ies, uint8_t bit, uint8_t was_falling) {
    if (was_falling)
        presses++;
    *ies ^= bit;
}

__attribute__((interrupt(PORT1_VECTOR))) void port1_isr(void) {
    uint16_t iv = P1IV;
    if (iv) {
        uint8_t bit = 1 << (iv / 2 - 1);
        edge(&P1IES, bit, P1IES & bit);
    }
    __bic_SR_register_on_exit(LPM3_bits);
}

__attribute__((interrupt(PORT2_VECTOR))) void port2_isr(void) {
    uint16_t iv = P2IV;
    if (iv) {
        uint8_t bit = 1 << (iv / 2 - 1);
        edge(&P2IES, bit, P2IES & bit);
    }
    __bic_SR_register_on_exit(LPM3_bits);
}

__attribute__((interrupt(PORT3_VECTOR))) void port3_isr(void) {
    uint16_t iv = P3IV;
    if (iv) {
        uint8_t bit = 1 << (iv / 2 - 1);
        edge(&P3IES, bit, P3IES & bit);
    }
    __bic_SR_register_on_exit(LPM3_bits);
}
