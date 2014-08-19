/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef PRINTER_H
#define PRINTER_H

#include "rpt2pnp.h"
#include "board.h"
#include "corner-part-collector.h"

class Printer {
public:
    virtual ~Printer() {}
    virtual void Init(const Dimension& dimension) = 0;
    virtual void PrintPart(const Part &part) = 0;
    virtual void Finish() = 0;
};

// Some implementations of a printer. For lazyness reasons all in this header
class GCodeDispensePrinter : public Printer {
public:
    // "init_ms" number of milliseconds to switch on the dispenser, then
    // "area_ms" is milliseconds per mm^2
    GCodeDispensePrinter(float init_ms, float area_ms);

    virtual void Init(const Dimension& dimension);
    virtual void PrintPart(const Part &part);
    virtual void Finish();

private:
    const float init_ms_;
    const float area_ms_;
};

class GCodeCornerIndicator : public Printer {
public:
    GCodeCornerIndicator(float init_ms, float area_ms);

    virtual void Init(const Dimension& dim);
    virtual void PrintPart(const Part &part);
    virtual void Finish();

private:
    CornerPartCollector corners_;
    const float init_ms_;
    const float area_ms_;
};

class GCodePickNPlace : public Printer {
public:
    GCodePickNPlace(const std::string& filename);

    virtual void Init(const Dimension& dim);
    virtual void PrintPart(const Part& part);
    virtual void Finish();

private:
    struct Config;

    static Config *ParseConfig(const std::string& filename);

    Config* config_;
};

#endif  // PRINTER_H
