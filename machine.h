/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef MACHINE_H_
#define MACHINE_H_

#include <stdio.h>

#include <string>
#include <set>
#include <functional>

struct PnPConfig;
class Dimension;
class Part;
class Pad;
class Tape;
class Position;

// A machine provides the actions.
class Machine {
public:
    virtual ~Machine() {}

    // Initialize machine. The comment should be added to the output file
    // if possible.
    virtual bool Init(const PnPConfig *config,
                      const std::string &init_comment,
                      const Dimension &dimension) = 0;

    // Pick "part" from given "tape". Tape provides absolute positions,
    // Part-position is relative to configured board origin.
    // The "tape" can be null in which case this operation might not succeed.
    virtual void PickPart(const Part &part, const Tape *tape) = 0;

    // Place "part" coming from "tape" on board.
    // Tape provides absolute positions, Part-position is relative to
    // configured board origin.
    // The "tape" can be null in which case this operation might not succeed.
    virtual void PlacePart(const Part &part, const Tape *tape) = 0;

    // Dispense "pad".
    virtual void Dispense(const Part &part, const Pad &pad) = 0;

    // Finish - shut down machine etc.
    virtual void Finish() = 0;
};

// A machine
class GCodeMachine : public Machine {
public:
    // A gcode machine. Each line is delivered to the write_line() function
    // which can be implemented by the caller for finer-grained control what
    // should happen to each gcode-line.
    GCodeMachine(std::function<void(const char *str, size_t len)> write_line,
                 float init_ms, float area_ms);

    // Simple output to a buffered FILE.
    GCodeMachine(FILE *output, float init_ms, float area_ms);

    bool Init(const PnPConfig *config, const std::string &init_comment,
              const Dimension &dimension) override;
    void PickPart(const Part &part, const Tape *tape) override;
    void PlacePart(const Part &part, const Tape *tape) override;
    void Dispense(const Part &part, const Pad &pad) override;
    void Finish() override;

private:
    // Send the commands to the write_line_() function, line by line.
    void SendFormattedCommands(const char *format, ...);

    std::function<void(const char *str, size_t len)> const write_line_;
    const float init_ms_;
    const float area_ms_;
    const PnPConfig *config_;
};

// A machine simulation that just shows the oiutput in postscript.
class PostScriptMachine : public Machine {
public:
    PostScriptMachine(FILE *output);

    bool Init(const PnPConfig *config, const std::string &init_comment,
              const Dimension &dimension) override;
    void PickPart(const Part &part, const Tape *tape) override;
    void PlacePart(const Part &part, const Tape *tape) override;
    void Dispense(const Part &part, const Pad &pad) override;
    void Finish() override;

private:
    FILE *const output_;
    const PnPConfig *config_;
    std::set<const Part *> dispense_parts_printed_;
};

#endif  // MACHINE_H_
