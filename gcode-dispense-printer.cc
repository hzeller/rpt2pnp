/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "printer.h"

#define Z_DISPENSING "1.7"        // Position to dispense stuff. Just above board.
#define Z_HOVER_DISPENSER "2.5"   // Hovering above position.
#define Z_HIGH_UP_DISPENSER "5"   // high up to separate paste.

#include "printer.h"

// Printer for dispensing pads (not really working right now)
GCodeDispensePrinter::GCodeDispensePrinter(float init_ms, float area_ms)
    : init_ms_(init_ms), area_ms_(area_ms) {}

void GCodeDispensePrinter::Init(const Dimension& dim) {
    //OptimizeParts(&parts);

    printf("; rpt2pnp -d %.2f -D %.2f file.rpt\n", init_ms_, area_ms_);
    // G-code preamble. Set feed rate, homing etc.
    printf(
           //    "G28\n" assume machine is already homed before g-code is executed
           "G21\n" // set to mm
           "G0 F20000\n"
           "G1 F4000\n"
           "G0 Z" Z_HIGH_UP_DISPENSER "\n"
           );
}

void GCodeDispensePrinter::PrintPart(const Part &part) {
    // move to new position, above board
    printf("G0 X%.3f Y%.3f E%.3f Z" Z_HOVER_DISPENSER " ; comp=%s val=%s\n",
           // "G1 Z" Z_HIGH_UP_DISPENSER "\n", // high above to have paste is well separated
           part.pos.x, part.pos.y, part.angle,
           part.component_name.c_str(), part.value.c_str());
}

void GCodeDispensePrinter::Finish() {
    printf(";done\n");
}
