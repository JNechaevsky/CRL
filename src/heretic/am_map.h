//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2025 Julia Nechaevskaya
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
#ifndef __AMMAP_H__
#define __AMMAP_H__


#include "r_local.h"


typedef struct
{
    int64_t x, y;
} mpoint_t;

extern int64_t m_x, m_y;
extern fixed_t AM_UnArchiveScaleMtof (void);

extern int am_followplayer;
extern int ravmap_cheating;
extern int am_grid;
extern angle_t mapangle;

#define AM_NUMMARKPOINTS 10
extern int markpointnum;
extern int markpointnum_max;
extern mpoint_t markpoints[AM_NUMMARKPOINTS];

extern vertex_t KeyPoints[];
extern const char *LevelNames[];

extern void AM_ArchiveScaleMtof (fixed_t scale);
extern void AM_Init (void);
extern void AM_initOverlayMode (void);
extern void AM_SetMapCenter (fixed_t x, fixed_t y);
extern void AM_Start (void);
extern void AM_Stop (void);


//
// Automap colors:
//

// Common walls
#define WALLCOLORS      96
#define TSWALLCOLORS    40
#define FDWALLCOLORS    112
#define CDWALLCOLORS    80

// Hidden lines
#define MLDONTDRAW1     40
#define MLDONTDRAW2     43

// Teleporters
#define TELEPORTERS     156

// Exits
#define EXITS           182

// Players (no antialiasing)
#define PL_WHITE        32
#define PL_GREEN        221
#define PL_YELLOW       241
#define PL_RED          160
#define PL_BLUE         198

// Grid (no antialiasing)
#define GRIDCOLORS      39

// Things
#define THINGCOLORS     38

// Keys
#define BLUEKEY         197
#define YELLOWKEY       144
#define GREENKEY        220



#endif





