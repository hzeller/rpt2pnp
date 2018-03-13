/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "board.h"
#include "tape.h"
#include "pnp-config.h"
#include "machine.h"
#include "rpt-parser.h"
#include "rpt2pnp.h"
#include "machine-connection.h"

volatile sig_atomic_t interrupt_received = 0;
static void InterruptHandler(int signo) {
  interrupt_received = 1;
}

static const float minimum_milliseconds = 50;
static const float area_to_milliseconds = 25;  // mm^2 to milliseconds.

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-l|-d|-p] <options> <rpt-file>\n"
            "Options:\n"
            "There are one of three operations to choose:\n"
            "[Operations. Choose one of these]\n"
            "\t-d      : Dispensing solder paste.\n"
            "\t-p      : Pick'n place.\n"
            "\t-l      : Create BOM list <footprint>@<component> <count>\n"
            "\t          from parts found in rpt to stdout.\n"
            "\n[Output]\n"
            "\tDefault output is gcode to stdout\n"
            "\t-P      : Preview: Output as PostScript instead of GCode.\n"
            "\t-m<tty> : Connect to machine. Sample \"/dev/ttyACM0,b115200\"\n"
            "\t-O<file>: Output to specified file instead of stdout\n"
            "\n[Choice of components to handle]\n"
            "\t-b      : Handle back-of-board (default: front)\n"
            "\t-x<list>: Comma-separated list of component references "
            "to exclude\n"
            "\n[Configuration]\n"
            "\t-t          : Create human-editable config template to "
            "stdout\n"
            "\t-c <config> : read such a config\n"
            "\t-D<init-ms,area-to-ms> : Milliseconds to leave pressure on to\n"
            "\t            dispense. init-ms is initial offset, area-to-ms is\n"
            "\t            milliseconds per mm^2 area covered.\n"
            "\n[Homer config]\n"
            "\t-H          : Create homer configuration template to stdout.\n"
            "\t-C <config> : Use homer config created via homer from -H\n",
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

void CreateConfigTemplate(const Board& board) {
    const Board::PartList& list = board.parts();

    const float origin_x = 10, origin_y = 10;

    printf("Board:\norigin: %.0f %.0f 1.6 # x/y/z origin of the board; (z=thickness).\n\n", origin_x, origin_y);

    printf("# Where the tray with all the tapes start.\n");
    printf("Tape-Tray-Origin: 0 %.1f 0\n\n", origin_y + board.dimension().h);

    printf("# This template provides one <footprint>@<component> per tape,\n");
    printf("# but if you have multiple components that are indeed the same\n");
    printf("# e.g. smd0805@100n smd0805@0.1uF, then you can just put them\n");
    printf("# space delimited behind each Tape:\n");
    printf("#   Tape: smd0805@100n smd0805@0.1uF\n");
    printf("# Each Tape section requires\n");
    printf("#   'origin:', which is the (x/y/z) position (relative to "
           "Tape-Tray-Origin) of\n");
    printf("# the top of the first component (z: pick-up-height).\n# And\n");
    printf("#   'spacing:', (dx,dy) to the next one\n#\n");
    printf("# Also there are the following optional parameters\n");
    printf("#angle: 0     # Optional: Default rotation of component on tape.\n");
    printf("#count: 1000  # Optional: available count on tape\n");
    printf("\n");

    int ypos = 0;
    ComponentCount components;
    const int total_count = ExtractComponents(list, &components);
    for (const Part* part : list) {
        const std::string key = part->footprint + "@" + part->value;
        const auto found_count = components.find(key);
        if (found_count == components.end())
            continue; // already printed
        int width = abs(part->bounding_box.p1.x - part->bounding_box.p0.x) + 5;
        int height = abs(part->bounding_box.p1.y - part->bounding_box.p0.y);
        printf("\nTape: %s\n", key.c_str());
        printf("count: %d\n", found_count->second);
        printf("origin:  %d %d 2 # fill me\n", 10 + height/2, ypos + width/2);
        printf("spacing: %d 0   # fill me\n",
               height < 4 ? 4 : height + 2);
        ypos += width;
        components.erase(key);
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
        const int next_pos = std::min(std::max(2, pair.second), 4);
        printf("tape%d:%s\tfind %d. component\n",
               next_pos, pair.first.c_str(), next_pos);
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
    OptimizeList all_pads;
    for (const Part *part : board.parts()) {
        for (const Pad &pad : part->pads) {
            all_pads.push_back(std::make_pair(part, &pad));
        }
    }
    OptimizeParts(&all_pads);

    for (const auto &p : all_pads) {
        if (interrupt_received)
            break;
        machine->Dispense(*p.first, *p.second);
    }
}

static Tape *FindTapeForPart(const PnPConfig *config, const Part *part) {
    const std::string key = part->footprint + "@" + part->value;
    auto found = config->tape_for_component.find(key);
    if (found == config->tape_for_component.end())
        return NULL;
    return found->second;
}

struct ComponentHeightComparator {
    ComponentHeightComparator(const PnPConfig *config) : config_(config) {}

    bool operator()(const Part *a, const Part *b) {
        if (a == b) return 0;
        const float a_height = GetHeight(a);
        const float b_height = GetHeight(b);
        if (a_height == b_height) {
            return a->component_name < b->component_name;
        }
        return a_height < b_height;
    }

    float GetHeight(const Part *part) {
        const Tape *tape = FindTapeForPart(config_, part);
        return tape == NULL ? -1 : tape->height();
    }
    const PnPConfig *config_;
};
void PickNPlace(const PnPConfig *config, const Board &board, Machine *machine) {
    // TODO: lowest height components first to not knock over bigger ones.
    std::vector<const Part *> list(board.parts());
    if (config) {
        std::sort(list.begin(), list.end(), ComponentHeightComparator(config));
    }
    for (const Part *part : list) {
        if (interrupt_received)
            break;

        Tape *tape = NULL;
        if (config) {
            tape = FindTapeForPart(config, part);
            if (tape == NULL) {
                fprintf(stderr, "No tape for '%s'\n",
                        part->component_name.c_str());
            }
        }
        machine->PickPart(*part, tape);
        machine->PlacePart(*part, tape);
        if (tape) tape->Advance();
    }
}

std::set<std::string> ParseCommaSeparated(const char *start) {
    // TODO: use absl::StrSplit instead.
    std::set<std::string> result;
    const char *end;
    while ((end = strchr(start, ',')) != NULL) {
        if (end > start)  // ignore empty strings.
            result.insert(std::string(start, end - start));
        start = end + 1;
    }
    std::string last(start);
    if (!last.empty()) result.insert(last);
    return result;
}

int main(int argc, char *argv[]) {
    enum Operation {
        OP_NONE,
        OP_DISPENSING,
        OP_PICKNPLACE,
        OP_CONFIG_TEMPLATE,
        OP_CONFIG_LIST,
        OP_HOMER_INSTRUCTION,
    } do_operation = OP_NONE;

    enum OutputOption {
        OUT_POSTSCRIPT,
        OUT_GCODE,
        OUT_MACHINE,
    } out_option = OUT_GCODE;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    const char *config_filename = NULL;
    const char *simple_config_filename = NULL;
    bool handle_top_of_board = true;
    std::set<std::string> blacklist;
    FILE *output = NULL;
    int tty_fd = -1;

    int opt;
    while ((opt = getopt(argc, argv, "Pc:C:D:tlHpdbx:O:m:")) != -1) {
        switch (opt) {
        case 'P':
            out_option = OUT_POSTSCRIPT;
            break;
        case 'm':
            tty_fd = OpenMachineConnection(optarg);
            if (tty_fd < 0) {
                fprintf(stderr, "Can't connect to machine. Exiting.\n");
                return 1;
            }
            out_option = OUT_MACHINE;
            break;
        case 'c':
            config_filename = strdup(optarg);
            break;
        case 'C':
            simple_config_filename = strdup(optarg);
            break;
        case 'D':
            if (2 != sscanf(optarg, "%f,%f", &start_ms, &area_ms)) {
                fprintf(stderr, "Invalid -D spec\n");
                return usage(argv[0]);
            }
            break;
        case 'O':
            output = fopen(optarg, "w");
            if (output == NULL) {
                perror("Couldn't open requested output file for write");
                return 1;
            }
            break;
        case 't':
            do_operation = OP_CONFIG_TEMPLATE;
            break;
        case 'l':
            do_operation = OP_CONFIG_LIST;
            break;
        case 'H':
            do_operation = OP_HOMER_INSTRUCTION;
            break;
        case 'p':
            do_operation = OP_PICKNPLACE;
            break;
        case 'd':
            do_operation = OP_DISPENSING;
            break;
        case 'b':
            handle_top_of_board = false;
            break;
        case 'x':
            blacklist = ParseCommaSeparated(optarg);
            break;
        default: /* '?' */
            return usage(argv[0]);
        }
    }

    if (optind >= argc) {
        return usage(argv[0]);
    }

    if (output != NULL && out_option == OUT_MACHINE) {
        fprintf(stderr, "Machine output is chosen with -m. "
                "But also output file with -O. Choose only one.\n\n");
        return usage(argv[0]);
    }

    if (output == NULL) {
        output = stdout;
    }

    const char *rpt_file = argv[optind];

    Board::ReadFilter inclusion_filter
        = [handle_top_of_board, &blacklist](const Part &part) {
        if (part.is_front_layer != handle_top_of_board)
            return false;
        return blacklist.find(part.component_name) == blacklist.end();
    };

    Board board;
    if (!board.ParseFromRpt(rpt_file, inclusion_filter))
        return 1;
    fprintf(stderr, "Board: %s, %.1fmm x %.1fmm\n",
            rpt_file, board.dimension().w, board.dimension().h);

    /*
     * Simple operations: output some metadata.
     */
    switch (do_operation) {
    case OP_NONE:
        fprintf(stderr, "Please choose operation with -d or -p\n");
        return usage(argv[0]);
    case OP_CONFIG_TEMPLATE:
        CreateConfigTemplate(board);
        return 0;
    case OP_CONFIG_LIST:
        CreateList(board.parts());
        return 0;
    case OP_HOMER_INSTRUCTION:
        CreateHomerInstruction(board);
        return 0;

    case OP_DISPENSING:
    case OP_PICKNPLACE:
        /* more involved operations need a machine to be initialized, to
         * be dealt with below.
         */
        break;
    }

    PnPConfig *config = NULL;

    if (config_filename != NULL) {
        config = ParsePnPConfiguration(config_filename);
    }
    else if (simple_config_filename != NULL) {
        config = ParseSimplePnPConfiguration(board, simple_config_filename);
    }
    else if (do_operation == OP_DISPENSING) {
        // Only in the dispensing operation, a very simple config is feasible.
        fprintf(stderr, "Didn't get configuration. Creating a simple one "
                "for dispensing\n");
        config = CreateEmptyConfiguration();
    }

    Machine *machine = NULL;
    switch (out_option) {
    case OUT_GCODE:
        machine = new GCodeMachine(output, start_ms, area_ms);
        break;
    case OUT_POSTSCRIPT:
        machine = new PostScriptMachine(output);
        break;
    case OUT_MACHINE:
        machine = new GCodeMachine(tty_fd, tty_fd, start_ms, area_ms);
        break;
    }

    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);

    std::string all_args;
    for (int i = 0; i < argc; ++i) {
        all_args.append(argv[i]).append(" ");
    }
    if (!machine->Init(config, all_args, board.dimension())) {
        fprintf(stderr, "Initialization failed\n");
        return 1;
    }

    if (do_operation == OP_DISPENSING) {
        SolderDispense(board, machine);
    }
    else if (do_operation == OP_PICKNPLACE) {
        PickNPlace(config, board, machine);
    }

    if (interrupt_received) {
        fprintf(stderr, "Got interrupted by Ctrl-C. Shutting down safely.\n");
    }
    machine->Finish();

    delete machine;
    delete config;
    return 0;
}
