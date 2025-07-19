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


#include <stdio.h>
#include <math.h>

#include "i_timer.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomstat.h"
#include "m_menu.h"
#include "m_misc.h"
#include "p_local.h"

#include "crlvars.h"
#include "crlfunc.h"


// =============================================================================
//
//                                Spectator mode
//
// =============================================================================

// -----------------------------------------------------------------------------
// CRL_MoveTo_Camera
//  [JN] Moves player to spectator camera position.
// -----------------------------------------------------------------------------

void CRL_MoveTo_Camera (void)
{
    // It's single player only function, so operate with consoleplayer.
    player_t *player = &players[consoleplayer];

    // Define subsector we will move on.
    subsector_t *ss = R_PointInSubsector(viewx, viewy);

    // Supress interpolation for next frame.
    player->mo->interp = -1;    
    // Unset player from subsector and/or block links.
    P_UnsetThingPosition(player->mo);
    // Set new position.
    player->mo->x = CRL_camera_x;
    player->mo->y = CRL_camera_y;
    // Things a big more complicated in uncapped frame rate, so we have
    // to properly update both z and viewz to prevent one frame jitter.
    player->mo->z = CRL_camera_z - player->viewheight;
    player->viewz = player->mo->z + player->viewheight;
    // Supress any horizontal and vertical momentums.
    player->mo->momx = player->mo->momy = player->mo->momz = 0;
    // Set angle and heights.
    player->mo->angle = viewangle;
    player->mo->floorz = ss->sector->interpfloorheight;
    player->mo->ceilingz = ss->sector->interpceilingheight;
    // Set new position in subsector and/or block links.
    P_SetThingPosition(player->mo);
    // Check for surroundings for possible interaction with pickups.
    P_CheckPosition(player->mo, player->mo->x, player->mo->y);
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

// [JN] Enum for widget strings and values.
enum
{
    widget_kis_str,
    widget_kills,
    widget_items,
    widget_secret,
    widget_plyr1,
    widget_plyr2,
    widget_plyr3,
    widget_plyr4,
    widget_time_str,
    widget_time_val,
    widget_render_str,
    widget_render_val,
    widget_coords_str,
    widget_coords_val,
    widget_speed_str,
    widget_speed_val,
} widgetcolor_t;

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

static byte *CRL_WidgetColor (const int i)
{
    static byte *player_colors[4];
    static const int plyr_indices[] = {widget_plyr1, widget_plyr2, widget_plyr3, widget_plyr4};

    player_colors[0] = cr[CR_GREEN];
    player_colors[1] = cr[CR_GRAY];
    player_colors[2] = cr[CR_BROWN];
    player_colors[3] = cr[CR_RED];

    switch (i)
    {
        case widget_kis_str:
        case widget_time_str:
        case widget_render_str:
        case widget_coords_str:
        case widget_speed_str:
            return cr[CR_GRAY];
        
        case widget_kills:
            return
                CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                CRLWidgets.kills == 0 ? cr[CR_RED] :
                CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN];
        case widget_items:
            return
                CRLWidgets.totalitems == 0 ? cr[CR_GREEN] :
                CRLWidgets.items == 0 ? cr[CR_RED] :
                CRLWidgets.items < CRLWidgets.totalitems ? cr[CR_YELLOW] : cr[CR_GREEN];
        case widget_secret:
            return
                CRLWidgets.totalsecrets == 0 ? cr[CR_GREEN] :
                CRLWidgets.secrets == 0 ? cr[CR_RED] :
                CRLWidgets.secrets < CRLWidgets.totalsecrets ? cr[CR_YELLOW] : cr[CR_GREEN];

        case widget_time_val:
            return cr[CR_LIGHTGRAY];

        case widget_render_val:
        case widget_coords_val:
        case widget_speed_val:
            return cr[CR_GREEN];

        default:
            for (int j = 0; j < 4; j++)
            {
                if (i == plyr_indices[j])
                {
                    return player_colors[j];
                }
            }
    }

    return NULL;
}

// [JN/PN] Enum for widget type values.
enum
{
    widget_kis_kills,
    widget_kis_items,
    widget_kis_secrets,
} widget_kis_count_t;

// [PN] Function for safe division to prevent division by zero.
// Returns the percentage or 0 if the total is zero.
static int safe_percent (int value, int total)
{
    return (total == 0) ? 0 : (value * 100) / total;
}

// [PN/JN] Main function to format KIS counts based on format and widget type.
static void CRL_WidgetKISCount (char *buffer, size_t buffer_size, const int i)
{
    int value = 0, total = 0;
    
    // [PN] Set values for kills, items, or secrets based on widget type
    switch (i)
    {
        case widget_kis_kills:
            value = CRLWidgets.kills;
            total = CRLWidgets.totalkills;
            break;
        
        case widget_kis_items:
            value = CRLWidgets.items;
            total = CRLWidgets.totalitems;
            break;
        
        case widget_kis_secrets:
            value = CRLWidgets.secrets;
            total = CRLWidgets.totalsecrets;
            break;
        
        default:
            // [PN] Default case for unsupported widget type
            snprintf(buffer, buffer_size, "N/A");
            return;
    }

    // [PN] Format based on crl_widget_kis_format
    switch (crl_widget_kis_format)
    {
        case 1: // Remaining
            snprintf(buffer, buffer_size, "%d", total - value);
            break;

        case 2: // Percent
            snprintf(buffer, buffer_size, "%d%%", 
                     safe_percent(value, total));
            break;

        default: // Ratio
            snprintf(buffer, buffer_size, "%d/%d", value, total);
            break;
    }
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

    CRL_MAX_x = viewx;
    CRL_MAX_y = viewy;
    if (crl_spectating)
    {
        CRL_MAX_z = LerpFixed(CRL_camera_oldz, CRL_camera_z) - VIEWHEIGHT;
    }
    else
    {
        CRL_MAX_z = LerpFixed(player->mo->oldz, player->mo->z);
    }
    CRL_MAX_ang = viewangle;
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

// Power-up counters:
int CRL_invul_counter;
int CRL_invis_counter;
int CRL_rad_counter;
int CRL_amp_counter;

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
        int yy1 = 0;

        // Icon of Sin spitter targets (32 max)
        if ((crl_widget_playstate == 1 
        ||  (crl_widget_playstate == 2 && CRL_brain_counter > 32)) && CRL_brain_counter)
        {
            char brn[32];

            M_WriteText(0, 57, "BRN:", CRL_StatColor_Str(CRL_brain_counter, 32));
            M_snprintf(brn, 16, "%d/32", CRL_brain_counter);
            M_WriteText(32, 57, brn, CRL_StatColor_Val(CRL_brain_counter, 32));
        }

        // Slightly shift remaining widgets down if no BRN 
        // counter is active to better line up with menu lines.
        if (!CRL_brain_counter)
        {
            yy1 -= 5;
        }

        // Buttons (16 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_buttons_counter > 16))
        {
            char btn[32];

            M_WriteText(0, 66+yy1, "BTN:", CRL_StatColor_Str(CRL_buttons_counter, 16));
            M_snprintf(btn, 16, "%d/16", CRL_buttons_counter);
            M_WriteText(32, 66+yy1, btn, CRL_StatColor_Val(CRL_buttons_counter, 16));
        }

        // Plats (30 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            M_WriteText(0, 75+yy1, "PLT:", CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            M_WriteText(32, 75+yy1, plt, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            M_WriteText(0, 84+yy1, "ANI:", CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            M_WriteText(32, 84+yy1, ani, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
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

        // Apply translucency while Save/Load menu is active.
        dp_translucent = savemenuactive;

        sprintf(stra, "TIME ");
        M_WriteText(0, 151 - yy2 + yy3, stra, cr[CR_GRAY]);
 
        sprintf(strb, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        M_WriteText(0 + M_StringWidth(stra), 151 - yy2 + yy3, strb, cr[CR_LIGHTGRAY]);

        dp_translucent = false;
    }

    // K/I/S stats
    if (crl_widget_kis == 1
    || (crl_widget_kis == 2 && automapactive))
    {
        const int yy4 = automapactive ? 8 : -1;

        // Apply translucency while Save/Load menu is active.
        dp_translucent = savemenuactive;

        if (!deathmatch)
        {
            char str1[8], str2[16];  // kills
            char str3[8], str4[16];  // items
            char str5[8], str6[16];  // secret

            // Kills:
            sprintf(str1, "K ");
            M_WriteText(0, 159 - yy4, str1, CRL_WidgetColor(widget_kis_str));
            CRL_WidgetKISCount(str2, sizeof(str2), widget_kis_kills);
            M_WriteText(0 + M_StringWidth(str1), 159 - yy4, str2, CRL_WidgetColor(widget_kills));

            // Items:
            if (crl_widget_kis_items)
            {
            sprintf(str3, " I ");
            M_WriteText(M_StringWidth(str1) + 
                        M_StringWidth(str2), 159 - yy4, str3, CRL_WidgetColor(widget_kis_str));
            CRL_WidgetKISCount(str4, sizeof(str4), widget_kis_items);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3), 159 - yy4, str4, CRL_WidgetColor(widget_items));
            }
            else
            {
            str3[0] = '\0';
            str4[0] = '\0';
            }

            // Secret:
            sprintf(str5, " S ");
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) +
                        M_StringWidth(str3) +
                        M_StringWidth(str4), 159 - yy4, str5, CRL_WidgetColor(widget_kis_str));

            CRL_WidgetKISCount(str6, sizeof(str6), widget_kis_secrets);
            M_WriteText(M_StringWidth(str1) +
                        M_StringWidth(str2) + 
                        M_StringWidth(str3) +
                        M_StringWidth(str4) +
                        M_StringWidth(str5), 159 - yy4, str6, CRL_WidgetColor(widget_secret));
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
                M_WriteText(0, 159 - yy4, str1, CRL_WidgetColor(widget_plyr1));

                sprintf(str2, "%d ", CRLWidgets.frags_g);
                M_WriteText(M_StringWidth(str1), 159 - yy4, str2, CRL_WidgetColor(widget_plyr1));
            }
            // Indigo
            if (playeringame[1])
            {
                sprintf(str3, "I ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2),
                            159 - yy4, str3, CRL_WidgetColor(widget_plyr2));

                sprintf(str4, "%d ", CRLWidgets.frags_i);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3),
                            159 - yy4, str4, CRL_WidgetColor(widget_plyr2));
            }
            // Brown
            if (playeringame[2])
            {
                sprintf(str5, "B ");
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4),
                            159 - yy4, str5, CRL_WidgetColor(widget_plyr3));

                sprintf(str6, "%d ", CRLWidgets.frags_b);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5),
                            159 - yy4, str6, CRL_WidgetColor(widget_plyr3));
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
                            159 - yy4, str7, CRL_WidgetColor(widget_plyr4));

                sprintf(str8, "%d ", CRLWidgets.frags_r);
                M_WriteText(M_StringWidth(str1) +
                            M_StringWidth(str2) +
                            M_StringWidth(str3) +
                            M_StringWidth(str4) +
                            M_StringWidth(str5) +
                            M_StringWidth(str6) +
                            M_StringWidth(str7),
                            159 - yy4, str8, CRL_WidgetColor(widget_plyr4));
            }
        }
        
        dp_translucent = false;
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
        if (CRL_invul_counter)
        {
            char invl[4];

            M_WriteText(292 - M_StringWidth("INVL:"), 106, "INVL:", cr[CR_GRAY]);
            M_snprintf(invl, 4, "%d", CRL_invul_counter);
            M_WriteText(296, 106, invl, CRL_PowerupColor(CRL_invul_counter, 30));
        }

        if (CRL_invis_counter)
        {
            char invs[4];

            M_WriteText(292 - M_StringWidth("INVS:"), 115, "INVS:", cr[CR_GRAY]);
            M_snprintf(invs, 4, "%d", CRL_invis_counter);
            M_WriteText(296, 115, invs, CRL_PowerupColor(CRL_invis_counter, 60));
        }

        if (CRL_rad_counter)
        {
            char rad[4];

            M_WriteText(292 - M_StringWidth("RAD:"), 124, "RAD:", cr[CR_GRAY]);
            M_snprintf(rad, 4, "%d", CRL_rad_counter);
            M_WriteText(296, 124, rad, CRL_PowerupColor(CRL_rad_counter, 60));
        }

        if (CRL_amp_counter)
        {
            char amp[4];

            M_WriteText(292 - M_StringWidth("AMP:"), 133, "AMP:", cr[CR_GRAY]);
            M_snprintf(amp, 4, "%d", CRL_amp_counter);
            M_WriteText(296, 133, amp, CRL_PowerupColor(CRL_amp_counter, 120));
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
    int yy = 9;

    // [JN] If demo timer is active and running, shift FPS widget one line down.
    if ((demoplayback && (crl_demo_timer == 1 || crl_demo_timer == 3))
    ||  (demorecording && (crl_demo_timer == 2 || crl_demo_timer == 3)))
    {
        yy += 9;
    }

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, "FPS");

    M_WriteText(SCREENWIDTH - 11 - M_StringWidth(fps) 
                                 - M_StringWidth(fps_str), yy, fps, cr[CR_GRAY]);

    M_WriteText(SCREENWIDTH - 7 - M_StringWidth(fps_str), yy, "FPS", cr[CR_GRAY]);
}

// =============================================================================
//
//                             Demo enhancements
//
// =============================================================================

// -----------------------------------------------------------------------------
// CRL_DemoTimer
//  [crispy] Demo Timer widget
//  [PN/JN] Reduced update frequency to once per gametic from every frame.
// -----------------------------------------------------------------------------

void CRL_DemoTimer (const int time)
{
    static char n[16];
    static int  last_update_gametic = -1;
    static int  hours = 0;

    if (last_update_gametic < gametic)
    {
        hours = time / (3600 * TICRATE);
        const int mins = time / (60 * TICRATE) % 60;
        const float secs = (float)(time % (60 * TICRATE)) / TICRATE;

        if (hours)
        {
            M_snprintf(n, sizeof(n), "%02i:%02i:%05.02f", hours, mins, secs);
        }
        else
        {
            M_snprintf(n, sizeof(n), "%02i:%05.02f", mins, secs);
        }

        last_update_gametic = gametic;
    }

    const int x = 237 + (hours > 0 ? 0 : 20);
    M_WriteText(x, 9, n, cr[CR_LIGHTGRAY]);
}

// -----------------------------------------------------------------------------
// CRL_DemoBar
//  [crispy] print a bar indicating demo progress at the bottom of the screen
// -----------------------------------------------------------------------------

void CRL_DemoBar (void)
{
    static boolean colors_set = false;
    static int black = 0;
    static int white = 0;
    const int i = SCREENWIDTH * defdemotics / deftotaldemotics;

    // [JN] Don't rely on palette indexes,
    // try to find nearest colors instead.
    if (!colors_set)
    {
        black = I_GetPaletteIndex(0, 0, 0);
        white = I_GetPaletteIndex(255, 255, 255);
        colors_set = true;
    }

    V_DrawHorizLine(0, SCREENHEIGHT - 2, i, black); // [crispy] black
    V_DrawHorizLine(0, SCREENHEIGHT - 1, i, white); // [crispy] white
}

// -----------------------------------------------------------------------------
// CRL_HealthColor, CRL_TargetHealth
//  [JN/PN] Indicates and colorizes current target's health.
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
    const player_t *const player = &players[displayplayer];
    byte *color;

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    const int yy = crl_widget_speed ? 9 : 0;
    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);
    color = CRL_HealthColor(player->targetsheath, player->targetsmaxheath);

    switch (crl_widget_health)
    {
        case 1:  // Top
            M_WriteTextCentered(18, str, color);
            break;
        case 2:  // Top + name
            M_WriteTextCentered(9, player->targetsname, color);
            M_WriteTextCentered(18, str, color);
            break;
        case 3:  // Bottom
            M_WriteTextCentered(152 - yy, str, color);
            break;
        case 4:  // Bottom + name
            M_WriteTextCentered(144 - yy, player->targetsname, color);
            M_WriteTextCentered(152 - yy, str, color);
            break;
    }
}

// -----------------------------------------------------------------------------
// CRL_DrawPlayerSpeed
//  [PN/JN] Draws player movement speed in map untits per second format.
//  Based on the implementation by ceski from the Woof source port.
// -----------------------------------------------------------------------------

void CRL_DrawPlayerSpeed (void)
{
    static char str[8];
    static char val[16];
    static double speed = 0;
    const player_t *player = &players[displayplayer];

    // Calculate speed only every game tic, not every frame.
    if (oldgametic < gametic)
    {
        const double dx = (double)(player->mo->x - player->mo->oldx) / FRACUNIT;
        const double dy = (double)(player->mo->y - player->mo->oldy) / FRACUNIT;
        const double dz = (double)(player->mo->z - player->mo->oldz) / FRACUNIT;
        speed = sqrt(dx * dx + dy * dy + dz * dz) * TICRATE;
    }

    M_snprintf(str, sizeof(str), "SPD:");
    M_snprintf(val, sizeof(val), " %.0f", speed);

    const int x_val = (SCREENWIDTH / 2);
    const int x_str = x_val - M_StringWidth(str);

    M_WriteText(x_str, 151, str, CRL_WidgetColor(widget_speed_str));
    M_WriteText(x_val, 151, val, CRL_WidgetColor(widget_speed_val));
}
