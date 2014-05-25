/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "rpt-parser.h"
#include "rpt2paste.h"

static const float minimum_milliseconds = 50;
static const float area_to_milliseconds = 25;  // mm^2 to milliseconds.

// Smallest point from origin.
static float offset_x = 10;
static float offset_y = 10;

#define Z_DISPENSING "1.7"        // Position to dispense stuff. Just above board.
#define Z_HOVER_DISPENSER "2.5"   // Hovering above position.
#define Z_HIGH_UP_DISPENSER "5"   // high up to separate paste.

// Determiness coordinates that are closest to the corner.
class CornerPadCollector {
public:
    void SetCorners(float min_x, float min_y, float max_x, float max_y) {
        corners_[0].x = min_x;  corners_[0].y = min_y;
        corners_[1].x = max_x;  corners_[1].y = min_y;
        corners_[2].x = min_x;  corners_[2].y = max_y;
        corners_[3].x = max_x;  corners_[3].y = max_y;
        for (int i = 0; i < 4; ++i) corner_distance_[i] = -1;
    }

    void Update(const Position &pos, const Pad &pad) {
        for (int i = 0; i < 4; ++i) {
            const float distance = Distance(corners_[i], pos);
            if (corner_distance_[i] < 0 || distance < corner_distance_[i]) {
                corner_distance_[i] = distance;
                closest_match_[i] = pos;
                closest_pad_[i] = pad;
            }
        }
    }

    const ::Pad &get_pad(int i) const { return closest_pad_[i]; }
    const Position &get_closest(int i) const { return closest_match_[i]; }

private:
    Position corners_[4];
    float corner_distance_[4];
    Position closest_match_[4];
    Pad closest_pad_[4];
};

class Printer {
public:
    virtual ~Printer() {}
    virtual void Init(float min_x, float min_y, float max_x, float max_y) = 0;
    virtual void Pad(const Position &pos, const Pad &pad) = 0;
    virtual void Finish() = 0;
};

class GCodePrinter : public Printer {
public:
    GCodePrinter(float init_ms, float area_ms) : init_ms_(init_ms), area_ms_(area_ms) {}
    virtual void Init(float min_x, float min_y, float max_x, float max_y) {
        printf("; rpt2paste -d %.2f -D %.2f file.rpt\n", init_ms_, area_ms_);
        // G-code preamble. Set feed rate, homing etc.
        printf(
               //    "G28\n" assume machine is already homed before g-code is executed
               "G21\n" // set to mm
               "G0 F20000\n"
               "G1 F4000\n"
               "G0 Z" Z_HIGH_UP_DISPENSER "\n"
               );
    }

    virtual void Pad(const Position &pos, const ::Pad &pad) {
        printf(
               "G0 X%.3f Y%.3f Z" Z_HOVER_DISPENSER " ; comp=%s, pad=%s\n"  // move to new position, above board
               "G1 Z" Z_DISPENSING "\n"                   // ready to dispense.
               "M106\n"               // switch on fan (=solenoid)
               "G4 P%.1f ; %.2f mm^2\n"  // Wait given milliseconds; dependend on area.
               "M107\n"               // switch off fan
               "G1 Z" Z_HIGH_UP_DISPENSER "\n", // high above to have paste is well separated
               pos.x, pos.y, pad.component_name.c_str(), pad.pad_name.c_str(),
               init_ms_ + pad.area * area_ms_,
               pad.area);
    }

    virtual void Finish() {
        printf(";done\n");
    }
private:
    const float init_ms_;
    const float area_ms_;
};

class GCodeCornerIndicator : public Printer {
public:
    GCodeCornerIndicator(float init_ms, float area_ms) : init_ms_(init_ms), area_ms_(area_ms) {}

    virtual void Init(float min_x, float min_y, float max_x, float max_y) {
        corners_.SetCorners(min_x, min_y, max_x, max_y);
        // G-code preamble. Set feed rate, homing etc.
        printf(
               //    "G28\n" assume machine is already homed before g-code is executed
               "G21\n" // set to mm
               "G1 F2000\n"
               "G0 Z4\n" // X0 Y0 may be outside the reachable area, and no need to go there
               );
    }

    virtual void Pad(const Position &pos, const ::Pad &pad) {
        corners_.Update(pos, pad);
    }

    virtual void Finish() {
        for (int i = 0; i < 4; ++i) {
            const ::Pad &p = corners_.get_pad(i);
            const Position pos = corners_.get_closest(i);
            printf("G0 X%.3f Y%.3f Z" Z_DISPENSING " ; comp=%s pad=%s\n"
                   "G4 P2000 ; area=%.1f mm^2; dispense-time=%.1fms\n"
                   "G0 Z" Z_HIGH_UP_DISPENSER "\n",
                   pos.x, pos.y, p.component_name.c_str(), p.pad_name.c_str(),
                   p.area, init_ms_ + p.area * area_ms_);

        }
        printf(";done\n");
    }

private:
    CornerPadCollector corners_;
    const float init_ms_;
    const float area_ms_;
};

class PostScriptPrinter : public Printer {
public:
    virtual void Init(float min_x, float min_y, float max_x, float max_y) {
        corners_.SetCorners(min_x, min_y, max_x, max_y);
        min_x -= 2; min_y -=2; max_x +=2; max_y += 2;
        const float mm_to_point = 1 / 25.4 * 72.0;
        printf("%%!PS-Adobe-3.0\n%%%%BoundingBox: %.0f %.0f %.0f %.0f\n\n",
               min_x * mm_to_point, min_y * mm_to_point,
               max_x * mm_to_point, max_y * mm_to_point);
        printf("%% PastePad. Stack: <diameter>\n/pp { 0.2 setlinewidth 0 360 arc stroke } def\n\n"
               "%% Move. Stack: <x> <y>\n/m { 0.01 setlinewidth lineto currentpoint stroke } def\n\n");
        printf("72.0 25.4 div dup scale  %% Switch to mm\n");
        printf("%.1f %.1f moveto\n", offset_x, offset_y);
    }

    virtual void Pad(const Position &pos, const ::Pad &pad) {
        corners_.Update(pos, pad);
        printf("%.3f %.3f m %.3f pp \n%.3f %.3f moveto ",
               pos.x, pos.y, sqrtf(pad.area / M_PI), pos.x, pos.y);
    }

    virtual void Finish() {
        printf("0 0 1 setrgbcolor\n");
        for (int i = 0; i < 4; ++i) {
            //const ::Pad &pad = corners_.get_pad(i);
            const Position &pos = corners_.get_closest(i);
            printf("%.1f 2 add %.1f moveto %.1f %.1f 2 0 360 arc stroke\n",
                   pos.x, pos.y, pos.x, pos.y);
        }
        printf("showpage\n");
    }

private:
    CornerPadCollector corners_;
};

class PadCollector : public ParseEventReceiver {
public:
    PadCollector(std::vector<const Pad*> *pads) : origin_x_(0), origin_y_(0), current_pad_(NULL),
                                                  collected_pads_(pads) {}

    virtual void StartComponent(const std::string &c) {
        component_name_ = c;
        assert(current_pad_ == NULL);
    }
    virtual void EndComponent() { }

    virtual void StartPad(const std::string &c) {
        current_pad_ = new Pad();
        current_pad_->component_name = component_name_;
        current_pad_->pad_name = c;
    }
    virtual void EndPad() {
        if (current_pad_->drill != 0)
            delete current_pad_;  // through-hole. We're not interested in that.
        else
            collected_pads_->push_back(current_pad_);
        current_pad_ = NULL;
    }

    virtual void Position(float x, float y) {
        if (current_pad_ != NULL) {
            rotateXY(&x, &y);
            current_pad_->pos.x = origin_x_ + x;
            current_pad_->pos.y = origin_y_ + y;
        }
        else {
            origin_x_ = x;
            origin_y_ = y;
        }
    }
    virtual void Size(float w, float h) {
        if (current_pad_ == NULL) return;
        current_pad_->area = w * h;
    }

    virtual void Drill(float size) {
        assert(current_pad_ != NULL);
        current_pad_->drill = size;
    }

    virtual void Orientation(float angle) {
        if (current_pad_ == NULL) {
            // Angle is in degrees, make that radians.
            // mmh, and it looks like it turned in negative direction ? Probably part
            // of the mirroring.
            angle_ = -M_PI * angle / 180.0;
        }
    }

private:
    void rotateXY(float *x, float *y) {
        float xnew = *x * cos(angle_) - *y * sin(angle_);
        float ynew = *x * sin(angle_) + *y * cos(angle_);
        *x = xnew;
        *y = ynew;
    }

    // Current coordinate system.
    float origin_x_;
    float origin_y_;
    float angle_;

    std::string component_name_;
    // If we have seen a start-pad, this is not-NULL.
    Pad *current_pad_;
    std::vector<const Pad*> *collected_pads_;
};

static int usage(const char *prog) {
    fprintf(stderr, "Usage: %s <options> <rpt-file>\n"
            "Options:\n"
            "\t-p      : Output as PostScript.\n"
            "\t-c      : Output corner DryRun G-Code.\n"
            "\t-d <ms> : Dispensing init time ms (default %.1f)\n"
            "\t-D <ms> : Dispensing time ms/mm^2 (default %.1f)\n",
            prog, minimum_milliseconds, area_to_milliseconds);
    return 1;
}

int main(int argc, char *argv[]) {
    enum OutputType {
        OUT_DISPENSING,
        OUT_CORNER_GCODE,
        OUT_POSTSCRIPT
    } output_type = OUT_DISPENSING;

    float start_ms = minimum_milliseconds;
    float area_ms = area_to_milliseconds;
    int opt;
    while ((opt = getopt(argc, argv, "pcd:D:")) != -1) {
        switch (opt) {
        case 'p':
            output_type = OUT_POSTSCRIPT;
            break;
        case 'c':
            output_type = OUT_CORNER_GCODE;
            break;
        case 'd':
            start_ms = atof(optarg);
            break;
        case 'D':
            area_ms = atof(optarg);
            break;
        default: /* '?' */
            return usage(argv[0]);
        }
    }

    if (optind >= argc) {
        return usage(argv[0]);
    }

    const char *rpt_file = argv[optind];

    std::vector<const Pad*> pads;
    PadCollector collector(&pads);
    std::ifstream in(rpt_file);
    RptParse(&in, &collector);

    // The coordinates coming out of the file are mirrored, so we determine the maximum
    // to mirror at these axes.
    // (mmh, looks like it is only mirrored on y axis ?)
    float min_x = pads[0]->pos.x, min_y = pads[0]->pos.y;
    float max_x = pads[0]->pos.x, max_y = pads[0]->pos.y;
    for (size_t i = 0; i < pads.size(); ++i) {
        min_x = std::min(min_x, pads[i]->pos.x);
        min_y = std::min(min_y, pads[i]->pos.y);
        max_x = std::max(max_x, pads[i]->pos.x);
        max_y = std::max(max_y, pads[i]->pos.y);
    }

    Printer *printer;
    switch (output_type) {
    case OUT_DISPENSING:   printer = new GCodePrinter(start_ms, area_ms); break;
    case OUT_CORNER_GCODE: printer = new GCodeCornerIndicator(start_ms, area_ms); break;
    case OUT_POSTSCRIPT:   printer = new PostScriptPrinter(); break;
    }

    OptimizePads(&pads);

    printer->Init(offset_x, offset_y,
                  (max_x - min_x) + offset_x, (max_y - min_y) + offset_y);

    float dispense_time = 0;
    for (size_t i = 0; i < pads.size(); ++i) {
        const Pad *pad = pads[i];
        // We move x-coordinates relative to the smallest X.
        // Y-coordinates are mirrored at the maximum Y (that is how the come out of the file)
        printer->Pad(Position(pad->pos.x + offset_x - min_x,
                              max_y - pad->pos.y + offset_y),
                     *pad);
        dispense_time += start_ms + pad->area * area_ms;
    }

    printer->Finish();

    fprintf(stderr, "Dispensed %zd pads (%.0f ms + %.0f mm/mm^2). Total dispense time: %.1fs\n",
            pads.size(), start_ms, area_ms, dispense_time / 1000.0f);
    for (size_t i = 0; i < pads.size(); ++i) {
        delete pads[i];
    }
    delete printer;
    return 0;
}
