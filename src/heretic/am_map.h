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


// For use if I do walls with outsides/insides
#define REDS		12*8
#define REDRANGE	1       //16
#define BLUES		(256-4*16+8)
#define BLUERANGE	1       //8
#define GREENS		(33*8)
#define GREENRANGE	1       //16
#define GRAYS		(5*8)
#define GRAYSRANGE	1       //16
#define BROWNS		(14*8)
#define BROWNRANGE	1       //16
#define YELLOWS		10*8
#define YELLOWRANGE	1
#define BLACK		0
#define WHITE		4*8
#define PARCH		13*8-1
#define BLOODRED  150
#define BLUEKEY 	197
#define YELLOWKEY 144
#define GREENKEY  220

// Automap colors
#define BACKGROUND	PARCH
#define YOURCOLORS	WHITE
#define YOURRANGE	0
#define WALLCOLORS	REDS
#define WALLRANGE	REDRANGE
#define TSWALLCOLORS	GRAYS
#define TSWALLRANGE	GRAYSRANGE
#define FDWALLCOLORS	BROWNS
#define FDWALLRANGE	BROWNRANGE
#define CDWALLCOLORS	YELLOWS
#define CDWALLRANGE	YELLOWRANGE
#define THINGCOLORS	GREENS
#define THINGRANGE	GREENRANGE
#define SECRETWALLCOLORS WALLCOLORS
#define SECRETWALLRANGE WALLRANGE
#define GRIDCOLORS	(GRAYS + GRAYSRANGE/2)
#define GRIDRANGE	0
#define XHAIRCOLORS	GRAYS

// drawing stuff
#define	FB		0

#define AM_NUMMARKPOINTS 10

typedef struct
{
    int64_t x, y;
} mpoint_t;

extern int am_followplayer;

extern void AM_Init (void);
extern fixed_t AM_UnArchiveScaleMtof (void);
extern void AM_ArchiveScaleMtof (fixed_t scale);
void AM_SetMapCenter (fixed_t x, fixed_t y);

//extern boolean automapactive;
extern vertex_t KeyPoints[];
extern const char *LevelNames[];
extern int ravmap_cheating;

extern void AM_initOverlayMode (void);
extern void AM_Start (void);
extern void AM_Stop (void);


#endif
