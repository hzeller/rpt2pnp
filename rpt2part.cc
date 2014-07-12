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
    PartCollector(std::vector<const Part*> *parts,
                  Dimension *board_dimension)
        : current_part_(NULL),
          collected_parts_(parts), board_dimension_(board_dimension) {}

protected:
    virtual void StartBoard(float max_x, float max_y) {
        board_dimension_->w = max_y;
        board_dimension_->h = max_x;
    }

    virtual void StartComponent(const std::string &c) {
        in_pad_ = false;
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
        in_pad_ = true;
    }
    virtual void EndPad() {
        in_pad_ = false;
    }

    virtual void Position(float x, float y) {
        if (in_pad_) {
            rotateXY(&x, &y);
            pad_position_.Set(x, y);
        } else {
            current_part_->pos.x = x;
            current_part_->pos.y = y;
        }
    }
    virtual void Size(float w, float h) {
        if (in_pad_) {
            float x, y;
            x = pad_position_.x - w/2;
            if (x < current_part_->bounding_box.p0.x)
                current_part_->bounding_box.p0.x = x;
            x = pad_position_.x + w/2;
            if (x > current_part_->bounding_box.p1.x)
                current_part_->bounding_box.p1.x = x;
            y = pad_position_.y - h/2;
            if (y < current_part_->bounding_box.p0.y)
                current_part_->bounding_box.p0.y = y;
            y = pad_position_.y + h/2;
            if (y > current_part_->bounding_box.p1.y)
                current_part_->bounding_box.p1.y = y;
        }
    }

    virtual void Drill(float size) {
        drillSum += size; // looking for nonzero drill size
    }

    virtual void Orientation(float angle) {
        if (in_pad_)
            return;
        // Angle is in degrees, make that radians.
        // mmh, and it looks like it turned in negative direction ? Probably part
        // of the mirroring.
        angle_ = -M_PI * angle / 180.0;
        current_part_->angle = angle; // change to angle_ if you really want radians
    }

private:
    void rotateXY(float *x, float *y) {
        float xnew = *x * cos(angle_) - *y * sin(angle_);
        float ynew = *x * sin(angle_) + *y * cos(angle_);
        *x = xnew;
        *y = ynew;
    }

    // Current coordinate system.
    float angle_;
    float drillSum; // add up all the pad drill sizes, should be 0 for smt
    bool in_pad_;

    ::Position pad_position_;
    std::string component_name_;
    Part *current_part_;
    std::vector<const Part*> *collected_parts_;
    Dimension *board_dimension_;
};
}  // namespace

// public interface
bool ReadRptFile(const std::string& rpt_file,
                 std::vector<const Part*> *result,
                 Dimension* board_dimension) {
    PartCollector collector(result, board_dimension);
    std::ifstream in(rpt_file);
    return RptParse(&in, &collector);
}
