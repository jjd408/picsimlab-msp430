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

// include files

#include "board_MSP430_Board.h"
#include "../lib/oscilloscope.h"
#include "../lib/picsimlab.h"
#include "../lib/spareparts.h"

/* ids of inputs of input map*/
enum {
    I_ICSP,  // ICSP connector
    I_PWR,   // Power button
    I_RST,   // Reset button
};

/* ids of outputs of output map*/
enum {
    O_LPWR,  // Power LED
    O_MP,    // uController name
    O_RST    // Reset button
};

// return the input ids numbers of names used in input map

unsigned short cboard_MSP430_Board::GetInputId(char* name) {
    if (strcmp(name, "PG_ICSP") == 0)
        return I_ICSP;
    if (strcmp(name, "SW_PWR") == 0)
        return I_PWR;
    if (strcmp(name, "PB_RST") == 0)
        return I_RST;

    printf("Error input '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

// return the output ids numbers of names used in output map

unsigned short cboard_MSP430_Board::GetOutputId(char* name) {
    if (strcmp(name, "IC_CPU") == 0)
        return O_MP;
    if (strcmp(name, "LD_LPWR") == 0)
        return O_LPWR;
    if (strcmp(name, "PB_RST") == 0)
        return O_RST;

    printf("Error output '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

// Constructor called once on board creation

cboard_MSP430_Board::cboard_MSP430_Board(void) {
    Proc = "MSP430FR2355";  // default microcontroller if none defined in preferences
    ReadMaps();    // Read input and output board maps
    micbmp = PICSimLab.CanvasCmd(
        {.cmd = CC_LOADIMAGE, .LoadImage{(PICSimLab.GetSharePath() + "boards/Common/ic40.svg").c_str(), Scale, 1, 0}});
}

// Destructor called once on board destruction

cboard_MSP430_Board::~cboard_MSP430_Board(void) {
    PICSimLab.CanvasCmd({.cmd = CC_FREEBITMAP, .FreeBitmap{micbmp}});
    micbmp = -1;
}

// Reset board status

void cboard_MSP430_Board::Reset(void) {
    MReset(1);

    if (use_spare)
        SpareParts.Reset();
}

// Called to save board preferences in configuration file

void cboard_MSP430_Board::WritePreferences(void) {
    // write selected microcontroller of board_x to preferences
    PICSimLab.SavePrefs("MSP430_Board_proc", Proc);
    // write microcontroller clock to preferences
    PICSimLab.SavePrefs("MSP430_Board_clock", FloatStrFormat("%2.1f", PICSimLab.GetClock()));

    PICSimLab.SavePrefs("MSP430_Board_PWActivePrj", GetPWActiveProject());
    PICSimLab.SavePrefs("MSP430_Board_PWPrjType", GetPWProjectType());
}

// Called whe configuration file load  preferences

void cboard_MSP430_Board::ReadPreferences(char* name, char* value) {
    // read microcontroller of preferences
    if (!strcmp(name, "MSP430_Board_proc")) {
        Proc = value;
    }
    // read microcontroller clock
    if (!strcmp(name, "MSP430_Board_clock")) {
        PICSimLab.SetClock(atof(value));
    }

    if (!strcmp(name, "MSP430_Board_PWActivePrj")) {
        SetPWActiveProject(value);
    }

    if (!strcmp(name, "MSP430_Board_PWPrjType")) {
        SetPWProjectType(value);
    }
}

// Event on the board

void cboard_MSP430_Board::EvKeyPress(unsigned int key, unsigned int mask) {}

// Event on the board

void cboard_MSP430_Board::EvKeyRelease(unsigned int key, unsigned int mask) {}

// Event on the board

void cboard_MSP430_Board::EvMouseButtonPress(unsigned int button, unsigned int x, unsigned int y, unsigned int state) {
    int i;

    // search for the input area which owner the event
    for (i = 0; i < inputc; i++) {
        if (((input[i].x1 <= x) && (input[i].x2 >= x)) && ((input[i].y1 <= y) && (input[i].y2 >= y))) {
            switch (input[i].id) {
                    // if event is over I_ISCP area then load hex file
                case I_ICSP:
                    PICSimLab.OpenLoadHexFileDialog();
                    ;
                    break;
                    // if event is over I_PWR area then toggle board on/off
                case I_PWR:
                    if (PICSimLab.GetMcuPwr())  // if on turn off
                    {
                        PICSimLab.SetMcuPwr(0);
                        Reset();
                    } else  // if off turn on
                    {
                        PICSimLab.SetMcuPwr(1);
                        Reset();
                    }
                    output_ids[O_LPWR]->update = 1;
                    break;
                    // if event is over I_RST area then turn off and reset
                case I_RST:
                    /*
                    if (Window1.Get_mcupwr () && reset (-1))//if powered
                     {
                      PICSimLab.Set_mcupwr (0);
                      PICSimLab.Set_mcurst (1);
                     }
                     */
                    MReset(-1);
                    output_ids[O_RST]->update = 1;
                    p_RST = 0;
                    break;
            }
        }
    }
}

// Event on the board

void cboard_MSP430_Board::EvMouseButtonRelease(unsigned int button, unsigned int x, unsigned int y, unsigned int state) {
    int i;

    // search for the input area which owner the event
    for (i = 0; i < inputc; i++) {
        if (((input[i].x1 <= x) && (input[i].x2 >= x)) && ((input[i].y1 <= y) && (input[i].y2 >= y))) {
            switch (input[i].id) {
                    // if event is over I_RST area then turn on
                case I_RST:
                    if (PICSimLab.GetMcuRst())  // if powered
                    {
                        PICSimLab.SetMcuPwr(1);
                        PICSimLab.SetMcuRst(0);
                        /*
                                 if (reset (-1))
                                  {
                                   Reset ();
                                  }
                         */
                    }
                    p_RST = 1;
                    output_ids[O_RST]->update = 1;
                    break;
            }
        }
    }
}

// Called ever 100ms to draw board
// This is the critical code for simulator running speed

void cboard_MSP430_Board::Draw(void) {
    int i;
    Rect_t rec;
    unsigned int w, h;
    int update = 0;  // verifiy if updated is needed

    // board_0 draw
    for (i = 0; i < outputc; i++)  // run over all outputs
    {
        if (output[i].update)  // only if need update
        {
            output[i].update = 0;

            if (!update) {
                PICSimLab.CanvasCmd({.cmd = CC_INIT, .Init{Scale, Scale, 0}});
                PICSimLab.CanvasCmd({.cmd = CC_SETFONTWEIGHT, .SetFontWeight{CC_FONTWEIGHT_BOLD}});
            }
            update++;  // set to update buffer

            if (!output[i].r)  // if output shape is a rectangle
            {
                PICSimLab.CanvasCmd({.cmd = CC_SETFGCOLOR, .SetFgColor{0, 0, 0}});  // black

                switch (output[i].id)  // search for color of output
                {
                    case O_LPWR:  // Blue using mcupwr value
                        PICSimLab.CanvasCmd(
                            {.cmd = CC_SETCOLOR, .SetColor{(unsigned int)(200 * PICSimLab.GetMcuPwr() + 55), 0, 0}});
                        PICSimLab.CanvasCmd({.cmd = CC_RECTANGLE,
                                             .Rectangle{1, output[i].x1, output[i].y1, output[i].x2 - output[i].x1,
                                                        output[i].y2 - output[i].y1}});
                        break;
                    case O_MP:
                        PICSimLab.CanvasCmd(
                            {.cmd = CC_SETFONTSIZE,
                             .SetFontSize{(MGetPinCount() >= 100) ? 9 : ((MGetPinCount() > 14) ? 12 : 10)}});

                        PICSimLab.CanvasCmd({.cmd = CC_GETBITMAPSIZE, .GetBitmapSize{micbmp, &w, &h}});
                        PICSimLab.CanvasCmd({.cmd = CC_CHANGESCALE, .ChangeScale{1.0, 1.0}});
                        PICSimLab.CanvasCmd(
                            {.cmd = CC_PUTBITMAP, .PutBitmap{micbmp, output[i].x1 * Scale, output[i].y1 * Scale}});
                        PICSimLab.CanvasCmd({.cmd = CC_CHANGESCALE, .ChangeScale{Scale, Scale}});
                        PICSimLab.CanvasCmd({.cmd = CC_SETFGCOLOR, .SetFgColor{230, 230, 230}});

                        rec.x = output[i].x1;
                        rec.y = output[i].y1;
                        rec.width = w / Scale;
                        rec.height = h / Scale;
                        PICSimLab.CanvasCmd(
                            {.cmd = CC_TEXTONRECT,
                             .TextOnRect{Proc.c_str(), rec, CC_ALIGN_CENTER | CC_ALIGN_CENTER_VERTICAL}});
                        break;
                    case O_RST:
                        PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{100, 100, 100}});
                        PICSimLab.CanvasCmd({.cmd = CC_CIRCLE, .Circle{1, output[i].cx, output[i].cy, 11}});
                        if (p_RST) {
                            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{15, 15, 15}});
                        } else {
                            PICSimLab.CanvasCmd({.cmd = CC_SETCOLOR, .SetColor{55, 55, 55}});
                        }
                        PICSimLab.CanvasCmd({.cmd = CC_CIRCLE, .Circle{1, output[i].cx, output[i].cy, 9}});
                        break;
                }
            }
        }
    }

    // end draw
    if (update) {
        PICSimLab.CanvasCmd({.cmd = CC_END});
    }
}

void cboard_MSP430_Board::Run_CPU(void) {
    int i;
    // int j;
    unsigned char pi;
    unsigned int alm[64];
    const int pinc = MGetPinCount();

    // const int JUMPSTEPS = Window1.GetJUMPSTEPS ();
    // //number of steps skipped
    // FIXME NSTEP must be multiplied for 4
    const long int NSTEP = PICSimLab.GetNSTEP();  // number of steps in 100ms
    const float RNSTEP = 200.0 * pinc / NSTEP;

    // reset pins mean value
    memset(alm, 0, 64 * sizeof(unsigned int));

    // Spare parts window pre process
    if (use_spare)
        SpareParts.PreProcess();

    // j = JUMPSTEPS; //step counter
    pi = 0;
    if (PICSimLab.GetMcuPwr())       // if powered
        for (i = 0; i < NSTEP; i++)  // repeat for number of steps in 100ms
        {
            /*
            if (j >= JUMPSTEPS)//if number of step is bigger
            than steps to skip
             {
             }
             */
            // stopped by the debugger: time stands still
            if (DbgStopped())
                break;
            MStep();
            InstCounterInc();
            // Oscilloscope window process
            if (use_oscope)
                Oscilloscope.SetSample();
            // Spare parts window process
            if (use_spare)
                SpareParts.Process();

            // increment mean value counter if pin is high
            alm[pi] += pins[pi].value;
            pi++;
            if (pi == pinc)
                pi = 0;
            /*
            if (j >= JUMPSTEPS)//if number of step is bigger
            than steps to skip
             {
              j = -1; //reset counter
             }

            j++; //counter increment
             */
        }

    // calculate mean value
    for (pi = 0; pi < MGetPinCount(); pi++) {
        pins[pi].oavalue = (int)((alm[pi] * RNSTEP) + 55);
    }

    // Spare parts window pre post process
    if (use_spare)
        SpareParts.PostProcess();
}

int cboard_MSP430_Board::MInit(const char* processor, const char* fname, float freq_) {
    int ret = bsim_msp430::MInit(processor, fname, freq_);

    if (ret == -1) {
        PICSimLab.RegisterError("PICSimLab", "Unknown processor " + std::string(processor) + "! Loading Default.");
        bsim_msp430::MInit("MSP430FR2355", fname, freq_);
        Proc = "MSP430FR2355";
    }

    int bmp = PICSimLab.CanvasCmd(
        {.cmd = CC_LOADIMAGE,
         .LoadImage{(PICSimLab.GetSharePath() + "boards/Common/ic" + std::to_string(MGetPinCount()) + ".svg").c_str(),
                    Scale, 1, 0}});

    if (bmp < 0) {
        bmp = PICSimLab.CanvasCmd(
            {.cmd = CC_LOADIMAGE,
             .LoadImage{(PICSimLab.GetSharePath() + "boards/Common/ic6.svg").c_str(), Scale, 1, 0}});
        printf(
            "picsimlab: IC package with %i pins not "
            "found!\n",
            MGetPinCount());
        printf("picsimlab: %s not found!\n",
               (const char*)(PICSimLab.GetSharePath() + "boards/Common/ic" + std::to_string(MGetPinCount()) + ".svg")
                   .c_str());
    }

    if (micbmp)
        PICSimLab.CanvasCmd({.cmd = CC_FREEBITMAP, .FreeBitmap{micbmp}});
    micbmp = bmp;

    return ret;
}

void cboard_MSP430_Board::SetScale(double scale) {
    if (Scale == scale)
        return;

    Scale = scale;

    int bmp = PICSimLab.CanvasCmd(
        {.cmd = CC_LOADIMAGE,
         .LoadImage{(PICSimLab.GetSharePath() + "boards/Common/ic" + std::to_string(MGetPinCount()) + ".svg").c_str(),
                    Scale, 1, 0}});

    if (bmp < 0) {
        bmp = PICSimLab.CanvasCmd(
            {.cmd = CC_LOADIMAGE,
             .LoadImage{(PICSimLab.GetSharePath() + "boards/Common/ic6.svg").c_str(), Scale, 1, 0}});
        printf(
            "picsimlab: IC package with %i pins not "
            "found!\n",
            MGetPinCount());
        printf("picsimlab: %s not found!\n",
               (const char*)(PICSimLab.GetSharePath() + "boards/Common/ic" + std::to_string(MGetPinCount()) + ".svg")
                   .c_str());
    }

    if (micbmp)
        PICSimLab.CanvasCmd({.cmd = CC_FREEBITMAP, .FreeBitmap{micbmp}});
    micbmp = bmp;
}

std::string cboard_MSP430_Board::GetSupportedIDEs(void) {
    return "N/A,";
}

// Register the board in PICSimLab
board_init(BOARD_MSP430_Board_Name, cboard_MSP430_Board);
