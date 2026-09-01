//
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2016-2026 Julia Nechaevskaya
// Copyright(C) 2024-2026 Polina "Aura" N.
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


#include <stddef.h>     // ptrdiff_t
#include <stdlib.h>     // malloc, free
#include "doomdef.h"
#include "r_local.h"

#include "crlvars.h"


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
        // Released only when there is something to release: this branch
        // runs on every frame the mode is off, and by then the tables are gone
        // already. The pointers are cleared below for the same reason, or the
        // next frame would free them a second time.
        if (crl_sneak_marks != NULL || crl_sneak_sectors != NULL)
        {
            free(crl_sneak_marks);
            free(crl_sneak_sectors);

            crl_sneak_marks = NULL;
            crl_sneak_sectors = NULL;
            crl_sneak_numsegs = crl_sneak_numsectors = 0;
        }

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
// in r_draw.c, so the address is built the same way here. Heretic has no blocky
// detail mode, so one view column is always one screen pixel.
static void CRL_SneakHide (void **__table, void *what, int x, int __top, int __bottom)
{
    if (x < 0 || x >= viewwidth)
    {
        return;
    }

    for (int y = __top; y <= __bottom; y++)
    {
        if (y >= 0 && y < viewheight)
        {
            CRL_MarkPixelP(__table, what,
                           CRLSurface + (y + viewwindowy) * SCREENWIDTH
                                      + viewwindowx + x);
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
static int CRL_SneakSegIndex (const seg_t *const seg)
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
boolean CRL_SneakAllowWall (const seg_t *const seg)
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
boolean CRL_SneakAllowSector (const sector_t *const sector, boolean ceiling)
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
