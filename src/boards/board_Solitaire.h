/* ########################################################################

   PICSimLab - Programmable IC Simulator Laboratory

   ########################################################################

   Solitaire board: Copyright (c) 2026 jjd408. PICSimLab: Copyright (c) 2010-2026 Luis Claudio Gambôa Lopes.

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

#ifndef BOARD_Solitaire_H
#define BOARD_Solitaire_H

#include "../devices/lcd_ls013b7dh05.h"
#include "../sim_backend/bsim_msp430.h"

#define BOARD_Solitaire_Name "Solitaire"

// The solitaire handheld (hw/sol/sol.kicad_sch): MSP430FR2355, the Sharp LS013B7DH05 display and 16 buttons
// to ground, each on the pin the schematic gives it. Keys: 1-9 the stacks, Q W E R the suits, A S D SW1-SW3.
class cboard_Solitaire : public bsim_msp430 {
public:
    static const int BUTTONS = 16;

    std::string GetName(void) override { return BOARD_Solitaire_Name; };
    std::string GetAboutInfo(void) override { return "jjd408"; };
    cboard_Solitaire(void);
    ~cboard_Solitaire(void);
    void Draw(void) override;
    void Run_CPU(void) override;
    int MInit(const char* processor, const char* fname, float freq) override;
    std::string GetSupportedDevices(void) override { return "MSP430FR2355,"; };
    void Reset(void) override;
    void EvMouseButtonPress(unsigned int button, unsigned int x, unsigned int y, unsigned int state) override;
    void EvMouseButtonRelease(unsigned int button, unsigned int x, unsigned int y, unsigned int state) override;
    void EvKeyPress(unsigned int key, unsigned int mask) override;
    void EvKeyRelease(unsigned int key, unsigned int mask) override;
    void WritePreferences(void) override;
    void ReadPreferences(char* name, char* value) override;
    unsigned short GetInputId(char* name) override;
    unsigned short GetOutputId(char* name) override;

private:
    void Press(int button, int down);
    int KeyButton(unsigned int key);

    lcd_ls013b7dh05 lcd;
    unsigned long steps;
    int pressed[BUTTONS];
    unsigned char rc_state[BUTTONS];  // remote-control view of the buttons (1 = pressed)
    std::string status_shown;
};

#endif /* BOARD_Solitaire_H */
