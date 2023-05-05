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
// DESCRIPTION:
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//


#include <stdio.h>
#include <stdlib.h>
#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"
#include "doomstat.h"
#include "p_local.h"
#include "m_misc.h"

#include "crlcore.h"
#include "crlvars.h"


//
// opening
//

// Here comes the obnoxious "visplane".
#define MAXVISPLANES	128
#define REALMAXVISPLANES	4096
visplane_t		visplanes[REALMAXVISPLANES];
visplane_t*		lastvisplane;
visplane_t*		floorplane;
visplane_t*		ceilingplane;

// ?
// [JN] CRL - remove MAXOPENINGS limit enterily. 
// Render limits level will still do actual drawing limit.
size_t  maxopenings;
int    *openings;     // [JN] 32-bit integer math
int    *lastopening;  // [JN] 32-bit integer math


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
int  floorclip[SCREENWIDTH];    // [JN] 32-bit integer math
int  ceilingclip[SCREENWIDTH];  // [JN] 32-bit integer math

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int			spanstart[SCREENHEIGHT];
int			spanstop[SCREENHEIGHT];

//
// texture mapping
//
lighttable_t**		planezlight;
fixed_t			planeheight;

fixed_t			yslope[SCREENHEIGHT];
fixed_t			distscale[SCREENWIDTH];
fixed_t			basexscale;
fixed_t			baseyscale;

fixed_t			cachedheight[SCREENHEIGHT];
fixed_t			cacheddistance[SCREENHEIGHT];
fixed_t			cachedxstep[SCREENHEIGHT];
fixed_t			cachedystep[SCREENHEIGHT];


/**
 * Identify the used visplane.
 *
 * @param __what The plane.
 * @param __info Plane information.
 * @return The plane value.
 */
void GAME_IdentifyPlane(void* __what, CRLPlaneData_t* __info)
{
	visplane_t* pl = ((visplane_t*)__what);
	
	// Set
	__info->id = (intptr_t)(pl - visplanes);
	__info->isf = pl->isfindplane;
	
	__info->emitline = pl->emitline;
	__info->emitlineid = pl->emitline - segs;
	if (__info->emitlineid < 0 || __info->emitlineid >= numsegs)
		__info->emitlineid = -1;
	
	__info->emitsub = pl->emitsub;
	__info->emitsubid = pl->emitsub - subsectors;
	if (__info->emitsubid < 0 || __info->emitsubid >= numsubsectors)
		__info->emitsubid = -1;
	
	__info->emitsect = NULL;
	__info->emitsectid = 0;
	if (pl->emitsub != NULL)
	{
		__info->emitsect = pl->emitsub->sector;
		__info->emitsectid = pl->emitsub->sector - sectors;
		if (__info->emitsectid < 0 || __info->emitsectid >= numsectors)
			__info->emitsectid = -1;
	}
	
	__info->onfloor = pl->height < viewz;
}

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes (void)
{
  // Doh!
}


//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void
R_MapPlane
( int		y,
  int		x1,
  int		x2, visplane_t* __plane)
{
    angle_t	angle;
    fixed_t	distance;
    fixed_t	length;
    unsigned	index;
	
#ifdef RANGECHECK
    if (x2 < x1
     || x1 < 0
     || x2 >= viewwidth
     || y > viewheight)
    {
	I_Error ("R_MapPlane: %i, %i at %i",x1,x2,y);
    }
#endif

    if (planeheight != cachedheight[y])
    {
	cachedheight[y] = planeheight;
	distance = cacheddistance[y] = FixedMul (planeheight, yslope[y]);
	ds_xstep = cachedxstep[y] = FixedMul (distance,basexscale);
	ds_ystep = cachedystep[y] = FixedMul (distance,baseyscale);
    }
    else
    {
	distance = cacheddistance[y];
	ds_xstep = cachedxstep[y];
	ds_ystep = cachedystep[y];
    }
	
    length = FixedMul (distance,distscale[x1]);
    angle = (viewangle + xtoviewangle[x1])>>ANGLETOFINESHIFT;
    ds_xfrac = viewx + FixedMul(finecosine[angle], length);
    ds_yfrac = -viewy - FixedMul(finesine[angle], length);

    if (fixedcolormap)
	ds_colormap = fixedcolormap;
    else
    {
	index = distance >> LIGHTZSHIFT;
	
	if (index >= MAXLIGHTZ )
	    index = MAXLIGHTZ-1;

	ds_colormap = planezlight[index];
    }
	
    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;

    // high or low detail
    dc_visplaneused = __plane;
    spanfunc ();	
    dc_visplaneused = NULL;
}


//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes (void)
{
    int		i;
    angle_t	angle;
    
    // opening / clipping determination
    for (i=0 ; i<viewwidth ; i++)
    {
	floorclip[i] = viewheight;
	ceilingclip[i] = -1;
    }

    lastvisplane = visplanes;
    lastopening = openings;
    
    // texture calculation
    memset (cachedheight, 0, sizeof(cachedheight));

    // left to right mapping
    angle = (viewangle-ANG90)>>ANGLETOFINESHIFT;
	
    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv (finecosine[angle],centerxfrac);
    baseyscale = -FixedDiv (finesine[angle],centerxfrac);
}




//
// R_FindPlane
//
visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel, seg_t* __line, subsector_t* __sub)
{
    visplane_t*	check;
	
    if (picnum == skyflatnum)
    {
	height = 0;			// all skys map together
	lightlevel = 0;
    }
	
    for (check=visplanes; check<lastvisplane; check++)
    {
	if (height == check->height
	    && picnum == check->picnum
	    && lightlevel == check->lightlevel)
	{
	    break;
	}
    }
    
			
    if (check < lastvisplane)
	return check;
		
    if (lastvisplane - visplanes == CRL_MaxVisPlanes)
	{
    	// [JN] Print in-game warning.
        CRL_SetCriticalMessage("R_FindPlane:", M_StringJoin("no more visplanes (",
                                            CRL_LimitsName, " crashes here)", NULL), 2);
    	longjmp(CRLJustIncaseBuf, CRL_JUMP_VPO);
	}
	
	// RestlessRodent -- Count plane before write
	CRL_CountPlane(check, 1, (intptr_t)(lastvisplane - visplanes));
    
    lastvisplane++;

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
    check->minx = SCREENWIDTH;
    check->maxx = -1;
    
    // RestlessRodent -- Store emitting seg
    check->isfindplane = 1;
    check->emitline = __line;
    check->emitsub = __sub;
    
    memset (check->top,0xff,sizeof(check->top));
		
    return check;
}


//
// R_CheckPlane
//
visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop, seg_t* __line, subsector_t* __sub)
{
    int		intrl;
    int		intrh;
    int		unionl;
    int		unionh;
    int		x = 0;
	
    if (start < pl->minx)
    {
	intrl = pl->minx;
	unionl = start;
    }
    else
    {
	unionl = pl->minx;
	intrl = start;
    }
	
    if (stop > pl->maxx)
    {
	intrh = pl->maxx;
	unionh = stop;
    }
    else
    {
	unionh = pl->maxx;
	intrh = stop;
    }

    for (x=intrl ; x<= intrh ; x++)
	if (pl->top[x] != 0xffffffffu) // [JN] hires / 32-bit integer math
	    break;
	
    if (x > intrh)
    {
	pl->minx = unionl;
	pl->maxx = unionh;

	// use the same one
	return pl;		
    }
	
    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;
    
    if (lastvisplane - visplanes == CRL_MaxVisPlanes)
    {
        // [JN] Print in-game warning.
        CRL_SetCriticalMessage("R_CheckPlane:", M_StringJoin("no more visplanes (",
                                            CRL_LimitsName, " crashes here)", NULL), 2);
    }

    pl = lastvisplane++;
    
	// RestlessRodent -- Count plane before write
	CRL_CountPlane(pl, 0, (intptr_t)((lastvisplane - 1) - visplanes));
    
    pl->minx = start;
    pl->maxx = stop;
    
    // RestlessRodent -- Store emitting seg
    pl->isfindplane = 0;
    pl->emitline = __line;
    pl->emitsub = __sub;

    memset (pl->top,0xff,sizeof(pl->top));
		
    return pl;
}


//
// R_MakeSpans
//
void
R_MakeSpans
( unsigned int		x,   // [JN] 32-bit integer math
  unsigned int		t1,  // [JN] 32-bit integer math
  unsigned int		b1,  // [JN] 32-bit integer math
  unsigned int		t2,  // [JN] 32-bit integer math
  unsigned int		b2,  // [JN] 32-bit integer math
  visplane_t* __plane)
{
    while (t1 < t2 && t1<=b1)
    {
	R_MapPlane (t1,spanstart[t1],x-1, __plane);
	t1++;
    }
    while (b1 > b2 && b1>=t1)
    {
	R_MapPlane (b1,spanstart[b1],x-1, __plane);
	b1--;
    }
	
    while (t2 < t1 && t2<=b2)
    {
	spanstart[t2] = x;
	t2++;
    }
    while (b2 > b1 && b2>=t2)
    {
	spanstart[b2] = x;
	b2--;
    }
}



//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes (void)
{
    visplane_t*		pl;
    int			light;
    int			x;
    int			stop;
    int			angle;
    int                 lumpnum;
    const int dsegs = ds_p - drawsegs;  // [JN] Shortcut.
				
    // [JN] CRL - openings counter.
    CRLData.numopenings = lastopening - openings;

#ifdef RANGECHECK
    // [JN] Print in-game warning about MAXDRAWSEGS overflow.
    if (dsegs > CRL_MaxDrawSegs)
    {
        CRL_SetCriticalMessage("R_DrawPlanes:", M_StringJoin("drawsegs overflow (",
                                            CRL_LimitsName, " crashes here)", NULL), 2);

        // Supress render and don't go any farther.
        return;
    }

    // [JN] Print in-game warning about MAVVISPLANES overflow.
    if (lastvisplane - visplanes > CRL_MaxVisPlanes)
    {
        CRL_SetCriticalMessage("R_DrawPlanes:", M_StringJoin("visplane overflow (",
                                            CRL_LimitsName, " crashes here)", NULL), 2);
    	longjmp(CRLJustIncaseBuf, CRL_JUMP_VPO);
    }
    
    // [JN] Print in-game warning about MAXOPENINGS overflow.
    if (lastopening - openings > CRL_MaxOpenings)
    {
        CRL_SetCriticalMessage("R_DrawPlanes:", M_StringJoin("opening overflow (",
                                            CRL_LimitsName, " crashes here)", NULL), 2);

        // Supress render and don't go any farther.
        return;
    }
#endif

    for (pl = visplanes ; pl < lastvisplane ; pl++)
    {
	if (pl->minx > pl->maxx)
	    continue;

	
	// sky flat
	if (pl->picnum == skyflatnum)
	{
	    dc_iscale = pspriteiscale>>detailshift;
	    
	    // Sky is allways drawn full bright,
	    //  i.e. colormaps[0] is used.
	    // Because of this hack, sky is not affected
	    //  by INVUL inverse mapping.
	    dc_colormap = colormaps;
	    dc_texturemid = skytexturemid;
	    for (x=pl->minx ; x <= pl->maxx ; x++)
	    {
		dc_yl = pl->top[x];
		dc_yh = pl->bottom[x];

		if ((unsigned) dc_yl <= dc_yh)  // [JN] 32-bit integer math
		{
		    angle = (viewangle + xtoviewangle[x])>>ANGLETOSKYSHIFT;
		    dc_x = x;
		    dc_source = R_GetColumn(skytexture, angle);
		    
		    dc_visplaneused = pl;
		    colfunc ();
		    dc_visplaneused = NULL;
		}
	    }
	    continue;
	}
	
	// regular flat
        lumpnum = firstflat + flattranslation[pl->picnum];
	ds_source = W_CacheLumpNum(lumpnum, PU_STATIC);
	
	planeheight = abs(pl->height-viewz);
	light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

	if (light >= LIGHTLEVELS)
	    light = LIGHTLEVELS-1;

	if (light < 0)
	    light = 0;

	planezlight = zlight[light];

	pl->top[pl->maxx+1] = 0xffffffffu; // [crispy] hires / 32-bit integer math
	pl->top[pl->minx-1] = 0xffffffffu; // [crispy] hires / 32-bit integer math
		
	stop = pl->maxx + 1;

	for (x=pl->minx ; x<= stop ; x++)
	{
	    R_MakeSpans(x,pl->top[x-1],
			pl->bottom[x-1],
			pl->top[x],
			pl->bottom[x], pl);
	}
	
        W_ReleaseLumpNum(lumpnum);
    }
}
