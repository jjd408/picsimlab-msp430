/* ########################################################################

   PICSimLab - Programmable IC Simulator Laboratory

   ########################################################################

   MSP430 Board: Copyright (c) 2026 jjd408, from uCboard. PICSimLab: Copyright (c) 2010-2026 Luis Claudio Gambôa Lopes.

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

#ifndef BOARD_MSP430_Board_H
#define BOARD_MSP430_Board_H

#include "../sim_backend/bsim_msp430.h"

#define BOARD_MSP430_Board_Name "MSP430 Board"

// new board class must be derived from board class defined in board.h
class cboard_MSP430_Board : public bsim_msp430 {
private:
    int micbmp;

public:
    void SetScale(double scale) override;
    // Return the board name
    std::string GetName(void) override { return BOARD_MSP430_Board_Name; };
    std::string GetAboutInfo(void) override { return "jjd408, based on uCboard by L.C. Gamboa"; };
    // Constructor called once on board creation
    cboard_MSP430_Board(void);
    // Destructor called once on board destruction
    ~cboard_MSP430_Board(void);
    // Called ever 100ms to draw board
    void Draw(void) override;
    void Run_CPU(void) override;
    int MInit(const char* processor, const char* fname, float freq) override;
    // Return a list of board supported microcontrollers
    std::string GetSupportedDevices(void) override { return "MSP430FR2355,"; };
    // Return a list of supported IDEs in project wizard
    std::string GetSupportedIDEs(void) override;
    // Reset board status
    void Reset(void) override;
    // Event on the board
    void EvMouseButtonPress(unsigned int button, unsigned int x, unsigned int y, unsigned int state) override;
    // Event on the board
    void EvMouseButtonRelease(unsigned int button, unsigned int x, unsigned int y, unsigned int state) override;
    // Event on the board
    void EvKeyPress(unsigned int key, unsigned int mask) override;
    // Event on the board
    void EvKeyRelease(unsigned int key, unsigned int mask) override;
    // Called to save board preferences in configuration file
    void WritePreferences(void) override;
    // Called whe configuration file load  preferences
    void ReadPreferences(char* name, char* value) override;
    // return the input ids numbers of names used in input map
    unsigned short GetInputId(char* name) override;
    // return the output ids numbers of names used in output map
    unsigned short GetOutputId(char* name) override;
};

#endif /* BOARD_MSP430_Board_H */
