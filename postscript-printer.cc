/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "postscript-printer.h"

PostScriptPrinter::PostScriptPrinter(const PnPConfig *pnp_config) {
    // TODO: read config.
}

void PostScriptPrinter::Init(const Dimension& board_dim) {
    corners_.SetCorners(0, 0, board_dim.w, board_dim.h);
    const float mm_to_point = 1 / 25.4 * 72.0;
    printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
           -2 * mm_to_point, -2 * mm_to_point,
           board_dim.w * mm_to_point, board_dim.h * mm_to_point);
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
    0 0 0.1 0 360 arc
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
}

void PostScriptPrinter::PrintPart(const Part &part) {
    corners_.Update(part.pos, part);
    printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pp\n",
           part.bounding_box.p1.x - part.bounding_box.p0.x,
           part.bounding_box.p1.y - part.bounding_box.p0.y,
           part.bounding_box.p0.x, part.bounding_box.p0.y,
           "", //(part.footprint + "@" + part.value).c_str(),
           part.component_name.c_str(),
           part.angle, part.pos.x, part.pos.y);
}

void PostScriptPrinter::Finish() {
#if 0
    // Doesn't work that well currently.
    printf("0 0 1 setrgbcolor\n");
    for (int i = 0; i < 4; ++i) {
        //const ::Part &part = corners_.get_part(i);
        const Position &pos = corners_.get_closest(i);
        printf("%.1f 2 add %.1f moveto %.1f %.1f 2 0 360 arc stroke\n",
               pos.x, pos.y, pos.x, pos.y);
    }
#endif
    printf("showpage\n");
}
