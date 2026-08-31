//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2026 Julia Nechaevskaya
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
//	Rendering main loop and setup functions,
//	 utility functions (BSP, geometry, trigonometry).
//	See tables.c, too.
//


#include <stdlib.h>
#include <string.h>     // [PN] memset for the Sneaking marks
#include <math.h>
#include "doomstat.h" // [AM] leveltime, paused, menuactive
#include "m_bbox.h"
#include "m_menu.h"
#include "p_local.h"
#include "r_local.h"    // [PN] segs, numsegs, viewz for the Sneaking mode
#include "v_video.h"
#include "w_wad.h"
#include "st_bar.h"

#include "crlcore.h"
#include "crlvars.h"


// Fineangles in the SCREENWIDTH wide window.
#define FIELDOFVIEW		2048	


int			viewangleoffset;

// increment every time a check is made
int			validcount = 1;		


lighttable_t*		fixedcolormap;

int			centerx;
int			centery;

fixed_t			centerxfrac;
fixed_t			centeryfrac;
fixed_t			projection;

fixed_t			viewx;
fixed_t			viewy;
fixed_t			viewz;

angle_t			viewangle;
localview_t		localview; // [crispy]

fixed_t			viewcos;
fixed_t			viewsin;

player_t*		viewplayer;

// 0 = high, 1 = low
int			detailshift;	

//
// precalculated math tables
//
angle_t			clipangle;

// The viewangletox[viewangle + FINEANGLES/4] lookup
// maps the visible view angles to screen X coordinates,
// flattening the arc to a flat projection plane.
// There will be many angles mapped to the same X. 
int			viewangletox[FINEANGLES/2];

// The xtoviewangleangle[] table maps a screen pixel
// to the lowest viewangle that maps back to x ranges
// from clipangle to -clipangle.
angle_t			xtoviewangle[SCREENWIDTH+1];

lighttable_t*		scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
lighttable_t*		scalelightfixed[MAXLIGHTSCALE];
lighttable_t*		zlight[LIGHTLEVELS][MAXLIGHTZ];

// bumped light from gun blasts
int			extralight;			



void (*colfunc) (void);
void (*basecolfunc) (void);
void (*fuzzcolfunc) (void);
void (*transcolfunc) (void);
void (*spanfunc) (void);



//
// R_AddPointToBox
// Expand a given bbox
// so that it encloses a given point.
//
void
R_AddPointToBox
( int		x,
  int		y,
  fixed_t*	box )
{
    if (x< box[BOXLEFT])
	box[BOXLEFT] = x;
    if (x> box[BOXRIGHT])
	box[BOXRIGHT] = x;
    if (y< box[BOXBOTTOM])
	box[BOXBOTTOM] = y;
    if (y> box[BOXTOP])
	box[BOXTOP] = y;
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//
int
R_PointOnSide
( fixed_t	x,
  fixed_t	y,
  const node_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
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
	
    // Try to quickly decide by looking at sign bits.
    if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (node->dy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( node->dy>>FRACBITS , dx );
    right = FixedMul ( dy , node->dx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


int
R_PointOnSegSide
( fixed_t	x,
  fixed_t	y,
  seg_t*	line )
{
    fixed_t	lx;
    fixed_t	ly;
    fixed_t	ldx;
    fixed_t	ldy;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
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
	
    // Try to quickly decide by looking at sign bits.
    if ( (ldy ^ ldx ^ dx ^ dy)&0x80000000 )
    {
	if  ( (ldy ^ dx) & 0x80000000 )
	{
	    // (left is negative)
	    return 1;
	}
	return 0;
    }

    left = FixedMul ( ldy>>FRACBITS , dx );
    right = FixedMul ( dy , ldx>>FRACBITS );
	
    if (right < left)
    {
	// front side
	return 0;
    }
    // back side
    return 1;			
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangle[] table.

//




angle_t
R_PointToAngle
( fixed_t	x,
  fixed_t	y )
{	
    x -= viewx;
    y -= viewy;
    
    if ( (!x) && (!y) )
	return 0;

    if (x>= 0)
    {
	// x >=0
	if (y>= 0)
	{
	    // y>= 0

	    if (x>y)
	    {
		// octant 0
		return tantoangle[ SlopeDiv(y,x)];
	    }
	    else
	    {
		// octant 1
		return ANG90-1-tantoangle[ SlopeDiv(x,y)];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 8
		return 0-tantoangle[SlopeDiv(y,x)];
	    }
	    else
	    {
		// octant 7
		return ANG270+tantoangle[ SlopeDiv(x,y)];
	    }
	}
    }
    else
    {
	// x<0
	x = -x;

	if (y>= 0)
	{
	    // y>= 0
	    if (x>y)
	    {
		// octant 3
		return ANG180-1-tantoangle[ SlopeDiv(y,x)];
	    }
	    else
	    {
		// octant 2
		return ANG90+ tantoangle[ SlopeDiv(x,y)];
	    }
	}
	else
	{
	    // y<0
	    y = -y;

	    if (x>y)
	    {
		// octant 4
		return ANG180+tantoangle[ SlopeDiv(y,x)];
	    }
	    else
	    {
		 // octant 5
		return ANG270-1-tantoangle[ SlopeDiv(x,y)];
	    }
	}
    }
    return 0;
}


angle_t
R_PointToAngle2
( fixed_t	x1,
  fixed_t	y1,
  fixed_t	x2,
  fixed_t	y2 )
{	
    viewx = x1;
    viewy = y1;
    
    return R_PointToAngle (x2, y2);
}


fixed_t
R_PointToDist
( fixed_t	x,
  fixed_t	y )
{
    int		angle;
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	temp;
    fixed_t	dist;
    fixed_t     frac;
	
    dx = abs(x - viewx);
    dy = abs(y - viewy);
	
    if (dy>dx)
    {
	temp = dx;
	dx = dy;
	dy = temp;
    }

    // Fix crashes in udm1.wad

    if (dx != 0)
    {
        frac = FixedDiv(dy, dx);
    }
    else
    {
	frac = 0;
    }
	
    angle = (tantoangle[frac>>DBITS]+ANG90) >> ANGLETOFINESHIFT;

    // use as cosine
    dist = FixedDiv (dx, finesine[angle] );	
	
    return dist;
}


//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//
fixed_t R_ScaleFromGlobalAngle (angle_t visangle)
{
    fixed_t		scale;
    angle_t		anglea;
    angle_t		angleb;
    int			sinea;
    int			sineb;
    fixed_t		num;
    int			den;

    // UNUSED
#if 0
{
    fixed_t		dist;
    fixed_t		z;
    fixed_t		sinv;
    fixed_t		cosv;
	
    sinv = finesine[(visangle-rw_normalangle)>>ANGLETOFINESHIFT];	
    dist = FixedDiv (rw_distance, sinv);
    cosv = finecosine[(viewangle-visangle)>>ANGLETOFINESHIFT];
    z = abs(FixedMul (dist, cosv));
    scale = FixedDiv(projection, z);
    return scale;
}
#endif

    anglea = ANG90 + (visangle-viewangle);
    angleb = ANG90 + (visangle-rw_normalangle);

    // both sines are allways positive
    sinea = finesine[anglea>>ANGLETOFINESHIFT];	
    sineb = finesine[angleb>>ANGLETOFINESHIFT];
    num = FixedMul(projection,sineb)<<detailshift;
    den = FixedMul(rw_distance,sinea);

    if (den > num>>FRACBITS)
    {
	scale = FixedDiv (num, den);

	if (scale > 64*FRACUNIT)
	    scale = 64*FRACUNIT;
	else if (scale < 256)
	    scale = 256;
    }
    else
	scale = 64*FRACUNIT;
	
    return scale;
}




//
// R_InitTextureMapping
//
static void R_InitTextureMapping (void)
{
    int			i;
    int			x;
    int			t;
    fixed_t		focallength;
    
    // Use tangent table to generate viewangletox:
    //  viewangletox will give the next greatest x
    //  after the view angle.
    //
    // Calc focallength
    //  so FIELDOFVIEW angles covers SCREENWIDTH.
    focallength = FixedDiv (centerxfrac,
			    finetangent[FINEANGLES/4+FIELDOFVIEW/2] );
	
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
	if (finetangent[i] > FRACUNIT*2)
	    t = -1;
	else if (finetangent[i] < -FRACUNIT*2)
	    t = viewwidth+1;
	else
	{
	    t = FixedMul (finetangent[i], focallength);
	    t = (centerxfrac - t+FRACUNIT-1)>>FRACBITS;

	    if (t < -1)
		t = -1;
	    else if (t>viewwidth+1)
		t = viewwidth+1;
	}
	viewangletox[i] = t;
    }
    
    // Scan viewangletox[] to generate xtoviewangle[]:
    //  xtoviewangle will give the smallest view angle
    //  that maps to x.	
    for (x=0;x<=viewwidth;x++)
    {
	i = 0;
	while (viewangletox[i]>x)
	    i++;
	xtoviewangle[x] = (i<<ANGLETOFINESHIFT)-ANG90;
    }
    
    // Take out the fencepost cases from viewangletox.
    for (i=0 ; i<FINEANGLES/2 ; i++)
    {
	t = FixedMul (finetangent[i], focallength);
	t = centerx - t;
	
	if (viewangletox[i] == -1)
	    viewangletox[i] = 0;
	else if (viewangletox[i] == viewwidth+1)
	    viewangletox[i]  = viewwidth;
    }
	
    clipangle = xtoviewangle[0];
}



//
// R_InitLightTables
// Only inits the zlight table,
//  because the scalelight table changes with view size.
//
#define DISTMAP		2

static void R_InitLightTables (void)
{
    int		i;
    int		j;
    int		level;
    int		start_map; 	
    int		scale;
    
    // Calculate the light levels to use
    //  for each level / distance combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
	start_map = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
	for (j=0 ; j<MAXLIGHTZ ; j++)
	{
	    scale = FixedDiv ((SCREENWIDTH/2*FRACUNIT), (j+1)<<LIGHTZSHIFT);
	    scale >>= LIGHTSCALESHIFT;
	    level = start_map - scale/DISTMAP;
	    
	    if (level < 0)
		level = 0;

	    if (level >= NUMCOLORMAPS)
		level = NUMCOLORMAPS-1;

	    zlight[i][j] = colormaps + level*256;
	}
    }
}

//
// R_InitSkyMap
// Called whenever the view size changes.
//
int			skyflatnum;
int			skytexture;
int			skytexturemid;

static void R_InitSkyMap (void)
{
    skytexturemid = SCREENHEIGHT/2*FRACUNIT;
}



//
// R_SetViewSize
// Do not really change anything here,
//  because it might be in the middle of a refresh.
// The change will take effect next refresh.
//
boolean		setsizeneeded;
int		setblocks;
int		setdetail;


void
R_SetViewSize
( int		blocks,
  int		detail )
{
    setsizeneeded = true;
    setblocks = blocks;
    setdetail = detail;
}


//
// R_ExecuteSetViewSize
//
void R_ExecuteSetViewSize (void)
{
    fixed_t	cosadj;
    fixed_t	dy;
    int		i;
    int		j;
    int		level;
    int		start_map; 	

    setsizeneeded = false;

    if (setblocks >= 11)
    {
	scaledviewwidth = SCREENWIDTH;
	viewheight = SCREENHEIGHT;
    }
    else
    {
	scaledviewwidth = setblocks*32;
	viewheight = (setblocks*168/10)&~7;
    }
    
    detailshift = setdetail;
    viewwidth = scaledviewwidth>>detailshift;
	
    centery = viewheight/2;
    centerx = viewwidth/2;
    centerxfrac = centerx<<FRACBITS;
    centeryfrac = centery<<FRACBITS;
    projection = centerxfrac;

    if (!detailshift)
    {
	colfunc = basecolfunc = R_DrawColumn;
	fuzzcolfunc = R_DrawFuzzColumn;
	transcolfunc = R_DrawTranslatedColumn;
	spanfunc = R_DrawSpan;
    }
    else
    {
	colfunc = basecolfunc = R_DrawColumnLow;
	fuzzcolfunc = R_DrawFuzzColumnLow;
	transcolfunc = R_DrawTranslatedColumnLow;
	spanfunc = R_DrawSpanLow;
    }

    R_InitBuffer (scaledviewwidth, viewheight);
	
    R_InitTextureMapping ();
    
    // psprite scales
    pspritescale = FRACUNIT*viewwidth/SCREENWIDTH;
    pspriteiscale = FRACUNIT*SCREENWIDTH/viewwidth;
    
    // thing clipping
    for (i=0 ; i<viewwidth ; i++)
	screenheightarray[i] = viewheight;
    
    // planes
    for (i=0 ; i<viewheight ; i++)
    {
	dy = ((i-viewheight/2)<<FRACBITS)+FRACUNIT/2;
	dy = abs(dy);
	yslope[i] = FixedDiv ( (viewwidth<<detailshift)/2*FRACUNIT, dy);
    }
	
    for (i=0 ; i<viewwidth ; i++)
    {
	cosadj = abs(finecosine[xtoviewangle[i]>>ANGLETOFINESHIFT]);
	distscale[i] = FixedDiv (FRACUNIT,cosadj);
    }
    
    // Calculate the light levels to use
    //  for each level / scale combination.
    for (i=0 ; i< LIGHTLEVELS ; i++)
    {
	start_map = ((LIGHTLEVELS-1-i)*2)*NUMCOLORMAPS/LIGHTLEVELS;
	for (j=0 ; j<MAXLIGHTSCALE ; j++)
	{
	    level = start_map - j*SCREENWIDTH/(viewwidth<<detailshift)/DISTMAP;
	    
	    if (level < 0)
		level = 0;

	    if (level >= NUMCOLORMAPS)
		level = NUMCOLORMAPS-1;

	    scalelight[i][j] = colormaps + level*256;
	}
    }

    // Erase old menu stuff
    R_FillBackScreen();

    st_fullupdate = true; // [JN] Redraw status bar background.
}



//
// R_Init
//



void R_Init (void)
{
    // [JN] CRL - define static engine limits.
    CRL_SetStaticLimits("DOOM+");

    R_InitData ();
    printf (".");
    // viewwidth / viewheight / detailLevel are set by the defaults
    R_SetViewSize (crl_screen_size, detailLevel);
    R_InitPlanes ();
    printf (".");
    R_InitLightTables ();
    printf (".");
    R_InitSkyMap ();
    R_InitTranslationTables ();
    printf (".");
    printf ("]");
}


//
// R_PointInSubsector
//
subsector_t*
R_PointInSubsector
( fixed_t	x,
  fixed_t	y )
{
    node_t*	node;
    int		side;
    int		nodenum;

    // single subsector is a special case
    if (!numnodes)				
	return subsectors;
		
    nodenum = numnodes-1;

    while (! (nodenum & NF_SUBSECTOR) )
    {
	node = &nodes[nodenum];
	side = R_PointOnSide (x, y, node);
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


//
// R_SetupFrame
//
static void R_SetupFrame (player_t* player)
{		
    int		i;
    
    viewplayer = player;
    
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
        }
        else
        {
            viewx = bx;
            viewy = by;
            viewz = bz;
            viewangle = ba;
        }
    }
    else
    {
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
            const boolean use_localview = CheckLocalView(player);

            viewx = LerpFixed(player->mo->oldx, player->mo->x);
            viewy = LerpFixed(player->mo->oldy, player->mo->y);
            viewz = LerpFixed(player->oldviewz, player->viewz);

            if (use_localview)
            {
                viewangle = (player->mo->angle + localview.angle -
                            localview.ticangle + LerpAngle(localview.oldticangle,
                                                           localview.ticangle)) + viewangleoffset;
            }
            else
            {
                viewangle = LerpAngle(player->mo->oldangle, player->mo->angle) + viewangleoffset;
            }
        }
        else
        {
            viewx = player->mo->x;
            viewy = player->mo->y;
            viewz = player->viewz;
            viewangle = player->mo->angle + viewangleoffset;
        }
	}
    
    extralight = player->extralight;
    extralight += crl_level_brightness;  // [JN] Extra Level Brightness feature.
    
    // RestlessRodent -- Just report it
    CRL_ReportPosition(viewx, viewy, viewz, viewangle);
    
    viewsin = finesine[viewangle>>ANGLETOFINESHIFT];
    viewcos = finecosine[viewangle>>ANGLETOFINESHIFT];
	
    if (player->fixedcolormap)
    {
    // [PN] A11Y - Invulnerability grayscaled effect
    if (crl_a11y_invul 
    && (player->powers[pw_invulnerability] > (4 * 32) || (player->powers[pw_invulnerability] & 8)))
    {
        fixedcolormap = grayscale_colormap;
    }
    else
    {
        fixedcolormap = colormaps + player->fixedcolormap * 256;
    }
	
	walllights = scalelightfixed;

	for (i=0 ; i<MAXLIGHTSCALE ; i++)
	    scalelightfixed[i] = fixedcolormap;
    }
    else
	fixedcolormap = 0;
		
    validcount++;
}


// -----------------------------------------------------------------------------
// CRL_SneakFrameBegin, CRL_SneakFrameEnd, CRL_SneakAllowWall, CRL_SneakAllowSector
//  [PN] Sneaking mode: the walls and the sector surfaces that have been drawn
//  in the frame captured when the mode was turned on are remembered, and from
//  that frame on nothing else is allowed to draw. The projection stays live, so
//  the known geometry keeps moving as the player moves, while what he has not
//  seen yet is not rendered at all, only outlined. Sprites take no part and keep
//  animating, and the collision is not touched.
// -----------------------------------------------------------------------------

// [PN] Per seg: was its wall drawn in the reference frame.
#define SNEAK_WALL 1

// [PN] Per sector: were its ceiling and its floor drawn in the reference frame.
#define SNEAK_CEILING 1
#define SNEAK_FLOOR   2

static byte *crl_sneak_marks;
static byte *crl_sneak_sectors;

// [PN] Spans the mode refused, in the same layout as CRLPlaneSurface: one table
//  for planes, one for walls. Plain static arrays on purpose, the zone is kept
//  for the game itself.
static void *crl_sneak_hidden[SCREENAREA];
static void *crl_sneak_hidden_segs[SCREENAREA];

static int     crl_sneak_numsegs;
static int     crl_sneak_numsectors;
static boolean crl_sneak_capture = true;
static boolean crl_sneak_record;

// [PN] Forget what has been drawn: on a new level and on toggling the mode.
void CRL_InvalidateSneak (void)
{
    crl_sneak_capture = true;
}

// [PN] Called before the frame is rendered. Returns whether the view buffer has
// to be cleared: a part of the world that is not allowed to draw leaves its
// pixels alone, so without a clear they would keep the previous frame there.
boolean CRL_SneakFrameBegin (void)
{
    if (!crl_sneaking)
    {
        // [PN] The mode is not game state, so its memory goes to the C heap
        // and not to the zone, which the port keeps limited as in DOS.
        free(crl_sneak_marks);
        free(crl_sneak_sectors);

        crl_sneak_marks = NULL;
        crl_sneak_sectors = NULL;
        crl_sneak_numsegs = crl_sneak_numsectors = 0;

        return false;
    }

    // [PN] Another level has another amount of segs and sectors. A failed
    // allocation leaves the mode without its tables, which the predicates read
    // as "draw everything".
    if (crl_sneak_marks == NULL || numsegs != crl_sneak_numsegs)
    {
        free(crl_sneak_marks);

        crl_sneak_marks   = malloc(numsegs * sizeof(*crl_sneak_marks));
        crl_sneak_numsegs = numsegs;

        crl_sneak_capture = true;
    }

    if (crl_sneak_sectors == NULL || numsectors != crl_sneak_numsectors)
    {
        free(crl_sneak_sectors);

        crl_sneak_sectors    = malloc(numsectors * sizeof(*crl_sneak_sectors));
        crl_sneak_numsectors = numsectors;

        crl_sneak_capture = true;
    }

    if (crl_sneak_capture)
    {
        memset(crl_sneak_marks, 0, numsegs * sizeof(*crl_sneak_marks));
        memset(crl_sneak_sectors, 0, numsectors * sizeof(*crl_sneak_sectors));

        crl_sneak_capture = false;
        crl_sneak_record  = true;
    }
    else
    {
        // [PN] A frame the renderer abandoned through its overflow guards
        // never reaches the end of the pass, so recording ends here as well.
        crl_sneak_record = false;
    }

    // [PN] The refused spans live one frame only.
    memset(crl_sneak_hidden, 0, sizeof(crl_sneak_hidden));
    memset(crl_sneak_hidden_segs, 0, sizeof(crl_sneak_hidden_segs));

    return true;
}

// [PN] The planes are drawn: outline what the mode refused, walls included, so
// that the outlines stay under the sprites. Recording continues, because masked
// midtextures are gated later, in the masked pass.
void CRL_SneakFrameEnd (void)
{
    if (crl_sneaking)
    {
        CRL_DrawPlaneBorders(crl_sneak_hidden, 0);
        CRL_DrawPlaneBorders(crl_sneak_hidden_segs, 1);
    }
}

// [PN] The masked pass is over: nothing to draw here anymore, only stop
// recording and gating, so the overlays that follow are not affected.
void CRL_SneakMaskedEnd (void)
{
    crl_sneak_record = false;
}

// [PN] Remember that a span was refused on these rows of this view column, so
// that its shape can be outlined. R_InitBuffer keeps the plane rows relative to
// the view window and folds its offsets into ylookup and columnofs, which live
// in r_draw.c, so the address is built the same way here. In blocky mode one
// view column covers two screen pixels, like in R_DrawColumnLow, and the
// visplane marking fills both of them, so this does the same.
static void CRL_SneakHide (void **__table, void *what, int x, int __top, int __bottom)
{
    const int block = 1 << detailshift; // [PN] screen pixels per view column
    int y, i;

    if (x < 0 || x >= viewwidth)
    {
        return;
    }

    x <<= detailshift;

    for (y = __top; y <= __bottom; y++)
    {
        if (y >= 0 && y < viewheight)
        {
            pixel_t *dest = (pixel_t *) CRLSurface
                          + (y + viewwindowy) * SCREENWIDTH + viewwindowx + x;

            for (i = 0; i < block; i++)
            {
                CRL_MarkPixelP(__table, what, dest + i);
            }
        }
    }
}

// [PN] A plane the mode refused to draw its spans of.
void CRL_SneakHideSpan (void *plane, int x, int __top, int __bottom)
{
    CRL_SneakHide(crl_sneak_hidden, plane, x, __top, __bottom);
}

// [PN] A wall the mode refused to draw a column of.
void CRL_SneakHideWall (void *seg, int x, int __top, int __bottom)
{
    CRL_SneakHide(crl_sneak_hidden_segs, seg, x, __top, __bottom);
}

// [PN] Position of a seg within the level, or -1 when it is not one of them.
static int CRL_SneakSegIndex (const seg_t *seg)
{
    const ptrdiff_t index = seg - segs;

    if (index < 0 || index >= (ptrdiff_t) numsegs)
    {
        return -1;
    }

    return (int) index;
}

// [PN] May this seg draw its wall? In the reference frame it is remembered
// instead, and drawn of course.
boolean CRL_SneakAllowWall (const seg_t *seg)
{
    if (!crl_sneaking || crl_sneak_marks == NULL)
    {
        return true;
    }

    const int index = CRL_SneakSegIndex(seg);

    if (index < 0)
    {
        return true;
    }

    if (crl_sneak_record)
    {
        crl_sneak_marks[index] |= SNEAK_WALL;
        return true;
    }

    return (crl_sneak_marks[index] & SNEAK_WALL) != 0;
}

// [PN] May the ceiling or the floor of this sector mark its span? Visplanes are
// merged by flat, height and light, so a plane is not an object one can
// recognize, but the span always belongs to the sector it comes from.
boolean CRL_SneakAllowSector (const sector_t *sector, boolean ceiling)
{
    if (!crl_sneaking || crl_sneak_sectors == NULL)
    {
        return true;
    }

    const ptrdiff_t index = sector - sectors;
    const byte      mark  = ceiling ? SNEAK_CEILING : SNEAK_FLOOR;

    // [PN] Not one of the level's sectors: better draw than not to.
    if (index < 0 || index >= (ptrdiff_t) numsectors)
    {
        return true;
    }

    if (crl_sneak_record)
    {
        crl_sneak_sectors[index] |= mark;
        return true;
    }

    return (crl_sneak_sectors[index] & mark) != 0;
}


//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{
	int js;
	
	// RestlessRodent -- Start of frame
	CRL_ChangeFrame(0);
	
	// RestlessRodent -- Store current position and go back to it in case the
	// renderer does something fancy
	js = setjmp(CRLJustIncaseBuf);
	
	// RestlessRodent -- Do not spawn it just in case.
	if (js == 0)
	{
		// Start frame
		R_SetupFrame (player);
		
		// Clear the view buffer
		// [JN] CRL - allow to choose HOM effect.
		// [PN] Sneaking asks for the same clear: what the mode does not allow
		// to draw would otherwise keep the previous frame in its pixels.
		const boolean sneak_clear = CRL_SneakFrameBegin();

		if (crl_hom_effect || sneak_clear)
		{
			V_DrawFilledBox(viewwindowx, viewwindowy,
							scaledviewwidth, viewheight,
							sneak_clear ? 0 : CRL_homcolor);  // [JN] TODO - optional different HOM for sneaking?
		}

		// Clear buffers.
		R_ClearClipSegs ();
		R_ClearDrawSegs ();
		R_ClearPlanes ();
		R_ClearSprites ();

		// check for new console commands.
		NetUpdate ();

		// [crispy] smooth texture scrolling
		if (!crl_freeze)
		{
			R_InterpolateTextureOffsets();
		}

		// The head node is the last node output.
		R_RenderBSPNode (numnodes-1);
		
		// Check for new console commands.
		NetUpdate ();
		
		// RestlessRodent -- Draw Visplanes
		R_DrawPlanes ();
		CRL_DrawVisPlanes(0);

		// [PN] CRL - Sneaking mode stops gating here, sprites are not affected
		CRL_SneakFrameEnd ();
		
		// Check for new console commands.
		NetUpdate ();
		
		// [crispy] draw fuzz effect independent of rendering frame rate
		R_SetFuzzPosDraw();
		R_DrawMasked ();

		// [PN] CRL - Sneaking mode: recording ends with the masked pass
		CRL_SneakMaskedEnd ();

		// Check for new console commands.
		NetUpdate ();
		
		// RestlessRodent -- No errors, set jump to negative for OK
		js = -1;
	}

	// RestlessRodent -- End of frame
	CRL_ChangeFrame(js);
}
