/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef RPT2PNP_H
#define RPT2PNP_H

#include <vector>
#include <string>

struct Position {
    Position(float xx, float yy) : x(xx), y(yy) {}
    Position() : x(0), y(0) {}
    float x, y;
};

struct Dimension {
    Dimension(float ww, float hh) : w(ww), h(hh) {}
    Dimension() : w(0), h(0) {}
    float w, h;
};

struct Part {
    Part() : pos(), angle(0) {}
    std::string component_name;
    std::string value;
    Position pos;
    Dimension dimension;
    float angle;
    // vector<Pad> // for paste dispensing. Not needed here.
};

float Distance(const Position& a, const Position& b);

// Find acceptable route for pad visiting. Ideally solves TSP, but
// heuristics are good as well. (optimizer.cc)
void OptimizeParts(std::vector<const Part*> *parts);

// Read an rpt file. (rpt2part.cc)
bool ReadRptFile(const std::string& rpt_file,
                 std::vector<const Part*> *result);

#endif // RPT2PNP_H
