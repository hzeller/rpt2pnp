/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <vector>
#include <string>

struct Position {
    Position(float xx, float yy) : x(xx), y(yy) {}
    Position() : x(0), y(0) {}
    float x, y;
};

struct Part {
    Part() : pos(), angle(0) {}
    std::string component_name;
    std::string value;
    Position pos;
    float angle;
};

float Distance(const Position& a, const Position& b);

// Find acceptable route for pad visiting. Ideally solves TSP, but heuristics are good
// as well.
void OptimizeParts(std::vector<const Part*> *parts);
