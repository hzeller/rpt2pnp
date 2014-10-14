/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "board.h"

#include <math.h>
#include <fstream>

#include "rpt-parser.h"

namespace {
    // Helper class to read file from parse events.
    // Collect the parts from parse events.
class PartCollector : public ParseEventReceiver {
public:
    PartCollector(std::vector<const Part*> *parts,
                  Dimension *board_dimension)
        : current_part_(NULL),
          collected_parts_(parts), board_dimension_(board_dimension) {}

protected:
    void StartBoard(float max_x, float max_y) override {
        board_dimension_->w = max_x;
        board_dimension_->h = max_y;
    }

    void StartComponent(const std::string &c) override {
        in_pad_ = false;
        current_part_ = new Part();
        current_part_->component_name = c;
        drillSum = 0;
        angle_ = 0;
    }

    void Value(const std::string &c) override {
        current_part_->value = c;
    }

    void Footprint(const std::string &c) override {
        current_part_->footprint = c;
    }

    void EndComponent() override {
        if (drillSum > 0)
            delete current_part_;  // through-hole. We're not interested in that.
        else
            collected_parts_->push_back(current_part_);
        current_part_ = NULL;
    }

    // Not caring about pads right now.
    void StartPad(const std::string &c) override {
        current_pad_.name = c;
        in_pad_ = true;
    }
    void EndPad() override {
        in_pad_ = false;
        if (current_part_) {
            current_part_->pads.push_back(current_pad_);
        }
    }

    void Position(float x, float y) override {
        if (in_pad_) {
            current_pad_.pos.Set(x, y);
        } else {
            current_part_->pos.x = x;
            current_part_->pos.y = y;
        }
    }

    void Size(float w, float h) override {
        if (in_pad_) {
            current_pad_.size.w = w;
            current_pad_.size.h = h;

            // TODO:
            float x, y;
            x = current_pad_.pos.x - w/2;
            if (x < current_part_->bounding_box.p0.x)
                current_part_->bounding_box.p0.x = x;
            x = current_pad_.pos.x + w/2;
            if (x > current_part_->bounding_box.p1.x)
                current_part_->bounding_box.p1.x = x;
            y = current_pad_.pos.y - h/2;
            if (y < current_part_->bounding_box.p0.y)
                current_part_->bounding_box.p0.y = y;
            y = current_pad_.pos.y + h/2;
            if (y > current_part_->bounding_box.p1.y)
                current_part_->bounding_box.p1.y = y;
        }
    }

    void Drill(float size) override {
        drillSum += size; // looking for nonzero drill size
    }

    void Orientation(float angle) override {
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

    ::Pad current_pad_;
    std::string component_name_;
    Part *current_part_;
    std::vector<const Part*> *collected_parts_;
    Dimension *board_dimension_;
};
}  // namespace

Board::Board() {}

Board::~Board() {
    for (const Part* part : parts_) {
        delete part;
    }
}

bool Board::ParseFromRpt(const std::string& filename) {
    PartCollector collector(&parts_, &board_dim_);
    std::ifstream in(filename);
    if (!in.is_open()) {
        fprintf(stderr, "Can't open %s\n", filename.c_str());
        return false;
    }
    return RptParse(&in, &collector);
}
