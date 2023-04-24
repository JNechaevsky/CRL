//
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
#include "tables.h"
#include "v_trans.h"

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
static int     _maxplanes;

static int* _usecolors;
static int  _didcolors;
static int  _numcolors;

#define MAXSHADES 16
#define DARKSHADE 8
#define DARKMASK  7
static uint8_t _colormap[MAXSHADES][256];

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
boolean CRL_vilebomb;


// -----------------------------------------------------------------------------
// CRL_Init
// Initializes things.
// -----------------------------------------------------------------------------

void CRL_Init (int* __colorset, int __numcolors, int __pllim)
{
    // Make plane surface
    _planesize = SCREENAREA * sizeof(*CRLPlaneSurface);
    CRLPlaneSurface = Z_Malloc(_planesize, PU_STATIC, NULL);
    memset(CRLPlaneSurface, 0, _planesize);

    // Set colors
    _usecolors = __colorset;
    _numcolors = __numcolors;

    // Limits
    _maxplanes = __pllim;
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

void CRL_SetStaticLimits (void)
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
        CRL_LimitsName    = "DOOM+";
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
    if (__pl->id >= _maxplanes)
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
    return _colormap[shade][id & 0xFF];
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

// HOM color table.
#define HOMCOUNT 256
#define HOMQUAD (HOMCOUNT / 4)
static int _homtable[HOMCOUNT];
int CRL_homcolor;  // Color to use

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
static fixed_t  _campos[3];
static uint32_t _camang;

// -----------------------------------------------------------------------------
// CRL_GetCameraPos
//  Returns the camera position.
// -----------------------------------------------------------------------------

void CRL_GetCameraPos (int32_t* x, int32_t* y, int32_t* z, uint32_t* a)
{
    *x = _campos[0];
    *y = _campos[1];
    *z = _campos[2];
    *a = _camang;
}

// -----------------------------------------------------------------------------
// CRL_ReportPosition
//  Reports the position of the camera.
//  @param x The x position.
//  @param y The y position.
//  @param z The z position.
//  @param angle The angle used.
// -----------------------------------------------------------------------------

void CRL_ReportPosition (fixed_t x, fixed_t y, fixed_t z, uint32_t angle)
{
	_campos[0] = x;
	_campos[1] = y;
	_campos[2] = z;
	_camang = angle;
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCamera
//  @param fwm Forward movement.
//  @param swm Sideways movement.
//  @param at Angle turning.
// -----------------------------------------------------------------------------

void CRL_ImpulseCamera (int32_t fwm, int32_t swm, uint32_t at)
{
    // [JN] Don't move camera while active menu.
    if (menuactive)
    {
        return;
    }

    // Rotate camera first
    _camang += at << FRACBITS;

    // Forward movement
    at = _camang >> ANGLETOFINESHIFT;
    _campos[0] += FixedMul(fwm * 32768, finecosine[at]); 
    _campos[1] += FixedMul(fwm * 32768, finesine[at]);

    // Sideways movement
    at = (_camang - ANG90) >> ANGLETOFINESHIFT;
    _campos[0] += FixedMul(swm * 32768, finecosine[at]); 
    _campos[1] += FixedMul(swm * 32768, finesine[at]);
}

// -----------------------------------------------------------------------------
// CRL_ImpulseCameraVert
//  [JN] Impulses the camera up/down.
//  @param direction: true = up, false = down.
//  @param intensity: 32 of 64 map unit, depending on player run mode.
// -----------------------------------------------------------------------------

void CRL_ImpulseCameraVert (boolean direction, const int32_t intensity)
{
    // [JN] Don't move camera while active menu.
    if (menuactive)
    {
        return;
    }

    if (direction)
    {
        _campos[2] += FRACUNIT*intensity;
    }
    else
    {
        _campos[2] -= FRACUNIT*intensity;
    }
}


// =============================================================================
//
//                        Render Counters and Widgets
//
// =============================================================================

extern int gametic;

// -----------------------------------------------------------------------------
// CRL_StatColor_Str, CRL_StatColor_Val
//  [JN] Colorizes counter strings and values respectively.
// -----------------------------------------------------------------------------

static byte *CRL_StatColor_Str (const int val1, const int val2)
{
    return
        val1 == val2 ? cr[CR_LIGHTGRAY] :
        val1 >= val2 ? (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : 
                       cr[CR_GRAY];
}

static byte *CRL_StatColor_Val (const int val1, const int val2)
{
    return
        val1 == val2 ? cr[CR_YELLOW] :
        val1 >= val2 ? (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) :
                       cr[CR_GREEN];
}

static byte *CRL_PowerupColor (const int val1, const int val2)
{
    return
        val1 > val2/2 ? cr[CR_GREEN]  :
        val1 > val2/4 ? cr[CR_YELLOW] :
                        cr[CR_RED]    ;
}

// -----------------------------------------------------------------------------
// Draws CRL stats.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void CRL_StatDrawer (void)
{
    // Player coords
    if (crl_widget_coords)
    {
        char str[128];

        M_WriteText(0, 27, "X:", cr[CR_GRAY]);
        M_WriteText(0, 36, "Y:", cr[CR_GRAY]);
        M_WriteText(0, 45, "ANG:", cr[CR_GRAY]);

        sprintf(str, "%d", CRLWidgets.x);
        M_WriteText(16, 27, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.y);
        M_WriteText(16, 36, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.ang);
        M_WriteText(32, 45, str, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        // Icon of Sin spitter targets (32 max)
        if ((crl_widget_playstate == 1 
        ||  (crl_widget_playstate == 2 && CRL_brain_counter > 32)) && CRL_brain_counter)
        {
            char brn[32];

            M_WriteText(0, 63, "BRN:", CRL_StatColor_Str(CRL_brain_counter, 32));
            M_snprintf(brn, 16, "%d/32", CRL_brain_counter);
            M_WriteText(32, 63, brn, CRL_StatColor_Val(CRL_brain_counter, 32));
        }

        // Buttons (16 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_buttons_counter > 16))
        {
            char btn[32];

            M_WriteText(0, 72, "BTN:", CRL_StatColor_Str(CRL_buttons_counter, 16));
            M_snprintf(btn, 16, "%d/16", CRL_buttons_counter);
            M_WriteText(32, 72, btn, CRL_StatColor_Val(CRL_buttons_counter, 16));
        }

        // Plats (30 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            M_WriteText(0, 81, "PLT:", CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            M_WriteText(32, 81, plt, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            M_WriteText(0, 90, "ANI:", CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            M_WriteText(32, 90, ani, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
        }
    }

    // Render counters
    if (crl_widget_render)
    {
        // Sprites (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsprites >= CRL_MaxVisSprites))
        {
            char spr[32];

            M_WriteText(0, 108, "SPR:", CRL_StatColor_Str(CRLData.numsprites, CRL_MaxVisSprites));
            M_snprintf(spr, 16, "%d/%d", CRLData.numsprites, CRL_MaxVisSprites);
            M_WriteText(32, 108, spr, CRL_StatColor_Val(CRLData.numsprites, CRL_MaxVisSprites));
        }

        // Segments (256 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsegs >= CRL_MaxDrawSegs))
        {
            char seg[32];

            M_WriteText(0, 117, "SEG:", CRL_StatColor_Str(CRLData.numsegs, CRL_MaxDrawSegs));
            M_snprintf(seg, 16, "%d/%d", CRLData.numsegs, CRL_MaxDrawSegs);
            M_WriteText(32, 117, seg, CRL_StatColor_Val(CRLData.numsegs, CRL_MaxDrawSegs));
        }

        // Planes (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numcheckplanes + CRLData.numfindplanes >= CRL_MaxVisPlanes))
        {
            char vis[32];
            const int totalplanes = CRLData.numcheckplanes
                                  + CRLData.numfindplanes;

            M_WriteText(0, 126, "PLN:", totalplanes >= CRL_MaxVisPlanes ? 
                       (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d/%d", totalplanes, CRL_MaxVisPlanes);
            M_WriteText(32, 126, vis, totalplanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
        }

        // Openings
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numopenings >= CRL_MaxOpenings))
        {
            char opn[64];

            M_WriteText(0, 135, "OPN:", CRL_StatColor_Str(CRLData.numopenings, CRL_MaxOpenings));
            M_snprintf(opn, 16, "%d/%d", CRLData.numopenings, CRL_MaxOpenings);
            M_WriteText(32, 135, opn, CRL_StatColor_Val(CRLData.numopenings, CRL_MaxOpenings));
        }
    }

    // Level timer
    if (crl_widget_time)
    {
        extern int leveltime;
        const int time = leveltime / 35;
        char stra[8];
        char strb[16];
        const int yy = automapactive ? 8 : 0;

        sprintf(stra, "TIME ");
        M_WriteText(0, 152 - yy, stra, cr[CR_GRAY]);
 
        sprintf(strb, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        M_WriteText(0 + M_StringWidth(stra), 152 - yy, strb, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis)
    {
        const int yy = automapactive ? 8 : 0;

        if (!deathmatch)
        {
            char str1[8], str2[16];  // kills
            char str3[8], str4[16];  // items
            char str5[8], str6[16];  // secret

            // Kills:
            sprintf(str1, "K ");
            M_WriteText(0, 160 - yy, str1, cr[CR_GRAY]);
            
            sprintf(str2, "%d/%d ", CRLWidgets.kills, CRLWidgets.totalkills);
            M_WriteText(0 + M_StringWidth(str1), 160 - yy, str2,
                        CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                        CRLWidgets.kills == 0 ? cr[CR_RED] :
                        CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Items:
            sprintf(str3, "I ");
            M_WriteText(M_StringWidth(str1) + M_StringWidth(str2), 160 - yy, str3, cr[CR_GRAY]);
            
            sprintf(str4, "%d/%d ", CRLWidgets.items, CRLWidgets.totalitems);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3), 160 - yy, str4,
                        CRLWidgets.totalitems == 0 ? cr[CR_GREEN] :
                        CRLWidgets.items == 0 ? cr[CR_RED] :
                        CRLWidgets.items < CRLWidgets.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Secret:
            sprintf(str5, "S ");
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3) +
                        M_StringWidth(str4), 160 - yy, str5, cr[CR_GRAY]);

            sprintf(str6, "%d/%d ", CRLWidgets.secrets, CRLWidgets.totalsecrets);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) + 
                        M_StringWidth(str3) +
                        M_StringWidth(str4) +
                        M_StringWidth(str5), 160 - yy, str6,
                        CRLWidgets.totalsecrets == 0 ? cr[CR_GREEN] :
                        CRLWidgets.secrets == 0 ? cr[CR_RED] :
                        CRLWidgets.secrets < CRLWidgets.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN]);
        }
        else
        {
            char str1[8], str2[16];  // Green
            char str3[8], str4[16];  // Indigo
            char str5[8], str6[16];  // Brown
            char str7[8], str8[16];  // Red

            // Green
            if (playeringame[0])
            {
                sprintf(str1, "G ");
                M_WriteText(0, 160 - yy, str1, cr[CR_GREEN]);

                sprintf(str2, "%d ", CRLWidgets.frags_g);
                M_WriteText(M_StringWidth(str1), 160 - yy, str2, cr[CR_GREEN]);
            }
            // Indigo
            if (playeringame[1])
            {
                sprintf(str3, "I ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2),
                            160 - yy, str3, cr[CR_GRAY]);

                sprintf(str4, "%d ", CRLWidgets.frags_i);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3),
                            160 - yy, str4, cr[CR_GRAY]);
            }
            // Brown
            if (playeringame[2])
            {
                sprintf(str5, "B ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4),
                            160 - yy, str5, cr[CR_BROWN]);

                sprintf(str6, "%d ", CRLWidgets.frags_b);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5),
                            160 - yy, str6, cr[CR_BROWN]);
            }
            // Red
            if (playeringame[3])
            {
                sprintf(str7, "R ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6),
                            160 - yy, str7, cr[CR_RED]);

                sprintf(str8, "%d ", CRLWidgets.frags_r);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6) +
                            M_StringWidth(str7),
                            160 - yy, str8, cr[CR_RED]);
            }
        }
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
        if (CRL_invul_counter)
        {
            char invl[4];

            M_WriteText(292 - M_StringWidth("INVL:"), 108, "INVL:", cr[CR_GRAY]);
            M_snprintf(invl, 4, "%d", CRL_invul_counter);
            M_WriteText(296, 108, invl, CRL_PowerupColor(CRL_invul_counter, 30));
        }

        if (CRL_invis_counter)
        {
            char invs[4];

            M_WriteText(292 - M_StringWidth("INVS:"), 117, "INVS:", cr[CR_GRAY]);
            M_snprintf(invs, 4, "%d", CRL_invis_counter);
            M_WriteText(296, 117, invs, CRL_PowerupColor(CRL_invis_counter, 60));
        }

        if (CRL_rad_counter)
        {
            char rad[4];

            M_WriteText(292 - M_StringWidth("RAD:"), 126, "RAD:", cr[CR_GRAY]);
            M_snprintf(rad, 4, "%d", CRL_rad_counter);
            M_WriteText(296, 126, rad, CRL_PowerupColor(CRL_rad_counter, 60));
        }

        if (CRL_amp_counter)
        {
            char amp[4];

            M_WriteText(292 - M_StringWidth("AMP:"), 135, "AMP:", cr[CR_GRAY]);
            M_snprintf(amp, 4, "%d", CRL_amp_counter);
            M_WriteText(296, 135, amp, CRL_PowerupColor(CRL_amp_counter, 120));
        }
    }
}

// -----------------------------------------------------------------------------
// CRL_DrawFPS.
//  [JN] Draw actual frames per second value.
//  Some M_StringWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void CRL_DrawFPS (void)
{
    char fps[4];
    char fps_str[4];

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, "FPS");

    M_WriteText(SCREENWIDTH - 11 - M_StringWidth(fps) 
                                 - M_StringWidth(fps_str), 27, fps, cr[CR_GRAY]);

    M_WriteText(SCREENWIDTH - 7 - M_StringWidth(fps_str), 27, "FPS", cr[CR_GRAY]);
}

// =============================================================================
//
//                             Demo enhancements
//
// =============================================================================


// [JN] Demo warp from Crispy Doom.
int demowarp;


// -----------------------------------------------------------------------------
// CRL_DemoTimer
//  [crispy] Demo Timer widget
// -----------------------------------------------------------------------------

void CRL_DemoTimer (const int time)
{
    const int hours = time / (3600 * TICRATE);
    const int mins = time / (60 * TICRATE) % 60;
    const float secs = (float)(time % (60 * TICRATE)) / TICRATE;
    char n[16];
    int x = 237;

    if (hours)
    {
        M_snprintf(n, sizeof(n), "%02i:%02i:%05.02f", hours, mins, secs);
    }
    else
    {
        M_snprintf(n, sizeof(n), "%02i:%05.02f", mins, secs);
        x += 20;
    }

    M_WriteText(x, 18, n, cr[CR_LIGHTGRAY]);
}

// -----------------------------------------------------------------------------
// CRL_DemoBar
//  [crispy] print a bar indicating demo progress at the bottom of the screen
// -----------------------------------------------------------------------------

void CRL_DemoBar (void)
{
    const int i = SCREENWIDTH * defdemotics / deftotaldemotics;

    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, 0); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, 4); // [crispy] white
}


// =============================================================================
//
//                            HSV Coloring routines
//
// =============================================================================


// V_ColorEntry_t -- HSV table
typedef union V_ColorEntry_s
{
	struct
	{
		uint8_t R;
		uint8_t G;
		uint8_t B;
	} RGB;
	
	struct
	{
		uint8_t H;
		uint8_t S;
		uint8_t V;
	} HSV;
} V_ColorEntry_t;

static V_ColorEntry_t _rgbgamecolors[256];
static V_ColorEntry_t _hsvgamecolors[256];

// V_HSVtoRGB() -- Convert HSV to RGB
V_ColorEntry_t V_HSVtoRGB(const V_ColorEntry_t HSV)
{
	int R, G, B, H, S, V, P, Q, T, F;
	V_ColorEntry_t Ret;
	
	// Get inital values
	H = HSV.HSV.H;
	S = HSV.HSV.S;
	V = HSV.HSV.V;
	
	// Gray Color?
	if (!S)
	{
		R = G = B = V;
	}
	
	// Real Color
	else
	{
		// Calculate Hue Shift
		F = ((H % 60) * 255) / 60;
		H /= 60;
		
		// Calculate channel values
		P = (V * (256 - S)) / 256;
		Q = (V * (256 - (S * F) / 256)) / 256;
		T = (V * (256 - (S * (256 - F)) / 256)) / 256;
		
		switch (H)
		{
			case 0:
				R = V;
				G = T;
				B = P;
				break;
				
			case 1:
				R = Q;
				G = V;
				B = P;
				break;
				
			case 2:
				R = P;
				G = V;
				B = T;
				break;
				
			case 3:
				R = P;
				G = Q;
				B = V;
				break;
				
			case 4:
				R = T;
				G = P;
				B = V;
				break;
				
			default:
				R = V;
				G = P;
				B = Q;
				break;
		}
	}
	
	// Set Return
	Ret.RGB.R = R;
	Ret.RGB.G = G;
	Ret.RGB.B = B;
	
	// Return
	return Ret;
}

// V_RGBtoHSV() -- Convert RGB to HSV
V_ColorEntry_t V_RGBtoHSV(const V_ColorEntry_t RGB)
{
	V_ColorEntry_t Ret;
	uint8_t rMin, rMax, rDif;
	
	// Get min/max
	rMin = 255;
	rMax = 0;
	
	// Get RGB minimum
	if (RGB.RGB.R < rMin)
		rMin = RGB.RGB.R;
	if (RGB.RGB.G < rMin)
		rMin = RGB.RGB.G;
	if (RGB.RGB.B < rMin)
		rMin = RGB.RGB.B;
		
	// Get RGB maximum
	if (RGB.RGB.R > rMax)
		rMax = RGB.RGB.R;
	if (RGB.RGB.G > rMax)
		rMax = RGB.RGB.G;
	if (RGB.RGB.B > rMax)
		rMax = RGB.RGB.B;
		
	// Obtain value
	Ret.HSV.V = rMax;
	
	// Short circuit?
	if (Ret.HSV.V == 0)
	{
		Ret.HSV.H = Ret.HSV.S = 0;
		return Ret;
	}
	// Obtain difference
	rDif = rMax - rMin;
	
	// Obtain saturation
	Ret.HSV.S = (uint8_t)(((uint32_t)255 * (uint32_t)rDif) / (uint32_t)Ret.HSV.V);
	
	// Short circuit?
	if (Ret.HSV.S == 0)
	{
		Ret.HSV.H = 0;
		return Ret;
	}
	
	/* Obtain hue */
	if (rMax == RGB.RGB.R)
		Ret.HSV.H = 43 * (RGB.RGB.G - RGB.RGB.B) / rMax;
	else if (rMax == RGB.RGB.G)
		Ret.HSV.H = 85 + (43 * (RGB.RGB.B - RGB.RGB.R) / rMax);
	else
		Ret.HSV.H = 171 + (43 * (RGB.RGB.R - RGB.RGB.G) / rMax);
		
	return Ret;
}

/* V_BestHSVMatch() -- Best match between HSV for tables */
static size_t V_BestHSVMatch(const V_ColorEntry_t* const Table, const V_ColorEntry_t HSV)
{
	size_t i, Best;
	V_ColorEntry_t tRGB, iRGB;
	int32_t BestSqr, ThisSqr, Dr, Dg, Db;
	
	/* Check */
	if (!Table)
		return 0;
		
	/* Convert input to RGB */
	iRGB = V_HSVtoRGB(HSV);
	
	/* Loop colors */
	for (Best = 0, BestSqr = 0x7FFFFFFFUL, i = 0; i < 256; i++)
	{
		// Convert table entry to RGB
		tRGB = V_HSVtoRGB(Table[i]);
		
		// Perfect match?
		if (iRGB.RGB.R == tRGB.RGB.R && iRGB.RGB.B == tRGB.RGB.B && iRGB.RGB.G == tRGB.RGB.G)
			return i;
			
		// Distance of colors
		Dr = tRGB.RGB.R - iRGB.RGB.R;
		Dg = tRGB.RGB.G - iRGB.RGB.G;
		Db = tRGB.RGB.B - iRGB.RGB.B;
		ThisSqr = (Dr * Dr) + (Dg * Dg) + (Db * Db);
		
		// Closer?
		if (ThisSqr < BestSqr)
		{
			Best = i;
			BestSqr = ThisSqr;
		}
	}
	
	/* Fail */
	return Best;
}

/**
 * Returns the best RGB color match.
 *
 * @param __r Red.
 * @param __g Green.
 * @param __b Blue.
 * @return The best matching color.
 * @since 2015/12/17
 */
int CRL_BestRGBMatch(int __r, int __g, int __b)
{
	V_ColorEntry_t rgb, hsv;
	
	// Setup color request
	rgb.RGB.R = __r;
	rgb.RGB.G = __g;
	rgb.RGB.B = __b;
	
	// Convert to HSV
	hsv = V_RGBtoHSV(rgb);
	
	// Find best match
	return (int)V_BestHSVMatch(_hsvgamecolors, hsv);
}

static byte CRL_GetLightness(byte r, byte g, byte b)
{
	V_ColorEntry_t rgb, hsv;
	int rv;
	double is, iv, ol;
	
	// Convert to HSV
	rgb.RGB.R = r;
	rgb.RGB.G = g;
	rgb.RGB.B = b;
	hsv = V_RGBtoHSV(rgb);
	
	// Convert to double space
	is = (double)hsv.HSV.S / 255.0;
	iv = (double)hsv.HSV.V / 255.0;
	
	// Calculate values
	ol = (0.5 * iv) * (2.0 - is);
	
	// Retur brightness
	rv = ol * 255;
	if (rv > 255)
		rv = 255;
	else if (rv < 0)
		rv = 0;
	return rv;
}

static void CRL_AdjustRedGreen(byte* r, byte* g, byte* b)
{
	int x = CRL_GetLightness(*r, *g, 0);
	
	*r = x;
	*g = x;
}

static void CRL_AdjustGreenBlue(byte* r, byte* g, byte* b)
{
	int x = CRL_GetLightness(0, *g, *b);
	
	*g = x;
	*b = x;
}

static void CRL_AdjustMonochrome(byte* r, byte* g, byte* b)
{
	V_ColorEntry_t rgb, hsv;
	
	// Convert to HSV
	rgb.RGB.R = *r;
	rgb.RGB.G = *g;
	rgb.RGB.B = *b;
	hsv = V_RGBtoHSV(rgb);
	
	// Use value
	*r = hsv.HSV.V;
	*g = hsv.HSV.V;
	*b = hsv.HSV.V;
}


/**
 * Sets the game colors.
 *
 * @param __colors Colors to use, RGB.
 */
void CRL_SetColors (uint8_t* colors, void* ref)
{
	static int lastcolorblind = -1;
	static void* oldref = NULL;
	
	int i, j, darksplit, idx, hueoff, count, mi, bb, k;
	int nowcolorblind;
	int qr, qg, qb;
	uint8_t* x;
	V_ColorEntry_t vc;
	void (*adjustcolor)(byte* r, byte* g, byte* b);
	
	// Current color blindness used
    // [JN] Use external config variable.
	nowcolorblind = crl_colorblind;
	
	// Only if no colors were set or colorblindness changed
	if (!_didcolors || (lastcolorblind != nowcolorblind) || (ref != oldref) ||
		nowcolorblind >= 1)
	{
		// Do no more
		_didcolors = 1;
		lastcolorblind = nowcolorblind;
		oldref = ref;
		
		// Colorblind simulation, modify colors
		adjustcolor = NULL;
		switch (nowcolorblind)
		{
				// Red/green
			case 1:
				adjustcolor = CRL_AdjustRedGreen;
				break;
			
				// Green/Blue
			case 2:
				adjustcolor = CRL_AdjustGreenBlue;
				break;
				
				// Monochrome
			case 3:
				adjustcolor = CRL_AdjustMonochrome;
				break;
			
				// Do not touch
			default:
				break;
		}
		
		// Adjust
		if (adjustcolor != NULL)
			for (i = 0; i < 768; i += 3)
				adjustcolor(&colors[i], &colors[i + 1], &colors[i + 2]);
		
		// go through them all
		x = colors;
		for (i = 0; i < 256; i++)
		{
			// Get colors
			vc.RGB.R = *(x++);
			vc.RGB.G = *(x++);
			vc.RGB.B = *(x++);
			
			// Set RGB
			_rgbgamecolors[i] = vc;
			_hsvgamecolors[i] = V_RGBtoHSV(vc);
		}
		
		// Find best hue colors for bright and dark
		darksplit = _numcolors >> 1;
		hueoff = 0;
		count = 0;
		for (i = 0; i < 256; i++)
		{
			// Dark color hue up
			for (j = 0; j < 2; j++)
			{
				// Get the index used here
				idx = _usecolors[(j * darksplit) + (i % darksplit)];
				vc = _hsvgamecolors[idx];
				
				// Increase count
				if (count++ >= darksplit)
				{
					hueoff += 16;
					count = 0;
				}
				
				// Do not mess with color
				if (hueoff == 0)
					_colormap[DARKSHADE * j][i] = idx;
				
				// Otherwise shift it slightly
				else
				{
					// Move hue over
					vc.HSV.H = ((int)vc.HSV.H + hueoff) & 0xFF;
					
					_colormap[DARKSHADE * j][i] = V_BestHSVMatch(_hsvgamecolors, vc);
				}
			}
			
			// Make it more red
			for (j = 1; j < DARKSHADE; j++)
				for (k = 0; k < 2; k++)
				{
					// Get major index to modify
					bb = DARKSHADE * k;
					mi = bb + j;
					
					// Get the color index
					idx = _colormap[bb][i];
					
					// Get base color used
					vc = _hsvgamecolors[idx];
					
					// Make it a pulsing red
					vc.HSV.H = 0;
					
					// Move color around a bit
					{
					double inv = (255.0 * ((double)j * (1.0 / (double)DARKSHADE)));
					if (inv < 0.0)
						inv = 0.0;
					else if (inv > 255.0)
					{
						inv = 255.0;
						
					}
					vc.HSV.V = (int)inv;
					}
					
					// Set new color
					_colormap[mi][i] = V_BestHSVMatch(_hsvgamecolors, vc);
				}
		}
	}
	
	// Initialize hom colors
	for (i = 0, k = 0; i < HOMCOUNT; k++)
		for (j = 0; j < HOMQUAD; j++)
		{
			// Use specific color ranges
			qr = (k == 0 || k == 3 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			qg = (k == 1 || k == 3 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			qb = (k == 2 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			
			// Find it and put it in
			_homtable[i++] = CRL_BestRGBMatch(qr, qg, qb);
		}
}

// =============================================================================
//
//                            Console output coloring
//
// =============================================================================

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
