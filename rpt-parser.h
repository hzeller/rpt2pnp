/* -*- c++ -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include <iostream>

#include <string>

// Event callbacks, to be implemented by whoever is interested in that stuff.
// These are the raw parse events, the recipient needs to gather all the
// relevant data.
// Units are in mm.
class ParseEventReceiver {
public:
    virtual void StartComponent(const std::string &name) = 0;  // 'module' in french.
    virtual void EndComponent() = 0;

    virtual void StartPad(const std::string &name) =  0;
    virtual void EndPad() =  0;

    // Can be called within 'compponent' or 'pad'
    virtual void Position(float x, float y) = 0;
    virtual void Size(float w, float h) = 0;
    virtual void Value(const std::string &name) = 0;

    // If this pad doesn't have a pad, then this drill size is 0.0
    virtual void Drill(float size) = 0;

    virtual void Orientation(float angle) = 0;
};

bool RptParse(std::istream *input, ParseEventReceiver *event);
