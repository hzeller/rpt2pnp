/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <iostream>

#include <string>
#include <vector>

// Event callbacks, to be implemented by whoever is interested in that stuff.
// These are the raw parse events, the recipient needs to gather all the
// relevant data. Implement what you need.
//
// Units are in mm.
class ParseEventReceiver {
public:
    // Maximum dimensions of the board. Board is normalized to be in range
    // (0,0) (max_x, max_y)
    virtual void StartBoard(float max_x, float max_y) {}

    virtual void StartComponent(const std::string &name) {}  // 'module' in french.
    virtual void Value(const std::string &name) {}
    virtual void Footprint(const std::string &name) {}

    virtual void EndComponent() {}

    virtual void StartPad(const std::string &name) {}
    virtual void EndPad() {}

    // Can be called within 'component' or 'pad'
    virtual void Position(float x, float y) {}
    virtual void Size(float w, float h) {}
    virtual void Layer(bool is_front) {}
    virtual void IsSMD(bool smd) {}

    // If this pad doesn't have a pad, then this drill size is 0.0
    virtual void Drill(float size) {}

    virtual void Orientation(float angle) {}
};

// parse RPT file, get raw parse events.
bool RptParse(std::istream *input, ParseEventReceiver *event);
