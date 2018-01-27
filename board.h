/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 *
 * Representation of a board with components.
 */
#ifndef PNP_BOARD_H
#define PNP_BOARD_H

#include <functional>
#include <string>
#include <vector>

#include "rpt2pnp.h"

struct Pad {
    Position pos;
    Dimension size;
    std::string name;
};

// A part on the board.
struct Part {
    Part() : pos(), angle(0), is_front_layer(true) {}
    std::string component_name;  // component name, e.g. R42
    std::string value;           // component value, e.g. 100k
    std::string footprint;       // footprint of component if known.
    Position pos;                // Relative to board
    float angle;                 // Rotation
    bool is_front_layer;         // on front of board ?
    // The pads are roated around pos with angle.
    std::vector<Pad> pads;       // For paste dispensing and image recognition.
    Box bounding_box;            // relative to pos
};

// Representation of the board and its components.
class Board {
public:
    typedef std::vector<const Part*> PartList;

    // A filter for parts to be included in the parts.
    typedef std::function<bool(const Part&)> ReadFilter;

    Board();
    ~Board();

    // Read from kicad rpt file.
    bool ParseFromRpt(const std::string& filename, ReadFilter filter);

    // Parts. All positions are referenced to (0,0)
    const PartList& parts() const { return parts_; }

    // The outline of the board.
    const Dimension& dimension() const { return board_dim_; }

    int PartCount() const { return parts_.size(); }

private:
    Dimension board_dim_;
    PartList parts_;
};

#endif  // PNP_BOARD_H
