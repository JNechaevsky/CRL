//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
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

#pragma once

#include "i_video.h"
#include "v_patch.h"

#include "crlcore.h"


#define ANGLETOSKYSHIFT     22      // sky map is 256*128*4 maps
#define BASEYCENTER         100
#define MAXHEIGHT           832
#define PI                  3.141592657
#define MINZ                (FRACUNIT * 4)
#define FIELDOFVIEW         2048    // fineangles in the SCREENWIDTH wide window
#define MAXWIDTH			1120

//
// lighting constants
//
#define	LIGHTLEVELS         16
#define	LIGHTSEGSHIFT       4
#define	MAXLIGHTSCALE       48
#define	LIGHTSCALESHIFT     12
#define	MAXLIGHTZ           128
#define	LIGHTZSHIFT         20
#define	NUMCOLORMAPS        32      // number of diminishing
#define	INVERSECOLORMAP     32


/*
==============================================================================

					INTERNAL MAP TYPES

==============================================================================
*/

//================ used by play and refresh

typedef struct
{
    fixed_t x, y;
} vertex_t;

struct line_s;

typedef struct
{
    fixed_t floorheight, ceilingheight;
    short floorpic, ceilingpic;
    short lightlevel;
    short special, tag;

    int soundtraversed;         // 0 = untraversed, 1,2 = sndlines -1
    mobj_t *soundtarget;        // thing that made a sound (or null)

    int blockbox[4];            // mapblock bounding box for height changes
    degenmobj_t soundorg;       // for any sounds played by the sector

    int validcount;             // if == validcount, already checked
    mobj_t *thinglist;          // list of mobjs in sector
    void *specialdata;          // thinker_t for reversable actions
    int linecount;
    struct line_s **lines;      // [linecount] size

    // [AM] Previous position of floor and ceiling before
    //      think.  Used to interpolate between positions.
    fixed_t	oldfloorheight;
    fixed_t	oldceilingheight;

    // [AM] Gametic when the old positions were recorded.
    //      Has a dual purpose; it prevents movement thinkers
    //      from storing old positions twice in a tic, and
    //      prevents the renderer from attempting to interpolate
    //      if old values were not updated recently.
    int     oldgametic;

    // [AM] Interpolated floor and ceiling height.
    //      Calculated once per tic and used inside
    //      the renderer.
    fixed_t	interpfloorheight;
    fixed_t	interpceilingheight;
} sector_t;

typedef struct
{
    fixed_t textureoffset;      // add this to the calculated texture col
    fixed_t rowoffset;          // add this to the calculated texture top
    short toptexture, bottomtexture, midtexture;
    sector_t *sector;

    // [crispy] smooth texture scrolling
    fixed_t	basetextureoffset;
} side_t;

typedef enum
{ ST_HORIZONTAL, ST_VERTICAL, ST_POSITIVE, ST_NEGATIVE } slopetype_t;

typedef struct line_s
{
    vertex_t *v1, *v2;
    fixed_t dx, dy;             // v2 - v1 for side checking
    short flags;
    short special, tag;
    short sidenum[2];           // sidenum[1] will be -1 if one sided
    fixed_t bbox[4];
    slopetype_t slopetype;      // to aid move clipping
    sector_t *frontsector, *backsector;
    int validcount;             // if == validcount, already checked
    void *specialdata;          // thinker_t for reversable actions
} line_t;


typedef struct subsector_s
{
    sector_t *sector;
    short numlines;
    short firstline;
} subsector_t;

typedef struct
{
    vertex_t *v1, *v2;
    fixed_t offset;
    angle_t angle;
    side_t *sidedef;
    line_t *linedef;
    sector_t *frontsector;
    sector_t *backsector;       // NULL for one sided lines
} seg_t;

typedef struct
{
    fixed_t x, y, dx, dy;       // partition line
    fixed_t bbox[2][4];         // bounding box for each child
    unsigned short children[2]; // if NF_SUBSECTOR its a subsector
} node_t;


/*
==============================================================================

						OTHER TYPES

==============================================================================
*/

typedef byte lighttable_t;      // this could be wider for >8 bit display

typedef struct
{
    fixed_t height;
    int picnum;
    int lightlevel;
    int special;
    int minx, maxx;

    // [JN] CRL visplane data:
    int         isfindplane;    // Is a find plane.
    seg_t       *emitline;      // The seg that emitted this.
    subsector_t *emitsub;       // The subsector this visplane is in.

    // leave pads for [minx-1]/[maxx+1]
    unsigned short pad1; 
    unsigned short top[SCREENWIDTH];
    unsigned short pad2;
    unsigned short pad3;
    unsigned short bottom[SCREENWIDTH];
    unsigned short pad4;
} visplane_t;

typedef struct drawseg_s
{
    seg_t *curline;
    int x1, x2;
    fixed_t scale1, scale2, scalestep;
    int silhouette;             // 0=none, 1=bottom, 2=top, 3=both
    fixed_t bsilheight;         // don't clip sprites above this
    fixed_t tsilheight;         // don't clip sprites below this
// pointers to lists for sprite clipping
// adjusted so [x1] is first value
    int *sprtopclip;            // [JN] 32-bit integer math
    int *sprbottomclip;         // [JN] 32-bit integer math
    int *maskedtexturecol;      // [JN] 32-bit integer math
} drawseg_t;

#define	SIL_NONE	0
#define	SIL_BOTTOM	1
#define SIL_TOP		2
#define	SIL_BOTH	3

#define	MAXDRAWSEGS		256
#define REALMAXDRAWSEGS	2048

// A vissprite_t is a thing that will be drawn during a refresh
typedef struct vissprite_s
{
    struct vissprite_s *prev, *next;
    int x1, x2;
    fixed_t gx, gy;             // for line side calculation
    fixed_t gz, gzt;            // global bottom / top for silhouette clipping
    fixed_t startfrac;          // horizontal position of x1
    fixed_t scale;
    fixed_t xiscale;            // negative if flipped
    fixed_t texturemid;
    int patch;
    lighttable_t *colormap;
    int mobjflags;              // for color translation and shadow draw
    boolean psprite;            // true if psprite
    fixed_t footclip;           // foot clipping
} vissprite_t;

// Sprites are patches with a special naming convention so they can be 
// recognized by R_InitSprites.  The sprite and frame specified by a 
// thing_t is range checked at run time.
// a sprite is a patch_t that is assumed to represent a three dimensional
// object and may have multiple rotations pre drawn.  Horizontal flipping 
// is used to save space. Some sprites will only have one picture used
// for all views.  

typedef struct
{
    boolean rotate;             // if false use 0 for any position
    short lump[8];              // lump to use for view angles 0-7
    byte flip[8];               // flip (1 = flip) to use for view angles 0-7
} spriteframe_t;

typedef struct
{
    int numframes;
    spriteframe_t *spriteframes;
} spritedef_t;

extern spritedef_t *sprites;
extern int numsprites;

extern int numvertexes;
extern vertex_t *vertexes;

extern int numsegs;
extern seg_t *segs;

extern int numsectors;
extern sector_t *sectors;

extern int numsubsectors;
extern subsector_t *subsectors;

extern int numnodes;
extern node_t *nodes;

extern int numlines;
extern line_t *lines;

extern int numsides;
extern side_t *sides;

//==============================================================================

// -----------------------------------------------------------------------------
// R_BSP.C
// -----------------------------------------------------------------------------

extern seg_t *curline;
extern side_t *sidedef;
extern line_t *linedef;
extern sector_t *frontsector, *backsector;

extern drawseg_t  drawsegs[REALMAXDRAWSEGS];
extern drawseg_t *ds_p;

extern void R_ClearClipSegs(void);
extern void R_ClearDrawSegs(void);
extern void R_RenderBSPNode(int bspnum);

// -----------------------------------------------------------------------------
// R_DATA.C
// -----------------------------------------------------------------------------

extern byte **texturecomposite;
extern byte  *R_GetColumn(int tex, int col);

extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;
extern fixed_t *spritewidth;    // needed for pre rendering (fracs)
extern fixed_t *textureheight;  // needed for texture pegging

extern int *texturecompositesize;
extern int *flattranslation;    // for global animation
extern int *texturetranslation; // for global animation
extern int  firstflat;
extern int  firstspritelump, lastspritelump, numspritelumps;
extern int  numflats;

extern lighttable_t *colormaps;

extern void R_InitData(void);
extern void R_PrecacheLevel(void);

// -----------------------------------------------------------------------------
// R_DRAW.C
// -----------------------------------------------------------------------------

extern byte *dc_source;         // first pixel in a column
extern byte *dc_translation;
extern byte *ds_source;         // start of a 64*64 tile image
extern byte *translationtables;
extern byte *ylookup[MAXHEIGHT];

extern fixed_t dc_iscale;
extern fixed_t dc_texturemid;
extern fixed_t ds_xfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_yfrac;
extern fixed_t ds_ystep;

extern int columnofs[MAXWIDTH];
extern int dc_x;
extern int dc_yh;
extern int dc_yl;
extern int ds_x1;
extern int ds_x2;
extern int ds_y;

extern lighttable_t *dc_colormap;
extern lighttable_t *ds_colormap;

extern visplane_t *dc_visplaneused; // RestlessRodent -- CRL

extern void R_DrawColumn(void);
extern void R_DrawColumnLow(void);
extern void R_DrawSpan(void);
extern void R_DrawSpanLow(void);
extern void R_DrawTLColumn(void);
extern void R_DrawTLColumnLow(void);
extern void R_DrawTranslatedColumn(void);
extern void R_DrawTranslatedColumnLow(void);
extern void R_DrawTranslatedTLColumn(void);
extern void R_InitBuffer(int width, int height);
extern void R_InitTranslationTables(void);

// -----------------------------------------------------------------------------
// R_MAIN.C
// -----------------------------------------------------------------------------

extern angle_t clipangle;
extern angle_t R_PointToAngle(fixed_t x, fixed_t y);
extern angle_t R_PointToAngle2(fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
extern angle_t viewangle;
extern angle_t xtoviewangle[SCREENWIDTH + 1];

extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern fixed_t projection;
extern fixed_t R_PointToDist(fixed_t x, fixed_t y);
extern fixed_t R_ScaleFromGlobalAngle(angle_t visangle);
extern fixed_t viewcos, viewsin;
extern fixed_t viewx, viewy, viewz;

// [crispy]
typedef struct localview_s
{
    angle_t oldticangle;
    angle_t ticangle;
    short ticangleturn;
    double rawangle;
    angle_t angle;
} localview_t;

extern int centerx, centery;
extern int detailLevel;
extern int detailshift;         // 0 = high, 1 = low
extern int extralight;
extern int flyheight;
extern int R_PointOnSegSide(fixed_t x, fixed_t y, seg_t * line);
extern int R_PointOnSide(fixed_t x, fixed_t y, node_t * node);
extern int scaledviewwidth;
extern int validcount;
extern int viewwidth, viewheight, viewwindowx, viewwindowy;
extern int viewangletox[FINEANGLES / 2];

extern lighttable_t *fixedcolormap;
extern lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern lighttable_t *scalelightfixed[MAXLIGHTSCALE];
extern lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

extern player_t *viewplayer;
extern localview_t localview; // [crispy]

extern subsector_t *R_PointInSubsector(fixed_t x, fixed_t y);

extern void (*basecolfunc) (void);
extern void (*colfunc) (void);
extern void (*spanfunc) (void);
extern void (*tlcolfunc) (void);

extern void R_AddPointToBox(int x, int y, fixed_t * box);
extern void R_InterpolateTextureOffsets (void);

inline static fixed_t LerpFixed(fixed_t oldvalue, fixed_t newvalue)
{
    return (oldvalue + FixedMul(newvalue - oldvalue, fractionaltic));
}

inline static int LerpInt(int oldvalue, int newvalue)
{
    return (oldvalue + (int)((newvalue - oldvalue) * FIXED2DOUBLE(fractionaltic)));
}

// [AM] Interpolate between two angles.
inline static angle_t LerpAngle(angle_t oangle, angle_t nangle)
{
    if (nangle == oangle)
        return nangle;
    else if (nangle > oangle)
    {
        if (nangle - oangle < ANG270)
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
    }
    else // nangle < oangle
    {
        if (oangle - nangle < ANG270)
            return oangle - (angle_t)((oangle - nangle) * FIXED2DOUBLE(fractionaltic));
        else // Wrapped around
            return oangle + (angle_t)((nangle - oangle) * FIXED2DOUBLE(fractionaltic));
    }
}

// -----------------------------------------------------------------------------
// R_PLANE.C
// -----------------------------------------------------------------------------

#define MAXVISPLANES        128
#define REALMAXVISPLANES    SHRT_MAX

extern fixed_t distscale[SCREENWIDTH];
extern fixed_t yslope[SCREENHEIGHT];

extern int *lastopening; // [JN] 32-bit integer math
extern int *openings;    // [JN] 32-bit integer math

extern int ceilingclip[SCREENWIDTH];
extern int floorclip[SCREENWIDTH];
extern int skyflatnum;

extern size_t  maxopenings; // [JN] 32-bit integer math

extern visplane_t *floorplane, *ceilingplane;
extern visplane_t  visplanes[REALMAXVISPLANES];
extern visplane_t *lastvisplane;
extern visplane_t *R_CheckPlane(visplane_t * pl, int start, int stop,
                                seg_t* __line, subsector_t* __sub);
extern visplane_t *R_FindPlane(fixed_t height, int picnum, int lightlevel, int special,
                                seg_t* __line, subsector_t* __sub);

extern void R_InitSkyMap(void);
extern void R_ClearPlanes(void);
extern void R_MakeSpans(unsigned int x, unsigned int t1, unsigned int b1,
                        unsigned int t2, unsigned int b2, visplane_t* __plane);
extern void R_DrawPlanes(void);

// -----------------------------------------------------------------------------
// R_SEGS.C
// -----------------------------------------------------------------------------

extern angle_t rw_normalangle;

extern boolean segtextured;
extern boolean markfloor;   // false if the back side is the same plane
extern boolean markceiling;

extern fixed_t rw_distance;

extern int rw_x;
extern int rw_stopx;
extern int rw_angle1;       // angle to line origin

extern lighttable_t **walllights;

extern void R_RenderMaskedSegRange(drawseg_t *ds, int x1, int x2);
extern void R_StoreWallRange(int start, int stop, seg_t* __line, subsector_t* __sub);

// -----------------------------------------------------------------------------
// R_THINGS.C
// -----------------------------------------------------------------------------

#define MAXVISSPRITES     128
#define REALMAXVISSPRITES 1024

extern boolean pspr_interp; // [crispy] interpolate weapon bobbing

extern fixed_t pspritescale, pspriteiscale;
extern fixed_t sprbotscreen;
extern fixed_t sprtopscreen;
extern fixed_t spryscale;

extern int *mceilingclip;  // [JN] 32-bit integer math
extern int *mfloorclip;    // [JN] 32-bit integer math
extern int negonearray[SCREENWIDTH];
extern int screenheightarray[SCREENWIDTH];

extern vissprite_t vissprites[REALMAXVISSPRITES], *vissprite_p;
extern vissprite_t vsprsortedhead;

extern void R_AddPSprites(void);
extern void R_AddSprites(sector_t *sec);
extern void R_ClearSprites(void);
extern void R_ClipVisSprite(vissprite_t *vis, int xl, int xh);
extern void R_DrawMasked(void);
extern void R_DrawMaskedColumn(column_t *column, signed int baseclip);
extern void R_DrawSprites(void);
extern void R_InitSprites(const char **namelist);
extern void R_SortVisSprites(void);


