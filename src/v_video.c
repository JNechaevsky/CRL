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
// DESCRIPTION:
//	Gamma correction LUT stuff.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//

#include <stdio.h>
#include <string.h>
#include <math.h>

#define MINIZ_NO_STDIO
#define MINIZ_NO_ZLIB_APIS
#include "miniz.h"

#include "i_system.h"
#include "doomtype.h"
#include "deh_str.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_video.h"
#include "m_bbox.h"
#include "m_config.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "crlcore.h"
#include "crlvars.h"


// TODO: There are separate RANGECHECK defines for different games, but this
// is common code. Fix this.
#define RANGECHECK

// [JN] Blending table used for text shadows.
byte *tintmap = NULL;

// Blending table used for fuzzpatch, etc.
// Only used in Heretic/Hexen
byte *tinttable = NULL;

// [JN] Color translation.
byte *dp_translation = NULL;
// [JN] Translucent patch.
boolean dp_translucent = false;

// The screen buffer that the v_video.c code draws to.

static pixel_t *dest_screen = NULL;

int dirtybox[4]; 



//
// V_MarkRect 
// 

void V_MarkRect(int x, int y, int width, int height) 
{ 
    // If we are temporarily using an alternate screen, do not 
    // affect the update box.

    if (dest_screen == I_VideoBuffer)
    {
        M_AddToBox (dirtybox, x, y); 
        M_AddToBox (dirtybox, x + width-1, y + height-1); 
    }
} 

//
// V_CopyRect 
// 

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty)
{ 
    pixel_t *src;
    pixel_t *dest;
 
#ifdef RANGECHECK 
    if (srcx < 0
     || srcx + width > SCREENWIDTH
     || srcy < 0
     || srcy + height > SCREENHEIGHT 
     || destx < 0
     || destx + width > SCREENWIDTH
     || desty < 0
     || desty + height > SCREENHEIGHT)
    {
        I_Error ("Bad V_CopyRect");
    }
#endif 

    V_MarkRect(destx, desty, width, height); 
 
    src = source + SCREENWIDTH * srcy + srcx; 
    dest = dest_screen + SCREENWIDTH * desty + destx; 

    for ( ; height>0 ; height--) 
    { 
        memcpy(dest, src, width * sizeof(*dest));
        src += SCREENWIDTH; 
        dest += SCREENWIDTH; 
    } 
} 

//
// V_DrawPatch
// Masks a column based masked pic to the screen. 
//

void V_DrawPatch(int x, int y, patch_t *patch, const char *name)
{ 
    int count;
    int col;
    column_t *column;
    pixel_t *desttop;
    pixel_t *dest;
    const byte *source;
    const byte *sourcetrans;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > SCREENWIDTH
     || y < 0
     || y + SHORT(patch->height) > SCREENHEIGHT)
    {
		// RestlessRodent -- Do not die
		// [JN] ... print a critical message instead.
        CRL_SetMessageCritical("V_DRAWPATCH:", 
        M_StringJoin("BAD V_DRAWPATCH \"", name, "\"", NULL), 2);
		return;
    }

    V_MarkRect(x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = sourcetrans = (byte *)column + 3;
            dest = desttop + column->topdelta*SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                if (dp_translation)
                {
                    sourcetrans = &dp_translation[*source++];
                }

                if (dp_translucent)
                {
                    *dest = tintmap[((*dest) << 8) + *sourcetrans++];
                }
                else
                {
                    *dest = *sourcetrans++;
                }

                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

//
// V_DrawPatchFlipped
// Masks a column based masked pic to the screen.
// Flips horizontally, e.g. to mirror face.
//

void V_DrawPatchFlipped(int x, int y, patch_t *patch)
{
    int count;
    int col; 
    column_t *column; 
    pixel_t *desttop;
    pixel_t *dest;
    const byte *source; 
    int w; 
 
    y -= SHORT(patch->topoffset); 
    x -= SHORT(patch->leftoffset); 

#ifdef RANGECHECK 
    if (x < 0
     || x + SHORT(patch->width) > SCREENWIDTH
     || y < 0
     || y + SHORT(patch->height) > SCREENHEIGHT)
    {
        // [JN] Do not crash, print a critical message instead.
        CRL_SetMessageCritical("V_DRAWPATCHFLIPPED:", 
        M_StringJoin("BAD V_DRAWPATCH \"", "UNKNOWN", "\"", NULL), 2);
        return;
    }
#endif

    V_MarkRect (x, y, SHORT(patch->width), SHORT(patch->height));

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for ( ; col<w ; x++, col++, desttop++)
    {
        column = (column_t *)((byte *)patch + LONG(patch->columnofs[w-1-col]));

        // step through the posts in a column
        while (column->topdelta != 0xff )
        {
            source = (byte *)column + 3;
            dest = desttop + column->topdelta*SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                *dest = *source++;
                dest += SCREENWIDTH;
            }
            column = (column_t *)((byte *)column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatch
// [JN] Masks a column based masked pic to the screen.
// Used by Doom with tintmap map.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatch (int x, int y, const patch_t *patch, const char *name)
{
    int       count, col, w;
    const byte *source;
    const byte *sourcetrans;
    pixel_t  *desttop, *dest, *dest2;
    column_t *column;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

#ifdef RANGECHECK
    if (x < 0
    ||  x + SHORT(patch->width) > SCREENWIDTH
    ||  y < 0
    ||  y + SHORT(patch->height) > SCREENHEIGHT)
    {
        // [JN] Do not crash, print a critical message instead.
        CRL_SetMessageCritical("V_DRAWPATCH:",
        M_StringJoin("BAD V_DRAWPATCH \"", name, "\"", NULL), 2);
        return;
    }
#endif

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for (; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = sourcetrans = (byte *) column + 3;
            dest = desttop + column->topdelta*SCREENWIDTH;
            dest2 = dest + SCREENWIDTH + 1;
            count = column->length;

            while (count--)
            {
                if (dp_translation)
                {
                    sourcetrans = &dp_translation[*source++];
                }
                if (crl_text_shadows)
                {
                    *dest2 = tintmap[((*dest2) << 8)];
                    dest2 += SCREENWIDTH;
                }

                if (dp_translucent)
                {
                    *dest = tintmap[((*dest) << 8) + *sourcetrans++];
                }
                else
                {
                    *dest = *sourcetrans++;
                }
                dest += SCREENWIDTH;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatchRaven
// Masks a column based masked pic to the screen.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatchRaven(int x, int y, patch_t *patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    const byte *source;
    const byte *sourcetrans;
    pixel_t *desttop2, *dest2;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > SCREENWIDTH
     || y < 0
     || y + SHORT(patch->height) > SCREENHEIGHT)
    {
        I_Error("Bad V_DrawShadowedPatch");
    }

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;
    desttop2 = dest_screen + (y + 2) * SCREENWIDTH + x + 2;

    w = SHORT(patch->width);
    for (; col < w; x++, col++, desttop++, desttop2++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            source = sourcetrans = (byte *) column + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            dest2 = desttop2 + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                if (dp_translation)
                {
                    sourcetrans = &dp_translation[*source++];
                }

                *dest2 = tinttable[((*dest2) << 8)];
                dest2 += SCREENWIDTH;

                *dest = *sourcetrans++;
                dest += SCREENWIDTH;

            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

// -----------------------------------------------------------------------------
// V_DrawShadowedPatchRavenOptional
// [JN] Masks a column based masked pic to the screen.
// Used by Heretic and Hexen with tinttable map and draws optional shadow.
// -----------------------------------------------------------------------------

void V_DrawShadowedPatchRavenOptional (int x, int y, const patch_t *patch, const char *name)
{
    int       count, col, w;
    const byte *source;
    const byte *sourcetrans;
    pixel_t  *desttop, *dest, *dest2;
    column_t *column;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

#ifdef RANGECHECK
    if (x < 0
    ||  x + SHORT(patch->width) > SCREENWIDTH
    ||  y < 0
    ||  y + SHORT(patch->height) > SCREENHEIGHT)
    {
        // [JN] Do not crash, print a critical message instead.
        CRL_SetMessageCritical("V_DRAWPATCH:",
        M_StringJoin("BAD V_DRAWPATCH \"", name, "\"", NULL), 2);
        return;
    }
#endif

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);

    for (; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            source = sourcetrans = (byte *) column + 3;
            dest = desttop + column->topdelta*SCREENWIDTH;
            dest2 = dest + SCREENWIDTH + 1;
            count = column->length;

            while (count--)
            {
                if (dp_translation)
                {
                    sourcetrans = &dp_translation[*source++];
                }
                if (crl_text_shadows)
                {
                    *dest2 = tinttable[((*dest2) << 8)];
                    dest2 += SCREENWIDTH;
                }
                *dest = *sourcetrans++;
                dest += SCREENWIDTH;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

//
// V_DrawTLPatch
//
// Masks a column based translucent masked pic to the screen.
//

void V_DrawTLPatch(int x, int y, patch_t * patch)
{
    int count, col;
    column_t *column;
    pixel_t *desttop, *dest;
    const byte *source;
    int w;

    y -= SHORT(patch->topoffset);
    x -= SHORT(patch->leftoffset);

    if (x < 0
     || x + SHORT(patch->width) > SCREENWIDTH 
     || y < 0
     || y + SHORT(patch->height) > SCREENHEIGHT)
    {
        I_Error("Bad V_DrawTLPatch");
    }

    col = 0;
    desttop = dest_screen + y * SCREENWIDTH + x;

    w = SHORT(patch->width);
    for (; col < w; x++, col++, desttop++)
    {
        column = (column_t *) ((byte *) patch + LONG(patch->columnofs[col]));

        // step through the posts in a column

        while (column->topdelta != 0xff)
        {
            source = (byte *) column + 3;
            dest = desttop + column->topdelta * SCREENWIDTH;
            count = column->length;

            while (count--)
            {
                *dest = tinttable[((*dest) << 8) + *source++];
                dest += SCREENWIDTH;
            }
            column = (column_t *) ((byte *) column + column->length + 4);
        }
    }
}

//
// Draw a "raw" screen (lump containing raw data to blit directly
// to the screen)
//
 
void V_DrawRawScreen (const byte *raw)
{
    memcpy(dest_screen, raw, SCREENWIDTH * SCREENHEIGHT);
}

//
// Load tint table from TINTTAB lump.
//

void V_LoadTintTable(void)
{
    tinttable = W_CacheLumpName("TINTTAB", PU_STATIC);
}

//
// V_DrawBlock
// Draw a linear block of pixels into the view buffer.
//

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src)
{ 
    pixel_t *dest;
 
#ifdef RANGECHECK 
    if (x < 0
     || x + width >SCREENWIDTH
     || y < 0
     || y + height > SCREENHEIGHT)
    {
	I_Error ("Bad V_DrawBlock");
    }
#endif 
 
    V_MarkRect (x, y, width, height); 
 
    dest = dest_screen + y * SCREENWIDTH + x; 

    while (height--) 
    { 
	memcpy (dest, src, width * sizeof(*dest));
	src += width; 
	dest += SCREENWIDTH; 
    } 
} 

void V_DrawFilledBox(int x, int y, int w, int h, int c)
{
    uint8_t *buf, *buf1;
    int x1, y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1)
    {
        buf1 = buf;

        for (x1 = 0; x1 < w; ++x1)
        {
            *buf1++ = c;
        }

        buf += SCREENWIDTH;
    }
}

void V_DrawHorizLine(int x, int y, int w, int c)
{
    uint8_t *buf;
    int x1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (x1 = 0; x1 < w; ++x1)
    {
        *buf++ = c;
    }
}

void V_DrawVertLine(int x, int y, int h, int c)
{
    uint8_t *buf;
    int y1;

    buf = I_VideoBuffer + SCREENWIDTH * y + x;

    for (y1 = 0; y1 < h; ++y1)
    {
        *buf = c;
        buf += SCREENWIDTH;
    }
}

void V_DrawBox(int x, int y, int w, int h, int c)
{
    V_DrawHorizLine(x, y, w, c);
    V_DrawHorizLine(x, y+h-1, w, c);
    V_DrawVertLine(x, y, h, c);
    V_DrawVertLine(x+w-1, y, h, c);
}

// [crispy] Unified function of flat filling. Used for intermission
// and finale screens, view border and status bar's wide screen mode.
void V_FillFlat(int y_start, int y_stop, int x_start, int x_stop,
                const byte *src, pixel_t *dest)
{
    int x, y;

    for (y = y_start; y < y_stop; y++)
    {
        for (x = x_start; x < x_stop; x++)
        {
            *dest++ = src[((y & 63) * 64)
                         + (x & 63)];
        }
    }
}

// Set the buffer that the code draws to.

void V_UseBuffer(pixel_t *buffer)
{
    dest_screen = buffer;
}

// Restore screen buffer to the i_video screen buffer.

void V_RestoreBuffer(void)
{
    dest_screen = I_VideoBuffer;
}

//
// SCREEN SHOTS
//


//
// WritePNGfile
//

static void WritePNGfile (const char *filename)
{
    byte *data;
    int width, height;
    size_t png_data_size = 0;

    I_RenderReadPixels(&data, &width, &height);
    {
        void *pPNG_data = tdefl_write_image_to_png_file_in_memory(data, width, height, 4, &png_data_size);

        if (!pPNG_data)
        {
            return;
        }
        else
        {
            FILE *handle = M_fopen(filename, "wb");
            fwrite(pPNG_data, 1, png_data_size, handle);
            fclose(handle);
            mz_free(pPNG_data);
        }
    }

    free(data);
}

//
// V_ScreenShot
//

void V_ScreenShot(const char *format)
{
    int i;
    char lbmname[16]; // haleyjd 20110213: BUG FIX - 12 is too small!
    const char *file;
    
    // find a file name to save it to

    for (i=0; i<=9999; i++)
    {
        M_snprintf(lbmname, sizeof(lbmname), format, i, "png");
        // [JN] Construct full path to screenshot file.
        file = M_StringJoin(screenshotdir, lbmname, NULL);

        if (!M_FileExists(file))
        {
            break;      // file doesn't exist
        }
    }

    if (i == 10000)
    {
        I_Error ("V_ScreenShot: Couldn't create a PNG");
    }

    WritePNGfile(file);
}

#define MOUSE_SPEED_BOX_WIDTH  120
#define MOUSE_SPEED_BOX_HEIGHT 9

//
// V_DrawMouseSpeedBox
//

// If box is only to calibrate speed, testing relative speed (as a measure
// of game pixels to movement units) is important whether physical mouse DPI
// is high or low. Line resolution starts at 1 pixel per 1 move-unit: if
// line maxes out, resolution becomes 1 pixel per 2 move-units, then per
// 3 move-units, etc.

static int linelen_multiplier = 1;

void V_DrawMouseSpeedBox(int speed)
{
    int bgcolor, bordercolor, red, black, white, yellow;
    int box_x, box_y;
    int original_speed;
    int redline_x;
    int linelen;
    int i;
    boolean draw_acceleration = false;

    // Get palette indices for colors for widget. These depend on the
    // palette of the game being played.

    bgcolor = I_GetPaletteIndex(0x77, 0x77, 0x77);
    bordercolor = I_GetPaletteIndex(0x55, 0x55, 0x55);
    red = I_GetPaletteIndex(0xff, 0x00, 0x00);
    black = I_GetPaletteIndex(0x00, 0x00, 0x00);
    yellow = I_GetPaletteIndex(0xff, 0xff, 0x00);
    white = I_GetPaletteIndex(0xff, 0xff, 0xff);

    // If the mouse is turned off, don't draw the box at all.
    if (!usemouse)
    {
        return;
    }

    // If acceleration is used, draw a box that helps to calibrate the
    // threshold point.
    if (mouse_threshold > 0 && fabs(mouse_acceleration - 1) > 0.01)
    {
        draw_acceleration = true;
    }

    // Calculate box position

    box_x = SCREENWIDTH - MOUSE_SPEED_BOX_WIDTH - 10;
    box_y = 15;

    V_DrawFilledBox(box_x, box_y,
                    MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT, bgcolor);
    V_DrawBox(box_x, box_y,
              MOUSE_SPEED_BOX_WIDTH, MOUSE_SPEED_BOX_HEIGHT, bordercolor);

    // Calculate the position of the red threshold line when calibrating
    // acceleration.  This is 1/3 of the way along the box.

    redline_x = MOUSE_SPEED_BOX_WIDTH / 3;

    // Calculate line length

    if (draw_acceleration && speed >= mouse_threshold)
    {
        // Undo acceleration and get back the original mouse speed
        original_speed = speed - mouse_threshold;
        original_speed = (int) (original_speed / mouse_acceleration);
        original_speed += mouse_threshold;

        linelen = (original_speed * redline_x) / mouse_threshold;
    }
    else
    {
        linelen = speed / linelen_multiplier;
    }

    // Draw horizontal "thermometer" 

    if (linelen > MOUSE_SPEED_BOX_WIDTH - 1)
    {
        linelen = MOUSE_SPEED_BOX_WIDTH - 1;
        if (!draw_acceleration)
        {
            linelen_multiplier++;
        }
    }

    V_DrawHorizLine(box_x + 1, box_y + 4, MOUSE_SPEED_BOX_WIDTH - 2, black);

    if (!draw_acceleration || linelen < redline_x)
    {
        V_DrawHorizLine(box_x + 1, box_y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        linelen, white);
    }
    else
    {
        V_DrawHorizLine(box_x + 1, box_y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        redline_x, white);
        V_DrawHorizLine(box_x + redline_x, box_y + MOUSE_SPEED_BOX_HEIGHT / 2,
                        linelen - redline_x, yellow);
    }

    if (draw_acceleration)
    {
        // Draw acceleration threshold line
        V_DrawVertLine(box_x + redline_x, box_y + 1,
                       MOUSE_SPEED_BOX_HEIGHT - 2, red);
    }
    else
    {
        // Draw multiplier lines to indicate current resolution
        for (i = 1; i < linelen_multiplier; i++)
        {
            V_DrawVertLine(
                box_x + (i * MOUSE_SPEED_BOX_WIDTH / linelen_multiplier),
                box_y + 1, MOUSE_SPEED_BOX_HEIGHT - 2, yellow);
        }
    }
}
