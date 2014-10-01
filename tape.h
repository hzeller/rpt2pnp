/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 *
 * Representation of a tape lying down with components.
 */

#ifndef PNP_TAPE_H
#define PNP_TAPE_H

#include <string>

class Tape {
public:
    Tape();

    void SetFirstComponentPosition(float x, float y, float z);
    void SetComponentSpacing(float dx, float dy);
    void SetNumberComponents(int n);
    void SetAngle(float a) { angle_ = a; }

    // Return angle, including slant
    float angle() const { return angle_ + slant_angle_; }

    // Get next component position. Returns 'true' if there is any, 'false'
    // if we exhausted our components. Does not advance tape.
    bool GetPos(float *x, float *y) const;

    // Advances on the tape, so each call yields a different position
    bool Advance();

    void DebugPrint() const;  // print to stderr.
    float height() const { return z_; }
    bool parts_available() const { return count_ > 0; }

private:
    float x_, y_, z_;
    float dx_, dy_;
    float angle_, slant_angle_;
    int count_;
};

#endif  // PNP_TAPE_H
