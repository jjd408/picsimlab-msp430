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

#include "board_Solitaire.h"
#include "../lib/oscilloscope.h"
#include "../lib/picsimlab.h"
#include "../lib/spareparts.h"

// Ids in the board map: BT01-BT16 are both inputs and outputs
enum { I_PWR = 0, I_RST, I_BT01 };
enum { O_LPWR = 0, O_RST, O_LCD, O_STATUS, O_BT01 };

// Package pins of each button (hw/sol/sol.kicad_sch): SW1-SW3, stacks 1-9, suits 1-4
static const int BUTTON_PIN[cboard_Solitaire::BUTTONS] = {
    3, 2, 1,                         // SW1 P3.0, SW2 P3.1, SW3 P3.2
    7, 6, 5, 4, 31, 30, 29, 27, 26,  // stacks 1-7: P1.0-P1.6; waste (stack 8) P2.0, stock (stack 9) P2.1
    25, 24, 15, 14,                  // suits: P2.2, P2.3, P2.4, P2.5
};
// Keys for the same buttons
static const char BUTTON_KEY[cboard_Solitaire::BUTTONS + 1] = "ASD123456789QWER";
// The display's pins: SCLK P4.5, SI P4.6, SCS P4.7, EXTCOMIN P5.0, DISP P5.1
enum { PIN_SCLK = 18, PIN_SI = 17, PIN_SCS = 16, PIN_EXTCOMIN = 37, PIN_DISP = 36 };

static const int SCALE = 2;
static const unsigned int WHITE[3] = {206, 210, 200}, BLACK[3] = {34, 37, 40};

unsigned short cboard_Solitaire::GetInputId(char* name) {
    if (strcmp(name, "SW_PWR") == 0)
        return I_PWR;
    if (strcmp(name, "PB_RST") == 0)
        return I_RST;
    if (strncmp(name, "PB_BT", 5) == 0)
        return I_BT01 + atoi(name + 5) - 1;
    printf("Error input '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

unsigned short cboard_Solitaire::GetOutputId(char* name) {
    if (strcmp(name, "LD_PWR") == 0)
        return O_LPWR;
    if (strcmp(name, "PB_RST") == 0)
        return O_RST;
    if (strcmp(name, "DS_LCD") == 0)
        return O_LCD;
    if (strcmp(name, "ST_STATUS") == 0)
        return O_STATUS;
    if (strncmp(name, "PB_BT", 5) == 0)
        return O_BT01 + atoi(name + 5) - 1;
    printf("Error output '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

cboard_Solitaire::cboard_Solitaire(void) {
    Proc = "MSP430FR2355";
    ReadMaps();
    steps = 0;
    for (int b = 0; b < BUTTONS; b++)
        pressed[b] = rc_state[b] = 0;
    // Remote control (set board.in[NN] 1/0) presses and releases buttons through these
    for (int i = 0; i < inputc; i++) {
        if (input[i].id >= I_BT01 && input[i].id < I_BT01 + BUTTONS)
            input[i].status = &rc_state[input[i].id - I_BT01];
    }
}

cboard_Solitaire::~cboard_Solitaire(void) {}

int cboard_Solitaire::MInit(const char* processor, const char* fname, float freq_) {
    int ret = bsim_msp430::MInit(processor, fname, freq_);
    if (ret == -1) {
        PICSimLab.RegisterError("PICSimLab", "Unknown processor " + std::string(processor) + "! Loading Default.");
        ret = bsim_msp430::MInit("MSP430FR2355", fname, freq_);
        Proc = "MSP430FR2355";
    }
    return ret;
}

void cboard_Solitaire::Reset(void) {
    MReset(1);
    lcd.Reset();
    steps = 0;
    for (int b = 0; b < BUTTONS; b++)
        Press(b, pressed[b]);
    status_shown.clear();
    if (use_spare)
        SpareParts.Reset();
}

void cboard_Solitaire::WritePreferences(void) {
    PICSimLab.SavePrefs("Solitaire_proc", Proc);
    PICSimLab.SavePrefs("Solitaire_clock", FloatStrFormat("%2.1f", PICSimLab.GetClock()));
}

void cboard_Solitaire::ReadPreferences(char* name, char* value) {
    if (!strcmp(name, "Solitaire_proc"))
        Proc = value;
    if (!strcmp(name, "Solitaire_clock"))
        PICSimLab.SetClock(atof(value));
}

/* ------------------------------------------------------------------------------------------------ */
/* Buttons                                                                                           */

// A button switches its pin to ground; released, the pin is left to the MCU's pull resistor.
void cboard_Solitaire::Press(int b, int down) {
    pressed[b] = down;
    rc_state[b] = down;
    if (chip) {
        if (down)
            chip->SetPinInput(BUTTON_PIN[b], 0);
        else
            chip->ReleasePin(BUTTON_PIN[b]);
        pins_sync();
    }
    if (output_ids[O_BT01 + b])
        output_ids[O_BT01 + b]->update = 1;
}

int cboard_Solitaire::KeyButton(unsigned int key) {
    if (key >= 'a' && key <= 'z')
        key -= 'a' - 'A';
    for (int b = 0; b < BUTTONS; b++) {
        if ((unsigned int)BUTTON_KEY[b] == key)
            return b;
    }
    return -1;
}

void cboard_Solitaire::EvKeyPress(unsigned int key, unsigned int mask) {
    int b = KeyButton(key);
    if (b >= 0 && !pressed[b])
        Press(b, 1);
}

void cboard_Solitaire::EvKeyRelease(unsigned int key, unsigned int mask) {
    int b = KeyButton(key);
    if (b >= 0 && pressed[b])
        Press(b, 0);
}

void cboard_Solitaire::EvMouseButtonPress(unsigned int button, unsigned int x, unsigned int y, unsigned int state) {
    for (int i = 0; i < inputc; i++) {
        if (input[i].x1 > x || input[i].x2 < x || input[i].y1 > y || input[i].y2 < y)
            continue;
        if (input[i].id >= I_BT01 && input[i].id < I_BT01 + BUTTONS) {
            Press(input[i].id - I_BT01, 1);
        } else if (input[i].id == I_PWR) {
            PICSimLab.SetMcuPwr(!PICSimLab.GetMcuPwr());
            Reset();
            output_ids[O_LPWR]->update = 1;
        } else if (input[i].id == I_RST) {
            if (chip)
                chip->SetResetPin(0);
            p_RST = 0;
            output_ids[O_RST]->update = 1;
        }
    }
}

void cboard_Solitaire::EvMouseButtonRelease(unsigned int button, unsigned int x, unsigned int y, unsigned int state) {
    // Release whatever the mouse was holding down, wherever it is now
    for (int b = 0; b < BUTTONS; b++) {
        if (pressed[b])
            Press(b, 0);
    }
    if (!p_RST) {
        if (chip)
            chip->SetResetPin(1);
        p_RST = 1;
        output_ids[O_RST]->update = 1;
    }
}

/* ------------------------------------------------------------------------------------------------ */
/* Drawing                                                                                           */

void cboard_Solitaire::Draw(void) {
    int update = 0;
    for (int i = 0; i < outputc; i++) {
        if (!output[i].update)
            continue;
        output[i].update = 0;
        if (!update) {
            PICSimLab.CanvasCmd({.cmd = CC_INIT, .Init{Scale, Scale, 0}});
            PICSimLab.CanvasCmd({.cmd = CC_SETFONTWEIGHT, .SetFontWeight{CC_FONTWEIGHT_BOLD}});
        }
        update++;
        float cx = (output[i].x1 + output[i].x2) / 2.0, cy = (output[i].y1 + output[i].y2) / 2.0;
        int id = output[i].id;

        if (id >= O_BT01 && id < O_BT01 + BUTTONS) {
            int down = pressed[id - O_BT01];
            unsigned int c = down ? 40 : 90;
            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{c, c + 10, c + 22}});
            PICSimLab.CanvasCmd({.cmd = CC_CIRCLE, .Circle{1, cx, cy, down ? 16.0f : 18.0f}});
        } else if (id == O_LPWR) {
            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{PICSimLab.GetMcuPwr() ? 255u : 64u, 32, 32}});
            PICSimLab.CanvasCmd({.cmd = CC_CIRCLE, .Circle{1, cx, cy, 6}});
        } else if (id == O_RST) {
            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{p_RST ? 70u : 30u, p_RST ? 70u : 30u, p_RST ? 70u : 30u}});
            PICSimLab.CanvasCmd({.cmd = CC_CIRCLE, .Circle{1, cx, cy, 12}});
        } else if (id == O_LCD) {
            for (int y = 0; y < lcd_ls013b7dh05::H; y++) {
                if (!lcd.dirty[y])
                    continue;
                lcd.dirty[y] = 0;
                int x = 0;
                while (x < lcd_ls013b7dh05::W) {
                    int white = lcd.Disp() ? lcd.Pixel(x, y) : 1;
                    int run = 1;
                    while (x + run < lcd_ls013b7dh05::W && (lcd.Disp() ? lcd.Pixel(x + run, y) : 1) == white)
                        run++;
                    const unsigned int* c = white ? WHITE : BLACK;
                    PICSimLab.CanvasCmd({.cmd = CC_SETFGCOLOR, .SetFgColor{c[0], c[1], c[2]}});
                    PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{c[0], c[1], c[2]}});
                    PICSimLab.CanvasCmd({.cmd = CC_RECTANGLE,
                                         .Rectangle{1, (float)(output[i].x1 + x * SCALE),
                                                    (float)(output[i].y1 + y * SCALE), (float)(run * SCALE),
                                                    (float)SCALE}});
                    x += run;
                }
            }
            lcd.any_dirty = 0;
        } else if (id == O_STATUS) {
            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{26, 34, 42}});
            PICSimLab.CanvasCmd({.cmd = CC_RECTANGLE,
                                 .Rectangle{1, output[i].x1, output[i].y1, output[i].x2 - output[i].x1,
                                            output[i].y2 - output[i].y1}});
            PICSimLab.CanvasCmd({.cmd = CC_SETFONTSIZE, .SetFontSize{8}});
            // Green only while the panel would be happy: no timing problem and EXTCOMIN at 57-66 Hz
            if (lcd.problem.empty() && lcd.com_hz >= 57 && lcd.com_hz <= 66)
                PICSimLab.CanvasCmd({.cmd = CC_SETFGCOLOR, .SetFgColor{160, 220, 160}});
            else
                PICSimLab.CanvasCmd({.cmd = CC_SETFGCOLOR, .SetFgColor{255, 120, 100}});
            Rect_t rec;
            rec.x = output[i].x1;
            rec.y = output[i].y1;
            rec.width = output[i].x2 - output[i].x1;
            rec.height = output[i].y2 - output[i].y1;
            PICSimLab.CanvasCmd({.cmd = CC_TEXTONRECT,
                                 .TextOnRect{status_shown.c_str(), rec, CC_ALIGN_CENTER | CC_ALIGN_CENTER_VERTICAL}});
        }
    }
    if (update)
        PICSimLab.CanvasCmd({.cmd = CC_END});
}

/* ------------------------------------------------------------------------------------------------ */
/* Simulation                                                                                        */

void cboard_Solitaire::Run_CPU(void) {
    unsigned int alm[64];
    const int pinc = MGetPinCount();
    const long int NSTEP = PICSimLab.GetNSTEP();
    const float RNSTEP = 200.0 * pinc / NSTEP;
    const double freq_ = MGetInstClockFreq();

    memset(alm, 0, sizeof(alm));
    if (use_spare)
        SpareParts.PreProcess();

    // Buttons pressed or released by remote control
    for (int b = 0; b < BUTTONS; b++) {
        if ((rc_state[b] != 0) != (pressed[b] != 0))
            Press(b, rc_state[b] != 0);
    }

    unsigned char pi = 0;
    if (PICSimLab.GetMcuPwr()) {
        for (long i = 0; i < NSTEP; i++) {
            if (DbgStopped())
                break;  // the debugger has the program stopped: time stands still, display and all
            MStep();
            InstCounterInc();
            steps++;
            lcd.Step(steps / freq_, pins[PIN_SCLK - 1].value, pins[PIN_SI - 1].value, pins[PIN_SCS - 1].value,
                     pins[PIN_EXTCOMIN - 1].value, pins[PIN_DISP - 1].value);
            if (use_oscope)
                Oscilloscope.SetSample();
            if (use_spare)
                SpareParts.Process();
            alm[pi] += pins[pi].value;
            if (++pi == pinc)
                pi = 0;
        }
    }
    for (pi = 0; pi < pinc; pi++)
        pins[pi].oavalue = (int)((alm[pi] * RNSTEP) + 55);

    if (use_spare)
        SpareParts.PostProcess();

    // Display and its status line
    if (lcd.any_dirty)
        output_ids[O_LCD]->update = 1;
    std::string status;
    if (!lcd.problem.empty()) {
        status = lcd.problem;
    } else if (lcd.com_hz < 57 || lcd.com_hz > 66) {
        char s[80];
        snprintf(s, sizeof(s), "EXTCOMIN %.0f Hz: the panel needs 57-66 Hz", lcd.com_hz);
        status = s;
    } else {
        char s[80];
        snprintf(s, sizeof(s), "EXTCOMIN %.0f Hz   %ld frames   %ld lines", lcd.com_hz, lcd.frames,
                 lcd.lines_written);
        status = s;
    }
    if (status != status_shown) {
        status_shown = status;
        output_ids[O_STATUS]->update = 1;
    }
}

// Register the board in PICSimLab
board_init(BOARD_Solitaire_Name, cboard_Solitaire);
