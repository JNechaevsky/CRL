//
// Copyright(C) 2011-2017 RestlessRodent
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

#include <setjmp.h>
#include <stdint.h>
#include <signal.h>

#include "doomtype.h"
#include "m_fixed.h"
#include "i_video.h"
#include "tables.h"

#define BETWEEN(l,u,x) (((l)>(x))?(l):((x)>(u))?(u):(x))

#define singleplayer (!demorecording && !demoplayback && !netgame)


//
// Data types
//

// Why a jump failed.
enum
{
    CRL_JUMP_UNKNOWN = 1,  // Not known.
    CRL_JUMP_VPO,          // Visplane overflow.
};

// Drawing data structures.
typedef struct CRL_Data_s
{
    int numsprites;     // [JN] Number of sprites.
    int numsegs;        // [JN] Number of wall segments.
    int numsolidsegs;   // [JN] Number of solid segs.
    int numcheckplanes; // Number of check planes.
    int numfindplanes;  // Number of find planes.
    int numopenings;    // [JN] Number of openings.
} CRL_Data_t;

// Visplane information
typedef struct CRLPlaneData_s
{
	int   id;          // ID number.
	int   isf;         // C or F plane?
	void *emitline;    // Identifying line.
	int   emitlineid;  // The ID of the line.
	void *emitsub;     // Identifying subsector.
	int   emitsubid;   // The ID of the subsector.
	void *emitsect;    // The emitting sector.
	int   emitsectid;  // The ID of the sector (of the subsector).
	int   onfloor;     // Is on floor.
} CRLPlaneData_t;

// Segment data.
typedef struct CRLSegData_s
{
    int id;         // Identity.
	int coords[4];  // Coordinates.
} CRLSegData_t;

// Subsector data.
#define MAXCRLSUBSEGS 128
typedef struct CRLSubData_s
{
	int id;                      // Id number.
	int numlines;                // Count of segs.
	void* lines[MAXCRLSUBSEGS];  // Seg data.
} CRLSubData_t;

// Rectangle.
// @since 2015/12/19
typedef struct CRLRect_s
{
    fixed_t x, y, w, h;
} CRLRect_t;

// Backup.
extern jmp_buf CRLJustIncaseBuf;

// Data info.
extern CRL_Data_t CRLData;

// Screen surface.
extern uint8_t* CRLSurface;

// Visplane surface.
extern void** CRLPlaneSurface;

// [JN] Widgets data. 
typedef struct CRL_Widgets_s
{
    int x;    // Player X coord
    int y;    // Player Y coord
    int ang;  // Player angle

    int time; // Time spent on the level.

    int kills;         // Current kill count
    int totalkills;    // Total enemy count on the level
    int items;         // Current items count
    int totalitems;    // Total item count on the level
    int secrets;       // Current secrets count
    int totalsecrets;  // Total secrets on the level

    int frags_g;       // Frags counter of green player
    int frags_i;       // Frags counter of indigo player
    int frags_b;       // Frags counter of brown player
    int frags_r;       // Frags counter of red player
} CRL_Widgets_t;

extern CRL_Widgets_t CRLWidgets;

//
// Drawing functions
//

extern void CRL_SetStaticLimits (char *name);
extern char *CRL_LimitsName;
extern int  CRL_MaxVisPlanes;
extern int  CRL_MaxDrawSegs;
extern int  CRL_MaxVisSprites;
extern int  CRL_MaxOpenings;
extern int  CRL_MaxPlats;
extern int  CRL_MaxAnims;

// [AM] Fractional part of the current tic, in the half-open
//      range of [0.0, 1.0).  Used for interpolation.
extern fixed_t fractionaltic;

extern void CRL_Init (int* __colorset, int __numcolors, int __pllim);
extern void CRL_ChangeFrame (int __err);
extern void CRL_MarkPixelP (void** __surface, void* __what, void* __drawp);
extern void CRL_DrawVisPlanes (int __over);
extern void CRL_CountPlane (void* __key, int __chorf, int __id);
extern void CRL_GetHOMMultiColor (void);
extern int  CRL_homcolor;

extern void GAME_IdentifyPlane (void* __what, CRLPlaneData_t* __info);
extern void GAME_IdentifySeg (void* __what, CRLSegData_t* __info);
extern void GAME_IdentifySubSector (void* __what, CRLSubData_t* __info);

//
// Automap
//

extern void CRL_DrawMap(void (*__fl)(int, int, int, int, int),
                        void (*__ml)(int, int, int, int, int));

//
// Spectator Mode
//

extern fixed_t CRL_camera_x, CRL_camera_y, CRL_camera_z;
extern fixed_t CRL_camera_oldx, CRL_camera_oldy, CRL_camera_oldz;
extern angle_t CRL_camera_ang;
extern angle_t CRL_camera_oldang;

extern void CRL_GetCameraPos (fixed_t *x, fixed_t *y, fixed_t *z, angle_t *a);
extern void CRL_ReportPosition (fixed_t x, fixed_t y, fixed_t z, angle_t angle);
extern void CRL_ImpulseCamera(fixed_t fwm, fixed_t swm, angle_t at);
extern void CRL_ImpulseCameraVert(boolean direction, fixed_t intensity);

//
// Render Counters and Widgets
//

extern void CRL_StatDrawer (void);

//
// HSV Coloring routines
//

extern void CRL_SetColors (uint8_t* colors, void* ref);

//
// Console output coloring
//

extern void CRL_printf (const char *message, const boolean critical);

//
// The rest of externals
//

extern void CRL_SetCriticalMessage (char *message1, char *message2, const int tics);

extern void CRL_WidgetsDrawer (void);
extern void CRL_ReloadPalette (void);

extern fixed_t CRL_MAX_x;
extern fixed_t CRL_MAX_y;
extern fixed_t CRL_MAX_z;
extern angle_t CRL_MAX_ang;

extern int  CRL_lineanims_counter;
extern int  CRL_plats_counter;
extern int  CRL_brain_counter;
extern int  CRL_buttons_counter;

extern int CRL_invul_counter;
extern int CRL_invis_counter;
extern int CRL_rad_counter;
extern int CRL_amp_counter;

extern void CRL_DrawFPS (void);
extern int  CRL_fps;

extern boolean CRL_vilebomb;

// [crispy] demo progress bar and timer widget
extern int  demowarp;

extern boolean  menuactive;
extern boolean  automapactive;

extern int      deathmatch;      // 0 = Cooperative; 1 = Deathmatch; 2 = Altdeath
extern boolean  playeringame[4]; // [MAXPLAYERS]

