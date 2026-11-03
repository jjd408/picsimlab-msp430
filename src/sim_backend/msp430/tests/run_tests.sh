#!/bin/sh
# Builds the test firmware and the command-line runner, runs each test and compares with expected output.
set -e
cd "$(dirname "$0")"
G=${MSP430_GCC:-$HOME/ti/msp430-gcc}
CC="$G/bin/msp430-elf-gcc -mmcu=msp430fr2355 -I$G/include -L$G/include -Os -g"
build() {  # output name, source, extra flags
    $CC $3 -o $1.elf $2
    "$G/bin/msp430-elf-objcopy" -O ihex $1.elf $1.hex
}
build step1 step1.c -mhwmult=none
build wdtreset wdtreset.c -mhwmult=none
build mpy_none mpy.c -mhwmult=none
build mpy_f5series mpy.c -mhwmult=f5series
build clocks clocks.c
build lpm45 lpm45.c
build timers timers.c
build updown updown.c
build timers_lpm3 timers.c -DSLEEP=LPM3_bits
build spi spi.c
build spi_m3lsb spi.c -DCFG=UCCKPL
build sharp sharp.c
build sharp_nosetup sharp.c -DSCS_SETUP=1
build sharp_fast sharp.c -DSPI_DIV=4
build buttons buttons.c
gcc -std=c99 -O2 -c ../msp430_cpu.c -o msp430_cpu.o
g++ -std=c++17 -O2 -Wall -o msp430_run msp430_run.cc ../fr2355.cc ../timer_b.cc ../eusci_b.cc \
    ../../../devices/lcd_ls013b7dh05.cc msp430_cpu.o

fail=0
check() {  # name, expected line, command...
    name=$1; expect=$2; shift 2
    out=$("$@")
    if echo "$out" | grep -qF -- "$expect"; then
        echo "pass  $name"
    else
        echo "FAIL  $name: expected '$expect' in:"; echo "$out" | sed 's/^/      /'
        fail=1
    fi
}
addr() {  # symbol address in an .elf, as hex
    "$G/bin/msp430-elf-nm" $1 | awk -v s=$2 '$3 == s { sub(/^0+/, "", $1); print toupper($1) }'
}

# --- Step 1: core, ports, watchdog, LPM3
# results[]: self-check 0x600D, 2 P1.1 interrupts, 8 WDT interrupts (4/s on ACLK), 10 wakes.
# P1.1 is left undriven at first, so its pull-up must hold it high until the first pulse.
check "step1: arithmetic, pin interrupts, LPM3, WDT interval" "2000: 600D 0002 0008 000A" \
    ./msp430_run step1.hex --seconds 2.1 --tick 1e6 --pin 6=0@0.5 --pin 6=1@0.6 --pin 6=0@1.2 --pin 6=1@1.3 \
    --peek 2000:4
check "step1: P1.0 toggles on each wake" "pin 7 (P1.0): 10 edges" \
    ./msp430_run step1.hex --seconds 2.1 --tick 1e6 --pin 6=0@0.5 --pin 6=1@0.6 --pin 6=0@1.2 --pin 6=1@1.3 \
    --edges 7
# A watchdog that is never stopped resets every 32768 SMCLK cycles. SMCLK starts at 32 * 32768 Hz
# = 1.049 MHz, so 1 s holds the first boot and 31 resets (the 32nd falls on the 1 s mark).
# The last reset cause (SYSRSTIV) is 0x16, a watchdog time-out.
check "wdtreset: watchdog resets and SYSRSTIV" "8000: 0016 0020" \
    ./msp430_run wdtreset.hex --seconds 1 --tick 1e6 --peek 8000:2

# --- Step 2: hardware multiplier, clock system, LPMx.5
# The same products with the MPY32 hardware multiplier and in software must match each other and the
# values worked out on a PC (uint8 * uint8 is a 16-bit signed int on the MSP430, so 200 * 177 overflows).
MPY32="0D93968C FF951644 0000968C 760B3D29 874891C0 FFFF8A48 FFFFE1EC D7EAA321 C414A532 8B4021C6 00000D08 00FD5BDE"
MPY64="00FD5BDE760B3D29 FFFF911A874891C0 451567366448E963"
for v in none f5series; do
    out=$(addr mpy_$v.elf out); out64=$(addr mpy_$v.elf out64)
    check "mpy ($v): 32-bit results" "$out: $MPY32" ./msp430_run mpy_$v.hex --seconds 0.2 --peek32 $out:12
    check "mpy ($v): 64-bit results" "$out64: $MPY64" ./msp430_run mpy_$v.hex --seconds 0.2 --peek64 $out64:3
done
# DCO at 8 MHz: CSCTL7 shows only XT1OFFG (no crystal); SMCLK / 32768 at 4 MHz gives 122 interrupts in a
# 1 s __delay_cycles (which also checks instruction timing); none while in LPM3, where SMCLK stops.
r=$(addr clocks.elf results)
check "clocks: FLL to 8 MHz, SMCLK divider, timing, LPM3 gating" "$r: 0002 007A 0000 600D" \
    ./msp430_run clocks.hex --seconds 3 --tick 8e6 --pin 6=0@2.5 --peek $r:4
check "clocks: MCLK frequency" "MCLK=7995392 Hz" ./msp430_run clocks.hex --seconds 0.1 --tick 8e6
# LPM4.5: P1.0 is held high while the chip is powered down; a P1.1 edge wakes it through a reset with
# SYSRSTIV 0x08, PMMIFG's LPM5 flag (0x8000, beside the power-on BOR flag 0x0100) and P1IFG.1 set.
check "lpm45: pin held in LPMx.5" "pin 7 (P1.0) at 0.400 s: 1 (in LPMx.5)" \
    ./msp430_run lpm45.hex --seconds 1 --tick 1e6 --sample 7@0.4 --pin 6=0@0.5
p=$(addr lpm45.elf wake_p1ifg)
check "lpm45: wake-up cause, flags, boots" "$p: 0002 8100 0008 0002" \
    ./msp430_run lpm45.hex --seconds 1 --tick 1e6 --pin 6=0@0.5 --peek $p:4

# --- Step 3: Timer_B
# TB2.1 toggling on ACLK every 273 counts is the handheld's EXTCOMIN: 60 Hz, 120 edges a second, in LPM0.
check "timers: EXTCOMIN 60 Hz on P5.0 (TB2.1)" "pin 37 (P5.0): 120 edges" \
    ./msp430_run timers.hex --seconds 1 --tick 4e6 --edges 37
check "timers: 25% PWM on P2.0 (TB1.1, reset/set)" "pin 27 (P2.0): high 25.1% of the time" \
    ./msp430_run timers.hex --seconds 1 --tick 4e6 --duty 27@0.1
# results[]: TB0 CCR0 at 10 Hz (the 10th lands just after 1 s), TB3 CCR1 and TBIFG at 100 Hz through TB3IV
# (the 100th wrap lands just after 1 s), the TB0R captured on a P1.6 rising edge at 0.5 s (16384 counts
# less the 7 before the timer started), one capture.
r=$(addr timers.elf results)
check "timers: CCR0 and TBxIV interrupts, capture" "$r: 0009 0064 0063 3FF9 0001 600D" \
    ./msp430_run timers.hex --seconds 1 --tick 4e6 --pin 29=0@0 --pin 29=1@0.5 --peek $r:6
check "updown: up/down mode, toggle/set output, CLLD" "pin 27 (P2.0): high 75.0% of the time" \
    ./msp430_run updown.hex --seconds 0.5 --tick 4e6 --duty 27@0.05
# In LPM3, as the handheld will sleep, ACLK keeps EXTCOMIN going.
check "timers: EXTCOMIN keeps running in LPM3" "pin 37 (P5.0): 120 edges" \
    ./msp430_run timers_lpm3.hex --seconds 1 --tick 4e6 --edges 37

# --- Step 4: eUSCI_B1 SPI master
# The pins carry "MSP430", 5A C3 (loopback) and 11 22 33 44 (sent from the UCTXIFG interrupt in LPM0), decoded
# from P4.5 (clock), P4.6 (SIMO) and P4.4 (STE) as a device in that SPI mode would. Received: 0xFF for each
# polled byte (SOMI held high), then 5A C3 from the loopback; the interrupt sent 4 bytes.
SPIBYTES="spi: 4D 53 50 34 33 30 5A C3 11 22 33 44"
r=$(addr spi.elf done)
for tick in 4e6 1e6; do
    check "spi: mode 0, MSB first (the display's), tick $tick" "$SPIBYTES" \
        ./msp430_run spi.hex --seconds 0.2 --tick $tick --pin 16=1@0 --spi 18,17,19,0,0,1
    check "spi: mode 3, LSB first, tick $tick" "$SPIBYTES" \
        ./msp430_run spi_m3lsb.hex --seconds 0.2 --tick $tick --pin 16=1@0 --spi 18,17,19,1,1,0
done
check "spi: received bytes, loopback, interrupt-driven sends" "$r: 600D 0004 FFFF FFFF FFFF C35A" \
    ./msp430_run spi.hex --seconds 0.2 --tick 4e6 --pin 16=1@0 --peek $r:6

# --- Step 5: the Sharp LS013B7DH05 display (the PICSimLab part's model), wired as on the handheld:
# SCLK P4.5 (18), SI P4.6 (17), SCS P4.7 (16), EXTCOMIN P5.0 (37), DISP P5.1 (36).
# sharp.c clears the panel and writes a test picture (border, seven card outlines, checkerboard, diagonal:
# 2344 black pixels) in one 168-line transfer, then sleeps in LPM3 with EXTCOMIN at 60 Hz.
SHARP="18,17,16,37,36"
check "sharp: picture, EXTCOMIN, timing" \
    "sharp: 2 frames, 168 lines written, EXTCOMIN 60 Hz, DISP 1, 2344 black pixels, no timing problems" \
    ./msp430_run sharp.hex --seconds 2.1 --tick 8e6 --sharp $SHARP
./msp430_run sharp.hex --seconds 2.1 --tick 8e6 --sharp $SHARP,sharp.pbm >/dev/null
check "sharp: picture matches" "$(cat sharp_expected.md5)" md5sum sharp.pbm
# The model catches what the real panel would object to
check "sharp: SCS set-up time too short is caught" "the panel needs 6 us (tsSCS)" \
    ./msp430_run sharp_nosetup.hex --seconds 0.5 --tick 8e6 --sharp $SHARP
check "sharp: SCLK above 1.1 MHz is caught" "the panel takes at most 1100 kHz" \
    ./msp430_run sharp_fast.hex --seconds 0.5 --tick 8e6 --sharp $SHARP

# --- Step 6: the whole handheld (the Solitaire board's wiring): buttons to ground on the schematic's pins,
# the display, LPM3 between presses. SW1 (P3.0, pin 3) is pressed and released; stack 5 (P1.4, pin 31) and
# suit 4 (P2.5, pin 14) are pressed and held. The firmware counts 3 presses and redraws on each of the 4
# edges (6 frames with the clear and the first draw); the screen shows the two held buttons' boxes filled.
BUTTONS="--pin 3=0@0.3 --pin 31=0@0.4 --pin 14=0@0.5 --pin 3=z@0.8"
r=$(addr buttons.elf presses)
check "buttons: presses counted" "$r: 0003" ./msp430_run buttons.hex --seconds 1.5 --tick 8e6 $BUTTONS --peek $r:1
check "buttons: display redrawn on each edge" "sharp: 6 frames, 840 lines written" \
    ./msp430_run buttons.hex --seconds 1.5 --tick 8e6 $BUTTONS --sharp $SHARP
./msp430_run buttons.hex --seconds 1.5 --tick 8e6 $BUTTONS --sharp $SHARP,buttons.pbm >/dev/null
check "buttons: screen shows the held buttons" "$(cat buttons_expected.md5)" md5sum buttons.pbm
exit $fail
