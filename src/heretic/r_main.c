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
// R_main.c

#include <stdlib.h>
#include <math.h>
#include "doomdef.h"
#include "m_bbox.h"
#include "r_local.h"
#include "tables.h"
#include "v_video.h"

#include "crlcore.h"
#include "crlvars.h"



// haleyjd: removed WATCOMC

int validcount = 1;             // increment every time a check is made

lighttable_t *fixedcolormap;

int centerx, centery;
fixed_t centerxfrac, centeryfrac;
fixed_t projection;

fixed_t viewx, viewy, viewz;
angle_t viewangle;
fixed_t viewcos, viewsin;
player_t *viewplayer;
localview_t localview; // [crispy]


//
// precalculated math tables
//
angle_t clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup maps the visible view
// angles  to screen X coordinates, flattening the arc to a flat projection
// plane.  There will be many angles mapped to the same X.
int viewangletox[FINEANGLES / 2];

// The xtoviewangleangle[] table maps a screen pixel to the lowest viewangle
// that maps back to x ranges from clipangle to -clipangle
angle_t xtoviewangle[SCREENWIDTH + 1];

lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t *scalelightfixed[MAXLIGHTSCALE];
lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

int extralight;                 // bumped light from gun blasts

void (*colfunc) (void);
void (*basecolfunc) (void);
void (*tlcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);

/*
===================
=
= R_AddPointToBox
=
===================
*/

void R_AddPointToBox(int x, int y, fixed_t * box)
{
    if (x < box[BOXLEFT])
        box[BOXLEFT] = x;
    if (x > box[BOXRIGHT])
        box[BOXRIGHT] = x;
    if (y < box[BOXBOTTOM])
        box[BOXBOTTOM] = y;
    if (y > box[BOXTOP])
        box[BOXTOP] = y;
}



/*
===============================================================================
=
= R_PointOnSide
=
= Returns side 0 (front) or 1 (back)
===============================================================================
*/

int R_PointOnSide(fixed_t x, fixed_t y, node_t * node)
{
    fixed_t dx, dy;
    fixed_t left, right;

    if (!node->dx)
    {
        if (x <= node->x)
            return node->dy > 0;
        return node->dy < 0;
    }
    if (!node->dy)
    {
        if (y <= node->y)
            return node->dx < 0;
        return node->dx > 0;
    }

    dx = (x - node->x);
    dy = (y - node->y);

// try to quickly decide by looking at sign bits
    if ((node->dy ^ node->dx ^ dx ^ dy) & 0x80000000)
    {
        if ((node->dy ^ dx) & 0x80000000)
            return 1;           // (left is negative)
        return 0;
    }

    left = FixedMul(node->dy >> FRACBITS, dx);
    right = FixedMul(dy, node->dx >> FRACBITS);

    if (right < left)
        return 0;               // front side
    return 1;                   // back side
}


int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t * line)
{
    fixed_t lx, ly;
    fixed_t ldx, ldy;
    fixed_t dx, dy;
    fixed_t left, right;

    lx = line->v1->x;
    ly = line->v1->y;

    ldx = line->v2->x - lx;
    ldy = line->v2->y - ly;

    if (!ldx)
    {
        if (x <= lx)
            return ldy > 0;
        return ldy < 0;
    }
    if (!ldy)
    {
        if (y <= ly)
            return ldx < 0;
        return ldx > 0;
    }

    dx = (x - lx);
    dy = (y - ly);

// try to quickly decide by looking at sign bits
    if ((ldy ^ ldx ^ dx ^ dy) & 0x80000000)
    {
        if ((ldy ^ dx) & 0x80000000)
            return 1;           // (left is negative)
        return 0;
    }

    left = FixedMul(ldy >> FRACBITS, dx);
    right = FixedMul(dy, ldx >> FRACBITS);

    if (right < left)
        return 0;               // front side
    return 1;                   // back side
}


/*
===============================================================================
=
= R_PointToAngle
=
===============================================================================
*/

angle_t R_PointToAngle(fixed_t x, fixed_t y)
{
    x -= viewx;
    y -= viewy;
    if ((!x) && (!y))
        return 0;
    if (x >= 0)
    {                           // x >=0
        if (y >= 0)
        {                       // y>= 0
            if (x > y)
                return tantoangle[SlopeDiv(y, x)];      // octant 0
            else
                return ANG90 - 1 - tantoangle[SlopeDiv(x, y)];  // octant 1
        }
        else
        {                       // y<0
            y = -y;
            if (x > y)
                return 0 - tantoangle[SlopeDiv(y, x)];     // octant 8
            else
                return ANG270 + tantoangle[SlopeDiv(x, y)];     // octant 7
        }
    }
    else
    {                           // x<0
        x = -x;
        if (y >= 0)
        {                       // y>= 0
            if (x > y)
                return ANG180 - 1 - tantoangle[SlopeDiv(y, x)]; // octant 3
            else
                return ANG90 + tantoangle[SlopeDiv(x, y)];      // octant 2
        }
        else
        {                       // y<0
            y = -y;
            if (x > y)
                return ANG180 + tantoangle[SlopeDiv(y, x)];     // octant 4
            else
                return ANG270 - 1 - tantoangle[SlopeDiv(x, y)]; // octant 5
        }
    }

    return 0;
}


angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
    viewx = x1;
    viewy = y1;
    return R_PointToAngle(x2, y2);
}


fixed_t R_PointToDist(fixed_t x, fixed_t y)
{
    int angle;
    fixed_t dx, dy, temp;
    fixed_t dist;

    dx = abs(x - viewx);
    dy = abs(y - viewy);

    if (dy > dx)
    {
        temp = dx;
        dx = dy;
        dy = temp;
    }

    angle =
        (tantoangle[FixedDiv(dy, dx) >> DBITS] + ANG90) >> ANGLETOFINESHIFT;

    dist = FixedDiv(dx, finesine[angle]);       // use as cosine

    return dist;
}


//=============================================================================

/*
================
=
= R_ScaleFromGlobalAngle
=
= Returns the texture mapping scale for the current line at the given angle
= rw_distance must be calculated first
================
*/

fixed_t R_ScaleFromGlobalAngle(angle_t visangle)
{
    fixed_t scale;
    angle_t anglea, angleb;
    int sinea, sineb;
    fixed_t num, den;

#if 0
    {
        fixed_t dist, z;
        fixed_t sinv, cosv;

        sinv = finesine[(visangle - rw_normalangle) >> ANGLETOFINESHIFT];
        dist = FixedDiv(rw_distance, sinv);
        cosv = finecosine[(viewangle - visangle) >> ANGLETOFINESHIFT];
        z = abs(FixedMul(dist, cosv));
        scale = FixedDiv(projection, z);
        return scale;
    }
#endif

    anglea = ANG90 + (visangle - viewangle);
    angleb = ANG90 + (visangle - rw_normalangle);
// bothe sines are allways positive
    sinea = finesine[anglea >> ANGLETOFINESHIFT];
    sineb = finesine[angleb >> ANGLETOFINESHIFT];
    num = FixedMul(projection, sineb);
    den = FixedMul(rw_distance, sinea);
    if (den > num >> 16)
    {
        scale = FixedDiv(num, den);
        if (scale > 64 * FRACUNIT)
            scale = 64 * FRACUNIT;
        else if (scale < 256)
            scale = 256;
    }
    else
        scale = 64 * FRACUNIT;

    return scale;
}

/*
=================
=
= R_InitTextureMapping
=
=================
*/

static void R_InitTextureMapping(void)
{
    int i;
    int x;
    int t;
    fixed_t focallength;


//
// use tangent table to generate viewangletox
// viewangletox will give the next greatest x after the view angle
//
    // calc focallength so FIELDOFVIEW angles covers SCREENWIDTH
    focallength =
        FixedDiv(centerxfrac, finetangent[FINEANGLES / 4 + FIELDOFVIEW / 2]);

    for (i = 0; i < FINEANGLES / 2; i++)
    {
        if (finetangent[i] > FRACUNIT * 2)
            t = -1;
        else if (finetangent[i] < -FRACUNIT * 2)
            t = viewwidth + 1;
        else
        {
            t = FixedMul(finetangent[i], focallength);
            t = (centerxfrac - t + FRACUNIT - 1) >> FRACBITS;
            if (t < -1)
                t = -1;
            else if (t > viewwidth + 1)
                t = viewwidth + 1;
        }
        viewangletox[i] = t;
    }

//
// scan viewangletox[] to generate xtoviewangleangle[]
//
// xtoviewangle will give the smallest view angle that maps to x
    for (x = 0; x <= viewwidth; x++)
    {
        i = 0;
        while (viewangletox[i] > x)
            i++;
        xtoviewangle[x] = (i << ANGLETOFINESHIFT) - ANG90;
    }

//
// take out the fencepost cases from viewangletox
//
    for (i = 0; i < FINEANGLES / 2; i++)
    {
        t = FixedMul(finetangent[i], focallength);
        t = centerx - t;
        if (viewangletox[i] == -1)
            viewangletox[i] = 0;
        else if (viewangletox[i] == viewwidth + 1)
            viewangletox[i] = viewwidth;
    }

    clipangle = xtoviewangle[0];
}

//=============================================================================

/*
====================
=
= R_InitLightTables
=
= Only inits the zlight table, because the scalelight table changes
= with view size
=
====================
*/

#define		DISTMAP	2

static void R_InitLightTables(void)
{
    int i, j, level, start_map;
    int scale;

//
// Calculate the light levels to use for each level / distance combination
//
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        start_map = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTZ; j++)
        {
            scale =
                FixedDiv((SCREENWIDTH / 2 * FRACUNIT),
                         (j + 1) << LIGHTZSHIFT);
            scale >>= LIGHTSCALESHIFT;
            level = start_map - scale / DISTMAP;
            if (level < 0)
                level = 0;
            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;
            zlight[i][j] = colormaps + level * 256;
        }
    }
}


/*
==============
=
= R_SetViewSize
=
= Don't really change anything here, because i might be in the middle of
= a refresh.  The change will take effect next refresh.
=
==============
*/

boolean setsizeneeded;
int setblocks, setdetail;

void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}

/*
==============
=
= R_ExecuteSetViewSize
=
==============
*/

void R_ExecuteSetViewSize(void)
{
    fixed_t cosadj, dy;
    int i, j, level, start_map;

    setsizeneeded = false;

    if (setblocks == 11)
    {
        scaledviewwidth = SCREENWIDTH;
        viewheight = SCREENHEIGHT;
    }
    else
    {
        scaledviewwidth = setblocks * 32;
        viewheight = (setblocks * 158 / 10);
    }

    viewwidth = scaledviewwidth;

    centery = viewheight / 2;
    centerx = viewwidth / 2;
    centerxfrac = centerx << FRACBITS;
    centeryfrac = centery << FRACBITS;
    projection = centerxfrac;

    colfunc = basecolfunc = R_DrawColumn;
    tlcolfunc = R_DrawTLColumn;
    transcolfunc = R_DrawTranslatedColumn;
    spanfunc = R_DrawSpan;

    R_InitBuffer(scaledviewwidth, viewheight);

    R_InitTextureMapping();

//
// psprite scales
//
    pspritescale = FRACUNIT * viewwidth / SCREENWIDTH;
    pspriteiscale = FRACUNIT * SCREENWIDTH / viewwidth;

//
// thing clipping
//
    for (i = 0; i < viewwidth; i++)
        screenheightarray[i] = viewheight;

//
// planes
//
    for (i = 0; i < viewheight; i++)
    {
        dy = ((i - viewheight / 2) << FRACBITS) + FRACUNIT / 2;
        dy = abs(dy);
        yslope[i] = FixedDiv(viewwidth / 2 * FRACUNIT, dy);
    }

    for (i = 0; i < viewwidth; i++)
    {
        cosadj = abs(finecosine[xtoviewangle[i] >> ANGLETOFINESHIFT]);
        distscale[i] = FixedDiv(FRACUNIT, cosadj);
    }

//
// Calculate the light levels to use for each level / scale combination
//
    for (i = 0; i < LIGHTLEVELS; i++)
    {
        start_map = ((LIGHTLEVELS - 1 - i) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
        for (j = 0; j < MAXLIGHTSCALE; j++)
        {
            level =
                start_map -
                j * SCREENWIDTH / viewwidth / DISTMAP;
            if (level < 0)
                level = 0;
            if (level >= NUMCOLORMAPS)
                level = NUMCOLORMAPS - 1;
            scalelight[i][j] = colormaps + level * 256;
        }
    }

    // Erase old menu stuff
    R_FillBackScreen();
}


/*
==============
=
= R_Init
=
==============
*/

int detailLevel;

void R_Init(void)
{
    // [JN] CRL - define static engine limits.
    CRL_SetStaticLimits("HERETIC+");

    //tprintf("R_InitData ", 1);
    R_InitData();
    printf (".");
    //tprintf("R_InitPointToAngle\n", 0);
    //R_InitPointToAngle();
    printf (".");
    //tprintf("R_InitTables ", 0);
    //R_InitTables();
    // viewwidth / viewheight / detailLevel are set by the defaults
    printf (".");
    R_SetViewSize(crl_screen_size, detailLevel);
    //tprintf("R_InitLightTables ", 0);
    R_InitLightTables();
    printf (".");
    //tprintf("R_InitSkyMap\n", 0);
    R_InitSkyMap();
    printf (".");
    R_InitTranslationTables();
}


/*
==============
=
= R_PointInSubsector
=
==============
*/

subsector_t *R_PointInSubsector(fixed_t x, fixed_t y)
{
    node_t *node;
    int side, nodenum;

    if (!numnodes)              // single subsector is a special case
        return subsectors;

    nodenum = numnodes - 1;

    while (!(nodenum & NF_SUBSECTOR))
    {
        node = &nodes[nodenum];
        side = R_PointOnSide(x, y, node);
        nodenum = node->children[side];
    }

    return &subsectors[nodenum & ~NF_SUBSECTOR];

}

// [crispy]
static inline boolean CheckLocalView(const player_t *player)
{
  return (
    // Don't use localview if the player is spying.
    player == &players[consoleplayer] &&
    // Don't use localview if the player is dead.
    player->playerstate != PST_DEAD &&
    // Don't use localview if the player just teleported.
    !player->mo->reactiontime &&
    // Don't use localview if a demo is playing or recording.
    !demoplayback && !demorecording &&
    // Don't use localview during a netgame (single-player only).
    !netgame
  );
}

//----------------------------------------------------------------------------
//
// PROC R_SetupFrame
//
//----------------------------------------------------------------------------

static void R_SetupFrame(player_t * player)
{
    int i;
    int tableAngle;
    int tempCentery;
    int pitch; // [crispy]

    viewplayer = player;
    // haleyjd: removed WATCOMC
    
    if (crl_spectating)
    {
        fixed_t bx, by, bz;
        angle_t ba;

    	// RestlessRodent -- Get camera position
    	CRL_GetCameraPos(&bx, &by, &bz, &ba);
        
        if (crl_uncapped_fps)
        {
            viewx = LerpFixed(CRL_camera_oldx, bx);
            viewy = LerpFixed(CRL_camera_oldy, by);
            viewz = LerpFixed(CRL_camera_oldz, bz);
            viewangle = LerpAngle(CRL_camera_oldang, ba);
            pitch = LerpFixed(CRL_camera_oldlookdir, CRL_camera_lookdir) / MLOOKUNIT;
        }
        else
        {
            viewx = bx;
            viewy = by;
            viewz = bz;
            viewangle = ba;
            pitch = CRL_camera_lookdir / MLOOKUNIT;
        }
    }
    else
    {
        const boolean use_localview = CheckLocalView(player);

        // [AM] Interpolate the player camera if the feature is enabled.
        if (crl_uncapped_fps &&
            // Don't interpolate on the first tic of a level,
            // otherwise oldviewz might be garbage.
            realleveltime > 1 &&
            // Don't interpolate if the player did something
            // that would necessitate turning it off for a tic.
            player->mo->interp == true &&
            // Don't interpolate during a paused state
            realleveltime > oldleveltime)
        {
            viewx = LerpFixed(player->mo->oldx, player->mo->x);
            viewy = LerpFixed(player->mo->oldy, player->mo->y);
            viewz = LerpFixed(player->oldviewz, player->viewz);

            if (use_localview)
            {
                viewangle = (player->mo->angle + localview.angle -
                            localview.ticangle + LerpAngle(localview.oldticangle,
                                                           localview.ticangle));
                pitch = LerpInt(player->r_oldlookdir, player->r_lookdir) / MLOOKUNIT;
            }
            else
            {
                viewangle = LerpAngle(player->mo->oldangle, player->mo->angle);
                pitch = LerpInt(player->oldlookdir, player->lookdir);
            }
        }
        else
        {
            viewx = player->mo->x;
            viewy = player->mo->y;
            viewz = player->viewz;
            viewangle = player->mo->angle;
            pitch = use_localview ? player->r_lookdir / MLOOKUNIT : // [JN] Precise vertical mouse look.
                                    player->lookdir; // [crispy]
        }
    }
    
    tableAngle = viewangle >> ANGLETOFINESHIFT;

    // [crispy] Set chicken attack view position
    if (player->chickenTics && player->chickenPeck)
    {
        viewx = player->mo->x + player->chickenPeck * finecosine[tableAngle];
        viewy = player->mo->y + player->chickenPeck * finesine[tableAngle];
    }

    extralight = player->extralight;
    extralight += crl_level_brightness;  // [JN] Extra Level Brightness feature.

    // [JN] RestlessRodent -- Just report it
    CRL_ReportPosition(viewx, viewy, viewz, viewangle);
    // [PN] Keep spectator pitch history in sync with position history.
    CRL_ReportLookdir(CRL_camera_lookdir);

    tempCentery = viewheight / 2 + pitch * crl_screen_size / 10;
    if (centery != tempCentery)
    {
        centery = tempCentery;
        centeryfrac = centery << FRACBITS;
        for (i = 0; i < viewheight; i++)
        {
            yslope[i] = FixedDiv(viewwidth / 2 * FRACUNIT,
                                 abs(((i - centery) << FRACBITS) +
                                     FRACUNIT / 2));
        }
    }
    viewsin = finesine[tableAngle];
    viewcos = finecosine[tableAngle];
    if (player->fixedcolormap)
    {
        // [PN] A11Y - Invulnerability grayscaled effect
        if (crl_a11y_invul 
        && (player->powers[pw_invulnerability] > BLINKTHRESHOLD || (player->powers[pw_invulnerability] & 8)))
        {
            fixedcolormap = grayscale_colormap;
        }
        else
        {
            fixedcolormap = colormaps + player->fixedcolormap * 256;
        }

        walllights = scalelightfixed;
        for (i = 0; i < MAXLIGHTSCALE; i++)
        {
            scalelightfixed[i] = fixedcolormap;
        }
    }
    else
    {
        fixedcolormap = 0;
    }
    validcount++;
}

/*
==============
=
= R_RenderView
=
==============
*/

void R_RenderPlayerView(player_t * player)
{
    int js;

	// [JN] RestlessRodent -- Start of frame
	CRL_ChangeFrame(0);

	// [JN] RestlessRodent -- Store current position and go back to it in case the
	// renderer does something fancy
	js = setjmp(CRLJustIncaseBuf);

	// [JN] RestlessRodent -- Do not spawn it just in case.
	if (js == 0)
	{
        R_SetupFrame(player);

		// Clear the view buffer
        // [JN] CRL - allow to choose HOM effect.
        // [PN] Sneaking asks for the same clear; what it refuses is filled
        // with black rather than the HOM cycling, so it reads as "not there".
        const boolean sneak_clear = CRL_SneakFrameBegin();

        // [PN] SHIMMER borrows the Doom trick: the window of three tics ago
        // goes under the frame instead of a color, so what stays uncovered shows
        // an old image. Sneaking still wants the plain clear to paint black.
        if (crl_hom_effect == 3 && !sneak_clear)
        {
            CRL_HomShimmer(viewwindowx, viewwindowy, scaledviewwidth, viewheight);
        }
        else if (crl_hom_effect || sneak_clear)
        {
            V_DrawFilledBox(viewwindowx, viewwindowy,
                            scaledviewwidth, viewheight,
                            sneak_clear ? CRL_sneak_homcolor : CRL_homcolor);
        }

        R_ClearClipSegs();
        R_ClearDrawSegs();
        R_ClearPlanes();
        R_ClearSprites();
        NetUpdate();                // check for new console commands
        if (!crl_freeze)
        {
            R_InterpolateTextureOffsets(); // [crispy] smooth texture scrolling
        }
        R_RenderBSPNode(numnodes - 1);      // the head node is the last node output
        NetUpdate();                // check for new console commands
        R_DrawPlanes();
        CRL_DrawVisPlanes(0);

        // [PN] CRL - Sneaking mode: outlines of what the mode refused
        CRL_SneakFrameEnd ();

        NetUpdate();                // check for new console commands
        R_DrawMasked();

        // [PN] CRL - Sneaking mode: recording ends with the masked pass
        CRL_SneakMaskedEnd ();

        NetUpdate();                // check for new console commands

        js = -1;                    // No errors, set jump to negative for OK
    }
    
    // [JN] RestlessRodent -- End of frame
    CRL_ChangeFrame(js);
}
