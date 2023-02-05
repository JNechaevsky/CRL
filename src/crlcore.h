// -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-
// ----------------------------------------------------------------------------
// Chocorenderlimits 2.0 <http://remood.org/?page=chocorenderlimits>
//   Copyright (C) 2014-2015 GhostlyDeath <ghostlydeath@remood.org>
//     For more credits, see AUTHORS.
// ----------------------------------------------------------------------------
// CRL is under the GNU General Public License v2 (or later), see COPYING.
// ----------------------------------------------------------------------------

/**
 * Core CRL Code that is shared for all game engines.
 */

#ifndef __CRLCORE_H__
#define __CRLCORE_H__

/*****************************************************************************/

#include <setjmp.h>
#include <stdint.h>
#include <signal.h>

#include "doomtype.h"  // [JN] boolean
#include "m_fixed.h"

#ifndef SCREENWIDTH
	#define SCREENWIDTH 320
#endif
#ifndef SCREENHEIGHT
	#define SCREENHEIGHT 200
#endif
#ifndef SCREENAREA
	#define SCREENAREA (SCREENWIDTH * SCREENHEIGHT)
#endif

#ifndef BETWEEN
	#define BETWEEN(l,u,x) (((l)>(x))?(l):((x)>(u))?(u):(x))
#endif

/**
 * Why a jump failed.
 */
enum
{
	/** Not known. */
	CRL_JUMP_UNKNOWN = 1,
	
	/** Visplane overflow. */
	CRL_JUMP_VPO,
};

/**
 * Drawing data structures.
 */
typedef struct CRL_Data_s
{
	/** [JN] Number of sprites. */
	int numsprites;

	/** [JN] Number of wall segments. */
	int numsegs;

	/** Number of check planes. */
	int numcheckplanes;
	
	/** Number of find planes. */
	int numfindplanes;

	/** [JN] Number of openings. */
	int numopenings;
} CRL_Data_t;

/**
 * Visplane information.
 */
typedef struct CRLPlaneData_s
{
	/** ID number. */
	int id;
	
	/** C or F plane?. */
	int isf;
	
	/** Identifying line. */
	void* emitline;
	
	/** The ID of the line. */
	int emitlineid;
	
	/** Identifying subsector. */
	void* emitsub;
	
	/** The ID of the subsector. */
	int emitsubid;
	
	/** The emitting sector. */
	void* emitsect;
	
	/** The ID of the sector (of the subsector). */
	int emitsectid;
	
	/** Is on floor. */
	int onfloor;
} CRLPlaneData_t;

/**
 * Segment data.
 */
typedef struct CRLSegData_s
{
	/** Identity. */
	int id;
	
	/** Coordinates. */
	int coords[4];
} CRLSegData_t;

/**
 * Subsector data.
 */
#define MAXCRLSUBSEGS 128
typedef struct CRLSubData_s
{
	/** Id number. */
	int id;
	
	/** Count of segs. */
	int numlines;
	
	/** Seg data. */
	void* lines[MAXCRLSUBSEGS];
} CRLSubData_t;

/**
 * Rectangle.
 *
 * @since 2015/12/19
 */
typedef struct CRLRect_s
{
	fixed_t x, y, w, h;
} CRLRect_t;

/** Backup. */
extern jmp_buf CRLJustIncaseBuf;

/** Data info. */
extern CRL_Data_t CRLData;

/** Screen surface. */
extern uint8_t* CRLSurface;

/** Visplane surface. */
extern void** CRLPlaneSurface;

/** Brute forcing the state? */
extern int CRLBruteForce;

/*****************************************************************************/

void CRL_Init(int* __colorset, int __numcolors, int __pllim);
void CRL_ReportPosition(fixed_t x, fixed_t y, fixed_t z, uint32_t angle);
void CRL_OutputReport(void);
void CRL_SetColors(uint8_t* colors, void* ref);
void CRL_ChangeFrame(int __err);
void CRL_CountPlane(void* __key, int __chorf, int __id);
void CRL_StatDrawer(void);
void CRL_MarkPixelP(void** __surface, void* __what, void* __drawp);
void CRL_DrawVisPlanes();
void CRL_ViewDrawer();
void CRL_DrawMap(void (*__fl)(int, int, int, int, int),
	void (*__ml)(int, int, int, int, int));

int CRL_IsSpectating(void);
void CRL_ImpulseCamera(int32_t fwm, int32_t swm, uint32_t at);
void CRL_ImpulseCameraVert(boolean direction, const int32_t intensity);
void CRL_GetCameraPos(int32_t* x, int32_t* y, int32_t* z, uint32_t* a);

int CRL_MaxVisPlanes(void);

void CRL_DrawHOMBack(int __x, int __y, int __w, int __h);

void CRL_BruteForceLoop(void);


CRLRect_t CRL_MapSize(void);

void GAME_IdentifyPlane(void* __what, CRLPlaneData_t* __info);
void GAME_IdentifySeg(void* __what, CRLSegData_t* __info);
void GAME_IdentifySubSector(void* __what, CRLSubData_t* __info);

/*****************************************************************************/

// [JN] Widgets data. 
typedef struct CRL_Widgets_s
{
    int kills;         // Current kill count
    int totalkills;    // Total enemy count on the level
    int items;         // Current items count
    int totalitems;    // Total item count on the level
    int secrets;       // Current secrets count
    int totalsecrets;  // Total secrets on the level

    int time; // Time spent on the level.

    int x;    // Player X coord
    int y;    // Player Y coord
    int z;    // Player Z coord
    int ang;  // Player angle
} CRL_Widgets_t;

extern CRL_Widgets_t CRLWidgets;


// [JN] External data.
extern void M_WriteText (int x, int y, const char *string, byte *table);
extern void M_WriteTextCentered (const int y, const char *string, byte *table);
extern int  M_StringWidth (const char *string);

extern void CRL_WidgetsDrawer (void);
extern void CRL_ReloadPalette (void);

extern boolean  CRL_intercepts_overflow;
extern int      CRL_lineanims_counter;
extern int      CRL_plats_counter;
extern int      CRL_brain_counter;

// [crispy] demo progress bar and timer widget
extern void CRL_DemoTimer (const int time);
extern void CRL_DemoBar (void);
extern int  defdemotics, deftotaldemotics;

extern boolean  menuactive;
extern boolean  automapactive;
extern int      screenblocks;



#endif /* __CRLCORE_H__ */

