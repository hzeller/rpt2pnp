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
#include "postscript-printer.h"
#include "printer.h"
#include "rpt-parser.h"
#include "rpt2pnp.h"

static const float minimum_milliseconds = 50;
static const float area_to_milliseconds = 25;  // mm^2 to milliseconds.

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s <options> <rpt-file>\n"
            "Options:\n"
            "\t-t      : Create config template from rpt to stdout. "
            "Needs editing.\n"
            "\t-l      : List found <footprint>@<component> <count> from rpt "
            "to stdout.\n"
            "\t-p <config> : Pick'n place. Using config + rpt.\n"
            "\t-P      : Output as PostScript.\n"
            "\t-c      : Output corner DryRun G-Code.\n"
#if 0
            // not working right now.
            "\t-d <ms> : Dispensing soler paste. Init time ms (default %.1f)\n"
            "\t-D <ms> : Dispensing time ms/mm^2 (default %.1f)\n",

            prog, minimum_milliseconds, area_to_milliseconds
#endif
            ,prog);
    return 1;
}

typedef std::map<std::string, int> ComponentCount;

// Extract components on board and their counts. Returns total components found.
int ExtractComponents(const Board::PartList& list, ComponentCount *c) {
    int total_count = 0;
    for (auto* part : list) {
        const std::string key = part->footprint + "@" + part->value;
        (*c)[key]++;
        ++total_count;
    }
    return total_count;
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

int main(int argc, char *argv[]) {
    enum OutputType {
        OUT_NONE,
        OUT_DISPENSING,
        OUT_CORNER_GCODE,
        OUT_POSTSCRIPT,
        OUT_CONFIG_TEMPLATE,
        OUT_CONFIG_LIST,
        OUT_PICKNPLACE,
    } output_type = OUT_NONE;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    const char *filename = NULL;
    int opt;
    while ((opt = getopt(argc, argv, "Pctlp:d:D:")) != -1) {
        switch (opt) {
        case 'P':
            output_type = OUT_POSTSCRIPT;
            break;
        case 'c':
            output_type = OUT_CORNER_GCODE;
            break;
        case 't':
            output_type = OUT_CONFIG_TEMPLATE;
            break;
        case 'l':
            output_type = OUT_CONFIG_LIST;
            break;
        case 'p':
            filename = strdup(optarg);
            break;
        case 'd':
            output_type = OUT_DISPENSING;
            start_ms = atof(optarg);
            break;
        case 'D':
            output_type = OUT_DISPENSING;
            area_ms = atof(optarg);
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

    if (output_type == OUT_NONE && filename != NULL) {
        output_type = OUT_PICKNPLACE;
    }

    if (output_type == OUT_CONFIG_TEMPLATE) {
        CreateConfigTemplate(board.parts());
        return 0;
    }
    if (output_type == OUT_CONFIG_LIST) {
        CreateList(board.parts());
        return 0;
    }

    Printer *printer = NULL;
    switch (output_type) {
    case OUT_DISPENSING:
        printer = new GCodeDispensePrinter(start_ms, area_ms);
        break;
    case OUT_CORNER_GCODE:
        printer = new GCodeCornerIndicator(start_ms, area_ms);
        break;
    case OUT_POSTSCRIPT:
        printer = new PostScriptPrinter(filename);
        break;
    case OUT_PICKNPLACE:
        // TODO: allow jogging to the various positions.
        printer = new GCodePickNPlace(filename);
        break;
    default:
        break;
    }

    if (printer == NULL) {
        usage(argv[0]);
        return 1;
    }

    printer->Init(board.dimension());

    for (const Part* part : board.parts()) {
        printer->PrintPart(*part);
    }

    printer->Finish();

    delete printer;
    return 0;
}
