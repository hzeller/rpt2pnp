/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */
#ifndef POSTSCRIPT_PRINTER_H
#define POSTSCRIPT_PRINTER_H

#include "printer.h"

struct PnPConfig;
class PostScriptPrinter : public Printer {
public:
    // If we get a pnp configuration (i.e. non-NULL), we print the process.
    PostScriptPrinter(const PnPConfig *config);

    ~PostScriptPrinter() override {}
    void Init(const std::string &init_comment,
              const Dimension& board_dim) override;
    void PrintPart(const Part &part) override;
    void Finish() override;
    
private:
    const PnPConfig *config_;
};

#endif  // POSTSCRIPT_PRINTER_H
