/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "printer.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "tape.h"

#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

#define Z_HOVERING 10

// All templates should be in a separate file somewhere so that we don't
// have to compile.

const char *const gcode_preamble = R"(
  ; Preamble. Fill be whatever is necessary to init.
  ; Assumes an 'A' axis that rotates the pick'n place nozzle. The values
  ; 0..360 correspond to degrees.
)";

// param: x, y, zdown, a, zup
const char *const pick_gcode = R"(
  G1 X%.3f Y%.3f Z%.3f A%.3f ; Move over component to pick.
  ; TODO(jerkey) code to switch on suckage
  G1 Z%.3f  ; Move up a bit for traveling
)";

// param: x, y, zup, a, zdown, zup
const char *const place_gcode = R"(
  G1 X%.3f Y%.3f Z%.3f A%.3f ; Move over component to place.
  G1 Z%.3f  ; move down.
  ; TODO(jerkey) code to switch off suckage
  ; TODO(jerkey) code to switch on 'spitting out': short burst
  G1 Z%.3f  ; Move up
)";

// component type -> Tape
struct GCodePickNPlace::Config {
    typedef std::map<std::string, Tape*> PartToTape;

    Position board_origin;  // TODO: potentially rotation...
    PartToTape tape_for_component;
};

GCodePickNPlace::Config *
GCodePickNPlace::ParseConfig(const std::string& filename) {
    std::unique_ptr<Config> result(new Config());

    std::string token;
    float x, y, z;
    Tape* current_tape = NULL;

    std::ifstream in(filename);
    while (result && !in.eof()) {
        token.clear();
        in >> token;

        char buffer[1024];
        in.getline(buffer, sizeof(buffer), '\n');

        if (token.empty() || token[0] == '#')
            continue;

        if (token == "Board:") {
            if (current_tape) current_tape = NULL;
        } else if (token == "Tape:") {
            current_tape = new Tape();
            // This tape is valid for multiple values/footprints possibly.
            // Lets all parse them
            token.clear();
            std::string all_the_names = buffer;
            std::stringstream parts(all_the_names);
            while (!parts.eof()) {
                parts >> token;
                result->tape_for_component[token] = current_tape;
            }
        } else if (token == "origin:") {
            if (current_tape) {
                if (3 != sscanf(buffer, "%f %f %f", &x, &y, &z)) {
                    fprintf(stderr, "Parse problem tape origin: '%s'\n",
                            buffer);
                    result.reset(NULL);
                }
                current_tape->SetFirstComponentPosition(x, y, z);
            } else {
                if (2 != sscanf(buffer, "%f %f",
                                &result->board_origin.x, 
                                &result->board_origin.y)) {
                    fprintf(stderr, "Parse problem board origin: '%s'\n",
                            buffer);
                    result.reset(NULL);
                }
            }
        } else if (token == "spacing:") {
            if (!current_tape) {
                std::cerr << "spacing without tape";
                result.reset(NULL);
                break;
            }
            if (2 != sscanf(buffer, "%f %f", &x, &y)) {
                fprintf(stderr, "Parse problem spacing: '%s'\n", buffer);  // line no ?
                result.reset(NULL);
            }
            if (x == 0 && y == 0) {
                fprintf(stderr, "Spacing: eat least one needs to be set '%s'\n",
                        buffer);  // line no ?
                result.reset(NULL);
            }
            current_tape->SetComponentSpacing(x, y);
        } else if (token == "spacing:") {
            if (!current_tape) {
                std::cerr << "spacing without tape";
                result.reset(NULL);
                break;
            }
            if (1 != sscanf(buffer, "%f", &x)) {
                fprintf(stderr, "Parse problem angle: '%s'\n", buffer);  // line no ?
                result.reset(NULL);
            }
            current_tape->SetAngle(x);
        } else if (token == "count:") {
            if (!current_tape) {
                std::cerr << "Count without tape.";
                result.reset(NULL);
                break;
            }
            int count;
            if (1 != sscanf(buffer, "%d", &count)) {
                fprintf(stderr, "Parse problem count: '%s'.\n", buffer);  // line no ?
                result.reset(NULL);
            }
            current_tape->SetNumberComponents(count);
        }
    }
    return result.release();
}

GCodePickNPlace::GCodePickNPlace(const std::string& filename)
    : config_(ParseConfig(filename)) {
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
    // param: x, y, zdown, a, zup
    printf(pick_gcode,
           px, py, pz,                  // component pos.
           fmod(tape->angle(), 360.0),  // pickup angle
           pz + Z_HOVERING);

    // TODO: right now, we are assuming the z is the same height as
    // param: x, y, zup, a, zdown, zup
    printf(place_gcode,
          part.pos.x, part.pos.y, pz + Z_HOVERING,
          fmod(part.angle - tape->angle() + 360, 360.0),
          pz,
          pz + Z_HOVERING);
}

void GCodePickNPlace::Finish() {
    printf(";done\n");
}
