/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "printer.h"

#include <assert.h>
#include <stdio.h>

#include "tape.h"

#include <memory>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

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
    fprintf(stderr, "Board-origin: (%.3f, %.3f)\n",
            config_->board_origin.x, config_->board_origin.y);
    for (const auto &t : config_->tape_for_component) {
        fprintf(stderr, "%s\t", t.first.c_str());
        t.second->DebugPrint();
        fprintf(stderr, "\n");
    }
}

void GCodePickNPlace::Init(const Dimension& dim) {
}
void GCodePickNPlace::PrintPart(const Part &part) {
}
void GCodePickNPlace::Finish() {
}
