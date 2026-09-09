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
//  [PN] Automap block-trail: leveltime stamps of blockmap cells that the
//  engine has recently queried, drawn white by the automap grid when
//  crl_automap_blocks is on.
//

#pragma once

#include "doomtype.h"
#include "m_fixed.h"

// Stamp array, bmapwidth * bmapheight unsigned ints (NULL before first level)
extern unsigned int *blocktouch;

void P_TrailAlloc (void);                 // allocate + clear, called from P_SetupLevel
void P_TrailTouch (int x, int y);         // direct (non-trace) block visit
void P_TrailBegin (void);                 // P_PathTraverse start marker
void P_TrailFinish (fixed_t x1, fixed_t y1,
                    fixed_t dx, fixed_t dy); // P_PathTraverse end: stamps the
                                             // cells up to the noted stop point
void P_TrailStopHere (fixed_t frac);      // note where a trace got blocked
boolean P_TrailStopFalse (fixed_t frac);  // note + return false
