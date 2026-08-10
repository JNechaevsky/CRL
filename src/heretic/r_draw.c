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
// R_draw.c

#include "doomdef.h"
#include "deh_str.h"
#include "r_local.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

#include "crlcore.h"
#include "crlvars.h"


/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

byte *viewimage;
int viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;
byte *ylookup[MAXHEIGHT];
int columnofs[MAXWIDTH];
byte translations[3][256];      // color tables for different players

// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.
static pixel_t *background_buffer = NULL;

/*
==================
=
= R_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

lighttable_t *dc_colormap;
int dc_x;
int dc_yl;
int dc_yh;
fixed_t dc_iscale;
fixed_t dc_texturemid;
byte *dc_source;                // first pixel in a column (possibly virtual)

// [JN] RestlessRodent -- CRL
visplane_t* dc_visplaneused = NULL;

void R_DrawColumn(void)
{
    int count;
    byte *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[dc_x];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        // [JN] RestlessRodent -- Possibly mark visplane
        if (dc_visplaneused != NULL)
        {
            CRL_MarkPixelP(CRLPlaneSurface, dc_visplaneused, dest);
        }

        *dest = dc_colormap[dc_source[(frac >> FRACBITS) & 127]];
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

// Translucent column draw - blended with background using tinttable.

void R_DrawTLColumn(void)
{
    int count;
    byte *dest;
    fixed_t frac, fracstep;

    // [crispy] Show transparent lines at top and bottom of screen.
    /*
    if (!dc_yl)
        dc_yl = 1;
    if (dc_yh == viewheight - 1)
        dc_yh = viewheight - 2;
    */

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawTLColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[dc_x];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        *dest =
            tinttable[((*dest) << 8) +
                      dc_colormap[dc_source[(frac >> FRACBITS) & 127]]];

        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

/*
========================
=
= R_DrawTranslatedColumn
=
========================
*/

byte *dc_translation;
byte *translationtables;

void R_DrawTranslatedColumn(void)
{
    int count;
    byte *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[dc_x];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        *dest = dc_colormap[dc_translation[dc_source[frac >> FRACBITS]]];
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

void R_DrawTranslatedTLColumn(void)
{
    int count;
    byte *dest;
    fixed_t frac, fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
        return;

#ifdef RANGECHECK
    if ((unsigned) dc_x >= SCREENWIDTH || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
        I_Error("R_DrawColumn: %i to %i at %i", dc_yl, dc_yh, dc_x);
#endif

    dest = ylookup[dc_yl] + columnofs[dc_x];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl - centery) * fracstep;

    do
    {
        *dest = tinttable[((*dest) << 8)
                          +
                          dc_colormap[dc_translation
                                      [dc_source[frac >> FRACBITS]]]];
        dest += SCREENWIDTH;
        frac += fracstep;
    }
    while (count--);
}

//--------------------------------------------------------------------------
//
// PROC R_InitTranslationTables
//
//--------------------------------------------------------------------------

void R_InitTranslationTables(void)
{
    int i;

    V_LoadTintTable();

    // Allocate translation tables
    translationtables = Z_Malloc(256 * 3, PU_STATIC, 0);

    // Fill out the translation tables
    for (i = 0; i < 256; i++)
    {
        if (i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225);     // yellow
            translationtables[i + 256] = 145 + (i - 225);       // red
            translationtables[i + 512] = 190 + (i - 225);       // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256]
                = translationtables[i + 512] = i;
        }
    }
}

/*
================
=
= R_DrawSpan
=
================
*/

int ds_y;
int ds_x1;
int ds_x2;
lighttable_t *ds_colormap;
fixed_t ds_xfrac;
fixed_t ds_yfrac;
fixed_t ds_xstep;
fixed_t ds_ystep;
byte *ds_source;                // start of a 64*64 tile image


void R_DrawSpan(void)
{
    fixed_t xfrac, yfrac;
    byte *dest;
    int count, spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1 < 0 || ds_x2 >= SCREENWIDTH
        || (unsigned) ds_y > SCREENHEIGHT)
        I_Error("R_DrawSpan: %i to %i at %i", ds_x1, ds_x2, ds_y);
#endif

    xfrac = ds_xfrac;
    yfrac = ds_yfrac;

    dest = ylookup[ds_y] + columnofs[ds_x1];
    count = ds_x2 - ds_x1;
    do
    {
        spot = ((yfrac >> (16 - 6)) & (63 * 64)) + ((xfrac >> 16) & 63);

        // [JN] RestlessRodent -- Possibly mark visplane
        if (dc_visplaneused != NULL)
        {
            CRL_MarkPixelP(CRLPlaneSurface, dc_visplaneused, dest);
        }

        *dest++ = ds_colormap[ds_source[spot]];
        xfrac += ds_xstep;
        yfrac += ds_ystep;
    }
    while (count--);
}

// -----------------------------------------------------------------------------
// R_InitBuffer 
// Initializes the buffer for a given view width and height.
// Handles border offsets and row/column calculations.
// [PN] Simplified logic and improved readability for better understanding.
// -----------------------------------------------------------------------------

void R_InitBuffer(int width, int height)
{
    int i;

    // [PN] Handle resize: calculate horizontal offset (viewwindowx).
    viewwindowx = (SCREENWIDTH - width) >> 1;

    // [PN] Calculate column offsets (columnofs).
    for (i = 0; i < width; i++) 
    {
        columnofs[i] = viewwindowx + i;
    }

    // [PN] Calculate vertical offset (viewwindowy).
    // Simplified using ternary operator.
    viewwindowy = (width == SCREENWIDTH) ? 0 : (SCREENHEIGHT - SBARHEIGHT - height) >> 1;

    // [crispy] make sure viewwindowy is always an even number
    viewwindowy &= ~1;

    // [PN] Precalculate row offsets (ylookup) for each row.
    for (i = 0; i < height; i++) 
    {
        ylookup[i] = I_VideoBuffer + (i + viewwindowy) * SCREENWIDTH;
    }

    // [PN] Free the background buffer if it exists.
    if (background_buffer != NULL)
    {
        Z_Free(background_buffer);
        background_buffer = NULL;
    }
}

// -----------------------------------------------------------------------------
// [JN] Replaced Heretic's original R_DrawViewBorder and R_DrawTopBorder
// functions with Doom's implementation to improve performance and avoid
// precision problems when drawing beveled edges on smaller screen sizes.
// [PN] Optimized for readability and reduced code duplication.
// Pre-cache patches and precompute commonly used values to improve efficiency.
// -----------------------------------------------------------------------------

void R_FillBackScreen (void) 
{
    // [PN] Allocate the background buffer if necessary
    if (scaledviewwidth == SCREENWIDTH)
        return;

    if (background_buffer == NULL)
    {
        const int size = SCREENWIDTH * (SCREENHEIGHT - SBARHEIGHT);
        background_buffer = Z_Malloc(size * sizeof(*background_buffer), PU_STATIC, 0);
    }

    // [PN] Adjust for precision issues on lower screen sizes
    const int yy = (crl_screen_size < 6) ? 1 : 0;

    // [PN] Use a separate buffer for drawing
    V_UseBuffer(background_buffer);

    // [PN] Retrieve the source flat for the background
    const byte *src = W_CacheLumpName(DEH_String(gamemode == shareware ? "FLOOR04" : "FLAT513"), PU_CACHE);

    // [crispy] Unified flat filling function
    V_FillFlat(0, SCREENHEIGHT - SBARHEIGHT, 0, SCREENWIDTH, src, background_buffer);

    // [PN] Calculate patch positions.
    // Align horizontal view edges robustly on high renderer scales (3x+):
    // use ceiling division for the left edge (prevents left-side seam) and
    // floor division for the right edge (prevents right-side seam).
    const int view_x = viewwindowx;
    const int view_y = viewwindowy;
    const int view_w = (viewwindowx + scaledviewwidth) - view_x;
    const int view_h = viewheight;

    // [PN] Draw top and bottom borders
    for (int x = view_x; x < view_x + view_w; x += 16)
    {
        V_DrawPatch(x, view_y - 4 + yy, W_CacheLumpName(DEH_String("BORDT"), PU_CACHE), "BORDT");
        V_DrawPatch(x, view_y + view_h, W_CacheLumpName(DEH_String("BORDB"), PU_CACHE), "BORDB");
    }

    // [PN] Draw left and right borders
    for (int y = view_y; y < view_y + view_h; y += 16)
    {
        V_DrawPatch(view_x - 4, y, W_CacheLumpName(DEH_String("BORDL"), PU_CACHE), "BORDL");
        V_DrawPatch(view_x + view_w, y, W_CacheLumpName(DEH_String("BORDR"), PU_CACHE), "BORDR");
    }

    // [PN] Draw corners
    V_DrawPatch(view_x - 4, view_y - 4 + yy, W_CacheLumpName(DEH_String("BORDTL"), PU_CACHE), "BORDTL");
    V_DrawPatch(view_x + view_w, view_y - 4 + yy, W_CacheLumpName(DEH_String("BORDTR"), PU_CACHE), "BORDTR");
    V_DrawPatch(view_x + view_w, view_y + view_h, W_CacheLumpName(DEH_String("BORDBR"), PU_CACHE), "BORDBR");
    V_DrawPatch(view_x - 4, view_y + view_h, W_CacheLumpName(DEH_String("BORDBL"), PU_CACHE), "BORDBL");

    // [PN] Restore the original buffer
    V_RestoreBuffer();
}

// -----------------------------------------------------------------------------
// Copy a screen buffer.
// [PN] Changed ofs to size_t for clarity and to represent offset more appropriately.
// -----------------------------------------------------------------------------

static void R_VideoErase (size_t ofs, int count)
{
    // [PN] Ensure the background buffer is valid before copying
    if (background_buffer != NULL)
    {
        // [PN] Copy from background buffer to video buffer
        memcpy(I_VideoBuffer + ofs, background_buffer + ofs, count * sizeof(*I_VideoBuffer));
    }
}

// -----------------------------------------------------------------------------
// R_DrawViewBorder
// Draws the border around the view for different size windows.
// [PN] Optimized by precomputing common offsets and reducing repeated calculations.
//      Simplified logic for top, bottom, and side erasing.
// -----------------------------------------------------------------------------

void R_DrawViewBorder (void)
{
    if (scaledviewwidth == SCREENWIDTH || background_buffer == NULL)
    {
        return;
    }

    // [PN] Adjust for precision issues on lower screen sizes
    const int yy2 = (crl_screen_size < 6) ? 3 : 0;
    const int yy3 = (crl_screen_size < 6) ? 2 : 0;

    // [PN] Calculate top, side, and offsets
    const int top = ((SCREENHEIGHT - SBARHEIGHT) - viewheight) / 2;
    const int top2 = ((SCREENHEIGHT - SBARHEIGHT) - viewheight + yy2) / 2;
    const int top3 = (((SCREENHEIGHT - SBARHEIGHT) - viewheight) - yy3) / 2;
    int side = (SCREENWIDTH - scaledviewwidth) / 2;

    // [PN] Copy top and one line of left side
    R_VideoErase(0, top * SCREENWIDTH + side);

    // [PN] Copy one line of right side and bottom
    int ofs = (viewheight + top3) * SCREENWIDTH - side;
    R_VideoErase(ofs, top2 * SCREENWIDTH + side);

    // [PN] Copy sides using wraparound
    ofs = top * SCREENWIDTH + SCREENWIDTH - side;
    side <<= 1;

    for (int i = 1; i < viewheight; i++)
    {
       R_VideoErase(ofs, side);
       ofs += SCREENWIDTH;
    }
}
