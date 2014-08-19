/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */
#ifndef POSTSCRIPT_PRINTER_H
#define POSTSCRIPT_PRINTER_H

#include "printer.h"
#include "corner-part-collector.h"

class PostScriptPrinter : public Printer {
public:
    virtual ~PostScriptPrinter() {}
    virtual void Init(const Dimension& board_dim);
    virtual void PrintPart(const Part &part);
    virtual void Finish();
    
private:
    CornerPartCollector corners_;
};

#endif  // POSTSCRIPT_PRINTER_H
