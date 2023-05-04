//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//
// DESCRIPTION:  the automap code
//


#include <stdio.h>
#include "deh_main.h"
#include "z_zone.h"
#include "st_bar.h"
#include "p_local.h"
#include "w_wad.h"
#include "m_controls.h"
#include "m_misc.h"
#include "v_video.h"
#include "doomstat.h"
#include "dstrings.h"
#include "m_menu.h"
#include "am_map.h"
#include "v_trans.h"

#include "crlcore.h"
#include "crlvars.h"


#define MAYBEBLAND(x) (blandcolor == 0 ? (x) : 0)
#define LIMRANGE(x) (blandcolor == 0 ? (x) : 1)

// For use if I do walls with outsides/insides
#define REDS        MAYBEBLAND(176)
#define REDRANGE    LIMRANGE(16)
#define GREENS      MAYBEBLAND(112)
#define GREENRANGE  LIMRANGE(16)
#define GRAYS       MAYBEBLAND(96)
#define GRAYSRANGE  LIMRANGE(16)
#define BROWNS      MAYBEBLAND(64)
#define BROWNRANGE  LIMRANGE(16)
#define YELLOWS     MAYBEBLAND(231)
#define YELLOWRANGE LIMRANGE(1)
#define BLACK       MAYBEBLAND(0)
#define WHITE       MAYBEBLAND(209)

// Automap colors
#define BACKGROUND       BLACK
#define YOURCOLORS       WHITE
#define YOURRANGE        0
#define WALLCOLORS       REDS
#define WALLRANGE        REDRANGE
#define TSWALLCOLORS     GRAYS
#define TSWALLRANGE      GRAYSRANGE
#define FDWALLCOLORS     BROWNS
#define FDWALLRANGE      BROWNRANGE
#define CDWALLCOLORS	 YELLOWS
#define CDWALLRANGE      YELLOWRANGE
#define THINGCOLORS      GREENS
#define THINGRANGE       GREENRANGE
#define SECRETWALLCOLORS 252
#define SECRETWALLRANGE  WALLRANGE
#define GRIDCOLORS       (GRAYS + GRAYSRANGE/2)
#define XHAIRCOLORS      GRAYS

// [JN] Make wall colors of secret sectors palette-independent.
static int secretwallcolors;

// drawing stuff
#define AM_NUMMARKPOINTS 10

// [JN] FRACTOMAPBITS: overflow-safe coordinate system.
// Written by Andrey Budko (entryway), adapted from prboom-plus/src/am_map.*
#define MAPBITS 12
#define FRACTOMAPBITS (FRACBITS-MAPBITS)

// scale on entry
#define INITSCALEMTOF (.2*FRACUNIT)

// [JN] How much the automap moves window per tic in frame-buffer coordinates.
static int f_paninc;
#define F_PANINC_SLOW 4   // 280 map units in 1 second.
#define F_PANINC_FAST 8  // 560 map units in 1 second.

// [JN] How much zoom-in per tic goes to 2x in 1 second.
static int m_zoomin;
#define M_ZOOMIN_SLOW ((int) (1.04*FRACUNIT))
#define M_ZOOMIN_FAST ((int) (1.08*FRACUNIT))

// [JN] How much zoom-out per tic pulls out to 0.5x in 1 second.
static int m_zoomout;
#define M_ZOOMOUT_SLOW ((int) (FRACUNIT/1.04))
#define M_ZOOMOUT_FAST ((int) (FRACUNIT/1.08))

// translates between frame-buffer and map distances
#define FTOM(x) (((int64_t)((x)<<16) * scale_ftom) >> FRACBITS)
#define MTOF(x) ((((int64_t)(x) * scale_mtof) >> FRACBITS)>>16)

// translates between frame-buffer and map coordinates
#define CXMTOF(x) (f_x + MTOF((x)-m_x))
#define CYMTOF(y) (f_y + (f_h - MTOF((y)-m_y)))

// [JN] ReMood-inspired IDDT monster coloring, slightly optimized
// for uncapped framerate and uses different coloring logics:
// Active monsters: up-up-up-up
// Inactive monsters: up-down-up-down
#define IDDT_REDS_RANGE (10)
#define IDDT_REDS_MIN   (REDS)
#define IDDT_REDS_MAX   (REDS + IDDT_REDS_RANGE)
static  int     iddt_reds_active;
static  int     iddt_reds_inactive = 176;
static  boolean iddt_reds_direction = false;
// [JN] Pulse player arrow in Spectator mode.
#define ARROW_WHITE_RANGE (10)
#define ARROW_WHITE_MIN   (80)
#define ARROW_WHITE_MAX   (96)
static  int     arrow_color = 80;
static  boolean arrow_color_direction = false;

typedef struct
{
    int x, y;
} fpoint_t;

typedef struct
{
    fpoint_t a, b;
} fline_t;

typedef struct
{
    int64_t x, y;
} mpoint_t;

typedef struct
{
    mpoint_t a, b;
} mline_t;

// RestlessRodent -- Bland color drawing
static boolean blandcolor = false;

// -----------------------------------------------------------------------------
// The vector graphics for the automap.
// A line drawing of the player pointing right, starting from the middle.
// -----------------------------------------------------------------------------

#define R ((8*FRACUNIT)/7)
static mline_t player_arrow[] = {
    { { -R+R/8,   0 }, {  R,      0   } }, // -----
    { {  R,       0 }, {  R-R/2,  R/4 } }, // ----->
    { {  R,       0 }, {  R-R/2, -R/4 } },
    { { -R+R/8,   0 }, { -R-R/8,  R/4 } }, // >---->
    { { -R+R/8,   0 }, { -R-R/8, -R/4 } },
    { { -R+3*R/8, 0 }, { -R+R/8,  R/4 } }, // >>--->
    { { -R+3*R/8, 0 }, { -R+R/8, -R/4 } }
};

static mline_t cheat_player_arrow[] = {
    { { -R+R/8,     0        }, {  R,           0 } }, // -----
    { {  R,         0        }, {  R-R/2,     R/4 } }, // ----->
    { {  R,         0        }, {  R-R/2,    -R/4 } },
    { { -R+R/8,     0        }, { -R-R/8,     R/4 } }, // >----->
    { { -R+R/8,     0        }, { -R-R/8,    -R/4 } },
    { { -R+3*R/8,   0        }, { -R+R/8,     R/4 } }, // >>----->
    { { -R+3*R/8,   0        }, { -R+R/8,    -R/4 } },
    { { -R/2,       0        }, { -R/2,      -R/6 } }, // >>-d--->
    { { -R/2,      -R/6      }, { -R/2+R/6,  -R/6 } },
    { { -R/2+R/6,  -R/6      }, { -R/2+R/6,   R/4 } },
    { { -R/6,       0        }, { -R/6,      -R/6 } }, // >>-dd-->
    { { -R/6,      -R/6      }, {  0,        -R/6 } },
    { {  0,        -R/6      }, {  0,         R/4 } },
    { {  R/6,       R/4      }, {  R/6,      -R/7 } }, // >>-ddt->
    { {  R/6,      -R/7      }, {  R/6+R/32, -R/7-R/32 } },
    { {  R/6+R/32, -R/7-R/32 }, {  R/6+R/10, -R/7 } }
};
#undef R

#define R (FRACUNIT)
static mline_t thintriangle_guy[] = {
    { { (fixed_t)(-.5*R), (fixed_t)(-.7*R) }, { (fixed_t)(R    ), (fixed_t)(0    ) } },
    { { (fixed_t)(R    ), (fixed_t)(0    ) }, { (fixed_t)(-.5*R), (fixed_t)(.7*R ) } },
    { { (fixed_t)(-.5*R), (fixed_t)(.7*R ) }, { (fixed_t)(-.5*R), (fixed_t)(-.7*R) } }
};
#undef R


boolean     automapactive = false;

static int  cheating = 0;
static int  grid = 0;

static const int  finit_width = SCREENWIDTH;
static const int  finit_height = SCREENHEIGHT - ST_HEIGHT;

// location of window on screen
static int  f_x;
static int  f_y;

// size of window on screen
static int  f_w;
static int  f_h;

static mpoint_t m_paninc;     // how far the window pans each tic (map coords)
static fixed_t  mtof_zoommul; // how far the window zooms in each tic (map coords)
static fixed_t  ftom_zoommul; // how far the window zooms in each tic (fb coords)
static fixed_t  curr_mtof_zoommul; // [JN] Zooming interpolation.

static int64_t  m_x, m_y;     // LL x,y where the window is on the map (map coords)
static int64_t  m_x2, m_y2;   // UR x,y where the window is on the map (map coords)
static fixed_t  prev_m_x, prev_m_y; // [JN] Panning interpolation.

// width/height of window on map (map coords)
static int64_t  m_w;
static int64_t  m_h;

// based on level size
static fixed_t  min_x;
static fixed_t  min_y; 
static fixed_t  max_x;
static fixed_t  max_y;

static fixed_t  max_w; // max_x-min_x,
static fixed_t  max_h; // max_y-min_y

static fixed_t  min_scale_mtof; // used to tell when to stop zooming out
static fixed_t  max_scale_mtof; // used to tell when to stop zooming in

// old stuff for recovery later
static int64_t old_m_w, old_m_h;
static int64_t old_m_x, old_m_y;

// used by MTOF to scale from map-to-frame-buffer coords
static fixed_t scale_mtof = (fixed_t)INITSCALEMTOF;
static fixed_t prev_scale_mtof = (fixed_t)INITSCALEMTOF; // [JN] Panning interpolation.
// used by FTOM to scale from frame-buffer-to-map coords (=1/scale_mtof)
static fixed_t scale_ftom;

static player_t *plr; // the player represented by an arrow

static patch_t *marknums[10]; // numbers used for marking by the automap
static mpoint_t markpoints[AM_NUMMARKPOINTS]; // where the points are
static int markpointnum = 0; // next point to be assigned

static int followplayer = 1; // specifies whether to follow the player around

cheatseq_t cheat_amap = CHEAT("iddt", 0);

static boolean stopped = true;

// [crispy] automap rotate mode ...
// ... needs these early on
static void AM_rotate (int64_t *x, int64_t *y, angle_t a);
static void AM_rotatePoint (mpoint_t *pt);
static mpoint_t mapcenter;
static angle_t mapangle;


// -----------------------------------------------------------------------------
// AM_Init
// [JN] Predefine some variables at program startup.
// -----------------------------------------------------------------------------

void AM_Init (void)
{
    if (original_playpal)
    {
        secretwallcolors = SECRETWALLCOLORS;
    }
    else
    {
        secretwallcolors = V_GetPaletteIndex(W_CacheLumpName("PLAYPAL", PU_CACHE),
                                                                     255, 0, 255);
    }
}

// -----------------------------------------------------------------------------
// AM_activateNewScale
// Changes the map scale after zooming or translating.
// -----------------------------------------------------------------------------

static void AM_activateNewScale (void)
{
    m_x += m_w/2;
    m_y += m_h/2;
    m_w  = FTOM(f_w);
    m_h  = FTOM(f_h);
    m_x -= m_w/2;
    m_y -= m_h/2;
    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_saveScaleAndLoc
// Saves the current center and zoom.
// Affects the variables that remember old scale and loc.
// -----------------------------------------------------------------------------

static void AM_saveScaleAndLoc (void)
{
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

// -----------------------------------------------------------------------------
// AM_restoreScaleAndLoc
// Restores the center and zoom from locally saved values.
// Affects global variables for location and scale.
// -----------------------------------------------------------------------------

static void AM_restoreScaleAndLoc (void)
{
    m_w = old_m_w;
    m_h = old_m_h;

    if (!followplayer)
    {
        m_x = old_m_x;
        m_y = old_m_y;
    }
    else 
    {
        m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
        m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;

    // Change the scaling multipliers
    scale_mtof = FixedDiv(f_w<<FRACBITS, m_w);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
}

// -----------------------------------------------------------------------------
// AM_addMark
// Adds a marker at the current location.
// -----------------------------------------------------------------------------

static void AM_addMark (void)
{
    if (!followplayer)
    {
        markpoints[markpointnum].x = m_x + m_w/2;
        markpoints[markpointnum].y = m_y + m_h/2;
    }
    else
    {
        markpoints[markpointnum].x = plr->mo->x >> FRACTOMAPBITS;
        markpoints[markpointnum].y = plr->mo->y >> FRACTOMAPBITS;
    }

    markpointnum = (markpointnum + 1) % AM_NUMMARKPOINTS;
}

// -----------------------------------------------------------------------------
// AM_findMinMaxBoundaries
// Determines bounding box of all vertices, 
// sets global variables controlling zoom range.
// -----------------------------------------------------------------------------

static void AM_findMinMaxBoundaries (void)
{
    int     i;
    fixed_t a, b;

    min_x = min_y =  INT_MAX;
    max_x = max_y = -INT_MAX;

    for (i = 0 ; i < numvertexes ; i++)
    {
        if (vertexes[i].x < min_x)
        {
            min_x = vertexes[i].x;
        }
        else if (vertexes[i].x > max_x)
        {
            max_x = vertexes[i].x;
        }

        if (vertexes[i].y < min_y)
        {
            min_y = vertexes[i].y;
        }
        else if (vertexes[i].y > max_y)
        {
            max_y = vertexes[i].y;
        }
    }

    // [crispy] cope with huge level dimensions which span the entire INT range
    max_w = (max_x >>= FRACTOMAPBITS) - (min_x >>= FRACTOMAPBITS);
    max_h = (max_y >>= FRACTOMAPBITS) - (min_y >>= FRACTOMAPBITS);

    a = FixedDiv(f_w<<FRACBITS, max_w);
    b = FixedDiv(f_h<<FRACBITS, max_h);

    min_scale_mtof = a < b ? a : b;
    max_scale_mtof = FixedDiv(f_h<<FRACBITS, 2*FRACUNIT);
}

// -----------------------------------------------------------------------------
// AM_changeWindowLoc
// Moves the map window by the global variables m_paninc.x, m_paninc.y
// -----------------------------------------------------------------------------

static void AM_changeWindowLoc (void)
{
    int64_t incx, incy;

    if (m_paninc.x || m_paninc.y)
    {
        followplayer = 0;
    }

    if (crl_uncapped_fps && realleveltime > oldleveltime)
    {
        incx = FixedMul(m_paninc.x, fractionaltic);
        incy = FixedMul(m_paninc.y, fractionaltic);
    }
    else
    {
        incx = m_paninc.x;
        incy = m_paninc.y;
    }

    if (crl_automap_rotate)
    {
        AM_rotate(&incx, &incy, -mapangle);
    }

    m_x = prev_m_x + incx;
    m_y = prev_m_y + incy;

    if (m_x + m_w/2 > max_x)
    {
        m_x = max_x - m_w/2;
    }
    else if (m_x + m_w/2 < min_x)
    {
        m_x = min_x - m_w/2;
    }

    if (m_y + m_h/2 > max_y)
    {
        m_y = max_y - m_h/2;
    }
    else if (m_y + m_h/2 < min_y)
    {
        m_y = min_y - m_h/2;
    }

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_initVariables
// -----------------------------------------------------------------------------

void AM_initVariables (void)
{
    automapactive = true;

    m_paninc.x = m_paninc.y = 0;
    ftom_zoommul = FRACUNIT;
    mtof_zoommul = FRACUNIT;

    m_w = FTOM(f_w);
    m_h = FTOM(f_h);

    // [JN] Find player to center.
    plr = &players[displayplayer];

    m_x = (plr->mo->x >> FRACTOMAPBITS) - m_w/2;
    m_y = (plr->mo->y >> FRACTOMAPBITS) - m_h/2;

    AM_Ticker();
    AM_changeWindowLoc();

    // for saving & restoring
    old_m_x = m_x;
    old_m_y = m_y;
    old_m_w = m_w;
    old_m_h = m_h;
}

// -----------------------------------------------------------------------------
// AM_loadPics
// -----------------------------------------------------------------------------

static void AM_loadPics (void)
{
    char namebuf[9];
  
    for (int i = 0 ; i < 10 ; i++)
    {
        DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
        marknums[i] = W_CacheLumpName(namebuf, PU_STATIC);
    }

}

// -----------------------------------------------------------------------------
// AM_unloadPics
// -----------------------------------------------------------------------------

static void AM_unloadPics (void)
{
    char namebuf[9];

    for (int i = 0 ; i < 10 ; i++)
    {
        DEH_snprintf(namebuf, 9, "AMMNUM%d", i);
        W_ReleaseLumpName(namebuf);
    }
}

// -----------------------------------------------------------------------------
// AM_clearMarks
// -----------------------------------------------------------------------------

static void AM_clearMarks (void)
{
    for (int i = 0 ; i < AM_NUMMARKPOINTS ; i++)
    {
        markpoints[i].x = -1; // means empty
    }
    markpointnum = 0;
}

// -----------------------------------------------------------------------------
// AM_LevelInit
// Should be called at the start of every level.
// Right now, i figure it out myself.
// -----------------------------------------------------------------------------

static void AM_LevelInit (void)
{
    f_x = f_y = 0;
    f_w = finit_width;
    f_h = finit_height;

    AM_clearMarks();

    AM_findMinMaxBoundaries();

    scale_mtof = FixedDiv(min_scale_mtof, (int) (0.7*FRACUNIT));

    if (scale_mtof > max_scale_mtof)
    {
        scale_mtof = min_scale_mtof;
    }

    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    // [JN] If running Deathmatch mode, mark all automap lines as mapped
    // so they will appear initially. DM mode is not about map reveal.
    if (deathmatch)
    {
        for (int i = 0 ; i < numlines ; i++)
        {
            lines[i].flags |= ML_MAPPED;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_Stop
// -----------------------------------------------------------------------------

void AM_Stop (void)
{
    AM_unloadPics();
    automapactive = false;
    stopped = true;
}

// -----------------------------------------------------------------------------
// AM_Start
// -----------------------------------------------------------------------------

void AM_Start (void)
{
    static int lastlevel = -1, lastepisode = -1;

    if (!stopped)
    {
        AM_Stop();
    }

    stopped = false;

    if (lastlevel != gamemap || lastepisode != gameepisode)
    {
        AM_LevelInit();
        lastlevel = gamemap;
        lastepisode = gameepisode;
    }

    AM_initVariables();
    AM_loadPics();
}

// -----------------------------------------------------------------------------
// AM_minOutWindowScale
// Set the window scale to the maximum size.
// -----------------------------------------------------------------------------

static void AM_minOutWindowScale (void)
{
    scale_mtof = min_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

// -----------------------------------------------------------------------------
// AM_maxOutWindowScale
// Set the window scale to the minimum size.
// -----------------------------------------------------------------------------

static void AM_maxOutWindowScale (void)
{
    scale_mtof = max_scale_mtof;
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);
    AM_activateNewScale();
}

// -----------------------------------------------------------------------------
// AM_Responder
// Handle events (user inputs) in automap mode.
// -----------------------------------------------------------------------------

boolean AM_Responder (event_t *ev)
{
    int         rc;
    static int  bigstate=0;
    static char buffer[20];
    int         key;
    extern boolean speedkeydown (void);

    // [JN] If run button is hold, pan/zoom Automap faster.    
    if (speedkeydown())
    {
        f_paninc = F_PANINC_FAST;
        m_zoomin = M_ZOOMIN_FAST;
        m_zoomout = M_ZOOMOUT_FAST;
    }
    else
    {
        f_paninc = F_PANINC_SLOW;
        m_zoomin = M_ZOOMIN_SLOW;
        m_zoomout = M_ZOOMOUT_SLOW;
    }

    rc = false;

    if (ev->type == ev_joystick && joybautomap >= 0
    && (ev->data1 & (1 << joybautomap)) != 0)
    {
        joywait = I_GetTime() + 5;

        if (!automapactive)
        {
            AM_Start ();
        }
        else
        {
            bigstate = 0;
            AM_Stop ();
        }

        return true;
    }

    if (!automapactive)
    {
        if (ev->type == ev_keydown && ev->data1 == key_map_toggle)
        {
            AM_Start ();
            rc = true;
        }
    }
    else if (ev->type == ev_keydown)
    {
        rc = true;
        key = ev->data1;

        if (key == key_map_east)          // pan right
        {
            if (!followplayer)
            {
                m_paninc.x = FTOM(f_paninc);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_west)     // pan left
        {
            if (!followplayer)
            {
                m_paninc.x = -FTOM(f_paninc);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_north)    // pan up
        {
            if (!followplayer)
            {
                m_paninc.y = FTOM(f_paninc);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_south)    // pan down
        {
            if (!followplayer)
            {
                m_paninc.y = -FTOM(f_paninc);
            }
            else
            {
                rc = false;
            }
        }
        else if (key == key_map_zoomout)  // zoom out
        {
            mtof_zoommul = m_zoomout;
            ftom_zoommul = m_zoomin;
        }
        else if (key == key_map_zoomin)   // zoom in
        {
            mtof_zoommul = m_zoomin;
            ftom_zoommul = m_zoomout;
        }
        else if (key == key_map_toggle)
        {
            bigstate = 0;
            AM_Stop ();
        }
        else if (key == key_map_maxzoom)
        {
            bigstate = !bigstate;

            if (bigstate)
            {
                AM_saveScaleAndLoc();
                AM_minOutWindowScale();
            }
            else
            {
                AM_restoreScaleAndLoc();
            }
        }
        else if (key == key_map_follow)
        {
            followplayer = !followplayer;

            CRL_SetMessage(plr, DEH_String(followplayer ?
                           AMSTR_FOLLOWON : AMSTR_FOLLOWOFF), false, NULL);
        }
        else if (key == key_map_grid)
        {
            grid = !grid;

            CRL_SetMessage(plr, DEH_String(grid ?
                           AMSTR_GRIDON : AMSTR_GRIDOFF), false, NULL);

        }
        else if (key == key_map_mark)
        {
            M_snprintf(buffer, sizeof(buffer), "%s %d",
                       DEH_String(AMSTR_MARKEDSPOT), markpointnum);
            CRL_SetMessage(plr, buffer, false, NULL);
            AM_addMark();
        }
        else if (key == key_map_clearmark)
        {
            AM_clearMarks();
            CRL_SetMessage(plr, DEH_String(AMSTR_MARKSCLEARED), false, NULL);
        }
        else if (key == key_crl_map_rotate)
        {
            // [JN] CRL - Automap rotate mode
            crl_automap_rotate = !crl_automap_rotate;
            CRL_SetMessage(plr, DEH_String(crl_automap_rotate ?
                           CRL_AUTOMAPROTATE_ON : CRL_AUTOMAPROTATE_OFF), false, NULL);
        }
        else if (key == key_crl_map_overlay)
        {
            // [JN] CRL - Automap overlay mode
            crl_automap_overlay = !crl_automap_overlay;
            CRL_SetMessage(plr, DEH_String(crl_automap_overlay ?
                           CRL_AUTOMAPOVERLAY_ON : CRL_AUTOMAPOVERLAY_OFF), false, NULL);
        }
        else
        {
            rc = false;
        }

        if ((!deathmatch || gameversion <= exe_doom_1_8)
        && cht_CheckCheat(&cheat_amap, ev->data2))
        {
            rc = false;
            cheating = (cheating + 1) % 3;
        }
    }
    else if (ev->type == ev_keyup)
    {
        rc = false;
        key = ev->data1;

        if (key == key_map_east)
        {
            if (!followplayer)
            {
                m_paninc.x = 0;
            }
        }
        else if (key == key_map_west)
        {
            if (!followplayer)
            {
                m_paninc.x = 0;
            }
        }
        else if (key == key_map_north)
        {
            if (!followplayer)
            {
                m_paninc.y = 0;
            }
        }
        else if (key == key_map_south)
        {
            if (!followplayer)
            {
                m_paninc.y = 0;
            }
        }
        else if (key == key_map_zoomout || key == key_map_zoomin)
        {
            mtof_zoommul = FRACUNIT;
            ftom_zoommul = FRACUNIT;
        }
    }

    return rc;
}

// -----------------------------------------------------------------------------
// AM_changeWindowScale
// Automap zooming.
// -----------------------------------------------------------------------------

static void AM_changeWindowScale (void)
{
    if (crl_uncapped_fps && realleveltime > oldleveltime)
    {
        float f_paninc_smooth = (float)f_paninc / (float)FRACUNIT * (float)fractionaltic;

        if (f_paninc_smooth < 0.01f)
        {
            f_paninc_smooth = 0.01f;
        }
    
        scale_mtof = prev_scale_mtof;

        if (curr_mtof_zoommul == m_zoomin)
        {
            mtof_zoommul = ((int) ((float)FRACUNIT * (1.00f + f_paninc_smooth / 200.0f)));
            ftom_zoommul = ((int) ((float)FRACUNIT / (1.00f + f_paninc_smooth / 200.0f)));
        }
        if (curr_mtof_zoommul == m_zoomout)
        {
            mtof_zoommul = ((int) ((float)FRACUNIT / (1.00f + f_paninc_smooth / 200.0f)));
            ftom_zoommul = ((int) ((float)FRACUNIT * (1.00f + f_paninc_smooth / 200.0f)));
        }
    }

    // Change the scaling multipliers
    scale_mtof = FixedMul(scale_mtof, mtof_zoommul);
    scale_ftom = FixedDiv(FRACUNIT, scale_mtof);

    if (scale_mtof < min_scale_mtof)
    {
        AM_minOutWindowScale();
    }
    else if (scale_mtof > max_scale_mtof)
    {
        AM_maxOutWindowScale();
    }
    else
    {
        AM_activateNewScale();
    }
}

// -----------------------------------------------------------------------------
// AM_doFollowPlayer
// Turn on follow mode - the map scrolls opposite to player motion.
// -----------------------------------------------------------------------------

static void AM_doFollowPlayer (void)
{
    // [JN] Use interpolated player coords for smooth
    // scrolling and static player arrow position.
    m_x = (viewx >> FRACTOMAPBITS) - m_w/2;
    m_y = (viewy >> FRACTOMAPBITS) - m_h/2;

    m_x2 = m_x + m_w;
    m_y2 = m_y + m_h;
}

// -----------------------------------------------------------------------------
// AM_Ticker
// Updates on Game Tick.
// -----------------------------------------------------------------------------

void AM_Ticker (void)
{
    if (!automapactive)
    {
        return;
    }

    prev_scale_mtof = scale_mtof;
    prev_m_x = m_x;
    prev_m_y = m_y;

    // [JN] Animate IDDT monster colors:

    // Inactive:
    if (gametic & 1)
    {
        // Brightening
        if (!iddt_reds_direction && ++iddt_reds_inactive == IDDT_REDS_MAX)
        {
            iddt_reds_direction = true;
        }
        // Darkening
        else
        if (iddt_reds_direction && --iddt_reds_inactive == IDDT_REDS_MIN)
        {
            iddt_reds_direction = false;
        }
    }

    // Active:
    iddt_reds_active = (REDS - 4) + ((gametic >> 1) % IDDT_REDS_RANGE);

    // [JN] Pulse player arrow in Spectator mode:

    // Brightening
    if (!arrow_color_direction && ++arrow_color == ARROW_WHITE_MAX)
    {
        arrow_color_direction = true;
    }
    // Darkening
    else
    if (arrow_color_direction && --arrow_color == ARROW_WHITE_MIN)
    {
        arrow_color_direction = false;
    }
}

// -----------------------------------------------------------------------------
// AM_clearFB
// Clear automap frame buffer.
// -----------------------------------------------------------------------------

static void AM_clearFB(int color)
{
    memset(I_VideoBuffer, color, f_w*f_h*sizeof(*I_VideoBuffer));
}

// -----------------------------------------------------------------------------
// AM_clipMline
// Automap clipping of lines.
//
// Based on Cohen-Sutherland clipping algorithm but with a slightly
// faster reject and precalculated slopes.  If the speed is needed,
// use a hash algorithm to handle  the common cases.
// -----------------------------------------------------------------------------

static boolean AM_clipMline (mline_t *ml, fline_t *fl)
{
    enum
    {
        LEFT   = 1,
        RIGHT  = 2,
        BOTTOM = 4,
        TOP	   = 8
    };

    int	outcode1 = 0;
    int	outcode2 = 0;
    int	outside;

    int      dx;
    int      dy;
    fpoint_t tmp;

#define DOOUTCODE(oc, mx, my) \
    (oc) = 0; \
    if ((my) < 0) (oc) |= TOP; \
    else if ((my) >= f_h) (oc) |= BOTTOM; \
    if ((mx) < 0) (oc) |= LEFT; \
    else if ((mx) >= f_w) (oc) |= RIGHT;

    // do trivial rejects and outcodes
    if (ml->a.y > m_y2)
    {
        outcode1 = TOP;
    }
    else if (ml->a.y < m_y)
    {
        outcode1 = BOTTOM;
    }

    if (ml->b.y > m_y2)
    {
        outcode2 = TOP;
    }
    else if (ml->b.y < m_y)
    {
        outcode2 = BOTTOM;
    }
    
    if (outcode1 & outcode2)
    {
        return false; // trivially outside
    }

    if (ml->a.x < m_x)
    {
        outcode1 |= LEFT;
    }
    else if (ml->a.x > m_x2)
    {
        outcode1 |= RIGHT;
    }

    if (ml->b.x < m_x)
    {
        outcode2 |= LEFT;
    }
    else if (ml->b.x > m_x2)
    {
        outcode2 |= RIGHT;
    }

    if (outcode1 & outcode2)
    {
        return false; // trivially outside
    }

    // transform to frame-buffer coordinates.
    fl->a.x = CXMTOF(ml->a.x);
    fl->a.y = CYMTOF(ml->a.y);
    fl->b.x = CXMTOF(ml->b.x);
    fl->b.y = CYMTOF(ml->b.y);

    DOOUTCODE(outcode1, fl->a.x, fl->a.y);
    DOOUTCODE(outcode2, fl->b.x, fl->b.y);

    if (outcode1 & outcode2)
    {
        return false;
    }

    while (outcode1 | outcode2)
    {
        // may be partially inside box
        // find an outside point
        if (outcode1)
        {
            outside = outcode1;
        }
        else
        {
            outside = outcode2;
        }

        // clip to each side
        if (outside & TOP)
        {
            dy = fl->a.y - fl->b.y;
            dx = fl->b.x - fl->a.x;
            tmp.x = fl->a.x + (dx*(fl->a.y))/dy;
            tmp.y = 0;
        }
        else if (outside & BOTTOM)
        {
            dy = fl->a.y - fl->b.y;
            dx = fl->b.x - fl->a.x;
            tmp.x = fl->a.x + (dx*(fl->a.y-f_h))/dy;
            tmp.y = f_h-1;
        }
        else if (outside & RIGHT)
        {
            dy = fl->b.y - fl->a.y;
            dx = fl->b.x - fl->a.x;
            tmp.y = fl->a.y + (dy*(f_w-1 - fl->a.x))/dx;
            tmp.x = f_w-1;
        }
        else if (outside & LEFT)
        {
            dy = fl->b.y - fl->a.y;
            dx = fl->b.x - fl->a.x;
            tmp.y = fl->a.y + (dy*(-fl->a.x))/dx;
            tmp.x = 0;
        }
        else
        {
            tmp.x = 0;
            tmp.y = 0;
        }

        if (outside == outcode1)
        {
            fl->a = tmp;
            DOOUTCODE(outcode1, fl->a.x, fl->a.y);
        }
        else
        {
            fl->b = tmp;
            DOOUTCODE(outcode2, fl->b.x, fl->b.y);
        }

        if (outcode1 & outcode2)
        {
            return false; // trivially outside
        }
    }

    return true;
}
#undef DOOUTCODE

// -----------------------------------------------------------------------------
// AM_drawFline
// Classic Bresenham w/ whatever optimizations needed for speed.
// -----------------------------------------------------------------------------

static void AM_drawFline (fline_t *fl, int color)
{
    int x;
    int y;
    int dx;
    int dy;
    int sx;
    int sy;
    int ax;
    int ay;
    int d;

    static int yuck = 0;

    // For debugging only
    if (fl->a.x < 0 || fl->a.x >= f_w
    ||  fl->a.y < 0 || fl->a.y >= f_h
    ||  fl->b.x < 0 || fl->b.x >= f_w
    ||  fl->b.y < 0 || fl->b.y >= f_h)
    {
        DEH_fprintf(stderr, "yuck %d \r", yuck++);
        return;
    }

#define PUTDOT(xx,yy,cc) I_VideoBuffer[(yy)*f_w+(xx)]=(cc)

    dx = fl->b.x - fl->a.x;
    ax = 2 * (dx<0 ? -dx : dx);
    sx = dx<0 ? -1 : 1;

    dy = fl->b.y - fl->a.y;
    ay = 2 * (dy<0 ? -dy : dy);
    sy = dy<0 ? -1 : 1;

    x = fl->a.x;
    y = fl->a.y;

    if (ax > ay)
    {
        d = ay - ax/2;

        while (1)
        {
            PUTDOT(x,y,color);

            if (x == fl->b.x)
            {
                return;
            }

            if (d>=0)
            {
                y += sy;
                d -= ax;
            }
            x += sx;
            d += ay;
        }
    }
    else
    {
        d = ax - ay/2;

        while (1)
        {
            PUTDOT(x, y, color);

            if (y == fl->b.y)
            {
                return;
            }

            if (d >= 0)
            {
                x += sx;
                d -= ay;
            }
            y += sy;
            d += ax;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawMline
// Clip lines, draw visible parts of lines.
// -----------------------------------------------------------------------------

static void AM_drawMline (mline_t *ml, int color)
{
    static fline_t fl;

    if (AM_clipMline(ml, &fl))
    {
        // draws it on frame buffer using fb coords
        AM_drawFline(&fl, color);
    }
}

// -----------------------------------------------------------------------------
// AM_drawGrid
// Draws flat (floor/ceiling tile) aligned grid lines.
// -----------------------------------------------------------------------------

static void AM_drawGrid (int color)
{
    int64_t x, y;
    int64_t start, end;
    const fixed_t gridsize = MAPBLOCKUNITS << MAPBITS;
    mline_t ml;

    // Figure out start of vertical gridlines
    start = m_x;
    if (crl_automap_rotate)
    {
        start -= m_h / 2;
    }

    if ((start-(bmaporgx>>FRACTOMAPBITS))%gridsize)
    {
        start -= ((start-(bmaporgx>>FRACTOMAPBITS))%gridsize);
    }

    end = m_x + m_w;

    if (crl_automap_rotate)
    {
        end += m_h / 2;
    }

    // draw vertical gridlines
    for (x = start ; x < end ; x += gridsize)
    {
        ml.a.x = x;
        ml.b.x = x;
        // [crispy] moved here
        ml.a.y = m_y;
        ml.b.y = m_y+m_h;
        if (crl_automap_rotate)
        {
            ml.a.y -= m_w / 2;
            ml.b.y += m_w / 2;
            AM_rotatePoint(&ml.a);
            AM_rotatePoint(&ml.b);
        }
        AM_drawMline(&ml, color);
    }

    // Figure out start of horizontal gridlines
    start = m_y;
    if (crl_automap_rotate)
    {
        start -= m_w / 2;
    }

    if ((start-(bmaporgy>>FRACTOMAPBITS))%gridsize)
    {
        start -= ((start-(bmaporgy>>FRACTOMAPBITS))%gridsize);
    }

    end = m_y + m_h;

    if (crl_automap_rotate)
    {
        end += m_w / 2;
    }

    // draw horizontal gridlines
    for (y = start ; y < end ; y += gridsize)
    {
        ml.a.y = y;
        ml.b.y = y;
        // [crispy] moved here
        ml.a.x = m_x;
        ml.b.x = m_x + m_w;
        if (crl_automap_rotate)
        {
            ml.a.x -= m_h / 2;
            ml.b.x += m_h / 2;
            AM_rotatePoint(&ml.a);
            AM_rotatePoint(&ml.b);
        }
        AM_drawMline(&ml, color);
    }
}

// -----------------------------------------------------------------------------
// AM_drawWalls
// Determines visible lines, draws them. 
// This is LineDef based, not LineSeg based.
// -----------------------------------------------------------------------------

static void AM_drawWalls (void)
{
    static mline_t l;

    for (int i = 0 ; i < numlines ; i++)
    {
        l.a.x = lines[i].v1->x >> FRACTOMAPBITS;
        l.a.y = lines[i].v1->y >> FRACTOMAPBITS;
        l.b.x = lines[i].v2->x >> FRACTOMAPBITS;
        l.b.y = lines[i].v2->y >> FRACTOMAPBITS;

        if (crl_automap_rotate)
        {
            AM_rotatePoint(&l.a);
            AM_rotatePoint(&l.b);
        }

        if (cheating || (lines[i].flags & ML_MAPPED))
        {
            if ((lines[i].flags & ML_DONTDRAW) && !cheating)
            {
                continue;
            }

            if (!lines[i].backsector)
            {
                // [JN] CRL - mark secret sectors.
                if (crl_automap_secrets && lines[i].frontsector->special == 9)
                {
                    AM_drawMline(&l, secretwallcolors);
                }
                else
                {
                    AM_drawMline(&l, WALLCOLORS);
                }
            }
            else
            {
                if (lines[i].special == 39)
                { // teleporters
                    AM_drawMline(&l, WALLCOLORS+WALLRANGE/2);
                }
                else
                if (lines[i].flags & ML_SECRET) // secret door
                {
                    // [JN] Note: this means "don't map as two sided".
                    AM_drawMline(&l, WALLCOLORS);
                }
                // [JN] CRL - mark secret sectors.
                else
                if (crl_automap_secrets 
                && (lines[i].frontsector->special == 9
                ||  lines[i].backsector->special == 9))
                {
                    AM_drawMline(&l, secretwallcolors);
                }
                else
                if (lines[i].backsector->floorheight
			    !=  lines[i].frontsector->floorheight)
                {
                    AM_drawMline(&l, FDWALLCOLORS); // floor level change
                }
                else
                if (lines[i].backsector->ceilingheight
                !=  lines[i].frontsector->ceilingheight)
                {
                    AM_drawMline(&l, CDWALLCOLORS); // ceiling level change
                }
                else
                if (cheating)
                {
                    AM_drawMline(&l, TSWALLCOLORS);
                }
            }
        }
        else if (plr->powers[pw_allmap])
        {
            if (!(lines[i].flags & ML_DONTDRAW))
            {
                AM_drawMline(&l, GRAYS+3);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// AM_rotate
// Rotation in 2D. Used to rotate player arrow line character.
// -----------------------------------------------------------------------------

static void AM_rotate (int64_t *x, int64_t *y, angle_t a)
{
    int64_t tmpx;

    a >>= ANGLETOFINESHIFT;

    tmpx = FixedMul(*x, finecosine[a])
         - FixedMul(*y, finesine[a]);

    *y = FixedMul(*x, finesine[a])
       + FixedMul(*y, finecosine[a]);

    *x = tmpx;
}

// -----------------------------------------------------------------------------
// AM_rotatePoint
// [crispy] rotate point around map center
// adapted from prboom-plus/src/am_map.c:898-920
// -----------------------------------------------------------------------------

static void AM_rotatePoint (mpoint_t *pt)
{
    int64_t tmpx;
    const angle_t actualangle = ((!(!followplayer && crl_automap_overlay)) ?
                                 ANG90 - viewangle : mapangle) >> ANGLETOFINESHIFT;

    pt->x -= mapcenter.x;
    pt->y -= mapcenter.y;

    tmpx = (int64_t)FixedMul(pt->x, finecosine[actualangle])
         - (int64_t)FixedMul(pt->y, finesine[actualangle])
         + mapcenter.x;

    pt->y = (int64_t)FixedMul(pt->x, finesine[actualangle])
          + (int64_t)FixedMul(pt->y, finecosine[actualangle])
          + mapcenter.y;

    pt->x = tmpx;
}

// -----------------------------------------------------------------------------
// AM_drawLineCharacter
// Draws a vector graphic according to numerous parameters.
// -----------------------------------------------------------------------------

static void AM_drawLineCharacter (mline_t *lineguy, int lineguylines,
                                  fixed_t scale, angle_t angle, int color,
                                  fixed_t x, fixed_t y)
{
    int     i;
    mline_t l;

    if (crl_automap_rotate)
    {
        angle += mapangle;
    }

    for (i = 0 ; i < lineguylines ; i++)
    {
        l.a.x = lineguy[i].a.x;
        l.a.y = lineguy[i].a.y;

        if (scale)
        {
            l.a.x = FixedMul(scale, l.a.x);
            l.a.y = FixedMul(scale, l.a.y);
        }

        if (angle)
        {
            AM_rotate(&l.a.x, &l.a.y, angle);
        }

        l.a.x += x;
        l.a.y += y;

        l.b.x = lineguy[i].b.x;
        l.b.y = lineguy[i].b.y;

        if (scale)
        {
            l.b.x = FixedMul(scale, l.b.x);
            l.b.y = FixedMul(scale, l.b.y);
        }

        if (angle)
        {
            AM_rotate(&l.b.x, &l.b.y, angle);
        }

        l.b.x += x;
        l.b.y += y;

        AM_drawMline(&l, color);
    }
}

// -----------------------------------------------------------------------------
// AM_drawPlayers
// Draws the player arrow in single player, 
// or all the player arrows in a netgame.
// -----------------------------------------------------------------------------


static void AM_drawPlayers (void)
{
    int       i;
    int       their_colors[] = { GREENS, GRAYS, BROWNS, REDS };
    int       their_color = -1;
    int       color;
    mpoint_t  pt;
    player_t *p;

    if (!netgame)
    {
        // [JN] Smooth player arrow rotation.
        // Keep arrow static in Spectator + rotate mode.
        const angle_t smoothangle = (crl_spectating && crl_automap_rotate) ?
                                     plr->mo->angle :
                                     crl_automap_rotate ? plr->mo->angle : viewangle;

        // [JN] Interpolate player arrow.
        pt.x = viewx >> FRACTOMAPBITS;
        pt.y = viewy >> FRACTOMAPBITS;

        if (crl_automap_rotate)
        {
            AM_rotatePoint(&pt);
        }

        AM_drawLineCharacter(cheat_player_arrow, cheating ?
                             arrlen(cheat_player_arrow) : arrlen(player_arrow), 0,
                             smoothangle, crl_spectating ? arrow_color : WHITE, pt.x, pt.y);

        return;
    }

    for (i = 0 ; i < MAXPLAYERS ; i++)
    {
        // [JN] Interpolate other player arrows angle.
        angle_t smoothangle;

        their_color++;
        p = &players[i];

        if ((deathmatch && !singledemo) && p != plr)
        {
            continue;
        }
        if (!playeringame[i])
        {
            continue;
        }

        if (p->powers[pw_invisibility])
        {
            color = 246; // *close* to black
        }
        else
        {
            color = their_colors[their_color];
        }

        // [JN] Interpolate other player arrows.
        if (crl_uncapped_fps && realleveltime > oldleveltime)
        {
            pt.x = (p->mo->oldx + FixedMul(p->mo->x - p->mo->oldx, fractionaltic)) >> FRACTOMAPBITS;
            pt.y = (p->mo->oldy + FixedMul(p->mo->y - p->mo->oldy, fractionaltic)) >> FRACTOMAPBITS;
        }
        else
        {
            pt.x = p->mo->x >> FRACTOMAPBITS;
            pt.y = p->mo->y >> FRACTOMAPBITS;
        }

        if (crl_automap_rotate)
        {
            AM_rotatePoint(&pt);
            smoothangle = p->mo->angle;
        }
        else
        {
            smoothangle = R_InterpolateAngle(p->mo->oldangle, p->mo->angle, fractionaltic);
        }

        AM_drawLineCharacter(player_arrow, arrlen(player_arrow), 0,
                             smoothangle, color, pt.x, pt.y);
    }
}

// -----------------------------------------------------------------------------
// AM_drawThings
// Draws the things on the automap in double IDDT cheat mode.
// -----------------------------------------------------------------------------

static void AM_drawThings (int colors, int colorrange)
{
    int       i;
    mpoint_t  pt;
    mobj_t   *t;
    angle_t   actualangle;
    // RestlessRodent -- Carbon copy from ReMooD
    int       color = colors;

    for (i = 0 ; i < numsectors ; i++)
    {
        t = sectors[i].thinglist;
        while (t)
        {
            // [JN] Use actual radius for things drawing.
            const fixed_t actualradius = t->radius >> FRACTOMAPBITS;
                
            // [crispy] do not draw an extra triangle for the player
            if (t == plr->mo)
            {
                t = t->snext;
                continue;
            }

            // [JN] Interpolate things if possible.
            if (crl_uncapped_fps && realleveltime > oldleveltime)
            {
                pt.x = (t->oldx + FixedMul(t->x - t->oldx, fractionaltic)) >> FRACTOMAPBITS;
                pt.y = (t->oldy + FixedMul(t->y - t->oldy, fractionaltic)) >> FRACTOMAPBITS;
                actualangle = R_InterpolateAngle(t->oldangle, t->angle, fractionaltic);
            }
            else
            {
                pt.x = t->x >> FRACTOMAPBITS;
                pt.y = t->y >> FRACTOMAPBITS;
                actualangle = t->angle;
            }

            // [JN] Keep things static in Spectator + rotate mode.
            if (crl_spectating && crl_automap_rotate)
            {
                actualangle = t->angle - mapangle - viewangle + ANG90;
            }

            if (crl_automap_rotate)
            {
                AM_rotatePoint(&pt);
            }

            // [JN] IDDT extended colors:
            // [crispy] draw blood splats and puffs as small squares
            if (t->type == MT_BLOOD || t->type == MT_PUFF)
            {
                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy),
                                     actualradius >> 2, actualangle, GRAYS, pt.x, pt.y);
            }
            else
            {
                // [JN] CRL - ReMooD-inspired monsters coloring.
                if (t->target && t->state && t->state->action.acv != A_Look)
                {
                    color = iddt_reds_active;
                }
                else
                {
                    color = iddt_reds_inactive;
                }

                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy), 
                                     actualradius, actualangle, 
                                     // Monsters
                                     t->flags & MF_COUNTKILL ? (t->health > 0 ? color : GRAYS) :
                                     // Lost Souls and Explosive barrels (does not have a MF_COUNTKILL flag)
                                     t->type == MT_SKULL || t->type == MT_BARREL ? YELLOWS :
                                     // Countable items
                                     t->flags & MF_COUNTITEM ? GREENS :
                                     // Everything else
                                     GRAYS,
                                     pt.x, pt.y);
            }

            t = t->snext;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawThings
// Draws the things on the automap in double IDDT cheat mode.
// -----------------------------------------------------------------------------

static void AM_drawSpectator (void)
{
    int       i;
    mpoint_t  pt;
    mobj_t   *t;
    angle_t   actualangle;

    for (i = 0 ; i < numsectors ; i++)
    {
        t = sectors[i].thinglist;
        while (t)
        {
            // [JN] Interpolate things if possible.
            if (crl_uncapped_fps && realleveltime > oldleveltime)
            {
                pt.x = (t->oldx + FixedMul(t->x - t->oldx, fractionaltic)) >> FRACTOMAPBITS;
                pt.y = (t->oldy + FixedMul(t->y - t->oldy, fractionaltic)) >> FRACTOMAPBITS;
                actualangle = R_InterpolateAngle(t->oldangle, t->angle, fractionaltic);
            }
            else
            {
                pt.x = t->x >> FRACTOMAPBITS;
                pt.y = t->y >> FRACTOMAPBITS;
                actualangle = t->angle;
            }

            // [JN] Keep things static in Spectator + rotate mode.
            if (crl_spectating && crl_automap_rotate)
            {
                actualangle = t->angle - mapangle - viewangle + ANG90;
            }

            if (crl_automap_rotate)
            {
                AM_rotatePoint(&pt);
            }

            // [crispy] do not draw an extra triangle for the player
            if (t == plr->mo)
            {
                AM_drawLineCharacter(thintriangle_guy, arrlen(thintriangle_guy),
                                     t->radius >> FRACTOMAPBITS, actualangle,
                                     arrow_color, pt.x, pt.y);
            }

            t = t->snext;
            continue;
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawThings
// Draw the marked locations on the automap.
// -----------------------------------------------------------------------------

static void AM_drawMarks (void)
{
    int i, fx, fy;
    mpoint_t pt;
    char name[9];

    for ( i = 0 ; i < AM_NUMMARKPOINTS ; i++)
    {
        if (markpoints[i].x != -1)
        {
            // [crispy] center marks around player
            pt.x = markpoints[i].x;
            pt.y = markpoints[i].y;

            if (crl_automap_rotate)
            {
                AM_rotatePoint(&pt);
            }

            fx = (CXMTOF(pt.x)) - 1;
            fy = (CYMTOF(pt.y)) - 2;

            if (fx >= f_x && fx <= f_w - 5 && fy >= f_y && fy <= f_h - 6)
            {
                // [JN] Construct proper patch name for possible error handling:
                sprintf(name, "AMMNUM%d", i);
                V_DrawPatch(fx, fy, marknums[i], name);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// AM_drawCrosshair
// -----------------------------------------------------------------------------

static void AM_drawCrosshair (void)
{
    // [JN] Simplify: (f_w*(f_h+1))/2) = (320 * (200 - 32 + 1) / 2) = 27040.
    // Color is always same, so macro can be used here safely.
    I_VideoBuffer[27040] = XHAIRCOLORS; // single point for now
}

// -----------------------------------------------------------------------------
// AM_CRLFLine
// -----------------------------------------------------------------------------

static void AM_CRLFLine (int __col, int __x1, int __y1, int __x2, int __y2)
{
	fline_t mt = {{__x1 >> FRACTOMAPBITS, __y1 >> FRACTOMAPBITS},
                  {__x2 >> FRACTOMAPBITS, __y2 >> FRACTOMAPBITS}};

	AM_drawFline(&mt, __col);
}

// -----------------------------------------------------------------------------
// AM_CRLMLine
// -----------------------------------------------------------------------------

static void AM_CRLMLine (int __col, int __x1, int __y1, int __x2, int __y2)
{
	mline_t mt = {{__x1 >> FRACTOMAPBITS, __y1 >> FRACTOMAPBITS},
                  {__x2 >> FRACTOMAPBITS, __y2 >> FRACTOMAPBITS}};

    // [JN] Rotate rendered visplanes as well.
    if (crl_automap_rotate)
    {
        AM_rotatePoint(&mt.a);
        AM_rotatePoint(&mt.b);
    }

	AM_drawMline(&mt, __col);
}

// -----------------------------------------------------------------------------
// AM_MapNameDrawer
// -----------------------------------------------------------------------------

static void AM_MapNameDrawer (void)
{
    static char str[128];
    extern const char *level_name;

    sprintf(str, "%s", level_name);
    M_WriteText(0, 160, str, NULL);
}

// -----------------------------------------------------------------------------
// AM_MapNameDrawer
// -----------------------------------------------------------------------------

void AM_Drawer (void)
{
    if (!automapactive)
    {
        return;
    }
    
    // [JN] Moved from AM_Ticker for drawing interpolation.
    if (followplayer)
    {
        AM_doFollowPlayer();
    }

    // Change the zoom if necessary.
    // [JN] Moved from AM_Ticker for zooming interpolation.
    if (ftom_zoommul != FRACUNIT)
    {
        AM_changeWindowScale();
    }

    // Change X and Y location.
    // [JN] Moved from AM_Ticker for paning interpolation.
    if (m_paninc.x || m_paninc.y)
    {
        AM_changeWindowLoc();
    }

    // [crispy] required for AM_rotatePoint()
    if (crl_automap_rotate)
    {
        mapcenter.x = m_x + m_w / 2;
        mapcenter.y = m_y + m_h / 2;
        // [crispy] keep the map static in overlay mode
        // if not following the player
        if (!(!followplayer && crl_automap_overlay))
        {
            mapangle = ANG90 - plr->mo->angle;
        }
    }

    // Force bland if > 0 ? Because the normal colors could screw with the
    // very bright and shiny CRL colors
    // [JN] Use external config variable.
	if (automapactive == 2 && crl_automap_mode)
    {
		blandcolor = true;
    }
	else
    {
		blandcolor = false;
    }

	if (automapactive == 1 && !crl_automap_overlay)
    {
		AM_clearFB(BACKGROUND);
    }

    if (grid)
    {
        AM_drawGrid(GRIDCOLORS);
    }

    AM_drawWalls();

    // [JN] CRL - always colorize automap with given drawing mode.
    CRL_DrawMap(AM_CRLFLine, AM_CRLMLine);

    AM_drawPlayers();

    if (cheating == 2)
    {
        AM_drawThings(THINGCOLORS, THINGRANGE);
    }

    // [JN] CRL - draw pulsing triangle for player in Spectator mode.
    if (crl_spectating)
    {
        AM_drawSpectator();
    }

    // [JN] Do not draw in following mode.
    if (!followplayer)
    {
        AM_drawCrosshair();
    }

    AM_drawMarks();
	
    AM_MapNameDrawer();

    V_MarkRect(f_x, f_y, f_w, f_h);
}
