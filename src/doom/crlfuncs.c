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
#include "m_menu.h"
#include "m_misc.h"
#include "v_trans.h"
#include "v_video.h"

#include "crlcore.h"
#include "crlvars.h"
#include "crlfuncs.h"


// =============================================================================
//
//                        Render Counters and Widgets
//
// =============================================================================

extern int gametic;

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

static byte *CRL_PowerupColor (const int val1, const int val2)
{
    return
        val1 > val2/2 ? cr[CR_GREEN]  :
        val1 > val2/4 ? cr[CR_YELLOW] :
                        cr[CR_RED]    ;
}

// -----------------------------------------------------------------------------
// Draws CRL stats.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void CRL_StatDrawer (void)
{
    // Count MAX visplanes for jumping
    static int CRL_MAX_count;
    const  int CRL_MAX_countOld = CRLData.numcheckplanes + CRLData.numfindplanes;

    // Should MAX be cleared?
    if (CRL_MAX_toClear)
    {
        CRL_MAX_count = 0;  // Will be recalculated in condition below.
        CRL_MAX_toClear = false;
    }
    // Update MAX value
    if (CRL_MAX_countOld > CRL_MAX_count)
    {
        CRL_MAX_count = CRL_MAX_countOld;
        CRL_MAX_toSet = true;
    }

    // Player coords
    if (crl_widget_coords)
    {
        char str[128];

        M_WriteText(0, 27, "X:", cr[CR_GRAY]);
        M_WriteText(0, 36, "Y:", cr[CR_GRAY]);
        M_WriteText(0, 45, "ANG:", cr[CR_GRAY]);

        sprintf(str, "%d", CRLWidgets.x);
        M_WriteText(16, 27, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.y);
        M_WriteText(16, 36, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.ang);
        M_WriteText(32, 45, str, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        // Icon of Sin spitter targets (32 max)
        if ((crl_widget_playstate == 1 
        ||  (crl_widget_playstate == 2 && CRL_brain_counter > 32)) && CRL_brain_counter)
        {
            char brn[32];

            M_WriteText(0, 63, "BRN:", CRL_StatColor_Str(CRL_brain_counter, 32));
            M_snprintf(brn, 16, "%d/32", CRL_brain_counter);
            M_WriteText(32, 63, brn, CRL_StatColor_Val(CRL_brain_counter, 32));
        }

        // Buttons (16 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_buttons_counter > 16))
        {
            char btn[32];

            M_WriteText(0, 72, "BTN:", CRL_StatColor_Str(CRL_buttons_counter, 16));
            M_snprintf(btn, 16, "%d/16", CRL_buttons_counter);
            M_WriteText(32, 72, btn, CRL_StatColor_Val(CRL_buttons_counter, 16));
        }

        // Plats (30 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            M_WriteText(0, 81, "PLT:", CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            M_WriteText(32, 81, plt, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            M_WriteText(0, 90, "ANI:", CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            M_WriteText(32, 90, ani, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
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

            M_WriteText(0, 108, "SPR:", CRL_StatColor_Str(CRLData.numsprites, CRL_MaxVisSprites));
            M_snprintf(spr, 16, "%d/%d", CRLData.numsprites, CRL_MaxVisSprites);
            M_WriteText(32, 108, spr, CRL_StatColor_Val(CRLData.numsprites, CRL_MaxVisSprites));
        }

        // Segments (256 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsegs >= CRL_MaxDrawSegs))
        {
            char seg[32];

            M_WriteText(0, 117, "SEG:", CRL_StatColor_Str(CRLData.numsegs, CRL_MaxDrawSegs));
            M_snprintf(seg, 16, "%d/%d", CRLData.numsegs, CRL_MaxDrawSegs);
            M_WriteText(32, 117, seg, CRL_StatColor_Val(CRLData.numsegs, CRL_MaxDrawSegs));
        }

        // Openings
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numopenings >= CRL_MaxOpenings))
        {
            char opn[64];

            M_WriteText(0, 126, "OPN:", CRL_StatColor_Str(CRLData.numopenings, CRL_MaxOpenings));
            M_snprintf(opn, 16, "%d/%d", CRLData.numopenings, CRL_MaxOpenings);
            M_WriteText(32, 126, opn, CRL_StatColor_Val(CRLData.numopenings, CRL_MaxOpenings));
        }


        // Planes (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numcheckplanes + CRLData.numfindplanes >= CRL_MaxVisPlanes))
        {
            char vis[32];
            const int totalplanes = CRLData.numcheckplanes
                                  + CRLData.numfindplanes;

            M_WriteText(0, 135, "PLN:", totalplanes >= CRL_MaxVisPlanes ? 
                       (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : cr[CR_GRAY]);
            M_snprintf(vis, 32, "%d/%d (MAX: %d)", totalplanes, CRL_MaxVisPlanes, CRL_MAX_count);
            M_WriteText(32, 135, vis, totalplanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
        }


    }

    // Level / DeathMatch timer
    if (crl_widget_time)
    {
        extern int leveltime;
        extern int levelTimeCount;
        extern boolean levelTimer;

        const int time = (deathmatch && levelTimer ? levelTimeCount : leveltime) / TICRATE;
        char stra[8];
        char strb[16];
        const int yy = automapactive ? 8 : 0;

        sprintf(stra, "TIME ");
        M_WriteText(0, 152 - yy, stra, cr[CR_GRAY]);
 
        sprintf(strb, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        M_WriteText(0 + M_StringWidth(stra), 152 - yy, strb, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis)
    {
        const int yy = automapactive ? 8 : 0;

        if (!deathmatch)
        {
            char str1[8], str2[16];  // kills
            char str3[8], str4[16];  // items
            char str5[8], str6[16];  // secret

            // Kills:
            sprintf(str1, "K ");
            M_WriteText(0, 160 - yy, str1, cr[CR_GRAY]);
            
            sprintf(str2, "%d/%d ", CRLWidgets.kills, CRLWidgets.totalkills);
            M_WriteText(0 + M_StringWidth(str1), 160 - yy, str2,
                        CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                        CRLWidgets.kills == 0 ? cr[CR_RED] :
                        CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Items:
            sprintf(str3, "I ");
            M_WriteText(M_StringWidth(str1) + M_StringWidth(str2), 160 - yy, str3, cr[CR_GRAY]);
            
            sprintf(str4, "%d/%d ", CRLWidgets.items, CRLWidgets.totalitems);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3), 160 - yy, str4,
                        CRLWidgets.totalitems == 0 ? cr[CR_GREEN] :
                        CRLWidgets.items == 0 ? cr[CR_RED] :
                        CRLWidgets.items < CRLWidgets.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Secret:
            sprintf(str5, "S ");
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3) +
                        M_StringWidth(str4), 160 - yy, str5, cr[CR_GRAY]);

            sprintf(str6, "%d/%d ", CRLWidgets.secrets, CRLWidgets.totalsecrets);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) + 
                        M_StringWidth(str3) +
                        M_StringWidth(str4) +
                        M_StringWidth(str5), 160 - yy, str6,
                        CRLWidgets.totalsecrets == 0 ? cr[CR_GREEN] :
                        CRLWidgets.secrets == 0 ? cr[CR_RED] :
                        CRLWidgets.secrets < CRLWidgets.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN]);
        }
        else
        {
            char str1[8] = {0}, str2[16] = {0};  // Green
            char str3[8] = {0}, str4[16] = {0};  // Indigo
            char str5[8] = {0}, str6[16] = {0};  // Brown
            char str7[8] = {0}, str8[16] = {0};  // Red

            // Green
            if (playeringame[0])
            {
                sprintf(str1, "G ");
                M_WriteText(0, 160 - yy, str1, cr[CR_GREEN]);

                sprintf(str2, "%d ", CRLWidgets.frags_g);
                M_WriteText(M_StringWidth(str1), 160 - yy, str2, cr[CR_GREEN]);
            }
            // Indigo
            if (playeringame[1])
            {
                sprintf(str3, "I ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2),
                            160 - yy, str3, cr[CR_GRAY]);

                sprintf(str4, "%d ", CRLWidgets.frags_i);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3),
                            160 - yy, str4, cr[CR_GRAY]);
            }
            // Brown
            if (playeringame[2])
            {
                sprintf(str5, "B ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4),
                            160 - yy, str5, cr[CR_BROWN]);

                sprintf(str6, "%d ", CRLWidgets.frags_b);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5),
                            160 - yy, str6, cr[CR_BROWN]);
            }
            // Red
            if (playeringame[3])
            {
                sprintf(str7, "R ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6),
                            160 - yy, str7, cr[CR_RED]);

                sprintf(str8, "%d ", CRLWidgets.frags_r);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6) +
                            M_StringWidth(str7),
                            160 - yy, str8, cr[CR_RED]);
            }
        }
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
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
    }
}

// -----------------------------------------------------------------------------
// CRL_DrawFPS.
//  [JN] Draw actual frames per second value.
//  Some M_StringWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void CRL_DrawFPS (void)
{
    char fps[4];
    char fps_str[4];

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, "FPS");

    M_WriteText(SCREENWIDTH - 11 - M_StringWidth(fps) 
                                 - M_StringWidth(fps_str), 27, fps, cr[CR_GRAY]);

    M_WriteText(SCREENWIDTH - 7 - M_StringWidth(fps_str), 27, "FPS", cr[CR_GRAY]);
}

// =============================================================================
//
//                             Demo enhancements
//
// =============================================================================





// -----------------------------------------------------------------------------
// CRL_DemoTimer
//  [crispy] Demo Timer widget
// -----------------------------------------------------------------------------

void CRL_DemoTimer (const int time)
{
    const int hours = time / (3600 * TICRATE);
    const int mins = time / (60 * TICRATE) % 60;
    const float secs = (float)(time % (60 * TICRATE)) / TICRATE;
    char n[16];
    int x = 237;

    if (hours)
    {
        M_snprintf(n, sizeof(n), "%02i:%02i:%05.02f", hours, mins, secs);
    }
    else
    {
        M_snprintf(n, sizeof(n), "%02i:%05.02f", mins, secs);
        x += 20;
    }

    M_WriteText(x, 18, n, cr[CR_LIGHTGRAY]);
}

// -----------------------------------------------------------------------------
// CRL_DemoBar
//  [crispy] print a bar indicating demo progress at the bottom of the screen
// -----------------------------------------------------------------------------

void CRL_DemoBar (void)
{
    const int i = SCREENWIDTH * defdemotics / deftotaldemotics;

    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, 0); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, 4); // [crispy] white
}
