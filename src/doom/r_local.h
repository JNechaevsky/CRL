//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
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
// DESCRIPTION:
//	Refresh (R_*) module, global header.
//	All the rendering/drawing stuff is here.
//


#pragma once


#include "tables.h"
#include "d_player.h"
#include "doomdef.h"
#include "i_video.h"
#include "v_patch.h"


#define MAXVISSPRITES   128
#define MAXREALVISSPRITES   1024
#define MAXDRAWSEGS     256
#define MAXREALDRAWSEGS 2048

// Silhouette, needed for clipping Segs (mainly) and sprites representing things.
#define SIL_NONE    0
#define SIL_BOTTOM  1
#define SIL_TOP     2
#define SIL_BOTH    3


// =============================================================================
// INTERNAL MAP TYPES
//  used by play and refresh
// =============================================================================

// Your plain vanilla vertex.
// Note: transformed values not buffered locally, 
//  like some DOOM-alikes ("wt", "WebView") did.

typedef struct
{
    fixed_t	x;
    fixed_t	y;
} vertex_t;

// Forward of LineDefs, for Sectors.
struct line_s;

// Each sector has a degenmobj_t in its center for sound origin purposes.
// I suppose this does not handle sound from moving objects (doppler),
// because position is prolly just buffered, not updated.

typedef struct
{
    thinker_t   thinker;  // not used for anything
    fixed_t     x;
    fixed_t     y;
    fixed_t     z;
} degenmobj_t;

// The SECTORS record, at runtime.
// Stores things/mobjs.

typedef	struct
{
    fixed_t floorheight;
    fixed_t ceilingheight;
    short   floorpic;
    short   ceilingpic;
    short   lightlevel;
    short   special;
    short   tag;

    // 0 = untraversed, 1,2 = sndlines -1
    int     soundtraversed;

    // thing that made a sound (or null)
    mobj_t *soundtarget;

    // mapblock bounding box for height changes
    int     blockbox[4];

    // origin for any sounds played by the sector
    degenmobj_t soundorg;

    // if == validcount, already checked
    int     validcount;

    // list of mobjs in sector
    mobj_t *thinglist;

    // thinker_t for reversable actions
    void   *specialdata;

    int     linecount;
    struct line_s **lines;  // linecount size

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

//
// The SideDef.
//

typedef struct
{
    // add this to the calculated texture column
    fixed_t	textureoffset;

    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture indices.
    // We do not maintain names here. 
    short toptexture;
    short bottomtexture;
    short midtexture;

    // Sector the SideDef is facing.
    sector_t *sector;

    // [crispy] smooth texture scrolling
    fixed_t	basetextureoffset;
} side_t;

//
// Move clipping aid for LineDefs.
//

typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

typedef struct line_s
{
    // Vertices, from v1 to v2.
    vertex_t *v1;
    vertex_t *v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t dx;
    fixed_t dy;

    // Animation related.
    short flags;
    short special;
    short tag;

    // Visual appearance: SideDefs. sidenum[1] will be -1 if one sided
    short sidenum[2];			

    // Neat. Another bounding box, for the extent of the LineDef.
    fixed_t bbox[4];

    // To aid move clipping.
    slopetype_t slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t *frontsector;
    sector_t *backsector;

    // if == validcount, already checked
    int validcount;

    // thinker_t for reversable actions
    void *specialdata;		
} line_t;

//
// A SubSector.
// References a Sector. Basically, this is a list of LineSegs, indicating 
// the visible walls that define (all or some) sides of a convex BSP leaf.
//

typedef struct subsector_s
{
    sector_t *sector;
    short     numlines;
    short     firstline;
} subsector_t;

//
// The LineSeg.
//

typedef struct
{
    vertex_t *v1;
    vertex_t *v2;
    fixed_t	  offset;
    angle_t	  angle;
    side_t   *sidedef;
    line_t   *linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one sided lines
    sector_t *frontsector;
    sector_t *backsector;
} seg_t;

//
// BSP node.
//

typedef struct
{
    // Partition line.
    fixed_t	x;
    fixed_t	y;
    fixed_t	dx;
    fixed_t	dy;

    // Bounding box for each child.
    fixed_t	bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];
} node_t;

//
// OTHER TYPES
//

// This could be wider for >8 bit display. Indeed, true color support is posibble
// precalculating 24bpp lightmap/colormap LUT. From darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef pixel_t lighttable_t;

//
// ?
//

typedef struct drawseg_s
{
    seg_t *curline;
    int    x1;
    int    x2;

    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int silhouette;

    // do not clip sprites above this
    fixed_t bsilheight;

    // do not clip sprites below this
    fixed_t tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    int *sprtopclip;        // [JN] 32-bit integer math
    int *sprbottomclip;     // [JN] 32-bit integer math
    int *maskedtexturecol;  // [JN] 32-bit integer math
} drawseg_t;

// A vissprite_t is a thing that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.

typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s *prev;
    struct vissprite_s *next;

    int         x1;
    int         x2;

    // for line side calculation
    fixed_t     gx;
    fixed_t     gy;		

    // global bottom / top for silhouette clipping
    fixed_t     gz;
    fixed_t     gzt;

    // horizontal position of x1
    fixed_t     startfrac;
    
    fixed_t     scale;
    
    // negative if flipped
    fixed_t     xiscale;	

    fixed_t     texturemid;
    int         patch;

    // for color translation and shadow draw, maxbright frames as well
    lighttable_t *colormap;
   
    int         mobjflags;
} vissprite_t;

//	
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//

typedef struct
{
    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    boolean	rotate;

    // Lump to use for view angles 0-7.
    short   lump[8];

    // Flip bit (1 = flip) to use for view angles 0-7.
    byte    flip[8];

} spriteframe_t;

//
// A sprite definition: a number of animation frames.
//

typedef struct
{
    int            numframes;
    spriteframe_t *spriteframes;
} spritedef_t;

//
// Now what is a visplane, anyway?
// 

typedef struct
{
    fixed_t height;
    int     picnum;
    int     lightlevel;
    int     minx;
    int     maxx;

    // Is a find plane.
    int     isfindplane;

    // The seg that emitted this.
    seg_t  *emitline;

    // The subsector this visplane is in.
    subsector_t *emitsub;

    // leave pads for [minx-1]/[maxx+1]
    unsigned short pad1;
    unsigned short top[SCREENWIDTH];
    unsigned short pad2;
    unsigned short pad3;
    unsigned short bottom[SCREENWIDTH];
    unsigned short pad4;
} visplane_t;

//
// Refresh internal data structures, for rendering.
//

// needed for texture pegging
extern fixed_t *textureheight;

// needed for pre rendering (fracs)
extern fixed_t *spritewidth;
extern fixed_t *spriteoffset;
extern fixed_t *spritetopoffset;

extern lighttable_t *colormaps;

extern int viewwidth;
extern int scaledviewwidth;
extern int viewheight;

extern int firstflat;

// for global animation
extern int *flattranslation;	
extern int *texturetranslation;	

// Sprite....
extern int  firstspritelump;
extern int  lastspritelump;
extern int  numspritelumps;

//
// Lookup tables for map data.
//

extern int numsprites;
extern int numvertexes;
extern int numsegs;
extern int numsectors;
extern int numsubsectors;
extern int numnodes;
extern int numlines;
extern int numsides;

extern spritedef_t *sprites;
extern vertex_t    *vertexes;
extern seg_t       *segs;
extern sector_t    *sectors;
extern subsector_t *subsectors;
extern node_t      *nodes;
extern line_t      *lines;
extern side_t      *sides;

//
// POV data.
//

extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;

extern angle_t   viewangle;
extern player_t *viewplayer;

extern int      viewangletox[FINEANGLES/2];
extern angle_t  xtoviewangle[SCREENWIDTH+1];
extern fixed_t  rw_distance;
extern angle_t  rw_normalangle;
extern angle_t  rw_angle1;
extern angle_t  clipangle;

extern visplane_t *floorplane;
extern visplane_t *ceilingplane;

// -----------------------------------------------------------------------------
// R_BSP
// -----------------------------------------------------------------------------

typedef void (*drawfunc_t) (int start, int stop);

extern void R_ClearClipSegs (void);
extern void R_ClearDrawSegs (void);
extern void R_RenderBSPNode (int bspnum);

extern seg_t    *curline;
extern side_t   *sidedef;
extern line_t   *linedef;
extern sector_t *frontsector;
extern sector_t *backsector;

extern int rw_x;
extern int rw_stopx;

extern boolean segtextured;
extern boolean markfloor;		
extern boolean markceiling;

extern drawseg_t  drawsegs[MAXREALDRAWSEGS];
extern drawseg_t *ds_p;

extern lighttable_t **hscalelight;
extern lighttable_t **vscalelight;
extern lighttable_t **dscalelight;

// -----------------------------------------------------------------------------
// R_DATA
// -----------------------------------------------------------------------------

extern byte *R_GetColumn (int tex, int col);
extern int   R_CheckTextureNumForName (const char *name);
extern int   R_FlatNumForName (const char *name);
extern int   R_TextureNumForName (const char *name);
extern void  R_InitData (void);
extern void  R_PrecacheLevel (void);

extern int   *texturecompositesize;
extern byte **texturecomposite;

extern int numflats;

// -----------------------------------------------------------------------------
// R_DRAW
// -----------------------------------------------------------------------------

extern void R_DrawColumn (void);
extern void R_DrawColumnLow (void);
extern void R_DrawFuzzColumn (void);
extern void R_DrawFuzzColumnLow (void);
extern void R_DrawSpan (void);
extern void R_DrawSpanLow (void);
extern void R_DrawTranslatedColumn (void);
extern void R_DrawTranslatedColumnLow (void);
extern void R_DrawViewBorder (void);
extern void R_FillBackScreen (void);
extern void R_InitBuffer (int width, int height);
extern void R_InitTranslationTables (void);
extern void R_SetFuzzPosDraw (void);
extern void R_SetFuzzPosTic (void);
extern void R_VideoErase (unsigned ofs, int count);

extern byte *dc_source;
extern byte *ds_source;		
extern byte *translationtables;
extern byte *dc_translation;

extern int dc_x;
extern int dc_yl;
extern int dc_yh;
extern int ds_y;
extern int ds_x1;
extern int ds_x2;

extern fixed_t dc_iscale;
extern fixed_t dc_texturemid;
extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;

extern lighttable_t *dc_colormap;
extern lighttable_t *ds_colormap;

// GhostlyDeath -- CRL
extern visplane_t   *dc_visplaneused;

// -----------------------------------------------------------------------------
// R_MAIN
// -----------------------------------------------------------------------------

extern void    R_ExecuteSetViewSize (void);
extern void    R_Init (void);
extern void    R_RenderPlayerView (player_t *player);
extern void    R_SetViewSize (int blocks, int detail);

// Utility functions.
extern angle_t R_PointToAngle (fixed_t x, fixed_t y);
extern angle_t R_PointToAngle2 (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);
extern fixed_t R_PointToDist (fixed_t x, fixed_t y);
extern fixed_t R_ScaleFromGlobalAngle (angle_t visangle);
extern int     R_PointOnSegSide (fixed_t x, fixed_t y, seg_t *line);
extern int     R_PointOnSide (fixed_t x, fixed_t y, node_t *node);
extern subsector_t *R_PointInSubsector (fixed_t x, fixed_t y);
extern void    R_AddPointToBox (int x, int y, fixed_t *box);

// [AM] Interpolate between two angles.
angle_t R_InterpolateAngle(angle_t oangle, angle_t nangle, fixed_t scale);

// Function pointers to switch refresh/drawing functions.
// Used to select shadow mode etc.
extern void (*colfunc) (void);
extern void (*transcolfunc) (void);
extern void (*basecolfunc) (void);
extern void (*fuzzcolfunc) (void);
extern void (*spanfunc) (void);

// POV related.
extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern fixed_t projection;
extern fixed_t viewcos;
extern fixed_t viewsin;
extern int     centerx;
extern int     centery;
extern int     validcount;
extern int     viewwindowx;
extern int     viewwindowy;

extern boolean setsizeneeded;
extern boolean original_playpal;

// Lighting LUT.
// Used for z-depth cuing per column/row,
//  and other lighting effects (sector ambient, flash).

// Lighting constants. Now why not 32 levels here?
#define LIGHTLEVELS     16
#define LIGHTSEGSHIFT   4

#define MAXLIGHTSCALE   48
#define LIGHTSCALESHIFT 12
#define MAXLIGHTZ       128
#define LIGHTZSHIFT     20

extern lighttable_t *scalelight[LIGHTLEVELS][MAXLIGHTSCALE];
extern lighttable_t *scalelightfixed[MAXLIGHTSCALE];
extern lighttable_t *zlight[LIGHTLEVELS][MAXLIGHTZ];

extern int           extralight;
extern lighttable_t *fixedcolormap;

// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.
#define NUMCOLORMAPS    32

// Blocky/low detail mode. 0 = high, 1 = low
extern int detailshift;	

// -----------------------------------------------------------------------------
// R_PLANE
// -----------------------------------------------------------------------------

extern void R_ClearPlanes (void);
extern void R_DrawPlanes (void);
extern void R_InitPlanes (void);
extern void R_MakeSpans (unsigned int x, unsigned int t1, unsigned int b1, 
                         unsigned int t2, unsigned int b2, visplane_t *__plane);
extern void R_MapPlane (int y, int x1, int x2, visplane_t *__plane);

extern int  floorclip[SCREENWIDTH];    // [JN] 32-bit integer math
extern int  ceilingclip[SCREENWIDTH];  // [JN] 32-bit integer math

#define MAXVISPLANES        128
#define REALMAXVISPLANES    SHRT_MAX
extern visplane_t  visplanes[REALMAXVISPLANES];
extern visplane_t *lastvisplane;

extern size_t  maxopenings;            // [JN] 32-bit integer maths
extern int    *lastopening;
extern int    *openings;


extern fixed_t yslope[SCREENHEIGHT];
extern fixed_t distscale[SCREENWIDTH];

extern visplane_t *R_FindPlane (fixed_t height, int picnum, int lightlevel,
                                seg_t *__line, subsector_t *__sub);
extern visplane_t *R_CheckPlane (visplane_t *pl, int start, int stop,
                                 seg_t *__line, subsector_t *__sub);

// -----------------------------------------------------------------------------
// R_SEGS
// -----------------------------------------------------------------------------

extern void R_RenderMaskedSegRange (drawseg_t *ds, int x1, int x2);

extern lighttable_t **walllights;

// -----------------------------------------------------------------------------
// R_SKY
// -----------------------------------------------------------------------------

// SKY, store the number for name.
#define SKYFLATNAME "F_SKY1"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT 22

extern int skytexture;
extern int skytexturemid;

// -----------------------------------------------------------------------------
// R_THINGS
// -----------------------------------------------------------------------------

extern void R_AddPSprites (void);
extern void R_AddSprites (sector_t *sec);
extern void R_ClearSprites (void);
extern void R_ClipVisSprite (vissprite_t *vis, int xl, int xh);
extern void R_DrawMasked (void);
extern void R_DrawMaskedColumn (column_t *column);
extern void R_DrawSprites (void);
extern void R_InitSprites (const char **namelist);
extern void R_SortVisSprites (void);

extern vissprite_t  vissprites[MAXREALVISSPRITES];
extern vissprite_t *vissprite_p;
extern vissprite_t  vsprsortedhead;

// Constant arrays used for psprite clipping and initializing clipping.
extern int negonearray[SCREENWIDTH];        // [JN] 32-bit integer math
extern int screenheightarray[SCREENWIDTH];  // [JN] 32-bit integer math

// vars for R_DrawMaskedColumn
extern int *mfloorclip;    // [JN] 32-bit integer math
extern int *mceilingclip;  // [JN] 32-bit integer math
extern fixed_t  spryscale;
extern fixed_t  sprtopscreen;

extern fixed_t pspritescale;
extern fixed_t pspriteiscale;

// [crispy] interpolate weapon bobbing
extern boolean pspr_interp;
