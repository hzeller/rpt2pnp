/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */
#ifndef POSTSCRIPT_PRINTER_H
#define POSTSCRIPT_PRINTER_H

#include "printer.h"
#include "corner-part-collector.h"

class PostScriptPrinter : public Printer {
public:
    virtual void Init(float min_x, float min_y, float max_x, float max_y);
    virtual void PrintPart(const Position &pos, const Part &part);
    virtual void Finish();
    
private:
    CornerPartCollector corners_;
};

#endif  // POSTSCRIPT_PRINTER_H
