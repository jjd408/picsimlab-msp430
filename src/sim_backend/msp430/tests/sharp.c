/* Drives the Sharp LS013B7DH05 as the solitaire handheld is wired (hw/sol/sol.kicad_sch):
 *   SCLK P4.5 (UCB1CLK), SI P4.6 (UCB1SIMO), SCS P4.7 (GPIO, active high),
 *   EXTCOMIN P5.0 (TB2.1, 60 Hz from ACLK), DISP P5.1 (GPIO), EXTMODE tied high.
 * MCLK = SMCLK = 8 MHz; SPI at SMCLK / 8 = 1 MHz, mode 0, LSB first (so line numbers go out as they are and
 * bit 0 of each byte is the leftmost pixel). Clears the panel, draws a test picture into a RAM framebuffer
 * (1 = white), sends it as one multi-line transfer, then sleeps in LPM3 with EXTCOMIN running. */
#include <msp430.h>
#include <stdint.h>
#include <string.h>

// Overridable to test the display model's checks: SCS_SETUP (cycles after SCS rises) and SPI_DIV (SMCLK / SPI_DIV)
#ifndef SCS_SETUP
#define SCS_SETUP 64
#endif
#ifndef SPI_DIV
#define SPI_DIV 8
#endif

#define W 144
#define H 168
static uint8_t fb[H][W / 8];

static void pixel(int x, int y, int black) {
    if (black)
        fb[y][x / 8] &= ~(1 << (x % 8));
    else
        fb[y][x / 8] |= 1 << (x % 8);
}

static void rect(int x0, int y0, int x1, int y1) {
    for (int x = x0; x <= x1; x++) {
        pixel(x, y0, 1);
        pixel(x, y1, 1);
    }
    for (int y = y0; y <= y1; y++) {
        pixel(x0, y, 1);
        pixel(x1, y, 1);
    }
}

static void spi(uint8_t b) {
    while (!(UCB1IFG & UCTXIFG))
        ;
    UCB1TXBUF = b;
}

static void spi_done(void) {
    while (UCB1STATW & UCBUSY)
        ;
}

static void select(void) {
    P4OUT |= BIT7;
    __delay_cycles(SCS_SETUP);   // tsSCS: 6 us (8 us at 8 MHz)
}

static void deselect(void) {
    spi_done();
    __delay_cycles(24);   // thSCS: 2 us (3 us)
    P4OUT &= ~BIT7;
    __delay_cycles(64);   // twSCSL: 6 us before the next frame
}

static void clear_panel(void) {
    select();
    spi(0x04);            // M2: all clear
    spi(0x00);            // dummy (at least 13 clocks in all)
    deselect();
}

static void send_frame(void) {
    select();
    spi(0x01);            // M0: data update; then M1, M2 = 0 and five dummy bits
    for (int y = 0; y < H; y++) {
        spi(y + 1);       // gate line address, 1-168
        for (int i = 0; i < W / 8; i++)
            spi(fb[y][i]);
        spi(0x00);        // dummy
    }
    spi(0x00);            // the last transfer period's address byte
    deselect();
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;

    // 8 MHz from the FLL, REFO reference; ACLK = REFO
    __bis_SR_register(SCG0);
    CSCTL3 = SELREF__REFOCLK;
    CSCTL1 = DCORSEL_3;
    CSCTL2 = FLLD_0 + 243;
    __delay_cycles(3);
    __bic_SR_register(SCG0);
    while (CSCTL7 & (FLLUNLOCK0 | FLLUNLOCK1))
        ;
    CSCTL4 = SELMS__DCOCLKDIV | SELA__REFOCLK;

    P4SEL0 |= BIT5 | BIT6;          // UCB1CLK, UCB1SIMO
    P4OUT &= ~BIT7;                 // SCS low
    P4DIR |= BIT7;
    P5SEL0 |= BIT0;                 // TB2.1 = EXTCOMIN
    P5DIR |= BIT0 | BIT1;
    P5OUT &= ~BIT1;                 // DISP low until the panel is cleared
    PM5CTL0 &= ~LOCKLPM5;

    UCB1CTLW0 = UCSWRST;
    UCB1CTLW0 |= UCCKPH | UCMST | UCSYNC | UCSSEL__SMCLK;   // mode 0, LSB first, 3-pin
    UCB1BRW = SPI_DIV;
    UCB1CTLW0 &= ~UCSWRST;

    TB2CCR0 = 272;                  // 32768 / 273 = 120 Hz: TB2.1 toggles at 60 Hz
    TB2CCR1 = 100;
    TB2CCTL1 = OUTMOD_4;
    TB2CTL = TBSSEL__ACLK | MC__UP | TBCLR;

    clear_panel();
    P5OUT |= BIT1;                  // DISP on

    memset(fb, 0xFF, sizeof(fb));
    rect(0, 0, W - 1, H - 1);
    for (int c = 0; c < 7; c++)     // seven card outlines
        rect(4 + c * 20, 8, 4 + c * 20 + 16, 30);
    for (int y = 60; y < 108; y++)  // checkerboard of 8 x 8 squares
        for (int x = 48; x < 96; x++)
            pixel(x, y, ((x / 8) + (y / 8)) & 1);
    for (int i = 0; i < 40; i++)    // a diagonal
        pixel(10 + i, 120 + i, 1);
    send_frame();

    __bis_SR_register(LPM3_bits);
    for (;;) ;
}
