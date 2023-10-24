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


#include <stdio.h>

#include "i_timer.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_menu.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_local.h"

#include "crlcore.h"
#include "crlvars.h"
#include "crlfunc.h"


// =============================================================================
//
//                            Visplanes coloring
//
// =============================================================================

int CRL_PlaneBorderColors[NUMPLANEBORDERCOLORS] =
{
    // LIGHT
    16,     // yucky pink
    176,    // red
    216,    // orange
    231,    // yellow

    112,    // green
    195,    // light blue (cyanish)
    202,    // Deep blue
    251,    // yuck, magenta

    // DARK
    26,
    183,
    232,
    164,

    122,
    198,
    240,
    254,
};

// =============================================================================
//
//                       Messages handling and drawing
//
// =============================================================================

// -----------------------------------------------------------------------------
// CRL_DrawMessage
// [JN] Draws message on the screen.
// -----------------------------------------------------------------------------

void CRL_DrawMessage (void)
{
    player_t *player = &players[displayplayer];

    if (player->messageTics <= 0 || !player->message)
    {
        return;  // No message
    }

    M_WriteText(0, 0, player->message, player->messageColor);
}

// -----------------------------------------------------------------------------
// CRL_DrawCriticalMessage
// [JN] Draws critical message on the second and third lines of the screen.
// -----------------------------------------------------------------------------

void CRL_DrawCriticalMessage (void)
{
    player_t *player = &players[displayplayer];

    if (player->criticalmessageTics <= 0
    || !player->criticalmessage1 || !player->criticalmessage2)
    {
        return;  // No message
    }

    if (crl_msg_critical == 0)
    {
        // Static
        M_WriteTextCritical(9, player->criticalmessage1, player->criticalmessage2,
                            cr[CR_RED]);
    }
    else
    {
        // Blinking
        M_WriteTextCritical(9, player->criticalmessage1, player->criticalmessage2,
                            gametic & 8 ? cr[CR_DARKRED] : cr[CR_RED]);
    }

}

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

static byte *CRL_PowerupColor (const int val1, const int val2)
{
    return
        val1 > val2/2 ? cr[CR_GREEN]  :
        val1 > val2/4 ? cr[CR_YELLOW] :
                        cr[CR_RED]    ;
}

// -----------------------------------------------------------------------------
// CRL_MAX_count
//  [JN] Handling of MAX visplanes, based on implementation from RestlessRodent.
// -----------------------------------------------------------------------------

static int CRL_MAX_count;

void CRL_Clear_MAX (void)
{
    CRL_MAX_count = 0;
    CRL_MAX_x = 0;
    CRL_MAX_y = 0;
    CRL_MAX_z = 0;
    CRL_MAX_ang = 0;
}

void CRL_Get_MAX (void)
{
    player_t *player = &players[displayplayer];

    if (crl_spectating)
    {
        if (crl_uncapped_fps)
        {
            CRL_MAX_x = CRL_camera_oldx + FixedMul(CRL_camera_x - CRL_camera_oldx, fractionaltic);
            CRL_MAX_y = CRL_camera_oldy + FixedMul(CRL_camera_y - CRL_camera_oldy, fractionaltic);
            CRL_MAX_z = CRL_camera_oldz + FixedMul(CRL_camera_z - CRL_camera_oldz, fractionaltic) - VIEWHEIGHT;
            CRL_MAX_ang = R_InterpolateAngle(CRL_camera_oldang, CRL_camera_ang, fractionaltic);
        }
        else
        {
            CRL_MAX_x = CRL_camera_x;
            CRL_MAX_y = CRL_camera_y;
            CRL_MAX_z = CRL_camera_z - VIEWHEIGHT;
            CRL_MAX_ang = CRL_camera_ang;
        }
    }
    else
    {
        if (crl_uncapped_fps)
        {
            CRL_MAX_x = player->mo->oldx + FixedMul(player->mo->x - player->mo->oldx, fractionaltic);
            CRL_MAX_y = player->mo->oldy + FixedMul(player->mo->y - player->mo->oldy, fractionaltic);
            CRL_MAX_z = player->mo->oldz + FixedMul(player->mo->z - player->mo->oldz, fractionaltic);
            CRL_MAX_ang = R_InterpolateAngle(player->mo->oldangle, player->mo->angle, fractionaltic);
        }
        else
        {
            CRL_MAX_x = player->mo->x;
            CRL_MAX_y = player->mo->y;
            CRL_MAX_z = player->mo->z;
            CRL_MAX_ang = player->mo->angle;
        }
    }
}

void CRL_MoveTo_MAX (void)
{
    player_t *player = &players[displayplayer];

    // Define subsector we will move on.
    subsector_t* ss = R_PointInSubsector(CRL_MAX_x, CRL_MAX_y);

    // Supress interpolation for next frame.
    player->mo->interp = -1;    
    // Unset player from subsector and/or block links.
    P_UnsetThingPosition(player->mo);
    // Set new position.
    player->mo->x = CRL_MAX_x;
    player->mo->y = CRL_MAX_y;
    player->mo->z = CRL_MAX_z;
    // Supress any horizontal and vertical momentums.
    player->mo->momx = player->mo->momy = player->mo->momz = 0;
    // Set angle and heights.
    player->mo->angle = CRL_MAX_ang;
    player->mo->floorz = ss->sector->interpfloorheight;
    player->mo->ceilingz = ss->sector->interpceilingheight;
    // Set new position in subsector and/or block links.
    P_SetThingPosition(player->mo);
}

static byte *CRL_Colorize_MAX (int style)
{
    switch (style)
    {
        case 1:  // Slow blinking
            return gametic & 8 ? cr[CR_YELLOW] : cr[CR_GREEN];
            break;

        case 2:  // Fast blinking
            return gametic & 16 ? cr[CR_YELLOW] : cr[CR_GREEN];
            break;

        default:
            return cr[CR_YELLOW];
            break;
    }
}

// -----------------------------------------------------------------------------
// Draws CRL stats.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

void CRL_StatDrawer (void)
{
    int yy = 0;
    int yy2 = 0;

    const int CRL_MAX_count_old = (int)(lastvisplane - visplanes);
    const int TotalVisPlanes = CRLData.numcheckplanes + CRLData.numfindplanes;

    // Count MAX visplanes for moving
    if (CRL_MAX_count_old > CRL_MAX_count)
    {
        // Set count
        CRL_MAX_count = CRL_MAX_count_old;
        // Set position and angle.
        // We have to account uncapped framerate for better precision.
        CRL_Get_MAX();
    }

    // Player coords
    if (crl_widget_coords == 1
    || (crl_widget_coords == 2 && automapactive))
    {
        char str[128];

        M_WriteText(0, 25, "X:", cr[CR_GRAY]);
        M_WriteText(0, 34, "Y:", cr[CR_GRAY]);
        M_WriteText(0, 43, "ANG:", cr[CR_GRAY]);

        sprintf(str, "%d", CRLWidgets.x);
        M_WriteText(16, 25, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.y);
        M_WriteText(16, 34, str, cr[CR_GREEN]);
        sprintf(str, "%d", CRLWidgets.ang);
        M_WriteText(32, 43, str, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        // Icon of Sin spitter targets (32 max)
        if ((crl_widget_playstate == 1 
        ||  (crl_widget_playstate == 2 && CRL_brain_counter > 32)) && CRL_brain_counter)
        {
            char brn[32];

            M_WriteText(0, 57, "BRN:", CRL_StatColor_Str(CRL_brain_counter, 32));
            M_snprintf(brn, 16, "%d/32", CRL_brain_counter);
            M_WriteText(32, 57, brn, CRL_StatColor_Val(CRL_brain_counter, 32));
        }

        // Buttons (16 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_buttons_counter > 16))
        {
            char btn[32];

            M_WriteText(0, 66, "BTN:", CRL_StatColor_Str(CRL_buttons_counter, 16));
            M_snprintf(btn, 16, "%d/16", CRL_buttons_counter);
            M_WriteText(32, 66, btn, CRL_StatColor_Val(CRL_buttons_counter, 16));
        }

        // Plats (30 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            M_WriteText(0, 75, "PLT:", CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            M_WriteText(32, 75, plt, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            M_WriteText(0, 84, "ANI:", CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            M_WriteText(32, 84, ani, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
        }
    }

    // Shift down render counters if no level time/KIS stats are active.
    if (!crl_widget_time)
    {
        yy += 9;
    }
    if (!crl_widget_kis)
    {
        yy += 9;
    }

    // Render counters
    if (crl_widget_render)
    {
        // Sprites (vanilla: 128, doom+: 1024)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsprites >= CRL_MaxVisSprites))
        {
            char spr[32];

            M_WriteText(0, 97+yy, "SPR:", CRL_StatColor_Str(CRLData.numsprites, CRL_MaxVisSprites));
            M_snprintf(spr, 16, "%d/%d", CRLData.numsprites, CRL_MaxVisSprites);
            M_WriteText(32, 97+yy, spr, CRL_StatColor_Val(CRLData.numsprites, CRL_MaxVisSprites));
        }

        // Solid segments (32 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsolidsegs >= 32))
        {
            char ssg[32];

            M_WriteText(0, 106+yy, "SSG:", CRL_StatColor_Str(CRLData.numsolidsegs, 32));
            M_snprintf(ssg, 32, "%d/32", CRLData.numsolidsegs);
            M_WriteText(32, 106+yy, ssg, CRL_StatColor_Val(CRLData.numsolidsegs, 32));
        }

        // Segments (256 max)
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsegs >= CRL_MaxDrawSegs))
        {
            char seg[32];

            M_WriteText(0, 115+yy, "SEG:", CRL_StatColor_Str(CRLData.numsegs, CRL_MaxDrawSegs));
            M_snprintf(seg, 16, "%d/%d", CRLData.numsegs, CRL_MaxDrawSegs);
            M_WriteText(32, 115+yy, seg, CRL_StatColor_Val(CRLData.numsegs, CRL_MaxDrawSegs));
        }

        // Openings
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numopenings >= CRL_MaxOpenings))
        {
            char opn[64];

            M_WriteText(0, 124+yy, "OPN:", CRL_StatColor_Str(CRLData.numopenings, CRL_MaxOpenings));
            M_snprintf(opn, 16, "%d/%d", CRLData.numopenings, CRL_MaxOpenings);
            M_WriteText(32, 124+yy, opn, CRL_StatColor_Val(CRLData.numopenings, CRL_MaxOpenings));
        }


        // Planes (vanilla: 128, doom+: 1024)
        // Show even if only MAX got overflow.
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && (TotalVisPlanes >= CRL_MaxVisPlanes
                                   ||  CRL_MAX_count >= CRL_MaxVisPlanes)))
        {
            char vis[32];
            char max[32];

            M_WriteText(0, 133+yy, "PLN:", TotalVisPlanes >= CRL_MaxVisPlanes ? 
                       (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : cr[CR_GRAY]);

            M_snprintf(vis, 32, "%d/%d (MAX: ", TotalVisPlanes, CRL_MaxVisPlanes);
            M_snprintf(max, 32, "%d", CRL_MAX_count);

            // PLN: x/x (MAX:
            M_WriteText(32, 133+yy, vis, TotalVisPlanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);

            // x
            M_WriteText(32 + M_StringWidth(vis), 133+yy, max, TotalVisPlanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : 
                       CRL_MAX_count >= CRL_MaxVisPlanes ? CRL_Colorize_MAX(crl_widget_maxvp) : cr[CR_GREEN]);

            // )
            M_WriteText(32 + M_StringWidth(vis) + M_StringWidth(max), 133+yy, ")", TotalVisPlanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
        }
    }

    if (crl_widget_kis)
    {
        yy2 = 9;
    }

    // Level / DeathMatch timer
    if (crl_widget_time == 1
    || (crl_widget_time == 2 && automapactive))
    {
        const int time = (deathmatch && levelTimer ? levelTimeCount : leveltime) / TICRATE;
        char stra[8];
        char strb[16];
        const int yy3 = automapactive ? 0 : 9;

        sprintf(stra, "TIME ");
        M_WriteText(0, 151 - yy2 + yy3, stra, cr[CR_GRAY]);
 
        sprintf(strb, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        M_WriteText(0 + M_StringWidth(stra), 151 - yy2 + yy3, strb, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis == 1
    || (crl_widget_kis == 2 && automapactive))
    {
        const int yy = automapactive ? 8 : -1;

        if (!deathmatch)
        {
            char str1[8], str2[16];  // kills
            char str3[8], str4[16];  // items
            char str5[8], str6[16];  // secret

            // Kills:
            sprintf(str1, "K ");
            M_WriteText(0, 159 - yy, str1, cr[CR_GRAY]);
            
            sprintf(str2, "%d/%d ", CRLWidgets.kills, CRLWidgets.totalkills);
            M_WriteText(0 + M_StringWidth(str1), 159 - yy, str2,
                        CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                        CRLWidgets.kills == 0 ? cr[CR_RED] :
                        CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Items:
            sprintf(str3, "I ");
            M_WriteText(M_StringWidth(str1) + M_StringWidth(str2), 159 - yy, str3, cr[CR_GRAY]);
            
            sprintf(str4, "%d/%d ", CRLWidgets.items, CRLWidgets.totalitems);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3), 159 - yy, str4,
                        CRLWidgets.totalitems == 0 ? cr[CR_GREEN] :
                        CRLWidgets.items == 0 ? cr[CR_RED] :
                        CRLWidgets.items < CRLWidgets.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN]);

            // Secret:
            sprintf(str5, "S ");
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3) +
                        M_StringWidth(str4), 159 - yy, str5, cr[CR_GRAY]);

            sprintf(str6, "%d/%d ", CRLWidgets.secrets, CRLWidgets.totalsecrets);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) + 
                        M_StringWidth(str3) +
                        M_StringWidth(str4) +
                        M_StringWidth(str5), 159 - yy, str6,
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
                M_WriteText(0, 159 - yy, str1, cr[CR_GREEN]);

                sprintf(str2, "%d ", CRLWidgets.frags_g);
                M_WriteText(M_StringWidth(str1), 159 - yy, str2, cr[CR_GREEN]);
            }
            // Indigo
            if (playeringame[1])
            {
                sprintf(str3, "I ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2),
                            159 - yy, str3, cr[CR_GRAY]);

                sprintf(str4, "%d ", CRLWidgets.frags_i);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3),
                            159 - yy, str4, cr[CR_GRAY]);
            }
            // Brown
            if (playeringame[2])
            {
                sprintf(str5, "B ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4),
                            159 - yy, str5, cr[CR_BROWN]);

                sprintf(str6, "%d ", CRLWidgets.frags_b);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5),
                            159 - yy, str6, cr[CR_BROWN]);
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
                            159 - yy, str7, cr[CR_RED]);

                sprintf(str8, "%d ", CRLWidgets.frags_r);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6) +
                            M_StringWidth(str7),
                            159 - yy, str8, cr[CR_RED]);
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
    char fps[8];
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

// -----------------------------------------------------------------------------
// CRL_HealthColor, CRL_TargetHealth
//  [JN] Indicates and colorizes current target's health.
// -----------------------------------------------------------------------------

static byte *CRL_HealthColor (const int val1, const int val2)
{
    return
        val1 <= val2/4 ? cr[CR_RED]    :
        val1 <= val2/2 ? cr[CR_YELLOW] :
                         cr[CR_GREEN]  ;
}

void CRL_DrawTargetsHealth (void)
{
    char str[16];
    player_t *player = &players[displayplayer];

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);

    if (crl_widget_health == 1)  // Top
    {
        M_WriteTextCentered(18, str, CRL_HealthColor(player->targetsheath,
                                                     player->targetsmaxheath));
    }
    else
    if (crl_widget_health == 2)  // Top + name
    {
        M_WriteTextCentered(9, player->targetsname, CRL_HealthColor(player->targetsheath,
                                                                    player->targetsmaxheath));
        M_WriteTextCentered(18, str, CRL_HealthColor(player->targetsheath,
                                                     player->targetsmaxheath));
    }
    else
    if (crl_widget_health == 3)  // Bottom
    {
        M_WriteTextCentered(152, str, CRL_HealthColor(player->targetsheath,
                                                      player->targetsmaxheath));
    }
    else
    if (crl_widget_health == 4)  // Bottom + name
    {
        M_WriteTextCentered(144, player->targetsname, CRL_HealthColor(player->targetsheath,
                                                                      player->targetsmaxheath));
        M_WriteTextCentered(152, str, CRL_HealthColor(player->targetsheath,
                                                      player->targetsmaxheath));
    }
}
