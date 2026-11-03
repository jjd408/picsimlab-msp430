/* Sharp LS013B7DH05 memory LCD (1.26in, 144 x 168, monochrome) for PICSimLab.
 *
 * From Sharp's spec LD-26912A: SPI, mode 0 (SI latched on SCLK's rising edge), SCS active high. Each frame
 * starts with three mode bits (M0 data update, M1 COM, M2 all clear) and five dummy bits. In data update
 * mode a line address follows (8 bits, AG0 first; lines 1-168), then 144 pixel bits (1 = white) and a 16-bit
 * transfer period (8 dummy bits and the next line's address). DISP low shows white but keeps the memory.
 *
 * Besides displaying, it checks the timing the real panel needs and keeps the problems it finds: SCLK at
 * most 1.1 MHz, SCS set up 6 us before the first clock and held 2 us after the last, EXTCOMIN at 57-66 Hz
 * (with EXTMODE high, as on the solitaire handheld), and line addresses 1-168.
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 */
#ifndef LCD_LS013B7DH05_H
#define LCD_LS013B7DH05_H

#include <stdint.h>
#include <string>

class lcd_ls013b7dh05 {
public:
    static const int W = 144, H = 168;

    lcd_ls013b7dh05(void);
    void Reset(void);

    // Called each simulation step with the pin levels and the time now (seconds).
    void Step(double now, int sclk, int si, int scs, int extcomin, int disp);

    int Pixel(int x, int y) const { return (mem[y][x / 8] >> (x % 8)) & 1; };  // 1 = white
    int Disp(void) const { return disp; };

    // Lines changed since the last call (bit per line), for redrawing only those.
    uint8_t dirty[H];
    int any_dirty;

    // Status: EXTCOMIN frequency over the last second (Hz) and the first timing problem seen, if any.
    double com_hz;
    std::string problem;
    long frames, lines_written;

private:
    void Bit(int b);
    void Complain(const std::string& what);

    uint8_t mem[H][W / 8];
    uint8_t line[W / 8];
    enum { S_MODE, S_ADDR, S_DATA, S_TRANSFER_DUMMY, S_TRANSFER_ADDR, S_IGNORE } state;
    int count, m0, m1, m2, addr;
    int last_sclk, last_scs, last_com, disp;
    double scs_rise, last_clock, last_edge;
    int clocks_in_frame;
    double com_window_start;
    int com_edges;
};

#endif
