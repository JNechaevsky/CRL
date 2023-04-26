//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2018-2023 Julia Nechaevskaya
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

#pragma once

#include "doomdef.h"
#include "v_patch.h"


#define HU_FONTSTART    '!'	// the first font characters
#define HU_FONTEND      '_'	// the last font characters

// Calculate # of glyphs in font.
#define HU_FONTSIZE (HU_FONTEND - HU_FONTSTART + 1)	


void CT_Ticker (void);
void CT_Drawer (void);

extern patch_t *hu_font[HU_FONTSIZE];

extern boolean chatmodeon;
extern char *chat_macros[10];
extern char *player_names[];

extern void CT_Init (void);
