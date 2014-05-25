/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "rpt2paste.h"

#include <math.h>
#include <unistd.h>

static float euklid(float a, float b) { return sqrtf(a*a + b*b); }
float Distance(const Position& a, const Position& b) {
    return euklid(a.x - b.x, a.y - b.y);
}
static void Swap(std::vector<const Pad*> *pads, size_t i, size_t j) {
    const Pad* tmp = (*pads)[i];
    (*pads)[i] = (*pads)[j];
    (*pads)[j] = tmp;
}

static int FindSmallestDistance(const std::vector<const Pad*> pads,
                                size_t range_start,
                                const Position &reference_pos) {
    float smallest_distance;
    int best = -1;
    for (size_t j = range_start; j < pads.size(); ++j) {
        float distance = Distance(reference_pos, pads[j]->pos);
        if (best < 0 || distance < smallest_distance) {
            best = j;
            smallest_distance = distance;
        }
    }
    return best;
}

// Very crude, O(n^2) optimization looking for nearest neighbor. Not TSP, but better than random
void OptimizePads(std::vector<const Pad*> *pads) {
    for (size_t i = 0; i < pads->size() - 1; ++i) {
        Swap(pads, i + 1, FindSmallestDistance(*pads, i + 1, (*pads)[i]->pos));
    }
}

