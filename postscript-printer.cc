/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "postscript-printer.h"

#include "pnp-config.h"
#include "tape.h"

PostScriptPrinter::PostScriptPrinter(const PnPConfig *pnp_config)
    : config_(pnp_config) {
    if (config_ == NULL) {
        config_ = new PnPConfig();
    }
}

void PostScriptPrinter::Init(const std::string &init_comment,
                             const Dimension& board_dim) {
    const float mm_to_point = 1 / 25.4 * 72.0;
    if (config_->tape_for_component.size() == 0) {
        printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
               config_->board.origin.x * mm_to_point,
               config_->board.origin.y * mm_to_point,
               board_dim.w * mm_to_point, board_dim.h * mm_to_point);
    } else {
        printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
               -2 * mm_to_point, -2 * mm_to_point,
               300 * mm_to_point, 300 * mm_to_point);
    }
    printf("%% %s\n", init_comment.c_str());
    printf("%s", R"(
% <dx> <dy> <x0> <y0>
/rect {
  moveto
  1 index 0 rlineto
  0 exch rlineto
  neg 0 rlineto
  closepath
  stroke
} def

% <dy> <dx>  <x0> <y0>  <value> <name>  <angle> <x> <y> pp
/pp {
    gsave
    translate
    rotate
    0 0 moveto
    0 0 0.1 0 360 arc  % mark center
    0 0 1 setrgbcolor show % name
    0 0 0 setrgbcolor ( / ) show
    1 0 0 setrgbcolor show % value
    0 0 0 setrgbcolor
    rect
    grestore
} def
)");
    printf("72.0 25.4 div dup scale  %% Switch to mm\n");
    printf("0.1 setlinewidth\n");
    printf("/Helvetica findfont 1 scalefont setfont\n");
    printf("%.1f %.1f moveto\n", 0.0, 0.0);
    printf("%.1f %.1f %.1f %.1f rect\n", board_dim.w, board_dim.h,
           config_->board.origin.x, config_->board.origin.y);
}

void PostScriptPrinter::PrintPart(const Part &part) {
    Tape *tape = NULL;
    const std::string key = part.footprint + "@" + part.value;
    auto found = config_->tape_for_component.find(key);
    if (found != config_->tape_for_component.end()) {
        tape = found->second;
    } else {
        // TODO: only print once.
        fprintf(stderr, "No tape for '%s'\n", key.c_str());
    }

    float tx, ty, tz;
    if (tape && tape->GetPos(&tx, &ty, &tz)) {
        tape->Advance();
        // Print component on tape
        printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pp\n",
               part.bounding_box.p1.x - part.bounding_box.p0.x,
               part.bounding_box.p1.y - part.bounding_box.p0.y,
               part.bounding_box.p0.x, part.bounding_box.p0.y,
               "", part.component_name.c_str(),
               tape->angle(),
               tx, ty);
    } else {
        // Maybe just print component in red ?
        //fprintf(stderr, "We are out of components for '%s'\n", key.c_str());
    }

    // TODO: line between component on tape and board

    // Print part on board.
    printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pp\n",
           part.bounding_box.p1.x - part.bounding_box.p0.x,
           part.bounding_box.p1.y - part.bounding_box.p0.y,
           part.bounding_box.p0.x, part.bounding_box.p0.y,
           "", //(part.footprint + "@" + part.value).c_str(),
           part.component_name.c_str(),
           part.angle,
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y);
}

void PostScriptPrinter::Finish() {
    printf("showpage\n");
}
