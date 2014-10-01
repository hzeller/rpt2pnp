/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>
#include <map>

#include "board.h"
#include "tape.h"
#include "pnp-config.h"
#include "machine.h"
#include "rpt-parser.h"
#include "rpt2pnp.h"

static const float minimum_milliseconds = 50;
static const float area_to_milliseconds = 25;  // mm^2 to milliseconds.

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s <options> <rpt-file>\n"
            "Options:\n"
            "\t-h      : Create homer configuration script from rpt.\n"
            //"\t-t      : Create config template from rpt to stdout. "
            //"Needs editing.\n"
            "\t-l      : List found <footprint>@<component> <count> from rpt "
            "to stdout.\n"
            "[Operations]\n"
            //"\t-c <config> : Use edited config from -t \n"
            "\t-C <config> : Use homer config created via homer from -h\n"
            "\t-d      : Dispensing solder paste GCode generation.\n"
            "\t-p      : Pick'n place GCode generation.\n"
            "\t-P      : Output as PostScript.\n",
            prog);
    return 1;
}

typedef std::map<std::string, int> ComponentCount;

// Extract components on board and their counts. Returns total components found.
int ExtractComponents(const Board::PartList& list, ComponentCount *c) {
    int total_count = 0;
    for (const Part* part : list) {
        const std::string key = part->footprint + "@" + part->value;
        (*c)[key]++;
        ++total_count;
    }
    return total_count;
}

const Part *FindPartClosestTo(const Board::PartList& list, const Position &pos) {
    const Part* result = NULL;
    float closest = -1;
    for (const Part* part : list) {
        const float dist = Distance(part->pos, pos);
        if (closest < 0 || dist < closest) {
            result = part;
            closest = dist;
        }
    }
    return result;
}

void CreateConfigTemplate(const Board::PartList& list) {
    printf("Board:\norigin: 100 100 # x/y origin of the board\n\n");    

    printf("# This template provides one <footprint>@<component> per tape,\n");
    printf("# but if you have multiple components that are indeed the same\n");
    printf("# e.g. smd0805@100n smd0805@0.1uF, then you can just put them\n");
    printf("# space delimited behind each Tape:\n");
    printf("#   Tape: smd0805@100n smd0805@0.1uF\n");
    printf("# Each Tape section requires\n");
    printf("#   'origin:', which is the (x/y/z) position of\n");
    printf("# the top of the first component (z: pick-up-height). And\n");
    printf("#   'spacing:', (dx,dy) to the next one\n#\n");
    printf("# Also there are the following optional parameters\n");
    printf("#angle: 0     # Optional: Default rotation of component on tape.\n");
    printf("#count: 1000  # Optional: available count on tape\n");
    printf("\n");

    ComponentCount components;
    const int total_count = ExtractComponents(list, &components);
    for (const auto &pair : components) {
        printf("\nTape: %s\n", pair.first.c_str());
        printf("origin:  10 20 2 # fill me\n");
        printf("spacing: 4 0   # fill me\n");
    }
    fprintf(stderr, "%d components total\n", total_count);
}

void CreateList(const Board::PartList& list) {
    ComponentCount components;
    const int total_count = ExtractComponents(list, &components);
    int longest = -1;
    for (const auto &pair : components) {
        longest = std::max((int)pair.first.length(), longest);
    }
    for (const auto &pair : components) {
        printf("%-*s %4d\n", longest, pair.first.c_str(), pair.second);
    }
    fprintf(stderr, "%d components total\n", total_count);
}

void CreateHomerInstruction(const Board &board) {
    printf("bedlevel:BedLevel-Z\tTouch needle on bed next to board\n");
    ComponentCount components;
    ExtractComponents(board.parts(), &components);
    for (const auto &pair : components) {
        printf("tape%d:%s\tfind first component\n",
               1, pair.first.c_str());
        int next_pos = std::min(pair.second, 4);
        if (next_pos > 1) {
            printf("tape%d:%s\tfind %d. component\n",
                   next_pos, pair.first.c_str(), next_pos);
        }
    }
    const Part *board_part = FindPartClosestTo(board.parts(), Position(0, 0));
    if (board_part) {
        printf("board:%s\tfind component center on board (bottom left)\n",
               board_part->component_name.c_str());
    }
    board_part = FindPartClosestTo(board.parts(), Position(board.dimension().w,
                                                           board.dimension().h));
    if (board_part) {
        printf("board:%s\tfind component center on board (top right)\n",
               board_part->component_name.c_str());
    }
}

void SolderDispense(const Board &board, Machine *machine) {
    Board::PartList part_list(board.parts());
    OptimizeParts(&part_list);
    // TODO: optimize pad sequence
    for (const Part *part : part_list) {
        for (const Pad &pad : part->pads) {
            machine->Dispense(*part, pad);
        }
    }
}

void PickNPlace(const PnPConfig *config, const Board &board, Machine *machine) {
    // TODO: lowest height components first to not knock over bigger ones.
    for (const Part *part : board.parts()) {
        Tape *tape = NULL;
        if (config) {
            const std::string key = part->footprint + "@" + part->value;
            auto found = config->tape_for_component.find(key);
            if (found == config->tape_for_component.end()) {
                fprintf(stderr, "No tape for '%s'\n", key.c_str());
            } else {
                tape = found->second;
            }
        }
        machine->PickPart(*part, tape);
        machine->PlacePart(*part, tape);
        if (tape) tape->Advance();
    }
}

int main(int argc, char *argv[]) {
    enum OutputType {
        OUT_NONE,
        OUT_DISPENSING,
        OUT_PICKNPLACE,
        OUT_CONFIG_TEMPLATE,
        OUT_CONFIG_LIST,
        OUT_HOMER_INSTRUCTION,
    } output_type = OUT_NONE;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    const char *config_filename = NULL;
    const char *simple_config_filename = NULL;
    bool out_postscript = false;

    int opt;
    while ((opt = getopt(argc, argv, "Pc:C:tlhpd")) != -1) {
        switch (opt) {
        case 'P':
            out_postscript = true;
            break;
        case 'c':
            config_filename = strdup(optarg);
            break;
        case 'C':
            simple_config_filename = strdup(optarg);
            break;
        case 't':
            output_type = OUT_CONFIG_TEMPLATE;
            break;
        case 'l':
            output_type = OUT_CONFIG_LIST;
            break;
        case 'h':
            output_type = OUT_HOMER_INSTRUCTION;
            break;
        case 'p':
            output_type = OUT_PICKNPLACE;
            break;
        case 'd':
            output_type = OUT_DISPENSING;
            break;
        default: /* '?' */
            return usage(argv[0]);
        }
    }

    if (optind >= argc) {
        return usage(argv[0]);
    }

    const char *rpt_file = argv[optind];

    Board board;
    if (!board.ReadPartsFromRpt(rpt_file))
        return 1;

    if (output_type == OUT_CONFIG_TEMPLATE) {
        CreateConfigTemplate(board.parts());
        return 0;
    }
    if (output_type == OUT_CONFIG_LIST) {
        CreateList(board.parts());
        return 0;
    }
    if (output_type == OUT_HOMER_INSTRUCTION) {
        CreateHomerInstruction(board);
        return 0;
    }

    PnPConfig *config = NULL;

    if (config_filename != NULL) {
        config = ParsePnPConfiguration(config_filename);
    } else if (simple_config_filename != NULL) {
        config = ParseSimplePnPConfiguration(board, simple_config_filename);
    }

    Machine *machine = out_postscript
        ? (Machine*) new PostScriptMachine()
        : (Machine*) new GCodeMachine(start_ms, area_ms);

    std::string all_args;
    for (int i = 0; i < argc; ++i) {
        all_args.append(argv[i]).append(" ");
    }
    machine->Init(config, all_args, board.dimension());

    if (output_type == OUT_DISPENSING) {
        SolderDispense(board, machine);
    } else if (output_type == OUT_PICKNPLACE) {
        PickNPlace(config, board, machine);
    } else {
        fprintf(stderr, "Please choose operation with -d or -p\n");
    }

    machine->Finish();

    delete machine;
    return 0;
}
