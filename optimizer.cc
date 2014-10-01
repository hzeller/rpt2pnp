/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 * Optimization for dispensing stuff.
 * Not really used right now; used to be for pads, doesn't make much sense for
 * parts.
 */

#include "rpt2pnp.h"

#include <math.h>
#include <unistd.h>

#include "board.h"  // definition of Part

static float euklid(float a, float b) { return sqrtf(a*a + b*b); }
float Distance(const Position& a, const Position& b) {
    return euklid(a.x - b.x, a.y - b.y);
}
static void Swap(OptimizeList *list, size_t i, size_t j) {
    const auto tmp = (*list)[i];
    (*list)[i] = (*list)[j];
    (*list)[j] = tmp;
}

// Extract position from part and pad. The pad is positioned relative to the
// part, thus we need to combine these, including rotation.
static Position ExtractPosition(const std::pair<const Part *, const Pad *> &p) {
    const Part &part = *p.first;
    const Pad &pad = *p.second;
    const float angle = 2 * M_PI * part.angle / 360.0;
    const float x = part.pos.x + pad.pos.x * cos(angle) - pad.pos.y * sin(angle);
    const float y = part.pos.y + pad.pos.x * sin(angle) + pad.pos.y * cos(angle);
    return Position(x, y);
}

static int FindSmallestDistance(const OptimizeList parts,
                                size_t range_start,
                                const Position &reference_pos) {
    float smallest_distance;
    int best = -1;
    for (size_t j = range_start; j < parts.size(); ++j) {
        float distance = Distance(reference_pos, ExtractPosition(parts[j]));
        if (best < 0 || distance < smallest_distance) {
            best = j;
            smallest_distance = distance;
        }
    }
    return best;
}

// Very crude, O(n^2) optimization looking for nearest neighbor.
// Not TSP solution, but better than random
void OptimizeParts(OptimizeList *list) {
    for (size_t i = 0; i < list->size() - 1; ++i) {
        Swap(list, i + 1,
             FindSmallestDistance(*list, i + 1, ExtractPosition((*list)[i])));
    }
}

