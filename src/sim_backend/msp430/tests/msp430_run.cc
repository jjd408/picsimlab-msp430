/* Command-line runner for the MSP430FR2355 model, for testing it without PICSimLab's GUI.
 *
 *   msp430_run firmware.hex [--seconds S] [--tick HZ] [--pin PIN=LEVEL@TIME]... [--peek ADDR:WORDS]...
 *              [--edges PIN]... [--peek32 ADDR:N]... [--peek64 ADDR:N]... [--sample PIN@TIME]...
 *              [--duty PIN@FROM]... [--spi CLK,DATA,CS,CPOL,CPHA,MSBFIRST]
 *
 * --pin drives a package pin from outside at a simulated time (seconds), as a PICSimLab part would.
 * --peek prints 16-bit words of memory at the end (--peek32/--peek64: 32- or 64-bit values). --edges counts
 * level changes on a pin; --sample prints a pin's level at a time; --duty prints the fraction of time a pin
 * was high from a start time (seconds) to the end. --spi decodes the bytes on a data pin as an SPI device in
 * that mode would (bits counted while the active-high CS pin is high), to check what the pins carry.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "../fr2355.h"

struct PinEvent {
    int pin, level;
    double time;
};

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s firmware.hex [options]\n", argv[0]);
        return 2;
    }
    double seconds = 1.0, tick_hz = 8e6;
    std::vector<PinEvent> events;
    std::vector<std::pair<unsigned, int>> peeks;
    std::vector<int> edge_pins;
    std::vector<std::pair<unsigned, int>> peeks32, peeks64;
    std::vector<PinEvent> samples, duties;
    int spi[6] = {0};
    int use_spi = 0;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--seconds") && i + 1 < argc) {
            seconds = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--tick") && i + 1 < argc) {
            tick_hz = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--pin") && i + 1 < argc) {
            PinEvent e;
            if (sscanf(argv[++i], "%d=%d@%lf", &e.pin, &e.level, &e.time) != 3) {
                fprintf(stderr, "bad --pin %s\n", argv[i]);
                return 2;
            }
            events.push_back(e);
        } else if (!strcmp(argv[i], "--peek") && i + 1 < argc) {
            unsigned addr;
            int words;
            if (sscanf(argv[++i], "%x:%d", &addr, &words) != 2) {
                fprintf(stderr, "bad --peek %s\n", argv[i]);
                return 2;
            }
            peeks.push_back({addr, words});
        } else if ((!strcmp(argv[i], "--peek32") || !strcmp(argv[i], "--peek64")) && i + 1 < argc) {
            unsigned addr;
            int n;
            int is64 = argv[i][6] == '6';
            if (sscanf(argv[++i], "%x:%d", &addr, &n) != 2) {
                fprintf(stderr, "bad %s\n", argv[i - 1]);
                return 2;
            }
            (is64 ? peeks64 : peeks32).push_back({addr, n});
        } else if (!strcmp(argv[i], "--sample") && i + 1 < argc) {
            PinEvent e;
            if (sscanf(argv[++i], "%d@%lf", &e.pin, &e.time) != 2) {
                fprintf(stderr, "bad --sample %s\n", argv[i]);
                return 2;
            }
            samples.push_back(e);
        } else if (!strcmp(argv[i], "--spi") && i + 1 < argc) {
            if (sscanf(argv[++i], "%d,%d,%d,%d,%d,%d", &spi[0], &spi[1], &spi[2], &spi[3], &spi[4], &spi[5]) != 6) {
                fprintf(stderr, "bad --spi %s\n", argv[i]);
                return 2;
            }
            use_spi = 1;
        } else if (!strcmp(argv[i], "--duty") && i + 1 < argc) {
            PinEvent e;
            if (sscanf(argv[++i], "%d@%lf", &e.pin, &e.time) != 2) {
                fprintf(stderr, "bad --duty %s\n", argv[i]);
                return 2;
            }
            duties.push_back(e);
        } else if (!strcmp(argv[i], "--edges") && i + 1 < argc) {
            edge_pins.push_back(atoi(argv[++i]));
        } else {
            fprintf(stderr, "unknown option %s\n", argv[i]);
            return 2;
        }
    }

    Fr2355* chip = new Fr2355();
    if (chip->LoadHex(argv[1]) < 0) {
        fprintf(stderr, "%s\n", chip->error.c_str());
        return 1;
    }
    chip->SetTickHz(tick_hz);
    chip->PowerOnReset();

    std::vector<int> last(edge_pins.size()), edges(edge_pins.size());
    std::vector<long> high(duties.size()), total(duties.size());
    // SPI decoder: samples DATA on the leading clock edge (CPHA 0) or the trailing one (CPHA 1)
    int spi_clk_last = use_spi ? chip->GetPinLevel(spi[0]) : 0, spi_bits = 0, spi_byte = 0, spi_cs_last = 0;
    std::vector<int> spi_bytes;
    for (size_t k = 0; k < edge_pins.size(); k++)
        last[k] = chip->GetPinLevel(edge_pins[k]);

    long ticks = (long)(seconds * tick_hz);
    for (long t = 0; t < ticks; t++) {
        double now = t / tick_hz;
        for (auto& e : events) {
            if (e.time >= 0 && now >= e.time) {
                chip->SetPinInput(e.pin, e.level);
                e.time = -1;
            }
        }
        for (auto& e : samples) {
            if (e.time >= 0 && now >= e.time) {
                printf("pin %d (%s) at %.3f s: %d%s\n", e.pin, Fr2355::pins[e.pin - 1].name, now,
                       chip->GetPinLevel(e.pin), chip->InLpm5() ? " (in LPMx.5)" : "");
                e.time = -1;
            }
        }
        chip->Tick();
        if (chip->halted)
            break;
        if (use_spi) {
            // A device samples the edge that ends a byte before chip select rises, which can happen in the
            // same tick: use chip select as it was in the previous tick.
            int c = chip->GetPinLevel(spi[0]);
            int cs = chip->GetPinLevel(spi[2]);
            if (!spi_cs_last) {
                spi_bits = spi_byte = 0;  // deselected: start a new byte
            } else if (c != spi_clk_last) {
                int leading = c != spi[3];  // leaving the idle level
                if (leading != spi[4]) {
                    int bit = chip->GetPinLevel(spi[1]);
                    if (spi[5])
                        spi_byte = (spi_byte << 1) | bit;
                    else
                        spi_byte |= bit << spi_bits;
                    if (++spi_bits == 8) {
                        spi_bytes.push_back(spi_byte);
                        spi_bits = spi_byte = 0;
                    }
                }
            }
            spi_clk_last = c;
            spi_cs_last = cs;
        }
        for (size_t k = 0; k < duties.size(); k++) {
            if (now >= duties[k].time) {
                total[k]++;
                high[k] += chip->GetPinLevel(duties[k].pin);
            }
        }
        for (size_t k = 0; k < edge_pins.size(); k++) {
            int level = chip->GetPinLevel(edge_pins[k]);
            if (level != last[k]) {
                edges[k]++;
                last[k] = level;
            }
        }
    }

    printf("ran %.3f s: %llu MCLK cycles%s\n", seconds, (unsigned long long)chip->cycle_count,
           chip->halted ? " (halted)" : "");
    if (chip->halted)
        printf("error: %s\n", chip->error.c_str());
    printf("PC=%05X SP=%05X SR=%04X MCLK=%.0f Hz\n", chip->cpu.regs[0], chip->cpu.regs[1], chip->cpu.regs[2],
           chip->mclk_hz);
    for (size_t k = 0; k < edge_pins.size(); k++)
        printf("pin %d (%s): %d edges, now %d\n", edge_pins[k], Fr2355::pins[edge_pins[k] - 1].name, edges[k],
               chip->GetPinLevel(edge_pins[k]));
    if (use_spi) {
        printf("spi:");
        for (int b : spi_bytes)
            printf(" %02X", b);
        printf("\n");
    }
    for (size_t k = 0; k < duties.size(); k++)
        printf("pin %d (%s): high %.1f%% of the time from %.3f s\n", duties[k].pin, Fr2355::pins[duties[k].pin - 1].name,
               total[k] ? 100.0 * high[k] / total[k] : 0.0, duties[k].time);
    for (auto& p : peeks) {
        printf("%04X:", p.first);
        for (int w = 0; w < p.second; w++) {
            uint32_t a = p.first + 2 * w;
            uint8_t lo = 0, hi = 0;
            if (a >= 0x2000 && a < 0x3000) {
                lo = chip->ram[a - 0x2000];
                hi = chip->ram[a + 1 - 0x2000];
            } else if (a >= 0x8000 && a < 0x10000) {
                lo = chip->fram[a - 0x8000];
                hi = chip->fram[a + 1 - 0x8000];
            }
            printf(" %04X", lo | (hi << 8));
        }
        printf("\n");
    }
    auto byte = [&](uint32_t a) -> uint64_t {
        if (a >= 0x2000 && a < 0x3000)
            return chip->ram[a - 0x2000];
        if (a >= 0x8000 && a < 0x10000)
            return chip->fram[a - 0x8000];
        return 0;
    };
    for (int size : {4, 8}) {
        for (auto& p : size == 4 ? peeks32 : peeks64) {
            printf("%04X:", p.first);
            for (int k = 0; k < p.second; k++) {
                uint64_t v = 0;
                for (int b = size - 1; b >= 0; b--)
                    v = (v << 8) | byte(p.first + size * k + b);
                printf(size == 4 ? " %08llX" : " %016llX", (unsigned long long)v);
            }
            printf("\n");
        }
    }
    delete chip;
    return 0;
}
