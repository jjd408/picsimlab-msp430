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

#ifndef BSIM_MSP430_H
#define BSIM_MSP430_H

#include "../lib/board.h"
#include "msp430/fr2355.h"

// Simulation backend for TI MSP430FR2355 microcontrollers, using the MSP430X core adapted from MSPDebug's
// simulator and the chip model in msp430/fr2355.cc. One MStep is one simulation tick of 1 / clock seconds;
// the chip runs its own MCLK, so the clock setting is the simulation's time resolution, not the CPU speed.
class bsim_msp430 : virtual public board {
public:
    bsim_msp430(void);
    ~bsim_msp430(void);
    int DebugInit(int dtyppe) override;
    std::string GetDebugName(void) override { return "none"; };
    void DebugLoop(void) override {};
    int CpuInitialized(void) override { return chip != NULL; };
    void MSetSerial(const char* port) override {};
    int MInit(const char* processor, const char* fname, float freq) override;
    void MEnd(void) override;
    int MGetArchitecture(void) override { return ARCH_MSP430; };
    std ::string GetSimBackends(void) override { return "MSP430 (MSPDebug core),"; };
    std ::string GetDebuggers(void) override { return "none,"; };
    int MDumpMemory(const char* fname) override;
    void MEraseFlash(void) override;
    void MSetFreq(float freq) override;
    float MGetFreq(void) override { return freq; };
    void MSetVCC(float vcc) override {};
    float MGetVCC(void) override { return 3.0; };
    float MGetInstClockFreq(void) override { return freq; };
    int MGetPinCount(void) override { return Fr2355::PIN_COUNT; };
    std::string MGetPinName(int pin) override;
    void MSetPin(int pin, unsigned char value) override;
    void MSetPinDOV(int pin, unsigned char ovalue) override;
    void MSetPinOAV(int pin, float value) override;
    void MSetAPin(int pin, float value) override;
    unsigned char MGetPin(int pin) override;
    const picpin* MGetPinsValues(void) override { return pins; };
    float* MGetPinOAVPtr(int pin) override { return &pins[pin - 1].oavalue; };
    void MStep(void) override;
    void MStepResume(void) override {};
    int MReset(int flags) override;
    int MGetResetPin(void) override { return 9; };
    int MGetIOUpdated(void) override { return ioupdated; };
    void MClearIOUpdated(void) override { ioupdated = 0; };
    unsigned int DBGGetPC(void) override { return chip ? chip->cpu.regs[0] : 0; };
    unsigned char* DBGGetRAM_p(void) override { return chip ? chip->ram : NULL; };
    unsigned char* DBGGetROM_p(void) override { return chip ? chip->fram : NULL; };
    unsigned int DBGGetRAMSize(void) override { return sizeof(chip->ram); };
    unsigned int DBGGetROMSize(void) override { return sizeof(chip->fram); };

protected:
    void pins_reset(void);
    void pins_sync(void);
    Fr2355* chip;
    picpin pins[MAX_PIN_COUNT];
    float freq;
};

#endif /* BSIM_MSP430_H */
