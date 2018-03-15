/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 * (c) h.zeller@acm.org. Free Software. GNU Public License v3.0 and above
 */

#ifndef TERMINAL_JOG_CONFIG_H
#define TERMINAL_JOG_CONFIG_H

#include "board.h"
#include "pnp-config.h"

// Provide a very simple UI on the terminal to jog the machine to
// various places on the board to determine origins.
// Modifies config.
// Returns 'true' if the user accepts the resulting config.
bool TerminalJogConfig(const Board &board, int machine_fd, PnPConfig *config);

#endif // TERMINAL_JOG_CONFIG_H
