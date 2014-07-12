/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef PRINTER_H
#define PRINTER_H

#include "rpt2pnp.h"

class Printer {
public:
    virtual ~Printer() {}
    virtual void Init(float min_x, float min_y, float max_x, float max_y) = 0;
    virtual void PrintPart(const Position &pos, const Part &part) = 0;
    virtual void Finish() = 0;
};
#endif  // PRINTER_H
