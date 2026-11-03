/* ########################################################################

   PICSimLab - Programmable IC Simulator Laboratory

   ########################################################################

   LS013B7DH05 part: Copyright (c) 2026 jjd408, after the pcd8544 part. PICSimLab: Copyright (c) 2010-2026 Luis Claudio Gambôa Lopes.

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

#include "output_LCD_ls013b7dh05.h"
#include "../lib/oscilloscope.h"
#include "../lib/picsimlab.h"
#include "../lib/spareparts.h"

/* outputs */
enum { O_P1, O_P2, O_P3, O_P4, O_P5, O_F1, O_F2, O_F3, O_LCD, O_STATUS };

static PCWProp pcwprop[11] = {{PCW_COMBO, "1-SCLK"},        {PCW_COMBO, "2-SI"},
                              {PCW_COMBO, "3-SCS"},         {PCW_COMBO, "4-EXTCOMIN"},
                              {PCW_COMBO, "5-DISP"},        {PCW_LABEL, "6-VDDA,+3V"},
                              {PCW_LABEL, "7-VDD,+3V"},     {PCW_LABEL, "8-EXTMODE,+3V"},
                              {PCW_LABEL, "9-VSS,GND"},     {PCW_LABEL, "10-VSSA,GND"},
                              {PCW_END, ""}};

static const int SCALE = 2;
// Reflective memory LCD: white is a pale grey, black a near-black grey
static const unsigned int WHITE[3] = {206, 210, 200}, BLACK[3] = {34, 37, 40};

cpart_LCD_ls013b7dh05::cpart_LCD_ls013b7dh05(const unsigned x, const unsigned y, const char* name,
                                             const char* type, board* pboard_, const int id_)
    : part(x, y, name, type, pboard_, id_) {
    X = x;
    Y = y;
    ReadMaps();
    BitmapId = -1;

    LoadPartImage();

    for (int i = 0; i < 5; i++)
        input_pins[i] = 0;
    steps = 0;

    SetPCWProperties(pcwprop);

    PinCount = 5;
    Pins = input_pins;
}

cpart_LCD_ls013b7dh05::~cpart_LCD_ls013b7dh05(void) {
    SpareParts.CanvasCmd({.partn = id, .cmd = CC_FREEBITMAP, .FreeBitmap{BitmapId}});
    SpareParts.CanvasCmd({.partn = id, .cmd = CC_DESTROY});
}

void cpart_LCD_ls013b7dh05::Reset(void) {
    lcd.Reset();
    steps = 0;
    status_shown.clear();
}

static void pin_box(int id, output_t& o, const char* text, int fg) {
    SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETFONTSIZE, .SetFontSize{8}});
    SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETCOLOR, .SetColor{49, 61, 99}});
    SpareParts.CanvasCmd(
        {.partn = id, .cmd = CC_RECTANGLE, .Rectangle{1, o.x1, o.y1, o.x2 - o.x1, o.y2 - o.y1}});
    SpareParts.CanvasCmd(
        {.partn = id, .cmd = CC_SETFGCOLOR, .SetFgColor{(unsigned)fg, (unsigned)fg, (unsigned)fg}});
    SpareParts.CanvasCmd({.partn = id, .cmd = CC_ROTATEDTEXT, .RotatedText{text, o.x1, o.y2, 90.0}});
}

void cpart_LCD_ls013b7dh05::DrawOutput(const unsigned int i) {
    switch (output[i].id) {
        case O_P1:
        case O_P2:
        case O_P3:
        case O_P4:
        case O_P5: {
            unsigned char pin = input_pins[output[i].id - O_P1];
            std::string label = pin ? SpareParts.GetPinName(pin) : "NC";
            pin_box(id, output[i], label.c_str(), 255);
            break;
        }
        case O_F1:
            pin_box(id, output[i], "3V", 155);
            break;
        case O_F2:
            pin_box(id, output[i], "3V", 155);  // EXTMODE high: EXTCOMIN drives COM inversion
            break;
        case O_F3:
            pin_box(id, output[i], "GND", 155);
            break;
        case O_LCD: {
            // Redraw the changed lines as runs of one colour, SCALE x SCALE per pixel
            int full = 0;
            for (int y = 0; y < lcd_ls013b7dh05::H; y++) {
                if (!lcd.dirty[y])
                    continue;
                lcd.dirty[y] = 0;
                full++;
                int x = 0;
                while (x < lcd_ls013b7dh05::W) {
                    int white = lcd.Disp() ? lcd.Pixel(x, y) : 1;
                    int run = 1;
                    while (x + run < lcd_ls013b7dh05::W && (lcd.Disp() ? lcd.Pixel(x + run, y) : 1) == white)
                        run++;
                    const unsigned int* c = white ? WHITE : BLACK;
                    SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETFGCOLOR, .SetFgColor{c[0], c[1], c[2]}});
                    SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETCOLOR, .SetColor{c[0], c[1], c[2]}});
                    SpareParts.CanvasCmd({.partn = id,
                                          .cmd = CC_RECTANGLE,
                                          .Rectangle{1, (float)(output[i].x1 + x * SCALE),
                                                     (float)(output[i].y1 + y * SCALE), (float)(run * SCALE),
                                                     (float)SCALE}});
                    x += run;
                }
            }
            lcd.any_dirty = 0;
            break;
        }
        case O_STATUS: {
            SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETCOLOR, .SetColor{40, 40, 40}});
            SpareParts.CanvasCmd({.partn = id,
                                  .cmd = CC_RECTANGLE,
                                  .Rectangle{1, output[i].x1, output[i].y1, output[i].x2 - output[i].x1,
                                             output[i].y2 - output[i].y1}});
            SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETFONTSIZE, .SetFontSize{8}});
            if (lcd.problem.empty())
                SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETFGCOLOR, .SetFgColor{160, 220, 160}});
            else
                SpareParts.CanvasCmd({.partn = id, .cmd = CC_SETFGCOLOR, .SetFgColor{255, 120, 100}});
            Rect_t rec;
            rec.x = output[i].x1;
            rec.y = output[i].y1;
            rec.width = output[i].x2 - output[i].x1;
            rec.height = output[i].y2 - output[i].y1;
            SpareParts.CanvasCmd({.partn = id,
                                  .cmd = CC_TEXTONRECT,
                                  .TextOnRect{status_shown.c_str(), rec, CC_ALIGN_CENTER | CC_ALIGN_CENTER_VERTICAL}});
            break;
        }
    }
}

unsigned short cpart_LCD_ls013b7dh05::GetInputId(char* name) {
    printf("Error input '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

unsigned short cpart_LCD_ls013b7dh05::GetOutputId(char* name) {
    static const char* names[] = {"PN_1", "PN_2", "PN_3", "PN_4", "PN_5", "PN_F1", "PN_F2", "PN_F3", "DS_LCD",
                                  "ST_STATUS"};
    for (unsigned n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
        if (strcmp(name, names[n]) == 0)
            return n;
    }
    printf("Error output '%s' don't have a valid id! \n", name);
    return INVALID_ID;
}

std::string cpart_LCD_ls013b7dh05::WritePreferences(void) {
    char prefs[256];
    sprintf(prefs, "%hhu,%hhu,%hhu,%hhu,%hhu", input_pins[0], input_pins[1], input_pins[2], input_pins[3],
            input_pins[4]);
    return prefs;
}

int cpart_LCD_ls013b7dh05::ReadPreferences(std::string value) {
    int ret = sscanf(value.c_str(), "%hhu,%hhu,%hhu,%hhu,%hhu", &input_pins[0], &input_pins[1], &input_pins[2],
                     &input_pins[3], &input_pins[4]);
    Reset();
    return ret;
}

void cpart_LCD_ls013b7dh05::ConfigurePropertiesWindow(void) {
    SetPCWComboWithPinNames("combo1", input_pins[0]);
    SetPCWComboWithPinNames("combo2", input_pins[1]);
    SetPCWComboWithPinNames("combo3", input_pins[2]);
    SetPCWComboWithPinNames("combo4", input_pins[3]);
    SetPCWComboWithPinNames("combo5", input_pins[4]);
}

void cpart_LCD_ls013b7dh05::ReadPropertiesWindow(void) {
    input_pins[0] = GetPWCComboSelectedPin("combo1");
    input_pins[1] = GetPWCComboSelectedPin("combo2");
    input_pins[2] = GetPWCComboSelectedPin("combo3");
    input_pins[3] = GetPWCComboSelectedPin("combo4");
    input_pins[4] = GetPWCComboSelectedPin("combo5");
}

void cpart_LCD_ls013b7dh05::Process(void) {
    steps++;
    if (!input_pins[0] || !input_pins[1] || !input_pins[2])
        return;
    const picpin* ppins = SpareParts.GetPinsValues();
    double now = steps / pboard->MGetInstClockFreq();
    int extcomin = input_pins[3] ? ppins[input_pins[3] - 1].value : 0;
    int disp = input_pins[4] ? ppins[input_pins[4] - 1].value : 1;
    lcd.Step(now, ppins[input_pins[0] - 1].value, ppins[input_pins[1] - 1].value, ppins[input_pins[2] - 1].value,
             extcomin, disp);
}

void cpart_LCD_ls013b7dh05::PostProcess(void) {
    if (lcd.any_dirty)
        output_ids[O_LCD]->update = 1;

    std::string status;
    if (!lcd.problem.empty()) {
        status = lcd.problem;
    } else if (input_pins[3] && (lcd.com_hz < 57 || lcd.com_hz > 66)) {
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

void cpart_LCD_ls013b7dh05::LoadPartImage(void) {
    part::LoadPartImage();
    memset(lcd.dirty, 1, sizeof(lcd.dirty));
    lcd.any_dirty = 1;
}

part_init(PART_LCD_LS013B7DH05_Name, cpart_LCD_ls013b7dh05, "Output");
