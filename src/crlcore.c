//
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2024 Julia Nechaevskaya
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


#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "i_timer.h"
#include "z_zone.h"
#include "v_video.h"
#include "m_argv.h"
#include "m_misc.h"
#include "v_trans.h"
#include "w_wad.h"

#include "crlcore.h"
#include "crlvars.h"


// Backup.
jmp_buf CRLJustIncaseBuf;

CRL_Data_t CRLData;
CRL_Widgets_t CRLWidgets;

// Visplane storage.
#define MAXCOUNTPLANES 4096
static void*   _planelist[MAXCOUNTPLANES];
static size_t  _planesize;
static int     _numplanes;

#define DARKSHADE 8
#define DARKMASK  7

// [JN] For MAX visplanes handling:

fixed_t CRL_MAX_x;
fixed_t CRL_MAX_y;
fixed_t CRL_MAX_z;
angle_t CRL_MAX_ang;

// [JN] Frame-independend counters:

// Icon of Sin spitter target counter (32 in vanilla).
// Will be reset on level restart.
int CRL_brain_counter;

// MAXLINEANIMS counter (64 in vanilla).
// Will be reset on level restart. 
int CRL_lineanims_counter;

// MAXPLATS counter (30 in vanilla).
// Will be reset on level restart.
int CRL_plats_counter;

// MAXBUTTONS counter (16 in vanilla)
// Will be reset on level restart.
int CRL_buttons_counter;

// Powerup timers.
// Will be reset on level restart.
int CRL_invul_counter;
int CRL_invis_counter;
int CRL_rad_counter;
int CRL_amp_counter;

// FPS counter.
int CRL_fps;

// [JN] Imitate jump by Arch-Vile's attack.
// Do not modify buttoncode_t (d_event.h) for consistency.
boolean CRL_vilebomb = false;
// [JN] Allow airborne controls while using Arch-Vile fly.
boolean CRL_aircontrol = false;

// [JN] Demo warp from Crispy Doom.
int demowarp;

// [JN] Make KIS/time widgets translucent while in active Save/Load menu.
// Primary needed for nicer drawing below save page and date/time lines.
boolean savemenuactive = false;


// =============================================================================
//
//                            Initialization functions
//
// =============================================================================

// VP color table.
#define NUMPLANEBORDERCOLORS 16
static int _vptable[NUMPLANEBORDERCOLORS];

// HOM color table.
#define HOMCOUNT 256
static int _homtable[HOMCOUNT];
int CRL_homcolor;  // Color to use

// -----------------------------------------------------------------------------
// CRL_Init
// Initializes things.
// -----------------------------------------------------------------------------

void CRL_Init (void)
{
    unsigned char *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
    int i;

    // Make plane surface
    _planesize = SCREENAREA * sizeof(*CRLPlaneSurface);
    CRLPlaneSurface = Z_Malloc(_planesize, PU_STATIC, NULL);
    memset(CRLPlaneSurface, 0, _planesize);

    // [JN] Initialize HOM (RGBY) multi colors, but prevent
    // using too bright values by multiplying by 3, not by 4.

    for (i = 0; i < 64 ; i++)
    {
        _homtable[i]     = V_GetPaletteIndex(playpal, i*3,   0,   0);
        _homtable[i+64]  = V_GetPaletteIndex(playpal,   0, i*3,   0);
        _homtable[i+128] = V_GetPaletteIndex(playpal,   0,   0, i*3);
        _homtable[i+192] = V_GetPaletteIndex(playpal, i*3, i*3,   0);
    }

    // [JN] Initialise VP color table. Using V_GetPaletteIndex is better
    // than directly referencing palette indexes, so we can make colours
    // as palette-independent as possible and support other games.

    // LIGHT
    _vptable[0]  = V_GetPaletteIndex(playpal, 255, 183, 183); // yucky pink
    _vptable[1]  = V_GetPaletteIndex(playpal, 255,   0,   0); // red
    _vptable[2]  = V_GetPaletteIndex(playpal, 243, 115,  23); // orange
    _vptable[3]  = V_GetPaletteIndex(playpal, 255, 255,   0); // yellow
    _vptable[4]  = V_GetPaletteIndex(playpal, 119, 255, 111); // green
    _vptable[5]  = V_GetPaletteIndex(playpal, 143, 143, 255); // light blue (cyanish)
    _vptable[6]  = V_GetPaletteIndex(playpal,   0,   0, 203); // deep blue
    _vptable[7]  = V_GetPaletteIndex(playpal, 255,   0, 255); // yuck, magenta

    // DARK
    _vptable[8]  = V_GetPaletteIndex(playpal, 191,  91,  91);
    _vptable[9]  = V_GetPaletteIndex(playpal, 167,   0,   0);
    _vptable[10] = V_GetPaletteIndex(playpal, 167,  63,   0);
    _vptable[11] = V_GetPaletteIndex(playpal, 175, 123,  31);
    _vptable[12] = V_GetPaletteIndex(playpal,  47,  99,  35);
    _vptable[13] = V_GetPaletteIndex(playpal,  55,  55, 255);
    _vptable[14] = V_GetPaletteIndex(playpal,   0,   0,  83);
    _vptable[15] = V_GetPaletteIndex(playpal, 111,   0, 107);

    W_ReleaseLumpName("PLAYPAL");
}


// =============================================================================
//
//                             Drawing functions
//
// =============================================================================

// -----------------------------------------------------------------------------
// CRL_SetStaticLimits
//  [JN] Allows to toggle between vanilla and doom-plus static engine limits.
//  Called at game startup (R_Init) and on toggling in CRL menu.
// -----------------------------------------------------------------------------

char *CRL_LimitsName;
int CRL_MaxVisPlanes;
int CRL_MaxDrawSegs;
int CRL_MaxVisSprites;
int CRL_MaxOpenings;
int CRL_MaxPlats;
int CRL_MaxAnims;

void CRL_SetStaticLimits (char *name)
{
    if (crl_vanilla_limits)
    {
        CRL_LimitsName    = "VANILLA";
        CRL_MaxVisPlanes  = 128;
        CRL_MaxDrawSegs   = 256;
        CRL_MaxVisSprites = 128;
        CRL_MaxOpenings   = 20480;
        CRL_MaxPlats      = 30;
        CRL_MaxAnims      = 64;
    }
    else
    {
        CRL_LimitsName    = name;
        CRL_MaxVisPlanes  = 1024;
        CRL_MaxDrawSegs   = 2048;
        CRL_MaxVisSprites = 1024;
        CRL_MaxOpenings   = 65536;
        CRL_MaxPlats      = 7680;
        CRL_MaxAnims      = 16384;
    }
}

// -----------------------------------------------------------------------------
// CRL_ChangeFrame
//  Starts the rendering of a new CRL, resetting any values.
//  @param __err Frame error, 0 starts, < 0 ends OK, else renderer crashed.
// -----------------------------------------------------------------------------

uint8_t* CRLSurface = NULL;
void**   CRLPlaneSurface = NULL;

static int _frame;
static int _pulse;
static int _pulsewas;
static int _pulsestage;

void CRL_ChangeFrame (int __err)
{
    CRL_Data_t old;	
    int fact, lim;

    // Clear state on zero
    if (__err == 0)
    {
        // Frame up
        int ntframe = (_frame++) >> 1;

        // Old for max stats
        memmove(&old, &CRLData, sizeof(CRLData));
        memset(&CRLData, 0, sizeof(CRLData));

        // Clear old plane surface
        memset(CRLPlaneSurface, 0, _planesize);

        // Plane set
        memset(_planelist, 0, sizeof(_planelist));
        _numplanes = 0;

        // Change pulse
        if (_pulsestage == 0)
        {
            if (ntframe > _pulsewas + 35)
            {
                _pulsewas = ntframe;
                _pulse = 0;
                _pulsestage++;
            }
        }

        // Pulse up or down
        else if (_pulsestage == 1 || _pulsestage == 2)
        {
            if (_pulsestage == 1)
            {
                fact = 1;
                lim = DARKMASK;
            }
            else
            {
                fact = -1;
                lim = 0;
            }

            // Will change color
            if (ntframe > _pulsewas + 4)
            {
                _pulsewas = ntframe;
                _pulse += fact;
                if (_pulse == lim)
                    _pulsestage++;
                if (_pulsestage == 3)
                {
                    _pulsestage = 0;
                    _pulsewas = ntframe;
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// CRL_MarkPixelP
//  Mark pixel that was drawn.
//  @param __surface Target surface that gets it.
//  @param __what What was drawn here.
//  @param __drawp Where it was drawn.
// -----------------------------------------------------------------------------

void CRL_MarkPixelP (void** __surface, void* __what, void* __drawp)
{
    // Set surface to what
    __surface[(uintptr_t)__drawp - (uintptr_t)CRLSurface] = __what;
}

// -----------------------------------------------------------------------------
// CRL_ColorizeThisPlane
//  Colorize the current plane for same color on the view and the automap.
//  @param __pl Plane to colorize.
//  @return Plane color
// -----------------------------------------------------------------------------

static int CRL_ColorizeThisPlane (CRLPlaneData_t *__pl)
{
    // Initial color
    int id = 0;
    int shade = DARKSHADE * __pl->isf;

    // Above plane limit
    if (__pl->id >= 128)
    {
        shade += _pulse & DARKMASK;
    }
	
    // Make the colors consistent when running around to determine where
    // things come from
    // The best initial thing to do is use segs because most visplanes are
    // generated by the segs in view
    if (__pl->emitlineid >= 0)
    {
        id = __pl->emitlineid;
    }
    // Next the subsector, since not all visplanes may have a seg (like the
    // sky)
    else if (__pl->emitsubid >= 0)
    {
        id = __pl->emitsubid;
    }
    // Sectors can have tons of visplanes, so just use the ID
    else
    {
        id = __pl->id;
    }

    // Return color
    return _vptable[id % NUMPLANEBORDERCOLORS];
}

// -----------------------------------------------------------------------------
// CRL_DrawVisPlanes
//  Draw visplanes (underlay or overlay).
// -----------------------------------------------------------------------------

void CRL_DrawVisPlanes (int __over)
{
    int isover, x, y, i, isbord, c;
    void* is;
    CRLPlaneData_t pd;

    // Get visplane drawing mode

    // Drawing nothing
    if (crl_visplanes_drawing == 0)
    {
        return;
    }

    // Overlay but not overlaying?
    isover = (crl_visplanes_drawing == 2 || crl_visplanes_drawing == 4);
    if (__over != isover)
    {
        return;
    }

    // Border colors
    isbord = (crl_visplanes_drawing == 3 || crl_visplanes_drawing == 4);

    // Go through all pixels and draw visplane if one is there
    memset(&pd, 0, sizeof(pd));
    for (i = 0, x = 0, y = 0; i < SCREENAREA; i++, x++)
    {
        // Increase y
        if (x >= SCREENWIDTH)
        {
            x = 0;
            y++;
        }

        // Get plane drawn here
        if (!(is = CRLPlaneSurface[i]))
        {
            continue;
        }

        // Border check
        if (isbord)
            if (x > 0 && x < SCREENWIDTH - 1 && y > 0 && y < SCREENHEIGHT - 1
                && (is == CRLPlaneSurface[i - 1] &&
                is == CRLPlaneSurface[i + 1] &&
                is == CRLPlaneSurface[i - SCREENWIDTH] &&
                is == CRLPlaneSurface[i + SCREENWIDTH]))
            continue;

        // Get plane identity
        GAME_IdentifyPlane(is, &pd);

        // Color the plane
        c = CRL_ColorizeThisPlane(&pd);

        // Draw plane colors
        CRLSurface[i] = c;
    }
}

// -----------------------------------------------------------------------------
// CRL_CountPlane
//  Counts visplane.
//  @param __key Visplane ID.
//  @param __chorf 0 = R_CheckPlane, 1 = R_FindPlane
//  @param __id ID Number.
// -----------------------------------------------------------------------------

void CRL_CountPlane (void* __key, int __chorf, int __id)
{
    // Update count
    if (__chorf == 0)
    {
        CRLData.numcheckplanes++;
    }
    else
    {
        CRLData.numfindplanes++;
    }

    // Add to global list
    if (_numplanes < MAXCOUNTPLANES)
    {
        _planelist[_numplanes++] = __key;
    }
}

// -----------------------------------------------------------------------------
// CRL_GetHOMMultiColor
//  [JN] Framerate-independent HOM multi coloring. Called in G_Ticker.
// -----------------------------------------------------------------------------

void CRL_GetHOMMultiColor (void)
{
    static int tic;

    CRL_homcolor = _homtable[(++tic) & (HOMCOUNT - 1)];
}


// =============================================================================
//
//                                 Automap
//
// =============================================================================


// -----------------------------------------------------------------------------
// CRL_DrawMap
//  Draws the automap.
//  @param __fl Normal line.
//  @param __ml Map line.
// -----------------------------------------------------------------------------

void CRL_DrawMap(void (*__fl)(int, int, int, int, int),
                 void (*__ml)(int, int, int, int, int))
{
    int i, c, j;
    CRLPlaneData_t pd;
    CRLSegData_t sd;
    CRLSubData_t ud;

    // Visplane emitting segs
    if (crl_automap_mode == 1 || crl_automap_mode == 2)
    {
        // Go through all planes
        for (i = 0; i < _numplanes; i++)
        {
            // Identify this plane
            GAME_IdentifyPlane(_planelist[i], &pd);

            // Floor/ceiling mismatch
            if ((!!(crl_automap_mode == 1)) != (!!pd.onfloor))
            {
                continue;
            }

            // Color the plane
            c = CRL_ColorizeThisPlane(&pd);

            // Has an emitting line
            if (pd.emitline)
            {
                // Identify it
                GAME_IdentifySeg(pd.emitline, &sd);

                // Draw its position as some color
                __ml(c, sd.coords[0], sd.coords[1],
                     sd.coords[2], sd.coords[3]);
            }
            // Has an emitting subsector, but no line
            else if (pd.emitsub)
            {
                // ID it
                GAME_IdentifySubSector(pd.emitsub, &ud);

                // Draw the subsector seg lines, the non implicit edges that
                // is.
                for (j = 0; j < ud.numlines; j++)
                {
                    // Id seg
                    GAME_IdentifySeg(ud.lines[j], &sd);

                    // Draw it
                    __ml(c, sd.coords[0], sd.coords[1],
                        sd.coords[2], sd.coords[3]);
                }
            }
        }
    }
}


// =============================================================================
//
//                              Spectator Mode
//
// =============================================================================

// Camera position and orientation.
fixed_t CRL_camera_x, CRL_camera_y, CRL_camera_z;
angle_t CRL_camera_ang;

// [JN] An "old" position and orientation used for interpolation.
fixed_t CRL_camera_oldx, CRL_camera_oldy, CRL_camera_oldz;
angle_t CRL_camera_oldang;

// -----------------------------------------------------------------------------
// CRL_GetCameraPos
//  Returns the camera position.
// -----------------------------------------------------------------------------

void CRL_GetCameraPos (fixed_t *x, fixed_t *y, fixed_t *z, angle_t *a)
{
    *x = CRL_camera_x;
    *y = CRL_camera_y;
    *z = CRL_camera_z;
    *a = CRL_camera_ang;
}

// -----------------------------------------------------------------------------
// CRL_ReportPosition
//  Reports the position of the camera.
//  @param x The x position.
//  @param y The y position.
//  @param z The z position.
//  @param angle The angle used.
// -----------------------------------------------------------------------------

void CRL_ReportPosition (fixed_t x, fixed_t y, fixed_t z, angle_t angle)
{
	CRL_camera_oldx = x;
	CRL_camera_oldy = y;
	CRL_camera_oldz = z;
	CRL_camera_oldang = angle;
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCamera
//  @param fwm Forward movement.
//  @param swm Sideways movement.
//  @param at Angle turning.
// -----------------------------------------------------------------------------

void CRL_ImpulseCamera (fixed_t fwm, fixed_t swm, angle_t at)
{
    // Rotate camera first
    CRL_camera_ang += at << FRACBITS;

    // Forward movement
    at = CRL_camera_ang >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(fwm * 32768, finecosine[at]); 
    CRL_camera_y += FixedMul(fwm * 32768, finesine[at]);

    // Sideways movement
    at = (CRL_camera_ang - ANG90) >> ANGLETOFINESHIFT;
    CRL_camera_x += FixedMul(swm * 32768, finecosine[at]); 
    CRL_camera_y += FixedMul(swm * 32768, finesine[at]);
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCameraVert
//  [JN] Impulses the camera up/down.
//  @param direction: true = up, false = down.
//  @param intensity: 32 of 64 map unit, depending on player run mode.
// -----------------------------------------------------------------------------

void CRL_ImpulseCameraVert (boolean direction, fixed_t intensity)
{
    if (direction)
    {
        CRL_camera_z += FRACUNIT*intensity;
    }
    else
    {
        CRL_camera_z -= FRACUNIT*intensity;
    }
}


// =============================================================================
//
//                  Critical message, console output coloring
//
// =============================================================================

// -----------------------------------------------------------------------------
// CRL_SetMessageCritical
// [JN] Sets critical message parameters.
//
// CRL's critical message is always taking second and third HUD line.
// Unlike common messages, these ones are not binded to player_t structure
// and may appear independently from any game states and conditions.
// -----------------------------------------------------------------------------

const char *messageCritical1;
const char *messageCritical2;
int         messageCriticalTics;

void CRL_SetMessageCritical (char *message1, char *message2, const int tics)
{
    messageCritical1 = message1;
    messageCritical2 = message2;
    messageCriticalTics = tics;
}

// -----------------------------------------------------------------------------
// CRL_printf
//  [JN] Prints colored message on Windows OS.
//  On other OSes just using standard, uncolored printf.
//  @param message: message to be printed.
//  @param critical: if true, use red color, else use yellow color.
// -----------------------------------------------------------------------------

void CRL_printf (const char *message, const boolean critical)
{
#ifdef _WIN32
    // Colorize text, depending on given type (critical or not).
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), critical ?
                            (FOREGROUND_RED | FOREGROUND_INTENSITY) :
                            (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY));
#endif

    // Print message, obviously.
    printf ("%s\n", message);

#ifdef _WIN32
    // Clear coloring, fallback to standard gray color.
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 
                            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
#endif
}
