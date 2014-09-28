/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "pnp-config.h"

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "tape.h"

PnPConfig *ParsePnPConfiguration(const std::string& filename) {
    std::unique_ptr<PnPConfig> result(new PnPConfig());

    // TODO: this parsing is very simplistic.
    // TODO: and ugly with c++ streams.

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
                                &result->board.origin.x, 
                                &result->board.origin.y)) {
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
                fprintf(stderr, "Parse problem spacing: '%s'\n", buffer);
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
                fprintf(stderr, "Parse problem angle: '%s'\n", buffer);
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
                fprintf(stderr, "Parse problem count: '%s'.\n", buffer);
                result.reset(NULL);
            }
            current_tape->SetNumberComponents(count);
        }
    }
    return result.release();
}

PnPConfig *ParseSimplePnPConfiguration(const std::string& filename) {
    std::unique_ptr<PnPConfig> result(new PnPConfig());

    FILE *in = fopen(filename.c_str(), "r");
    if (!in) {
        fprintf(stderr, "Can't open %s\n", filename.c_str());
        return NULL;
    }
    char buffer[1024];
    float x, y, z;
    int tape_idx;
    char package_id[256];
    while (fgets(buffer, sizeof(buffer), in)) {
        if (5 == sscanf(buffer, "tape%d:%s %f %f %f\n", &tape_idx, package_id,
                        &x, &y, &z)) {
            if (tape_idx == 1) {
                Tape *t = new Tape();
                t->SetFirstComponentPosition(x, y, z);
                result->tape_for_component[package_id] = t;
            } else {
                PnPConfig::PartToTape::iterator found;
                found = result->tape_for_component.find(package_id);
                if (found != result->tape_for_component.end()) {
                    Tape *t = found->second;
                    const int advance = tape_idx - 1;
                    float old_x, old_y, old_z;
                    t->GetPos(&old_x, &old_y, &old_z);
                    found->second->SetComponentSpacing((x - old_x) / advance,
                                                       (y - old_y) / advance);
                }
            }
            fprintf(stderr, "Yay, got %d '%s' (%.1f,%.1f,%.1f)\n", tape_idx,
                   package_id, x, y, z);
        } else {
            fprintf(stderr, "Couldn't parse '%s'\n", buffer);
        }
    }

    return result.release();
}

