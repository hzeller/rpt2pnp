/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */
#ifndef PNP_CONFIG_H
#define PNP_CONFIG_H

#include <string>
#include <map>

#include "rpt2pnp.h"

class Tape;
class Board;

// (for now: simple) configuration for the setup needed to do pick-n-place.
// TODO:
//  - reference position on board
//  - multiple boards
//  - different board-height for dispense-needle and pick'n place
struct PnPConfig {
    typedef std::map<std::string, Tape*> PartToTape;
    struct BoardConfig {
        Position origin;  // TODO: potentially rotation...
        float top;        // Z position of top-surface of board.
    };

    BoardConfig board;
    PartToTape tape_for_component;

    // Baseline. All z-coordinates in board and tape are larger than this.
    float bed_level = -1;
};

// Parse configuration and return newly allocated config object or NULL on
// parse error.
// (TODO: maybe get rid of this in favor of simple pnp config)
PnPConfig *ParsePnPConfiguration(const std::string& filename);

// Simplified PNP config: one line at a time
PnPConfig *ParseSimplePnPConfiguration(const Board &board,
                                       const std::string& filename);
#endif  // PNP_CONFIG_H
