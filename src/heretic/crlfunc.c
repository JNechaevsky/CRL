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
// CRL_StatColor_Str, CRL_StatColor_Val
//  [JN] Colorizes counter strings and values respectively.
// -----------------------------------------------------------------------------

static byte *CRL_StatColor_Str (const int val1, const int val2)
{
    return
        val1 == val2 ? cr[CR_LIGHTGRAY] :
        val1 >= val2 ? (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : 
                       cr[CR_GRAY];
}

static byte *CRL_StatColor_Val (const int val1, const int val2)
{
    return
        val1 == val2 ? cr[CR_YELLOW] :
        val1 >= val2 ? (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) :
                       cr[CR_GREEN];
}

/*
static byte *CRL_PowerupColor (const int val1, const int val2)
{
    return
        val1 > val2/2 ? cr[CR_GREEN]  :
        val1 > val2/4 ? cr[CR_YELLOW] :
                        cr[CR_RED]    ;
}
*/

// -----------------------------------------------------------------------------
// Draws CRL stats.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void CRL_StatDrawer (void)
{
    // Player coords
    if (crl_widget_coords)
    {
        char x[8] = {0}, xpos[128] = {0};
        char y[8] = {0}, ypos[128] = {0};
        char ang[128];

        M_snprintf(x, 8, "X: ");
        MN_DrTextA(x, 0, 30, cr[CR_GRAY]);
        M_snprintf(xpos, 128, "%d ", CRLWidgets.x);
        MN_DrTextA(xpos, MN_TextAWidth(x), 30, cr[CR_GREEN]);

        M_snprintf(y, 8, "Y: ");
        MN_DrTextA(y, MN_TextAWidth(x) + 
                      MN_TextAWidth(xpos), 30, cr[CR_GRAY]);
        M_snprintf(ypos, 128, "%d", CRLWidgets.y);
        MN_DrTextA(ypos, MN_TextAWidth(x) +
                         MN_TextAWidth(xpos) +
                         MN_TextAWidth(y), 30, cr[CR_GREEN]);

        MN_DrTextA("ANG:", 0, 40, cr[CR_GRAY]);
        M_snprintf(ang, 16, "%d", CRLWidgets.ang);
        MN_DrTextA(ang, 32, 40, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            MN_DrTextA("PLT:", 0, 55, CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            MN_DrTextA(plt, 32, 55, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            MN_DrTextA("ANI:", 0, 65, CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            MN_DrTextA(ani, 32, 65, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
        }
    }

    // Render counters
    if (crl_widget_render)
    {
        // Sprites (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsprites >= CRL_MaxVisSprites))
        {
            char spr[32];
            
            MN_DrTextA("SPR:", 0, 75, CRL_StatColor_Str(CRLData.numsprites, CRL_MaxVisSprites));
            M_snprintf(spr, 16, "%d/%d", CRLData.numsprites, CRL_MaxVisSprites);
            MN_DrTextA(spr, 32, 75, CRL_StatColor_Val(CRLData.numsprites, CRL_MaxVisSprites));
        }

        // Segments (256 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsegs >= CRL_MaxDrawSegs))
        {
            char seg[32];

            MN_DrTextA("SEG:", 0, 85, CRL_StatColor_Str(CRLData.numsegs, CRL_MaxDrawSegs));
            M_snprintf(seg, 16, "%d/%d", CRLData.numsegs, CRL_MaxDrawSegs);
            MN_DrTextA(seg, 32, 85, CRL_StatColor_Val(CRLData.numsegs, CRL_MaxDrawSegs));
        }

        // Solid segments (32 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsolidsegs >= 32))
        {
            char ssg[32];

            MN_DrTextA("SSG:", 0, 95, CRL_StatColor_Str(CRLData.numsolidsegs, 32));
            M_snprintf(ssg, 16, "%d/32", CRLData.numsolidsegs);
            MN_DrTextA(ssg, 32, 95, CRL_StatColor_Val(CRLData.numsolidsegs, 32));
        }

        // Planes (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numcheckplanes + CRLData.numfindplanes >= CRL_MaxVisPlanes))
        {
            char pln[32];
            const int totalplanes = CRLData.numcheckplanes + CRLData.numfindplanes;

            MN_DrTextA("PLN:", 0, 105, totalplanes >= CRL_MaxVisPlanes ? 
                      (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : cr[CR_GRAY]);
            M_snprintf(pln, 32, "%d/%d", totalplanes, CRL_MaxVisPlanes);
            MN_DrTextA(pln, 32, 105, totalplanes >= CRL_MaxVisPlanes ?
                      (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
        }

        // Openings
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numopenings >= CRL_MaxOpenings))
        {
            char opn[64];

            MN_DrTextA("OPN:", 0, 115, CRL_StatColor_Str(CRLData.numopenings, CRL_MaxOpenings));
            M_snprintf(opn, 16, "%d/%d", CRLData.numopenings, CRL_MaxOpenings);
            MN_DrTextA(opn, 32, 115, CRL_StatColor_Val(CRLData.numopenings, CRL_MaxOpenings));
        }
    }

    // Level timer
    if (crl_widget_time)
    {
        char stra[8];
        char strb[16];
        const int time = leveltime / TICRATE;
        
        M_snprintf(stra, 8, "TIME ");
        MN_DrTextA(stra, 0, 125, cr[CR_GRAY]);
        M_snprintf(strb, 16, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        MN_DrTextA(strb, MN_TextAWidth(stra), 125, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis)
    {
        char str1[8], str2[16];  // kills
        char str3[8], str4[16];  // items
        char str5[8], str6[16];  // secret

        // Kills:
        M_snprintf(str1, 8, "K ");
        MN_DrTextA(str1, 0, 135, cr[CR_GRAY]);
        M_snprintf(str2, 16, "%d/%d ", CRLWidgets.kills, CRLWidgets.totalkills);
        MN_DrTextA(str2, MN_TextAWidth(str1), 135,
                         CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                         CRLWidgets.kills == 0 ? cr[CR_RED] :
                         CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN]);

        // Items:
        M_snprintf(str3, 8, "I ");
        MN_DrTextA(str3, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2), 135, cr[CR_GRAY]);
        M_snprintf(str4, 16, "%d/%d ", CRLWidgets.items, CRLWidgets.totalitems);
        MN_DrTextA(str4, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3), 135,
                         CRLWidgets.totalitems == 0 ? cr[CR_GREEN] :
                         CRLWidgets.items == 0 ? cr[CR_RED] :
                         CRLWidgets.items < CRLWidgets.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN]);

        // Secrets:
        M_snprintf(str5, 8, "S ");
        MN_DrTextA(str5, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3) +
                         MN_TextAWidth(str4), 135, cr[CR_GRAY]);
        M_snprintf(str6, 16, "%d/%d ", CRLWidgets.secrets, CRLWidgets.totalsecrets);
        MN_DrTextA(str6, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3) +
                         MN_TextAWidth(str4) +
                         MN_TextAWidth(str5), 135,
                         CRLWidgets.totalsecrets == 0 ? cr[CR_GREEN] :
                         CRLWidgets.secrets == 0 ? cr[CR_RED] :
                         CRLWidgets.secrets < CRLWidgets.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN]);
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
        /*
        if (CRL_invul_counter)
        {
            char invl[4];

            M_WriteText(292 - M_StringWidth("INVL:"), 108, "INVL:", cr[CR_GRAY]);
            M_snprintf(invl, 4, "%d", CRL_invul_counter);
            M_WriteText(296, 108, invl, CRL_PowerupColor(CRL_invul_counter, 30));
        }

        if (CRL_invis_counter)
        {
            char invs[4];

            M_WriteText(292 - M_StringWidth("INVS:"), 117, "INVS:", cr[CR_GRAY]);
            M_snprintf(invs, 4, "%d", CRL_invis_counter);
            M_WriteText(296, 117, invs, CRL_PowerupColor(CRL_invis_counter, 60));
        }

        if (CRL_rad_counter)
        {
            char rad[4];

            M_WriteText(292 - M_StringWidth("RAD:"), 126, "RAD:", cr[CR_GRAY]);
            M_snprintf(rad, 4, "%d", CRL_rad_counter);
            M_WriteText(296, 126, rad, CRL_PowerupColor(CRL_rad_counter, 60));
        }

        if (CRL_amp_counter)
        {
            char amp[4];

            M_WriteText(292 - M_StringWidth("AMP:"), 135, "AMP:", cr[CR_GRAY]);
            M_snprintf(amp, 4, "%d", CRL_amp_counter);
            M_WriteText(296, 135, amp, CRL_PowerupColor(CRL_amp_counter, 120));
        }
        */
    }
}

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
