/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <string>
#include <iostream>

#include "rpt-parser.h"

// Very crude parser. No error handling. Quick hack.
bool RptParse(std::istream *input, ParseEventReceiver *event) {
    float unit_to_mm = 1;

    // Board dimensions.
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    bool in_pad = false;

    while (!input->eof()) {
        std::string token;
        (*input) >> token;

        if (token == "unit") {
            std::string value;
            (*input) >> value;
            if (value == "INCH")
                unit_to_mm = 25.4;
        }
        else if (token == "upper_left_corner") {  // in $BOARD
            (*input) >> x1 >> y1;
        }
        else if (token == "lower_right_corner") {  // in $BOARD
            (*input) >> x2 >> y2;
        }
        else if (token == "$EndBOARD") {
            // Now we have everything together to announcd the board
            // dimensions.
            event->StartBoard((x2 - x1) * unit_to_mm,
                              (y2 - y1) * unit_to_mm);
        }
        else if (token == "$MODULE") {
            std::string value;
            (*input) >> value;
            value = value.substr(1, value.length() - 2);
            event->StartComponent(value);
            in_pad = false;
        }
        else if (token == "$EndMODULE")
            event->EndComponent();
        else if (token == "$PAD") {
            in_pad = true;
            std::string value;
            (*input) >> value;
            event->StartPad(value);
        }
        else if (token == "$EndPAD")
            event->EndPad();
        else if (token == "position") {
            float x, y;
            (*input) >> x >> y;
            // Pad positions are relative to module positions
            if (in_pad) {
                y = -y;
            } else {
                x -= x1;
                y = y2 - y; // somehow we're mirrored.
            }
            event->Position(x * unit_to_mm, y * unit_to_mm);
        }
        else if (token == "size") {
            float w, h;
            (*input) >> w >> h;
            event->Size(w * unit_to_mm, h * unit_to_mm);
        }
        else if (token == "drill") {
            float dia;
            (*input) >> dia;
            event->Drill(dia * unit_to_mm);
        }
        else if (token == "orientation") {
            float angle;
            (*input) >> angle;
            event->Orientation(angle);
        }
        else if (token == "value") {
            std::string value;
            (*input) >> value;
            value = value.substr(1, value.length() - 2);
            event->Value(value);
        }
        else if (token == "footprint") {
            std::string value;
            (*input) >> value;
            value = value.substr(1, value.length() - 2);
            event->Footprint(value);
        }
    }
    return true;
}
