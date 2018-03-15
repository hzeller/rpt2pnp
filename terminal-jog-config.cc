/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#include "terminal-jog-config.h"

#include <termios.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "machine-connection.h"

// Hovering above the measured value.
static const float kSafeHovering = 5.0;

class TerminalCanonicalSetter {
public:
    TerminalCanonicalSetter() {
        tcgetattr(STDIN_FILENO, &orig_);
        struct termios raw = orig_;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 1;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }
    ~TerminalCanonicalSetter() {
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
    }

private:
    struct termios orig_;
};

static const Part *FindPartClosestTo(const Board::PartList& list,
                                     const Position &pos) {
    const Part* result = nullptr;
    float closest = -1;
    for (const Part* part : list) {
        if (part->pads.empty()) continue;
        const float dist = Distance(part->pos, pos);
        if (closest < 0 || dist < closest) {
            result = part;
            closest = dist;
        }
    }
    return result;
}

    // Define this with empty, if you're not using gcc.
#define PRINTF_FMT_CHECK(fmt_pos, args_pos)   \
    __attribute__ ((format (printf, fmt_pos, args_pos)))

static void SendMachineLine(int machine_fd, const char *msg, ...)
    PRINTF_FMT_CHECK(2, 3);

static void SendMachineLine(int machine_fd, const char *format, ...) {
    char *buffer = NULL;
    va_list ap;
    va_start(ap, format);
    int len = vasprintf(&buffer, format, ap);
    va_end(ap);

    assert(buffer[len-1] == '\n');  // Always use \n in cmds
    write(machine_fd, buffer, len);
    WaitForOkAck(machine_fd);
    free(buffer);
}

// The following is, uhm, somewhat hacky with manual decoding of escape codes.
// We encode the special characters as negative numbers.
enum SpecialChars {
    KEY_U = 'u', SHIFT_KEY_U = 'U', CTRL_KEY_U = 'U' - '@',
    KEY_D = 'd', SHIFT_KEY_D = 'D', CTRL_KEY_D = 'D' - '@',
    CURSOR_UP = -65,
    CURSOR_DN = -66,
    CURSOR_RIGHT = -67,
    CURSOR_LEFT = -68,

    SHIFT_CURSOR_UP = -165,
    SHIFT_CURSOR_DN = -166,
    SHIFT_CURSOR_RIGHT = -167,
    SHIFT_CURSOR_LEFT = -168,

    CTRL_CURSOR_UP = -265,
    CTRL_CURSOR_DN = -266,
    CTRL_CURSOR_RIGHT = -267,
    CTRL_CURSOR_LEFT = -268,
};

static int getChar() {
    TerminalCanonicalSetter t;
    int c;
    while ((c = getchar()) == -1)
        ;
    if (c != 27)
        return c;
    c = getchar();
    if (c != '[')
        return 27;
    c = getchar();
    if (c == 49 && getchar() == 59) {
        if (getchar() == 53)
            return -(getchar() + 200);
        else
            return -(getchar() + 100);
    }
    return -c;
}

// Start out at given position and jog machine to where the actual positions
// are.
static bool JogTo(int machine_fd, Position *out, float *z) {
    const Position start_pos = *out;
    const float start_z = *z - kSafeHovering;
    const float kSmallJog = 0.1;
    const float kBigJog = 1.0;
    fprintf(stderr,
            "-----------------------------------------\n"
            "Cursor keys: move x/y on bed\n"
            "             U=needle up, D=needle down\n"
            "Default:     0.1mm steps\n"
            "+CTRL-Key:   1.0mm steps (FAST)\n"
            "-----------------------------------------\n");
    bool success = false;
    bool done = false;
    while (!done) {
        SendMachineLine(machine_fd, "G1 X%.3f Y%.3f Z%.3f\n",
                        out->x, out->y, *z);
        for (int i = 0; i < 50; ++i) write(STDERR_FILENO, "\x08", 1);
        const Position delta = *out - start_pos;
        fprintf(stderr, "Delta: (%.1f, %.1f) ; top-of-board: %.1f  ",
                delta.x, delta.y, *z - start_z);
        const int c = getChar();
        switch (c) {
        case KEY_U :
            *z += kSmallJog;
            break;
        case SHIFT_KEY_U: case CTRL_KEY_U:
            *z += kBigJog;
            break;

        case KEY_D :
            *z -= kSmallJog;
            break;
        case SHIFT_KEY_D:  case CTRL_KEY_D:
            *z -= kBigJog;
            break;

        case CURSOR_UP:
            out->y += kSmallJog;
            break;
        case SHIFT_CURSOR_UP: case CTRL_CURSOR_UP:
            out->y += kBigJog;
            break;

        case CURSOR_DN:
            out->y -= kSmallJog;
            break;
        case SHIFT_CURSOR_DN: case CTRL_CURSOR_DN:
            out->y -= kBigJog;
            break;

        case CURSOR_RIGHT:
            out->x += kSmallJog;
            break;
        case SHIFT_CURSOR_RIGHT: case CTRL_CURSOR_RIGHT:
            out->x += kBigJog;
            break;

        case CURSOR_LEFT:
            out->x -= kSmallJog;
            break;
        case SHIFT_CURSOR_LEFT: case CTRL_CURSOR_LEFT:
            out->x -= kBigJog;
            break;

        case 3:   // CTRL-C
        case 27:  // <ESCAPE> key
            success = false;
            done = true;
            break;

        case 'q':
        case 10: case 13:  // <RETURN> key.
            success = true;
            done = true;
            break;

        default:
            fprintf(stderr, "unexpected: %d\n", c);
        }
    }
    fprintf(stderr, "\n-----------------------------------------\n");
    SendMachineLine(machine_fd, "M84\n");
    if (!success) {
        fprintf(stderr, "Aborting requested.\n");
    }
    return success;
}

static void PrintPos(const char *msg, const Position &p) {
    fprintf(stderr, "%s(%.1f, %.1f)\n", msg, p.x, p.y);
}

bool TerminalJogConfig(const Board &board, int machine_fd, PnPConfig *config) {
    if (machine_fd < 0) {
        fprintf(stderr, "In order to do the jog ajustment, you need to "
                "be connected to the machine (-m option).\n");
        return false;
    }

    //PrintPos("Initial board origin from config: ", config->board.origin);

    SendMachineLine(machine_fd, "G28 Y0\n");
    // The Printrbot simple complains if its phantom probe is too close to board
    SendMachineLine(machine_fd, "G1 Y140\n");
    SendMachineLine(machine_fd, "G28 X0\n");
    SendMachineLine(machine_fd, "G28 Z0\n");
    SendMachineLine(machine_fd, "G1 Z%.1f\n", kSafeHovering);

    const Part *board_part = FindPartClosestTo(board.parts(), Position(0, 0));
    if (board_part == nullptr) {
        fprintf(stderr, "No part found with a pad\n");
        return false;
    }

    // TODO: find lowest left actually.

    Position pad_pos = config->board.origin +
        board_part->padAbsPos(board_part->pads[0]);
    fprintf(stderr, "Find pad '%s' of %s (%.1f, %.1f) and touch needle.\n",
            board_part->pads[0].name.c_str(),
            board_part->component_name.c_str(), pad_pos.x, pad_pos.y);
    Position new_pos = pad_pos;
    float z = config->board.top + kSafeHovering;
    if (!JogTo(machine_fd, &new_pos, &z))
        return false;

    // Set the new values.
    config->board.top = z;
    const Position delta = new_pos - pad_pos;
    config->board.origin = config->board.origin + delta;
    PrintPos("Delta to original: ", delta);

    board_part = FindPartClosestTo(board.parts(),
                                   Position(board.dimension().w,
                                            board.dimension().h));
    pad_pos = config->board.origin
        + board_part->padAbsPos(board_part->pads[0]);

    // We can't do rotation yet, so for now, just show what the top right
    // would be.
    // TODO: get top right position, from there calculate rotation.
    fprintf(stderr, "\nDemo: this is pad '%s' of %s (%.1f, %.1f)\n"
            "      If this doesn't match, please CTRL-C now and straighten\n"
            "      board to be perfectly square with the bed.\n",
            board_part->pads[0].name.c_str(),
            board_part->component_name.c_str(), pad_pos.x, pad_pos.y);

    SendMachineLine(machine_fd, "G1 Z%.3f\n", z + 10);
    SendMachineLine(machine_fd, "G1 X%.3f Y%.3f\n", pad_pos.x, pad_pos.y);
    SendMachineLine(machine_fd, "G1 Z%.3f\n", z);

    fprintf(stderr, "\n[ OK ? RETURN. Otherwise: CTRL-C]\n");

    getchar();
    SendMachineLine(machine_fd, "G1 Z%.3f\n", config->board.top+kSafeHovering);
    return true;
}
