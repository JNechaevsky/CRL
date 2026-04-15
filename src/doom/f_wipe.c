//
// Copyright(C) 1993-1996 Id Software, Inc.
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
// DESCRIPTION:
//	Mission begin melt/wipe screen special effect.
//

#include <string.h>

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
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
    boolean	done = true;

    if (ticks > 0)
    {
        while (ticks--)
        {
            done = true;
            memcpy(y_prev, y, wipe_columns*sizeof(*y_prev));

            for (int i = 0 ; i < wipe_columns ; i++)
            {
                if (y_prev[i]<0)
                {
                    y[i] = y_prev[i] + 1;
                    done = false;
                }
                else
                if (y_prev[i] < SCREENHEIGHT)
                {
                    // [JN] Add support for "fast" wipe (crl_screenwipe == 2).
                    int dy = (y_prev[i] < 16) ? y_prev[i]+1 : (8 * crl_screenwipe);
                    int next = y_prev[i] + dy;

                    if (next > SCREENHEIGHT)
                    {
                        next = SCREENHEIGHT;
                    }

                    y[i] = next;
                    done = false;
                }
                else
                {
                    y[i] = SCREENHEIGHT;
                }
            }
        }
    }
    else
    {
        for (int i = 0 ; i < wipe_columns ; i++)
        {
            done = done && (y[i] >= SCREENHEIGHT);
        }
    }

    if (done)
    {
        memcpy(y_prev, y, wipe_columns*sizeof(*y_prev));
        memcpy(wipe_scr, wipe_scr_end, SCREENAREA*sizeof(*wipe_scr));
        return true;
    }

    wipe_renderMelt();

    return false;
}

// -----------------------------------------------------------------------------
// wipe_exitMelt
// -----------------------------------------------------------------------------

static void wipe_exitMelt (void)
{
    Z_Free(y);
    Z_Free(y_prev);
    Z_Free(wipe_scr_start);
    Z_Free(wipe_scr_end);
}

// -----------------------------------------------------------------------------
// wipe_StartScreen
// -----------------------------------------------------------------------------

void wipe_StartScreen (void)
{
    wipe_scr_start = Z_Malloc(SCREENAREA * sizeof(*wipe_scr_start), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_start);
}

// -----------------------------------------------------------------------------
// wipe_EndScreen
// -----------------------------------------------------------------------------

void wipe_EndScreen (void)
{
    wipe_scr_end = Z_Malloc(SCREENAREA * sizeof(*wipe_scr_end), PU_STATIC, NULL);
    I_ReadScreen(wipe_scr_end);
    V_DrawBlock(0, 0, SCREENWIDTH, SCREENHEIGHT, wipe_scr_start); // restore start scr.
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
        wipe_initMelt();
    }

    // final stuff
    if ((*wipe_doMelt)(ticks))
    {
        go = false;
        wipe_exitMelt();
    }

    return !go;
}
