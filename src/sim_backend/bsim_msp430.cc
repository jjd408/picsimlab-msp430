/* ########################################################################

   PICSimLab - Programmable IC Simulator Laboratory

   ########################################################################

   MSP430 backend: Copyright (c) 2026 jjd408. PICSimLab: Copyright (c) 2010-2026 Luis Claudio Gambôa Lopes.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   For e-mail suggestions :  lcgamboa@yahoo.com
   ######################################################################## */


#include "../lib/picsimlab.h"
#include "bsim_msp430.h"

bsim_msp430::bsim_msp430(void) {
    chip = NULL;
    gdb = NULL;
    freq = 8e6;
    memset(pins, 0, sizeof(pins));
}

bsim_msp430::~bsim_msp430(void) {
    delete gdb;
    MEnd();
}

int bsim_msp430::MInit(const char* processor, const char* fname, float freq_) {
    if (strcmp(processor, "MSP430FR2355")) {
        return -1;
    }
    if (!chip) {
        chip = new Fr2355();
    }
    chip->EraseFram();
    int ret = 0;
    FILE* f = fopen(fname, "r");
    if (f) {
        fclose(f);
        if (chip->LoadHex(fname) < 0) {
            printf("msp430: %s\n", chip->error.c_str());
            chip->EraseFram();
            ret = HEX_CHKSUM;
        }
    } else {
        ret = HEX_NFOUND;
    }
    MSetFreq(freq_);
    chip->PowerOnReset();
    pins_reset();
    return ret;
}

void bsim_msp430::MEnd(void) {
    delete chip;
    chip = NULL;
    gdb_target.SetChip(NULL);
}

int bsim_msp430::MDumpMemory(const char* fname) {
    if (!chip)
        return 0;
    if (chip->SaveHex(fname) < 0) {
        printf("msp430: %s\n", chip->error.c_str());
        return -1;
    }
    return 0;
}

void bsim_msp430::MEraseFlash(void) {
    if (chip)
        chip->EraseFram();
}

void bsim_msp430::MSetFreq(float freq_) {
    freq = freq_;
    if (chip)
        chip->SetTickHz(freq);
    TimerUpdateFrequency(freq);
}

/* ------------------------------------------------------------------------------------------------ */
/* Debugging: a GDB server on PICSimLab's debug port                                                 */

int bsim_msp430::DebugInit(int dtyppe) {
    if (!gdb) {
        gdb = new GdbServer(&gdb_target);
        gdb_target.changed = [this]() { pins_sync(); };
    }
    gdb_target.SetChip(chip);
    if (!gdb->Listen(PICSimLab.GetDebugPort())) {
        PICSimLab.RegisterError("PICSimLab", "Can't start the GDB server: " + gdb->error);
        return -1;
    }
    printf("PICSimLab: MSP430 GDB server on port %i (target remote :%i)\n", gdb->Port(), gdb->Port());
    return 1;
}

void bsim_msp430::DebugLoop(void) {
    if (gdb && chip) {
        gdb_target.SetChip(chip);  // the chip is made anew when a program is loaded
        gdb->Poll(50);
    }
}

std::string bsim_msp430::MGetPinName(int pin) {
    if (pin < 1 || pin > Fr2355::PIN_COUNT)
        return "error";
    return Fr2355::pins[pin - 1].name;
}

void bsim_msp430::pins_reset(void) {
    for (int p = 0; p < Fr2355::PIN_COUNT; p++) {
        const Fr2355::Pin& cp = Fr2355::pins[p];
        pins[p].port = NULL;
        pins[p].pord = cp.port ? cp.bit : -1;
        pins[p].avalue = 0;
        pins[p].lvalue = 0;
        pins[p].ovalue = 0;
        pins[p].oavalue = 55;
        pins[p].ptype = cp.port ? PT_DIGITAL : PT_POWER;
        pins[p].dir = PD_IN;
        pins[p].value = 0;
    }
    pins_sync();
}

// Copies the chip's pin levels and directions into PICSimLab's pin array.
void bsim_msp430::pins_sync(void) {
    for (int p = 0; p < Fr2355::PIN_COUNT; p++) {
        unsigned char dir = chip->GetPinIsOutput(p + 1) ? PD_OUT : PD_IN;
        unsigned char value = chip->GetPinLevel(p + 1);
        if (pins[p].dir != dir || pins[p].value != value) {
            pins[p].dir = dir;
            pins[p].value = value;
            ioupdated = 1;
        }
    }
}

void bsim_msp430::MSetPin(int pin, unsigned char value) {
    if (!chip || pin < 1 || pin > Fr2355::PIN_COUNT)
        return;
    if (pins[pin - 1].dir != PD_IN)
        return;
    chip->SetPinInput(pin, value);
    if (chip->IoUpdated()) {
        chip->ClearIoUpdated();
        pins_sync();
    }
}

void bsim_msp430::MSetPinDOV(int pin, unsigned char ovalue) {
    if (pin < 1 || pin > Fr2355::PIN_COUNT)
        return;
    pins[pin - 1].ovalue = ovalue;
}

void bsim_msp430::MSetPinOAV(int pin, float value) {
    if (pin < 1 || pin > Fr2355::PIN_COUNT)
        return;
    pins[pin - 1].oavalue = value;
}

void bsim_msp430::MSetAPin(int pin, float value) {
    if (pin < 1 || pin > Fr2355::PIN_COUNT)
        return;
    pins[pin - 1].avalue = value;
}

unsigned char bsim_msp430::MGetPin(int pin) {
    if (!chip || pin < 1 || pin > Fr2355::PIN_COUNT)
        return 0;
    return chip->GetPinLevel(pin);
}

void bsim_msp430::MStep(void) {
    if (!chip)
        return;
    chip->Tick();
    if (chip->IoUpdated()) {
        chip->ClearIoUpdated();
        pins_sync();
    }
}

int bsim_msp430::MReset(int flags) {
    if (chip) {
        chip->PowerOnReset();
        pins_sync();
    }
    return 0;
}
