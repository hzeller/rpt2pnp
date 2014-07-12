/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 *
 * Uses the RPT parser to create a list of parts.
 */

#include <math.h>
#include <fstream>

#include "rpt-parser.h"
#include "rpt2pnp.h"

namespace {
// Collect the parts from parse events.
class PartCollector : public ParseEventReceiver {
public:
    PartCollector(std::vector<const Part*> *parts)
        : origin_x_(0), origin_y_(0), current_part_(NULL),
          collected_parts_(parts) {}

protected:
    virtual void StartComponent(const std::string &c) {
        current_part_ = new Part();
        current_part_->component_name = c;
        drillSum = 0;
        angle_ = 0;
    }

    virtual void Value(const std::string &c) {
        current_part_->value = c;
    }

    virtual void EndComponent() {
        if (drillSum > 0)
            delete current_part_;  // through-hole. We're not interested in that.
        else
            collected_parts_->push_back(current_part_);
        current_part_ = NULL;
    }

    // Not caring about pads right now.
    virtual void StartPad(const std::string &c) {
        // collect pads for dispensing.
    }
    virtual void EndPad() { }

    virtual void Position(float x, float y) {
        // The first position callback we get is for the cmponent.
        if ((current_part_->pos.x == 0)
            && (current_part_->pos.y == 0)) {
            current_part_->pos.x = x;
            current_part_->pos.y = y;
        }
    }
    virtual void Size(float w, float h) {
        if (current_part_->dimension.w == 0
            && current_part_->dimension.h == 0) {
            current_part_->dimension.w = w;
            current_part_->dimension.h = h;
        }
    }

    virtual void Drill(float size) {
        drillSum += size; // looking for nonzero drill size
    }

    virtual void Orientation(float angle) {
        if (angle_ == 0) { // only take the first "position" of the component record
            // Angle is in degrees, make that radians.
            // mmh, and it looks like it turned in negative direction ? Probably part
            // of the mirroring.
            angle_ = -M_PI * angle / 180.0;
            current_part_->angle = angle; // change to _angle if you really want radians
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
    float drillSum; // add up all the pad drill sizes, should be 0 for smt

    std::string component_name_;
    Part *current_part_;
    std::vector<const Part*> *collected_parts_;
};
}  // namespace

// public interface
bool ReadRptFile(const std::string& rpt_file,
                 std::vector<const Part*> *result) {
    PartCollector collector(result);
    std::ifstream in(rpt_file);
    return RptParse(&in, &collector);
}
