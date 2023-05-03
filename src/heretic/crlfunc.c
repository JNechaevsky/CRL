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


#include <stdio.h>

#include "i_timer.h"
#include "m_misc.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomdef.h"

#include "crlcore.h"
#include "crlvars.h"
#include "crlfunc.h"


// =============================================================================
//
//                        Render Counters and Widgets
//
// =============================================================================


// -----------------------------------------------------------------------------
// CRL_DrawFPS.
//  [JN] Draw actual frames per second value.
//  Some MN_TextAWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void CRL_DrawFPS (void)
{
    char fps[4];
    char fps_str[4];

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, "FPS");

    MN_DrTextA(fps, SCREENWIDTH - 11 - MN_TextAWidth(fps) 
                                     - MN_TextAWidth(fps_str), 30, cr[CR_GRAY]);

    MN_DrTextA(fps_str, SCREENWIDTH - 7 - MN_TextAWidth(fps_str), 30, cr[CR_GRAY]);
}
