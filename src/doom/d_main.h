//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	System specific interface stuff.
//


#ifndef __D_MAIN__
#define __D_MAIN__

#include "doomdef.h"




// Read events from all input devices

void D_ProcessEvents (void); 
	

//
// BASE LEVEL
//
extern void D_DoomLoop (void);
extern void D_ConnectNetGame (void);
extern void D_CheckNetGame (void);
extern void D_PageTicker (void);
extern void D_PageDrawer (void);
extern void D_AdvanceDemo (void);
extern void D_DoAdvanceDemo (void);
extern void D_StartTitle (void);
 
//
// GLOBAL VARIABLES
//

extern  int show_endoom;
extern  gameaction_t    gameaction;
extern boolean advancedemo;


#endif

