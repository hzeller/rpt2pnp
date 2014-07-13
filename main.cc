/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>
#include <set>

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
            "\t-t      : Create config template from rpt. Needs editing.\n"
            "\t-p <config> : 'assemble': Pick'n place. Using config + rpt.\n"
            "\t-P      : Output as PostScript.\n"
            "\t-c      : Output corner DryRun G-Code.\n"
            "\t-d <ms> : Dispensing init time ms (default %.1f)\n"
            "\t-D <ms> : Dispensing time ms/mm^2 (default %.1f)\n",
            prog, minimum_milliseconds, area_to_milliseconds);
    return 1;
}

void CreateConfigTemplate(const Board::PartList& list) {
    printf("Board:\norigin: 100 100 # x/y origin of the board\n\n");

    printf("# Unlike this template, you can actually place multiple\n");
    printf("# space-delimited '<footprint>@<value>' behind each 'Tape:'\n");
    printf("# Each Tape section requires\n");
    printf("#   'origin:', which is the (x/y/z) position of\n");
    printf("# the first component (z: pick-up-height). And\n");
    printf("#   'spacing:', (dx,dy) to the next one\n#\n");
    printf("# Also there are the following optional parameters\n");
    printf("#angle: 0     # Optional: Angle on tape\n");
    printf("#count: 1000  # Optional: available count on tape\n");
    printf("\n");

    int total_count = 0;
    std::set<std::string> components;
    for (auto* part : list) {
        const std::string key = part->footprint + "@" + part->value;
        components.insert(key);
        ++total_count;
    }
    for (const std::string& c : components) {
        printf("\nTape: %s\n", c.c_str());
        printf("origin:  0 0 2 # fill me\n");
        printf("spacing: 4 0   # fill me\n");
    }
    fprintf(stderr, "%d components total\n", total_count);
}

int main(int argc, char *argv[]) {
    enum OutputType {
        OUT_DISPENSING,
        OUT_CORNER_GCODE,
        OUT_POSTSCRIPT,
        OUT_CONFIG_TEMPLATE,
        OUT_PICKNPLACE,
    } output_type = OUT_DISPENSING;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    std::string filename;
    int opt;
    while ((opt = getopt(argc, argv, "Pctp:d:D:")) != -1) {
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
        case 'p':
            output_type = OUT_PICKNPLACE;
            filename = optarg;
            break;
        case 'd':
            start_ms = atof(optarg);
            break;
        case 'D':
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
    board.ReadPartsFromRpt(rpt_file);

    if (output_type == OUT_CONFIG_TEMPLATE) {
        CreateConfigTemplate(board.parts());
        return 0;
    }

    Printer *printer = NULL;
    switch (output_type) {
    case OUT_DISPENSING:   printer = new GCodeDispensePrinter(start_ms, area_ms); break;
    case OUT_CORNER_GCODE: printer = new GCodeCornerIndicator(start_ms, area_ms); break;
    case OUT_POSTSCRIPT:   printer = new PostScriptPrinter(); break;
    case OUT_PICKNPLACE:
        // TODO: allow jogging to the various positions.
        printer = new GCodePickNPlace(filename);
        break;
    default:
        break;
    }

    if (printer == NULL)
        return 1;

    printer->Init(board.dimension());

    for (const Part* part : board.parts()) {
        printer->PrintPart(*part);
    }

    printer->Finish();

    delete printer;
    return 0;
}
