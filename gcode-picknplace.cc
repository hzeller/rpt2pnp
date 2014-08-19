/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "printer.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "tape.h"
#include "pnp-config.h"

// Hovering while transporting a component.
#define Z_HOVERING 10

// Placement needs to be a bit higher.
//#define TAPE_TO_BOARD_DIFFZ 1.6
#define TAPE_TO_BOARD_DIFFZ -2.0

// All templates should be in a separate file somewhere so that we don't
// have to compile.

// Multiplication to get 360 degrees mapped to one turn.
#define ANGLE_FACTOR (50.34965 / 360)

const char *const gcode_preamble = R"(
; Preamble. Fill be whatever is necessary to init.
; Assumes an 'A' axis that rotates the pick'n place nozzle. The values
; 0..360 correspond to absolute degrees.
; (correction: for now, we mess with an E-axis instead of A)
G28 X0 Y0  ; Now home (x/y) - needle over free space
G28 Z0     ; Now it is safe to home z
T1         ; Use E1 extruder
M302
G92 E0

G1 Z35 E0 F2500 ; Move needle out of way
)";

// param: name, x, y, zup, zdown, a, zup
const char *const pick_gcode = R"(
; Pick %s
G1 X%.3f Y%.3f Z%.3f E%.3f ; Move over component to pick.
G1 Z%.3f   ; move down
G4
M42 P6 S255  ; turn on suckage
G1 Z%.3f  ; Move up a bit for traveling
)";

// param: name, x, y, zup, a, zdown, zup
const char *const place_gcode = R"(
; Place %s
G1 X%.3f Y%.3f Z%.3f E%.3f ; Move over component to place.
G1 Z%.3f    ; move down.
G4
M42 P6 S0    ; turn off suckage
G4
M42 P8 S255  ; blow
G4 P100      ; .. for 100ms
M42 P8 S0    ; done.
G1 Z%.3f   ; Move up
)";

GCodePickNPlace::GCodePickNPlace(const char *filename)
    : config_(ParsePnPConfiguration(filename)) {
    assert(config_);
#if 0
    fprintf(stderr, "Board-origin: (%.3f, %.3f)\n",
            config_->board_origin.x, config_->board_origin.y);
    for (const auto &t : config_->tape_for_component) {
        fprintf(stderr, "%s\t", t.first.c_str());
        t.second->DebugPrint();
        fprintf(stderr, "\n");
    }
#endif
}

void GCodePickNPlace::Init(const Dimension& dim) {
    printf("%s", gcode_preamble);
}

void GCodePickNPlace::PrintPart(const Part &part) {
    const std::string key = part.footprint + "@" + part.value;
    auto found = config_->tape_for_component.find(key);
    if (found == config_->tape_for_component.end()) {
        fprintf(stderr, "No tape for '%s'\n", key.c_str());
        return;
    }
    Tape *tape = found->second;
    float px, py, pz;
    if (!tape->GetNextPos(&px, &py, &pz)) {
        fprintf(stderr, "We are out of components for '%s'\n", key.c_str());
        return;
    }

    const std::string print_name = part.component_name + " (" + key + ")";
    // param: name, x, y, zdown, a, zup
    printf(pick_gcode,
           print_name.c_str(),
           px, py, pz + Z_HOVERING,                  // component pos.
           ANGLE_FACTOR * fmod(tape->angle(), 360.0),  // pickup angle
           pz,   // down to component
           pz + Z_HOVERING);

    // TODO: right now, we are assuming the z is the same height as
    // param: name, x, y, zup, a, zdown, zup
    printf(place_gcode,
           print_name.c_str(),
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y, pz + Z_HOVERING,
           ANGLE_FACTOR * fmod(part.angle - tape->angle() + 360, 360.0),
           pz + TAPE_TO_BOARD_DIFFZ,
           pz + Z_HOVERING);
}

void GCodePickNPlace::Finish() {
    printf("\nM84 ; done.\n");
}
