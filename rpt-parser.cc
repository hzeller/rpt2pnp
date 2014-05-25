/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <string>
#include <iostream>

#include "rpt-parser.h"

// Very crude parser. No error handling. Quick hack.
bool RptParse(std::istream *input, ParseEventReceiver *event) {
    float unit_conversion = 1;
    while (!input->eof()) {
        std::string token;
        (*input) >> token;
        if (token == "unit") {
            std::string value;
            (*input) >> value;
            if (value == "INCH")
                unit_conversion = 25.4;
        }
        else if (token == "$MODULE") {
            std::string value;
            (*input) >> value;
            event->StartComponent(value);
        }
        else if (token == "$EndMODULE")
            event->EndComponent();
        else if (token == "$PAD") {
            std::string value;
            (*input) >> value;
            event->StartPad(value);
        }
        else if (token == "$EndPAD")
            event->EndPad();
        else if (token == "position") {
            float x, y;
            (*input) >> x >> y;
            event->Position(x * unit_conversion, y * unit_conversion);
        }
        else if (token == "size") {
            float w, h;
            (*input) >> w >> h;
            event->Size(w * unit_conversion, h * unit_conversion);
        }
        else if (token == "drill") {
            float dia;
            (*input) >> dia;
            event->Drill(dia * unit_conversion);
        }
        else if (token == "orientation") {
            float angle;
            (*input) >> angle;
            event->Orientation(angle);
        }
    }
    return true;
}
