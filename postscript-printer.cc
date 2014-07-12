/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "postscript-printer.h"

// Smallest point from origin. TODO: make that some constant somewhere.
static float offset_x = 10;
static float offset_y = 10;

void PostScriptPrinter::Init(float min_x, float min_y,
                             float max_x, float max_y) {
    corners_.SetCorners(min_x, min_y, max_x, max_y);
    min_x -= 2; min_y -=2; max_x +=2; max_y += 2;
    const float mm_to_point = 1 / 25.4 * 72.0;
    printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
           min_x * mm_to_point, min_y * mm_to_point,
           max_x * mm_to_point, max_y * mm_to_point);
    printf("%s", R"(
% <w> <h>
/centered-rect {
    currentpoint
    currentpoint 0.2 0 360 arc closepath stroke
    moveto
    
    2 copy
    % We want it centered around current point
    2 div neg exch 2 div neg exch rmoveto
    
    dup 0 exch rlineto

    exch  % now <h> <w>
    
    0 rlineto
    
    0 exch neg rlineto
    closepath
    stroke
} def

% Place Part
% <w> <h> <angle>
/pp {
  gsave
  rotate
  gsave 1 index 2 div 0.25 sub 0 rmoveto 0.5 0 rlineto stroke grestore
  centered-rect
  grestore
} def

% MovePart Stack:
% <x> <y>
/mp { currentpoint 0.01 setlinewidth lineto currentpoint stroke moveto } def

)");
    printf("72.0 25.4 div dup scale  %% Switch to mm\n");
    printf("%.1f %.1f moveto\n", offset_x, offset_y);
}

void PostScriptPrinter::PrintPart(const Position &pos, const Part &part) {
    corners_.Update(pos, part);
    printf("%.3f %.3f mp "
           "%.3f %.3f %.3f pp\n"
           "%.3f %.3f moveto ",
           pos.x, pos.y,
           part.dimension.w, part.dimension.h, part.angle,
           pos.x, pos.y);
}

void PostScriptPrinter::Finish() {
    printf("0 0 1 setrgbcolor\n");
    for (int i = 0; i < 4; ++i) {
        //const ::Part &part = corners_.get_part(i);
        const Position &pos = corners_.get_closest(i);
        printf("%.1f 2 add %.1f moveto %.1f %.1f 2 0 360 arc stroke\n",
               pos.x, pos.y, pos.x, pos.y);
    }
    printf("showpage\n");
}
