/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */
#include <assert.h>

#include "tape.h"
#include <stdio.h>

Tape::Tape()
    : x_(0), y_(0), z_(0),
      dx_(0), dy_(0),
      count_(1000) {
}

void Tape::SetFirstComponentPosition(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;
}

void Tape::SetComponentSpacing(float dx, float dy) {
    dx_ = dx;
    dy_ = dy;
    // No height difference between components (I hope :) )
}

void Tape::SetNumberComponents(int n) {
    count_ = n;
}

bool Tape::GetNextPos(float *x, float *y, float *z) {
    assert(x != NULL && y != NULL && z != NULL);
    if (count_ <= 0)
        return false;

    *x = x_;
    *y = y_;
    *z = z_;

    // Advance
    x_ = x_ + dx_;
    y_ = y_ + dy_;
    // z stays the same.
    --count_;

    return true;
}

void Tape::DebugPrint() const {
    fprintf(stderr, "%p: origin: (%.2f, %.2f, %.2f) delta: (%.2f,%.2f) "
            "count: %d", this, x_, y_, z_, dx_, dy_, count_);
}
