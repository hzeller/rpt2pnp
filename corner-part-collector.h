/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef CORNER_PART_COLLECTOR_H
#define CORNER_PART_COLLECTOR_H

#include "rpt2pnp.h"

// Determines coordinates that are closest to the corner.
class CornerPartCollector {
public:
    void SetCorners(float min_x, float min_y, float max_x, float max_y) {
        corners_[0].x = min_x;  corners_[0].y = min_y;
        corners_[1].x = max_x;  corners_[1].y = min_y;
        corners_[2].x = min_x;  corners_[2].y = max_y;
        corners_[3].x = max_x;  corners_[3].y = max_y;
        for (int i = 0; i < 4; ++i) corner_distance_[i] = -1;
    }

    void Update(const Position &pos, const Part &part) {
        for (int i = 0; i < 4; ++i) {
            const float distance = Distance(corners_[i], pos);
            if (corner_distance_[i] < 0 || distance < corner_distance_[i]) {
                corner_distance_[i] = distance;
                closest_match_[i] = pos;
                closest_part_[i] = part;
            }
        }
    }

    const ::Part &get_part(int i) const { return closest_part_[i]; }
    const Position &get_closest(int i) const { return closest_match_[i]; }

private:
    Position corners_[4];
    float corner_distance_[4];
    Position closest_match_[4];
    Part closest_part_[4];
};

#endif // CORNER_PART_COLLECTOR_H
