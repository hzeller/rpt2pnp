/* -*- c++ -*-
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

    // TODO: this is not accurate. We should make this relative to the
    // slant of the tape, e.g. its angle on the x/y table according to
    // SetComponentSpacing()
    float angle() const { return angle_; }

    // Get next component position. Returns 'true' if there is any, 'false'
    // if we exhausted our components.
    // Advances on the tape, so each call yields a different position.
    bool GetNextPos(float *x, float *y, float *z);

    void DebugPrint() const;  // print to stderr.

private:
    float x_, y_, z_;
    float dx_, dy_;
    float angle_;
    int count_;
};

#endif  // PNP_TAPE_H
