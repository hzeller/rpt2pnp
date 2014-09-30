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
// TODO: calculate that to not knock over already placed components.
#define Z_HOVERING 10

// Components are a bit higher as they are resting on some card-board. Let's
// assume some value here.
// Ultimately, we want the placement operation be a bit spring-loaded.
#define TAPE_THICK 0.0

// All templates should be in a separate file somewhere so that we don't
// have to compile.

// Multiplication to get 360 degrees mapped to one turn. This is specific to
// our stepper motor. TODO: make configurable
#define ANGLE_FACTOR (50.34965 / 360)

// Speeds in mm/s
#define TO_TAPE_SPEED 1000  // moving needle to tape
#define TO_BOARD_SPEED 300  // moving component from tape to board

// param: moving needle up.
static const char *const gcode_preamble = R"(
; Preamble. Fill with whatever is necessary to init.
; Assumes an 'A' axis that rotates the pick'n place nozzle. The values
; 0..360 correspond to absolute degrees.
; (correction: for now, we mess with an E-axis instead of A)
G28 X0 Y0  ; Home (x/y) - needle over free space
G28 Z0     ; Now it is safe to home z
G21        ; set to mm
T1         ; Use E1 extruder, our 'A' axis.
M302       ; cold extrusion override - because it is not actually an extruder.
G90        ; Use absolute positions in general.
G92 E0     ; 'home' E axis

G1 Z%.1f E0 ; Move needle out of way
)";

// param: name, move-speed, x, y, zup, zdown, a, zup
static const char *const pick_gcode = R"(
;; -- Pick %s
G0 F%d X%.3f Y%.3f Z%.3f E%.3f ; Move over component to pick.
G1 Z%-6.2f   F4000 ; move down on tape.
G4           ; flush buffer
M42 P6 S255  ; turn on suckage
G1 Z%-6.3f   ; Move up a bit for travelling
)";

// param: name, place-speed, x, y, zup, a, zdown, zup
static const char *const place_gcode = R"(
;; -- Place %s
G0 F%d X%.3f Y%.3f Z%.3f E%.3f ; Move component to place on board.
G1 Z%-6.3f F4000 ; move down over board thickness.
G4            ; flush buffer.
M42 P6 S0     ; turn off suckage
G4            ; flush buffer.
M42 P8 S255   ; blow
G4 P40        ; .. for 40ms
M42 P8 S0     ; done.
G1 Z%-6.2f    ; Move up
)";

static const char *const gcode_finish = R"(
M84   ; stop motors
)";

GCodePickNPlace::GCodePickNPlace(const PnPConfig *config) : config_(config) {
    if (config_ == NULL) {
        fprintf(stderr, "Need configuration for pick'n placing.");
        exit(1);
    }
    fprintf(stderr, "Board-thickness = %.1fmm\n",
            config_->board.top - config_->bed_level);
}

void GCodePickNPlace::Init(const std::string &init_comment,
                           const Dimension& dim) {
    printf("; %s\n", init_comment.c_str());
    float highest_tape = config_->board.top;
    for (const auto &t : config_->tape_for_component) {
        highest_tape = std::max(highest_tape, t.second->height());
    }
    printf(gcode_preamble, highest_tape + 10);
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
    if (!tape->GetPos(&px, &py, &pz)) {
        fprintf(stderr, "We are out of components for '%s'\n", key.c_str());
        return;
    }
    tape->Advance();

    const float board_thick = config_->board.top - config_->bed_level;
    const float travel_height = pz + board_thick + Z_HOVERING;
    const std::string print_name = part.component_name + " (" + key + ")";

    // param: name, x, y, zdown, a, zup
    printf(pick_gcode,
           print_name.c_str(),
           60 * TO_TAPE_SPEED,
           px, py, pz + Z_HOVERING,                    // component pos.
           ANGLE_FACTOR * fmod(tape->angle(), 360.0),  // pickup angle
           pz,                                         // down to component
           travel_height);                             // up for travel.

    // param: name, x, y, zup, a, zdown, zup
    printf(place_gcode,
           print_name.c_str(),
           60 * TO_BOARD_SPEED,
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y,
           travel_height,
           ANGLE_FACTOR * fmod(part.angle - tape->angle() + 360, 360.0),
           pz + board_thick - TAPE_THICK,
           travel_height);
}

void GCodePickNPlace::Finish() {
    printf(gcode_finish);
}
