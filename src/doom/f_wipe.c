//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2026 Julia Nechaevskaya
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
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
#include "i_system.h" // I_Realloc
#include "i_video.h"
#include "v_trans.h"  // [PN] V_GetPaletteIndex
#include "v_video.h"
#include "w_wad.h"    // [PN] W_CacheLumpName
#include "m_random.h"
#include "f_wipe.h"

#include "crlcore.h"
#include "crlvars.h"


// =============================================================================
// SCREEN WIPE PACKAGE
// =============================================================================

static pixel_t *wipe_scr_start;
static pixel_t *wipe_scr_end;
static pixel_t *wipe_scr;
static int     *y;
static int     *y_prev;
static int      wipe_columns;

// [PN] Crossfade effect support
static int   fade_counter;
static byte *crossfade_blend    = NULL;   // 256x256 blend table (rebuilt per step)
static byte *crossfade_palette  = NULL;   // Cached 768-byte PLAYPAL
static byte *crossfade_colorlut = NULL;   // 32x32x32 fast nearest-color lookup

// -----------------------------------------------------------------------------
// wipe_EnsureBuffers
//  [PN] Lazy allocation / resize of wipe buffers.
// -----------------------------------------------------------------------------

static void wipe_EnsureBuffers (void)
{
    static size_t wipe_capacity = 0;
    static size_t wipe_columns_capacity = 0;
    const size_t need_area = (size_t)SCREENAREA;
    const size_t need_columns = (size_t)SCREENWIDTH;

    if (need_area > wipe_capacity)
    {
        wipe_scr_start = (pixel_t *)I_Realloc(wipe_scr_start, need_area * sizeof(*wipe_scr_start));
        wipe_scr_end   = (pixel_t *)I_Realloc(wipe_scr_end, need_area * sizeof(*wipe_scr_end));
        y              =     (int *)I_Realloc(y, need_area * sizeof(*y));
        wipe_capacity  = need_area;
    }

    if (need_columns > wipe_columns_capacity)
    {
        y_prev = (int *)I_Realloc(y_prev, need_columns * sizeof(*y_prev));
        wipe_columns_capacity = need_columns;
    }
}

// -----------------------------------------------------------------------------
// wipe_buildColorLUT
//  [PN] Builds a 32x32x32 (32 KB) lookup table for fast nearest-color search
//  in the palette. Uses 5-bit precision per channel (32 levels). Called once.
// -----------------------------------------------------------------------------

static void wipe_buildColorLUT (void)
{
    if (crossfade_colorlut != NULL)
    {
        return;
    }

    crossfade_colorlut = (byte *)Z_Malloc(32*32*32, PU_STATIC, 0);

    for (int r = 0; r < 32; r++)
    {
        for (int g = 0; g < 32; g++)
        {
            for (int b = 0; b < 32; b++)
            {
                int best_dist = INT_MAX;
                int best_idx = 0;
                const int rr = r << 3;
                const int gg = g << 3;
                const int bb = b << 3;

                for (int i = 0; i < 256; i++)
                {
                    const int dr = rr - crossfade_palette[i*3+0];
                    const int dg = gg - crossfade_palette[i*3+1];
                    const int db = bb - crossfade_palette[i*3+2];
                    const int dist = dr*dr + dg*dg + db*db;

                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_idx = i;
                        if (dist == 0)
                        {
                            break;
                        }
                    }
                }

                crossfade_colorlut[(r << 10) | (g << 5) | b] = (byte)best_idx;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// wipe_buildCrossfadeTable
//  [PN] Rebuilds the 256x256 blend table for a given linear interpolation step.
//  step=1 is nearly all start screen, step=32 is exactly the end screen.
// -----------------------------------------------------------------------------

static void wipe_buildCrossfadeTable (int step)
{
    byte *tm = crossfade_blend;

    for (int i = 0; i < 256; i++)
    {
        for (int j = 0; j < 256; j++)
        {
            if (i == j)
            {
                *tm++ = (byte)i;
                continue;
            }

            // Linear blend: (32-step)/32 * palette[i] + step/32 * palette[j]
            // >> 5 is division by 32; max result is 255.
            const int r = ((32 - step) * crossfade_palette[i*3+0] + step * crossfade_palette[j*3+0]) >> 5;
            const int g = ((32 - step) * crossfade_palette[i*3+1] + step * crossfade_palette[j*3+1]) >> 5;
            const int b = ((32 - step) * crossfade_palette[i*3+2] + step * crossfade_palette[j*3+2]) >> 5;

            // Use 5-bit LUT for fast nearest-color search
            *tm++ = crossfade_colorlut[((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)];
        }
    }
}

// -----------------------------------------------------------------------------
// wipe_initCrossfade
//  [PN] Initializes the crossfade effect by building a blend lookup table
//  that smoothly interpolates between two palette colors with ~10% weight
//  toward the end screen per step. The table is built once and reused.
// -----------------------------------------------------------------------------

static void wipe_initCrossfade (void)
{
    // Copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));

    // Lazy allocation on first use
    if (crossfade_blend == NULL)
    {
        crossfade_blend = (byte *)Z_Malloc(256*256, PU_STATIC, 0);
    }

    if (crossfade_palette == NULL)
    {
        const byte *playpal = W_CacheLumpName("PLAYPAL", PU_STATIC);
        crossfade_palette = (byte *)Z_Malloc(768, PU_STATIC, 0);
        memcpy(crossfade_palette, playpal, 768);
        W_ReleaseLumpName("PLAYPAL");
    }

    // Build fast color lookup table on first use (~10ms one-time cost)
    wipe_buildColorLUT();

    // Build blend table for the first step
    wipe_buildCrossfadeTable(1);

    // 32 steps for a smooth transition
    fade_counter = 32;
}

// -----------------------------------------------------------------------------
// wipe_doCrossfade
//  [PN] Advances the crossfade effect by one step. Each step blends the
//  current screen approximately 10% closer to the end screen using the
//  precomputed lookup table. Returns true when the transition is complete.
// -----------------------------------------------------------------------------

static int wipe_doCrossfade (int ticks)
{
    // Advance by the number of elapsed ticks
    while (ticks-- > 0 && fade_counter > 0)
    {
        const int step = 33 - fade_counter; // step goes from 1 to 32
        wipe_buildCrossfadeTable(step);

        // Linear blend: interpolate between start and end screen
        for (int i = 0; i < SCREENAREA; i++)
        {
            wipe_scr[i] = crossfade_blend[(wipe_scr_start[i] << 8) + wipe_scr_end[i]];
        }
        fade_counter--;
    }

    // Check if transition is complete
    if (fade_counter <= 0)
    {
        // Final frame must be exact end-screen
        memcpy(wipe_scr, wipe_scr_end, SCREENAREA * sizeof(*wipe_scr));
        return true;
    }

    return false;
}

// -----------------------------------------------------------------------------
// wipe_initMelt
// -----------------------------------------------------------------------------

static void wipe_initMelt (void)
{
    // copy start screen to main screen
    memcpy(wipe_scr, wipe_scr_start, SCREENAREA*sizeof(*wipe_scr));

    // setup initial column positions
    // (y<0 => not ready to scroll yet)
    wipe_columns = SCREENWIDTH / 2;
    y = (int *) Z_Malloc(wipe_columns*sizeof(*y), PU_STATIC, 0);
    y_prev = (int *) Z_Malloc(wipe_columns*sizeof(*y_prev), PU_STATIC, 0);
    y[0] = -(M_Random()%16);

    for (int i = 1 ; i < wipe_columns ; i++)
    {
        int r = (M_Random()%3) - 1;

        y[i] = y[i-1] + r;

        if (y[i] > 0)
        {
            y[i] = 0;
        }
        else
        if (y[i] == -16)
        {
            y[i] = -15;
        }
    }

    memcpy(y_prev, y, wipe_columns*sizeof(*y_prev));
}

// -----------------------------------------------------------------------------
// wipe_renderMelt
// -----------------------------------------------------------------------------

static void wipe_drawStartColumn (int x, int y_offset)
{
    const pixel_t *s = wipe_scr_start + x;
    pixel_t *d = wipe_scr + x + y_offset * SCREENWIDTH;

    for (int row = y_offset ; row < SCREENHEIGHT ; row++)
    {
        *d = *s;
        d += SCREENWIDTH;
        s += SCREENWIDTH;
    }
}

static void wipe_renderMelt (void)
{
    const int column_width = SCREENWIDTH / wipe_columns;

    memcpy(wipe_scr, wipe_scr_end, SCREENAREA*sizeof(*wipe_scr));

    for (int col = 0 ; col < wipe_columns ; col++)
    {
        int currcol = col * column_width;
        const int currcolend = (col == wipe_columns - 1) ? SCREENWIDTH : currcol + column_width;
        int current = y[col];

        if (crl_uncapped_fps)
        {
            const int delta = y[col] - y_prev[col];
            current = y_prev[col] + (int)(delta * FIXED2DOUBLE(fractionaltic));
        }

        if (current >= SCREENHEIGHT)
        {
            continue;
        }

        if (current < 0)
        {
            current = 0;
        }

        for ( ; currcol < currcolend ; currcol++)
        {
            wipe_drawStartColumn(currcol, current);
        }
    }
}

// -----------------------------------------------------------------------------
// wipe_doMelt
// -----------------------------------------------------------------------------

static int wipe_doMelt (int ticks)
{
    boolean done = true;

    if (ticks > 0)
    {
        while (ticks--)
        {
            memcpy(y_prev, y, wipe_columns * sizeof(*y_prev));

            for (int col = 0; col < wipe_columns; ++col)
            {
                if (y_prev[col] < 0)
                {
                    y[col] = y_prev[col] + 1;
                    done = false;
                }
                else if (y_prev[col] < SCREENHEIGHT)
                {
                    // [PN] Accelerate after warm-up rows; speed still scales via crl_screenwipe.
                    // const int speed_factor = crl_screenwipe == 1 ? 1 : 2;
                    int dy = (y_prev[col] < 16) ? y_prev[col] + 1 : (8 * crl_screenwipe);
                    int next = y_prev[col] + dy;

                    if (next > SCREENHEIGHT)
                    {
                        next = SCREENHEIGHT;
                    }

                    y[col] = next;
                    done = false;
                }
                else
                {
                    y[col] = SCREENHEIGHT;
                }
            }
        }
    }
    else
    {
        for (int col = 0; col < wipe_columns; ++col)
        {
            done = done && (y[col] >= SCREENHEIGHT);
        }
    }

    if (done)
    {
        // [PN] Final frame must be exact end-screen; avoid sub-tic interpolation residue.
        memcpy(y_prev, y, wipe_columns * sizeof(*y_prev));
        memcpy(wipe_scr, wipe_scr_end, SCREENAREA * sizeof(*wipe_scr));
        return true;
    }

    wipe_renderMelt();

    return false;
}

// -----------------------------------------------------------------------------
// wipe_StartScreen
// -----------------------------------------------------------------------------

void wipe_StartScreen (void)
{
    wipe_EnsureBuffers();
    I_ReadScreen(wipe_scr_start);
}

// -----------------------------------------------------------------------------
// wipe_EndScreen
// -----------------------------------------------------------------------------

void wipe_EndScreen (void)
{
    wipe_EnsureBuffers();
    I_ReadScreen(wipe_scr_end);
}

// -----------------------------------------------------------------------------
// wipe_ScreenWipe
// -----------------------------------------------------------------------------

int wipe_ScreenWipe (const int ticks)
{
    // when zero, stop the wipe
    static boolean go = false;

    // initial stuff
    if (!go)
    {
        go = true;
        wipe_scr = I_VideoBuffer;

        // [PN] Select wipe effect based on crl_screenwipe:
        // 1/2 = Melt / 3 = Melt.
        if (crl_screenwipe == 3)
        {
            wipe_initCrossfade();
        }
        else
        {
            wipe_initMelt();
        }
    }

    // final stuff
    if (crl_screenwipe == 3)
    {
        if (wipe_doCrossfade(ticks))
        {
            go = false;
        }
    }
    else
    {
        if (wipe_doMelt(ticks))
        {
            go = false;
        }
    }

    return !go;
}
