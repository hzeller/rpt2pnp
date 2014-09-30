/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef PRINTER_H
#define PRINTER_H

#include <string>

#include "rpt2pnp.h"
#include "board.h"
#include "corner-part-collector.h"

struct PnPConfig;

class Printer {
public:
    virtual ~Printer() {}
    // Initialize printer. The comment should be added to the output file
    // if possible.
    virtual void Init(const std::string &init_comment,
                      const Dimension& dimension) = 0;
    virtual void PrintPart(const Part &part) = 0;
    virtual void Finish() = 0;
};

//-- Some implementations of a printer. For lazyness reasons all in this header

class GCodeDispensePrinter : public Printer {
public:
    // "init_ms" number of milliseconds to switch on the dispenser, then
    // "area_ms" is milliseconds per mm^2
    GCodeDispensePrinter(const PnPConfig *config, float init_ms, float area_ms);

    void Init(const std::string &init_comment,
              const Dimension& dimension) override;
    void PrintPart(const Part &part) override;
    void Finish() override;

private:
    const PnPConfig *const config_;
    const float init_ms_;
    const float area_ms_;
};

class GCodePickNPlace : public Printer {
public:
    GCodePickNPlace(const PnPConfig *pnp_config);

    void Init(const std::string &init_comemnt, const Dimension& dim) override;
    void PrintPart(const Part& part) override;
    void Finish() override;

private:
    const PnPConfig *config_;
};

#endif  // PRINTER_H
