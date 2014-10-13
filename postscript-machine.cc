/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "machine.h"

#include <math.h>

#include "pnp-config.h"
#include "tape.h"
#include "board.h"

#define DISPENSE_PART_COLOR "0.8 0.8 0.8"
#define PICK_COLOR          "0 0 0"
#define PLACE_COLOR         "0 0 0"
#define PLACE_MISSING_PART  "1 0.3 0"

static const char ps_preamble[] = R"(
% <width> <height> <x0> <y0>
/rect {
  moveto
  1 index 0 rlineto
  0 exch rlineto
  neg 0 rlineto
  closepath
  stroke
} def

/fillrect {
  moveto
  1 index 0 rlineto
  0 exch rlineto
  neg 0 rlineto
  closepath
  fill
} def

% x y
/showmark {
  gsave
  translate
  0 0 1 setrgbcolor
  45 rotate
  0 0 moveto
  -3 0 rmoveto 6 0 rlineto stroke
  0 0 moveto
  0 -3 rmoveto 0 6 rlineto stroke
  0  0 1 0 360 arc stroke
  grestore
} def

% print component
% <width> <height>  <x0> <y0> <r> <g> <b> <name> <angle> <x> <y> pp
/pc {
    gsave
    translate              % takes <x> <y>
    rotate                 % takes <angle>
    0 0 moveto
    0 0 0.1 0 360 arc      % mark center with tiny dot.
    0 0 1 setrgbcolor show % takes <name>
    setrgbcolor            % takes <r><g><b>
    rect                   % take <dy> <dx> <x0> <y0>
    grestore
} def

% PastePad.
% Stack: <diameter>
/pp { 0.2 setlinewidth 0 360 arc stroke } def

% Move, show path.
% Stack: <x> <y>
/m {
  0 0.5 0 setrgbcolor
  0 setlinewidth lineto
  currentpoint        % leave the new point on the stack
  stroke
  0 0 0 setrgbcolor
} def

72.0 25.4 div dup scale                  % Switch to mm
0.05 setlinewidth
/Helvetica findfont 1.5 scalefont setfont  % Small font
)";

bool PostScriptMachine::Init(const PnPConfig *config,
                             const std::string &init_comment,
                             const Dimension& board_dim) {
    config_ = config;
    if (config_ == NULL) {
        config_ = new PnPConfig();
    }
    dispense_parts_printed_.clear();
    const float mm_to_point = 1 / 25.4 * 72.0;
    if (config_->tape_for_component.size() == 0) {
        printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
               config_->board.origin.x * mm_to_point,
               config_->board.origin.y * mm_to_point,
               board_dim.w * mm_to_point, board_dim.h * mm_to_point);
    } else {
        printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
               0 * mm_to_point, 0 * mm_to_point,
               300 * mm_to_point, 300 * mm_to_point);
    }
    printf("%% %s\n", init_comment.c_str());
    printf("%s", ps_preamble);

    // Draw board
    printf("%.1f %.1f %.1f %.1f rect\n", board_dim.w, board_dim.h,
           config_->board.origin.x, config_->board.origin.y);
    printf("%.1f %.1f showmark\n", config_->board.origin.x, config_->board.origin.y);
    // Push a currentpoint on stack (dispense draws a line from here)
    printf("0 0 moveto\n");
    return true;
}

static void PrintPads(const Part &part, float offset_x, float offset_y,
                      float angle){
    // Print pads first, so that the bounding box is nice and black.
    printf("%%pads\n");
    printf("gsave\n %.3f %.3f translate %.3f rotate\n",
           offset_x, offset_y, angle);
    int padnum = 0;
    for (const Pad &pad : part.pads) {
        ++padnum;
        printf(" 0.7 0.9 0 setrgbcolor\n");
        printf(" %.3f %.3f %.3f %.3f fillrect\n",
               pad.size.w, pad.size.h,
               pad.pos.x - pad.size.w/2,
               pad.pos.y - pad.size.h/2);
        printf(" 0 0 0 setrgbcolor\n");
        printf(" %.3f %.3f moveto (%d) show stroke\n",
               pad.pos.x - pad.size.w/2,
               pad.pos.y - pad.size.h/2,
               padnum);
    }
    printf(" stroke\ngrestore\n");
}

void PostScriptMachine::PickPart(const Part &part, const Tape *tape) {
    if (tape == NULL) return;
    float tx, ty;
    if (tape->GetPos(&tx, &ty)) {
        // Print component on tape
        PrintPads(part, tx, ty, tape->angle());
        printf("%.3f %.3f   %.3f %.3f %s (%s) %.3f %.3f %.3f pc\n",
               part.bounding_box.p1.x - part.bounding_box.p0.x,
               part.bounding_box.p1.y - part.bounding_box.p0.y,
               part.bounding_box.p0.x, part.bounding_box.p0.y,
               PICK_COLOR,
               part.component_name.c_str(),
               tape->angle(),
               tx, ty);
    }
}

void PostScriptMachine::PlacePart(const Part &part, const Tape *tape) {
    // Print pads first, so that the bounding box is nice and black.
    PrintPads(part,
              config_->board.origin.x + part.pos.x,
              config_->board.origin.y + part.pos.y,
              part.angle);

    // Not available parts because tape is not there or exhausted are still
    // visualized, but with a warning color.
    const char *const color = (tape != NULL && tape->parts_available())
        ? PLACE_COLOR
        : PLACE_MISSING_PART;
    printf("%.3f %.3f   %.3f %.3f %s (%s) %.3f %.3f %.3f pc\n",
           part.bounding_box.p1.x - part.bounding_box.p0.x,
           part.bounding_box.p1.y - part.bounding_box.p0.y,
           part.bounding_box.p0.x, part.bounding_box.p0.y,
           color,
           //(part.footprint + "@" + part.value) +
           part.component_name.c_str(),
           part.angle,
           part.pos.x + config_->board.origin.x,
           part.pos.y + config_->board.origin.y);
}

void PostScriptMachine::Dispense(const Part &part, const Pad &pad) {
    if (dispense_parts_printed_.find(&part) == dispense_parts_printed_.end()) {
        // First time we see this component.
        printf("%.3f %.3f   %.3f %.3f %s (%s) %.3f %.3f %.3f pc\n",
               part.bounding_box.p1.x - part.bounding_box.p0.x,
               part.bounding_box.p1.y - part.bounding_box.p0.y,
               part.bounding_box.p0.x, part.bounding_box.p0.y,
               DISPENSE_PART_COLOR,
               part.component_name.c_str(),
               part.angle,
               part.pos.x + config_->board.origin.x,
               part.pos.y + config_->board.origin.y);
        dispense_parts_printed_.insert(&part);
    }

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
