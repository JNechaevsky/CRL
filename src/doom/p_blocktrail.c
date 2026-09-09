//
// Copyright(C) 2026 Julia Nechaevskaya
// Copyright(C) 2026 Polina "Aura" N.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//  [PN] Automap block-trail implementation (see p_blocktrail.h).
//

#include <stdlib.h>
#include <string.h>

#include "z_zone.h"
#include "doomstat.h"
#include "p_local.h"
#include "crlvars.h"

#include "p_blocktrail.h"

unsigned int *blocktouch;

// Trail length in tics; 8 ≈ 0.23 s reads as a tracer, 35 (TICRATE) is too much.
#define BLOCKTOUCH_TIME (leveltime + 8)

// While a P_PathTraverse runs, its direct cell visits are NOT stamped: the
// trace stamps only up to the distance where it actually got blocked (mode 1).
static boolean in_pathtraverse = false;
static fixed_t traverse_stopfrac = FRACUNIT;

void P_TrailAlloc (void)
{
    const size_t bytes = sizeof(*blocktouch) * bmapwidth * bmapheight;

    blocktouch = Z_Malloc(bytes, PU_LEVEL, 0);
    memset(blocktouch, 0, bytes);
}

void P_TrailTouch (int x, int y)
{
    if (crl_automap_blocks == 1 && !in_pathtraverse && blocktouch)
    {
        blocktouch[y * bmapwidth + x] = BLOCKTOUCH_TIME;
    }
}

void P_TrailBegin (void)
{
    traverse_stopfrac = FRACUNIT;
    in_pathtraverse = true;
}

void P_TrailStopHere (fixed_t frac)
{
    if (frac < traverse_stopfrac)
    {
        traverse_stopfrac = frac;
    }
}

boolean P_TrailStopFalse (fixed_t frac)
{
    P_TrailStopHere(frac);
    return false;
}

void P_TrailFinish (fixed_t x1, fixed_t y1, fixed_t dx, fixed_t dy)
{
    fixed_t stop;
    int x, y, x2, y2, w, h, sx, sy, err, steps;

    in_pathtraverse = false;

    if (crl_automap_blocks != 1 || !blocktouch || bmapwidth <= 0 || bmapheight <= 0)
    {
        return;
    }

    if (traverse_stopfrac < 0)
    {
        traverse_stopfrac = 0;
    }
    else if (traverse_stopfrac > FRACUNIT)
    {
        traverse_stopfrac = FRACUNIT;
    }
    stop = traverse_stopfrac;

    x  = (int)((x1 - bmaporgx) >> MAPBLOCKSHIFT);
    y  = (int)((y1 - bmaporgy) >> MAPBLOCKSHIFT);
    x2 = (int)((x1 + FixedMul(dx, stop) - bmaporgx) >> MAPBLOCKSHIFT);
    y2 = (int)((y1 + FixedMul(dy, stop) - bmaporgy) >> MAPBLOCKSHIFT);

    w = abs(x2 - x);
    h = abs(y2 - y);
    sx = x < x2 ? 1 : -1;
    sy = y < y2 ? 1 : -1;
    err = w - h;

    // Bounded the same way as the original block-stepping loop.
    for (steps = 0; steps < 64; steps++)
    {
        if (x >= 0 && y >= 0 && x < bmapwidth && y < bmapheight)
        {
            blocktouch[y * bmapwidth + x] = BLOCKTOUCH_TIME;
        }

        if (x == x2 && y == y2)
        {
            break;
        }

        {
            const int e2 = 2 * err;

            if (e2 > -h)
            {
                err -= h;
                x += sx;
            }
            if (e2 < w)
            {
                err += w;
                y += sy;
            }
        }
    }
}
