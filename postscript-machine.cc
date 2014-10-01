/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "machine.h"

#include <math.h>

#include "pnp-config.h"
#include "tape.h"
#include "board.h"

bool PostScriptMachine::Init(const PnPConfig *config,
                             const std::string &init_comment,
                             const Dimension& board_dim) {
    config_ = config;
    if (config_ == NULL) {
        config_ = new PnPConfig();
    }
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

% print component
% <dy> <dx>  <x0> <y0>  <value> <name>  <angle> <x> <y> pp
/pc {
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

% PastePad.
% Stack: <diameter>
/pp { 0.2 setlinewidth 0 360 arc stroke } def

% Move, show trace.
% Stack: <x> <y>
/m { 0.01 setlinewidth lineto currentpoint stroke } def
)");
    printf("72.0 25.4 div dup scale  %% Switch to mm\n");
    printf("0.1 setlinewidth\n");
    printf("/Helvetica findfont 1 scalefont setfont\n");
    printf("%.1f %.1f moveto\n", 0.0, 0.0);
    printf("%.1f %.1f %.1f %.1f rect\n", board_dim.w, board_dim.h,
           config_->board.origin.x, config_->board.origin.y);
    printf("0 0 moveto\n");
    return true;
}

void PostScriptMachine::PickPart(const Part &part, const Tape *tape) {
    if (tape == NULL) return;
    float tx, ty;
    if (tape->GetPos(&tx, &ty)) {
        // Print component on tape
        printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pc\n",
               part.bounding_box.p1.x - part.bounding_box.p0.x,
               part.bounding_box.p1.y - part.bounding_box.p0.y,
               part.bounding_box.p0.x, part.bounding_box.p0.y,
               "", part.component_name.c_str(),
               tape->angle(),
               tx, ty);
    }
}

void PostScriptMachine::PlacePart(const Part &part, const Tape *tape) {
    // Print in red if tape == NULL ?
    printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pc\n",
           part.bounding_box.p1.x - part.bounding_box.p0.x,
           part.bounding_box.p1.y - part.bounding_box.p0.y,
           part.bounding_box.p0.x, part.bounding_box.p0.y,
           "", //(part.footprint + "@" + part.value).c_str(),
           part.component_name.c_str(),
           part.angle,
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y);
}

void PostScriptMachine::Dispense(const Part &part, const Pad &pad) {
    // We print the same part for every single one of its pad. So could be more
    // optimal. Doesn't matter in real life.
    printf("%.3f %.3f   %.3f %.3f (%s) (%s) %.3f %.3f %.3f pc\n",
           part.bounding_box.p1.x - part.bounding_box.p0.x,
           part.bounding_box.p1.y - part.bounding_box.p0.y,
           part.bounding_box.p0.x, part.bounding_box.p0.y,
           "", part.component_name.c_str(),
           part.angle,
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y);

    // TODO: so this part looks like we shouldn't have to do it here.
    const float angle = 2 * M_PI * part.angle / 360.0;
    const float part_x = config_->board.origin.x + part.pos.x;
    const float part_y = config_->board.origin.y + part.pos.y;
    const float area = pad.size.w * pad.size.h;
    const float pad_x = pad.pos.x;
    const float pad_y = pad.pos.y;
    const float x = part_x + pad_x * cos(angle) - pad_y * sin(angle);
    const float y = part_y + pad_x * sin(angle) + pad_y * cos(angle);
    printf("%.3f %.3f m %.3f pp \n%.3f %.3f moveto ",
           x, y, sqrtf(area / M_PI), x, y);

}

void PostScriptMachine::Finish() {
    printf("showpage\n");
}
