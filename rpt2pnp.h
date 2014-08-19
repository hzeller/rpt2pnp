/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef RPT2PNP_H
#define RPT2PNP_H

#include <vector>
#include <string>

struct Part;

struct Position {
    Position(float xx, float yy) : x(xx), y(yy) {}
    Position() : x(0), y(0) {}
    void Set(float xx, float yy) { x = xx; y = yy; }

    float x, y;
};

struct Dimension {
    Dimension(float ww, float hh) : w(ww), h(hh) {}
    Dimension() : w(0), h(0) {}
    float w, h;
};

struct Box {
    Position p0;
    Position p1;
};

float Distance(const Position& a, const Position& b);

// Find acceptable route for pad visiting. Ideally solves TSP, but
// heuristics are good as well. (optimizer.cc)
void OptimizeParts(std::vector<const Part*> *parts);

#endif // RPT2PNP_H
