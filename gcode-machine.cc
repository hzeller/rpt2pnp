/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

/*
  TODO PnP
   - Instead of a one-size-fits-all 10mm, have a shallow hover height (1mm).
     Essentially the clearance we want above obstructions in the path.
   - determine actual hover depending on component height
   - Pick:
      - go hover height over tape.
      - go down, suck it.
      - pull up top-of-tape + tape-thickness + 1mm to get the component, that is
        supposedly as high as the tape, safely out of it
   - Place:
      - move it towards the board. Z should be
        top-of-board + component-height + 1mm. That way multiple of the same
        component are not knocked over
      - (thinner components come first in the order, so this is always safe)
   - Result: best travel times (less Z movement), but guaranteed non-knockover
     result.
   - TODO: These height decisions should not be made in the machine. They
     should happen outside whoever sends the plan to the machine.
     That way, it allows for easy testing the results by implementing a mock
     machine in the test.

  Also FIXME:
  Current assumption of tape is that is sits somewhere on the build-platform.
  (i.e. that tape-height == component-height).
  That needs to change, as it could well be elevated on a tray or something and
  then we would start dropping components from some height on the board :)
*/

#include "machine.h"

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#include "tape.h"
#include "board.h"

#include "pnp-config.h"

// TODO: most of these constants should be configurable or deduced from board/
// configuration.

// Hovering while transporting a component.
// TODO: calculate that to not knock over already placed components.
#define PNP_Z_HOVERING 10

// Components are a bit higher as they are resting on some card-board. Let's
// assume some value here.
// Ultimately, we want the placement operation be a bit spring-loaded.
#define PNP_TAPE_THICK 0.0

// Multiplication to get 360 degrees mapped to one turn. This is specific to
// our stepper motor.
#define PNP_ANGLE_FACTOR (50.34965 / 360)

// Speeds in mm/s
#define PNP_TO_TAPE_SPEED 1000      // moving needle to tape
#define PNP_TO_BOARD_SPEED 100      // moving component from tape to board

#define DISP_MOVE_SPEED 400         // move dispensing unit to next pad
#define DISP_DISPENSE_SPEED 100     // speed when doing the dispensing down/up

#define DISP_Z_DISPENSING_ABOVE 0.3      // Above board when dispensing
#define DISP_Z_HOVER_ABOVE 2             // Above board when moving around
#define DISP_Z_SEPARATE_DROPLET_ABOVE 5  // Above board right after dispensing.

// All templates should be in a separate file somewhere so that we don't
// have to compile.

// TODO: The positional arguments are pretty fragile.

// param: moving needle up.
static const char *const gcode_preamble = R"(
M107       (turn off dispensing solenoid)
M42 P6 S0  (turn off pnp vacuum)
G28 Y0     (Home y - away from holding bracket)
G28 X0     (Safe to home X now)
G28 Z0     (.. and z)
G21        (set to mm)
T1         (Use E1 extruder, our 'A' axis for PnP component rotation)
M302       (cold extrusion override - because it is not actually an extruder)
G90        (Use absolute positions in general)
G92 E0     ('home' E axis)

G1 Z%.1f E0 (Move needle out of way)
)";

// param: name, move-speed, x, y, zup, zdown, a, zup
static const char *const gcode_pick = R"(
( -- Pick %s -- )
G0 F%d X%.3f Y%.3f Z%.3f E%.3f (Move over component to pick.)
G1 Z%-6.2f   F4000 (move down on tape)
G4                 (flush buffer)
M42 P6 S255        (turn on suckage)
G1 Z%-6.3f         (Move up a bit for travelling)
)";

// param: name, place-speed, x, y, zup, a, zdown, zup
static const char *const gcode_place = R"(
( -- Place %s -- )
G0 F%d X%.3f Y%.3f Z%.3f E%.3f (Move component to place on board.)
G1 Z%-6.3f F4000 (move down over board thickness)
G4               (flush buffer.)
M42 P6 S0        (turn off suckage)
G4               (flush buffer.)
M42 P8 S255      (blow)
G4 P40           (.. for 40ms)
M42 P8 S0        (done.)
G1 Z%-6.2f       (Move up)
)";

// move to new position, above board.
// param: component-name, pad-name, x, y, hover-z
static const char *const gcode_dispense_move = R"(
( -- component %s, pad %s -- )
G0 F%d X%.3f Y%.3f Z%.3f (move there)
)";

// Dispense paste.
// param: z-dispense-height, wait-time-ms, area, z-separate-droplet
static const char *const gcode_dispense_paste =
    R"(G1 F%d Z%.2f  (Go down to dispense)
M106            (switch on fan=solenoid)
G4 P%-5.1f       (Wait time dependent on area %.2f mm^2)
M107            (switch off solenoid)
G1 Z%.2f        (high above to have paste separated)
)";

static const char *const gcode_finish = R"(
M107       (turn off dispensing solenoid)
M42 P6 S0  (turn off pnp vacuum)
G91        (we want to move z relative)
G1 Z20     (move above any obstacles)
G90        (back to sane absolute position default)
G28 X0 Y0  (Home x/y, but leave z clear)
M84        (stop motors)
)";

// TODO(hzeller): These helper functions should probably be somewhere
// else.
namespace {
// Wait for input to become ready for read or timeout reached.
// If the file-descriptor becomes readable, returns number of milli-seconds
// left.
// Returns 0 on timeout (i.e. no millis left and nothing to be read).
// Returns -1 on error.
static int AwaitReadReady(int fd, int timeout_millis) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout_millis * 1000;

    FD_SET(fd, &read_fds);
    int s = select(fd + 1, &read_fds, NULL, NULL, &tv);
    if (s < 0)
        return -1;
    return tv.tv_usec / 1000;
}

static int ReadLine(int fd, char *result, int len, bool do_echo) {
    int bytes_read = 0;
    char c = 0;
    while (c != '\n' && c != '\r' && bytes_read < len) {
        if (read(fd, &c, 1) < 0)
            return -1;
        ++bytes_read;
        *result++ = c;
        if (do_echo) write(STDERR_FILENO, &c, 1);  // echo back.
    }
    *result = '\0';
    return bytes_read;
}

// Discard all input until nothing is coming anymore within timeout. In
// particular on first connect, this helps us to get into a clean state.
static int DiscardAllInput(int fd, int timeout_ms) {
    int total_bytes = 0;
    char buf[128];
    while (AwaitReadReady(fd, timeout_ms) > 0) {
        int r = read(fd, buf, sizeof(buf));
        if (r < 0) {
            perror("reading trouble");
            return -1;
        }
        total_bytes += r;
        if (r > 0) write(STDERR_FILENO, buf, r);  // echo back.
    }
    return total_bytes;
}

// 'ok' comes on a single line, maybe followed by something.
static void WaitForOk(int fd) {
    char buffer[512];
    for (;;) {
        if (ReadLine(fd, buffer, sizeof(buffer), false) < 0)
            break;
        if (strncasecmp(buffer, "ok", 2) == 0)
            break;
    }
}
}

GCodeMachine::GCodeMachine(
    std::function<void(const char *str, size_t len)> write_line,
    float init_ms, float area_ms)
    : write_line_(std::move(write_line)), init_ms_(init_ms), area_ms_(area_ms),
      config_(NULL) {}

GCodeMachine::GCodeMachine(FILE *output, float init_ms, float area_ms)
    : GCodeMachine([output](const char *str, size_t len) {
            fwrite(str, 1, len, output);
            fflush(output);
        }, init_ms, area_ms) {}


GCodeMachine::GCodeMachine(int input_fd, int output_fd,
                           float init_ms, float area_ms)
    : GCodeMachine([input_fd, output_fd](const char *str, size_t len) {
            if (len == 0 || *str == '\n' || *str == ';' || *str == '(')
                return;  // Ignore empty lines or all-comment lines.
            write(output_fd, str, len);
            WaitForOk(input_fd);
        }, init_ms, area_ms) {
    DiscardAllInput(input_fd, 1000);  // Start with clean slate.
}

bool GCodeMachine::Init(const PnPConfig *config,
                        const std::string &init_comment,
                        const Dimension& dim) {
    assert(write_line_ != NULL);  // call any of SetOutputMode*() first.
    config_ = config;
    if (config_ == NULL) {
        fprintf(stderr, "Need configuration\n");
        return false;
    }
    fprintf(stderr, "Board-thickness = %.1fmm\n",
            config_->board.top - config_->bed_level);
    SendFormattedCommands("( %s )\n", init_comment.c_str());
    float highest_tape = config_->board.top;
    for (const auto &t : config_->tape_for_component) {
        highest_tape = std::max(highest_tape, t.second->height());
    }
    SendFormattedCommands(gcode_preamble, highest_tape + 10);
    return true;
}

void GCodeMachine::PickPart(const Part &part, const Tape *tape) {
    if (tape == NULL) return;
    float px, py;
    if (!tape->GetPos(&px, &py)) {
        fprintf(stderr, "We are out of components for %s %s\n",
                part.footprint.c_str(), part.value.c_str());
        return;
    }

    const float board_thick = config_->board.top - config_->bed_level;
    const float travel_height = tape->height() + board_thick + PNP_Z_HOVERING;
    const std::string print_name = part.component_name + " ("
        + part.footprint + "@" + part.value + ")";

    // param: name, x, y, zdown, a, zup
    SendFormattedCommands(
        gcode_pick,
        print_name.c_str(),
        60 * PNP_TO_TAPE_SPEED,
        px, py, tape->height() + PNP_Z_HOVERING,     // component pos.
        PNP_ANGLE_FACTOR * fmod(tape->angle(), 360.0),   // pickup angle
        tape->height(),                              // down to component
        travel_height);                              // up for travel.
}

void GCodeMachine::PlacePart(const Part &part, const Tape *tape) {
    if (tape == NULL) return;
    const float board_thick = config_->board.top - config_->bed_level;
    const float travel_height = tape->height() + board_thick + PNP_Z_HOVERING;
    const std::string print_name = part.component_name + " ("
        + part.footprint + "@" + part.value + ")";

    // param: name, x, y, zup, a, zdown, zup
    SendFormattedCommands(
        gcode_place,
        print_name.c_str(),
        60 * PNP_TO_BOARD_SPEED,
        part.pos.x + config_->board.origin.x,
        part.pos.y + config_->board.origin.y,
        travel_height,
        PNP_ANGLE_FACTOR * fmod(part.angle - tape->angle() + 360, 360.0),
        tape->height() + board_thick - PNP_TAPE_THICK,
        travel_height);
}

 void GCodeMachine::Dispense(const Part &part, const Pad &pad) {
     // TODO: so this part looks like we shouldn't have to do it here.
     const float angle = 2 * M_PI * part.angle / 360.0;
     const float part_x = config_->board.origin.x + part.pos.x;
     const float part_y = config_->board.origin.y + part.pos.y;
     const float area = pad.size.w * pad.size.h;
     const float pad_x = pad.pos.x;
     const float pad_y = pad.pos.y;
     const float x = part_x + pad_x * cos(angle) - pad_y * sin(angle);
     const float y = part_y + pad_x * sin(angle) + pad_y * cos(angle);
     SendFormattedCommands(gcode_dispense_move,
                           part.component_name.c_str(), pad.name.c_str(),
                           DISP_MOVE_SPEED * 60,
                           x, y, config_->board.top + DISP_Z_HOVER_ABOVE);
     SendFormattedCommands(gcode_dispense_paste, DISP_DISPENSE_SPEED * 60,
                           config_->board.top + DISP_Z_DISPENSING_ABOVE,
                           init_ms_ + area * area_ms_, area,
                           config_->board.top + DISP_Z_SEPARATE_DROPLET_ABOVE);
}

void GCodeMachine::Finish() {
    SendFormattedCommands(gcode_finish);
}

void GCodeMachine::SendFormattedCommands(const char *format, ...) {
    char *buffer = NULL;
    va_list ap;
    va_start(ap, format);
    int len = vasprintf(&buffer, format, ap);
    va_end(ap);

    // Now we have a buffer that we can send line-by-line. The write function
    // is owned by the caller, so they can implement e.g. flow control.
    const char *pos = buffer;
    const char *end = buffer + len;
    while (pos < end) {
        const char *eol = strchrnul(pos, '\n');
        // If a formatting string does not have a \n at the end of a line,
        // this is not allowed as we want to send full lines to write_line_().
        assert(*eol != '\0');  // error in templates above.
        write_line_(pos, eol - pos + 1);
        pos = eol + 1;
    }
    free(buffer);
}
