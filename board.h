/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 *
 * Representation of a board with components.
 */
#ifndef PNP_BOARD_H
#define PNP_BOARD_H

#include <string>
#include <vector>

#include "rpt2pnp.h"

// A part on the board.
struct Part {
    Part() : pos(), angle(0) {}
    std::string component_name;  // component name, e.g. R42
    std::string value;           // component value, e.g. 100k
    std::string footprint;       // footprint of component if known.
    Position pos;                // Relative to board
    Box bounding_box;            // relative to pos
    float angle;                 // Rotation
    // vector<Pad> // for paste dispensing. Not needed here for now.
};

class Board {
public:
    typedef std::vector<const Part*> PartList;

    Board();
    ~Board();

    // Read from kicad rpt file.
     bool ReadPartsFromRpt(const std::string& filename);

    // The actual origin on the dispense/pnp platform (unit: mm)
    void SetOrigin(const Position& pos) { origin_ = pos; }

    // We need more coordinates here so that we can figure out a slightly
    // twisted board on the platform, ideally calibrated by moving the nozzle
    // to certain pads.
    // For now, we just assume some fixed origin.

    const PartList& parts() const { return parts_; }
    const Dimension& dimension() const { return board_dim_; }

    int PartCount() const { return parts_.size(); }

private:
    Position origin_;
    Dimension board_dim_;
    PartList parts_;
};

#endif  // PNP_BOARD_H
