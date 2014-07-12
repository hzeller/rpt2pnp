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
            "\t-p      : Output as PostScript.\n"
            "\t-c      : Output corner DryRun G-Code.\n"
            "\t-l      : List different components + count found on board\n"
            "\t-a <config> : 'assemble': Pick'n place\n"
            "\t-d <ms> : Dispensing init time ms (default %.1f)\n"
            "\t-D <ms> : Dispensing time ms/mm^2 (default %.1f)\n",
            prog, minimum_milliseconds, area_to_milliseconds);
    return 1;
}

void CountComponents(const Board::PartList& list) {
    int total_count = 0;
    int longest_string = 0;
    std::map<std::string, int> counts;
    for (auto* part : list) {
        const std::string key = part->footprint + "@" + part->value;
        longest_string = std::max(longest_string, (int) key.length());
        counts[key]++;
        ++total_count;
    }
    for (auto& part_count : counts) {
        printf("%-*s %3d\n", longest_string, part_count.first.c_str(),
               part_count.second);
    }
    fprintf(stderr, "%d components total\n", total_count);
}

int main(int argc, char *argv[]) {
    enum OutputType {
        OUT_DISPENSING,
        OUT_CORNER_GCODE,
        OUT_POSTSCRIPT,
        OUT_COMPONENT_LIST,
        OUT_PICKNPLACE,
    } output_type = OUT_DISPENSING;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    std::string filename;
    int opt;
    while ((opt = getopt(argc, argv, "pcla:d:D:")) != -1) {
        switch (opt) {
        case 'p':
            output_type = OUT_POSTSCRIPT;
            break;
        case 'c':
            output_type = OUT_CORNER_GCODE;
            break;
        case 'l':
            output_type = OUT_COMPONENT_LIST;
            break;
        case 'a':
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

    if (output_type == OUT_COMPONENT_LIST) {
        CountComponents(board.parts());
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

    fprintf(stderr, "Dispensed %d parts. Total dispense time: %.1fs\n",
            board.PartCount(), 0.0f);
    delete printer;
    return 0;
}
