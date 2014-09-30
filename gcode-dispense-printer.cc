/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "printer.h"

#include "pnp-config.h"

#include <math.h>

// These might need tweaking
#define Z_DISPENSING_ABOVE 0.3      // Above board when dispensing
#define Z_HOVER_ABOVE 2             // Above board when moving around
#define Z_SEPARATE_DROPLET_ABOVE 5  // Above board right after dispensing.

// One parameter: initial height.
static const char *const gcode_preamble = R"(
G28 X0 Y0  ; Home (x/y) - needle over free space
G28 Z0     ; Now it is safe to home z
G21        ; set to mm
G90        ; Use absolute positions.
G0 F20000  ; fast moving...
G1 F4000   ; slower dispense moves.
G0 Z%.1f   ; Move needle out of way.
)";

// move to new position, above board.
// param: component-name, pad-name, x, y, hover-z
static const char *const gcode_move_pos = R"(
;; -- component %s, pad %s
G0 X%.3f Y%.3f Z%.3f   ; move there.)";

// Dispense paste.
// param: z-dispense-height, wait-time-ms, area, z-separate-droplet
static const char *const gcode_dispense = R"(
G1 Z%.2f ; Go down to dispense
M106      ; switch on fan (=solenoid)
G4 P%-5.1f ; Wait time dependent on area %.2f mm^2
M107      ; switch off solenoid
G1 Z%.2f ; high above to have paste separated
)";

static const char *const gcode_finish = R"(
M84   ; stop motors
)";

// Printer for dispensing pads (not really working right now)
GCodeDispensePrinter::GCodeDispensePrinter(const PnPConfig *config,
                                           float init_ms, float area_ms)
    : config_(config), init_ms_(init_ms), area_ms_(area_ms) {
    if (config_ == NULL) {
        fprintf(stderr, "Need configuration for dispensing.");
        exit(1);
    }
}

void GCodeDispensePrinter::Init(const std::string &init_comment,
                                const Dimension& dim) {
    printf("; %s\n", init_comment.c_str());
    printf("; init-ms=%.1f area-ms=%.1f\n", init_ms_, area_ms_);
    // G-code preamble. Set feed rate, homing etc.
    printf(gcode_preamble, config_->board.top + 15);
}

void GCodeDispensePrinter::PrintPart(const Part &part) {
    const float angle = 2 * M_PI * part.angle / 360.0;
    for (const Pad &pad : part.pads) {
        const float part_x = config_->board.origin.x + part.pos.x;
        const float part_y = config_->board.origin.y + part.pos.y;
        const float area = pad.size.w * pad.size.h;
        const float pad_x = pad.pos.x + pad.size.w/2;
        const float pad_y = pad.pos.y + pad.size.h/2;
        const float x = part_x + pad_x * cos(angle) - pad_y * sin(angle);
        const float y = part_y + pad_x * sin(angle) + pad_y * cos(angle);
        printf(gcode_move_pos,
               part.component_name.c_str(), pad.name.c_str(),
               x, y, config_->board.top + Z_HOVER_ABOVE);
        printf(gcode_dispense,
               config_->board.top + Z_DISPENSING_ABOVE,
               init_ms_ + area * area_ms_, area,
               config_->board.top + Z_SEPARATE_DROPLET_ABOVE);
    }
}

void GCodeDispensePrinter::Finish() {
    printf(gcode_finish);
}
