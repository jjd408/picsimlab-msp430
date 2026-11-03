/* Sharp LS013B7DH05 memory LCD for PICSimLab; see lcd_ls013b7dh05.h. */
#include "lcd_ls013b7dh05.h"

#include <stdio.h>
#include <string.h>

lcd_ls013b7dh05::lcd_ls013b7dh05(void) {
    // Power-up contents are undefined; Sharp says to clear them. Start with a pattern so a missing clear shows.
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W / 8; x++)
            mem[y][x] = ((x + y / 8) & 1) ? 0x55 : 0xAA;
    Reset();
}

void lcd_ls013b7dh05::Reset(void) {
    state = S_IGNORE;
    count = m0 = m1 = m2 = addr = 0;
    last_sclk = last_scs = last_com = 0;
    disp = 0;
    scs_rise = last_clock = last_edge = -1;
    clocks_in_frame = 0;
    com_window_start = 0;
    com_edges = 0;
    com_hz = 0;
    problem.clear();
    frames = lines_written = 0;
    memset(dirty, 1, sizeof(dirty));
    any_dirty = 1;
}

void lcd_ls013b7dh05::Complain(const std::string& what) {
    if (problem.empty()) {
        problem = what;
        printf("LS013B7DH05: %s\n", what.c_str());
    }
}

void lcd_ls013b7dh05::Step(double now, int sclk, int si, int scs, int extcomin, int disp_) {
    // DISP: low shows white, memory kept
    if (disp_ != disp) {
        disp = disp_;
        memset(dirty, 1, sizeof(dirty));
        any_dirty = 1;
    }

    // EXTCOMIN: count rising edges over one-second windows
    if (extcomin && !last_com)
        com_edges++;
    last_com = extcomin;
    if (now - com_window_start >= 1.0) {
        com_hz = com_edges / (now - com_window_start);
        com_window_start = now;
        com_edges = 0;
    }

    // SCS
    if (scs && !last_scs) {
        state = S_MODE;
        count = m0 = m1 = m2 = 0;
        scs_rise = now;
        clocks_in_frame = 0;
    } else if (!scs && last_scs) {
        // SCS falls: the frame ends (M0 and M2 clear). Hold time from the last clock: 2 us.
        if (clocks_in_frame && now - last_clock < 2e-6 - 1e-9)
            Complain("SCS fell " + std::to_string((int)((now - last_clock) * 1e9)) +
                     " ns after the last SCLK edge; the panel needs 2 us (thSCS)");
        state = S_IGNORE;
        frames++;
    }
    last_scs = scs;

    // SCLK rising edge while selected: latch SI
    if (sclk && !last_sclk && scs) {
        if (clocks_in_frame == 0 && now - scs_rise < 6e-6 - 1e-9)
            Complain("first SCLK edge " + std::to_string((int)((now - scs_rise) * 1e9)) +
                     " ns after SCS rose; the panel needs 6 us (tsSCS)");
        if (last_edge >= 0 && now - last_edge < 1.0 / 1.1e6 - 1e-9)
            Complain("SCLK at " + std::to_string((int)(1.0 / (now - last_edge) / 1e3)) +
                     " kHz; the panel takes at most 1100 kHz");
        last_edge = now;
        last_clock = now;
        clocks_in_frame++;
        Bit(si);
    } else if (sclk != last_sclk && scs) {
        last_clock = now;
    }
    last_sclk = sclk;
}

void lcd_ls013b7dh05::Bit(int b) {
    switch (state) {
        case S_MODE:
            if (count == 0)
                m0 = b;
            else if (count == 1)
                m1 = b;
            else if (count == 2)
                m2 = b;
            if (++count == 8) {  // three mode bits and five dummy bits
                count = 0;
                if (m2 && !m0) {
                    // All clear: memory to white
                    memset(mem, 0xFF, sizeof(mem));
                    memset(dirty, 1, sizeof(dirty));
                    any_dirty = 1;
                    state = S_IGNORE;
                } else if (m0) {
                    state = S_ADDR;
                    addr = 0;
                } else {
                    state = S_IGNORE;  // display mode: nothing to write
                }
            }
            break;
        case S_ADDR:
        case S_TRANSFER_ADDR:
            addr |= (b & 1) << count;  // AG0 first
            if (++count == 8) {
                count = 0;
                state = S_DATA;
                memset(line, 0, sizeof(line));
            }
            break;
        case S_DATA:
            // An address outside 1-168 only matters if data follows: the last transfer period of a frame is
            // 16 dummy clocks, whose second byte reads as address 0
            if (count == 0 && (addr < 1 || addr > H))
                Complain("line address " + std::to_string(addr) + " is outside 1-168");
            if (b)
                line[count / 8] |= 1 << (count % 8);
            if (++count == W) {
                count = 0;
                if (addr >= 1 && addr <= H) {
                    if (memcmp(mem[addr - 1], line, sizeof(line))) {
                        memcpy(mem[addr - 1], line, sizeof(line));
                        dirty[addr - 1] = 1;
                        any_dirty = 1;
                    }
                    lines_written++;
                }
                state = S_TRANSFER_DUMMY;
            }
            break;
        case S_TRANSFER_DUMMY:
            if (++count == 8) {
                count = 0;
                addr = 0;
                state = S_TRANSFER_ADDR;  // another line follows, unless SCS falls first
            }
            break;
        case S_IGNORE:
            break;
    }
}
