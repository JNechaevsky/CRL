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
#include "m_misc.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomdef.h"
#include "p_local.h"
#include "r_local.h"

#include "crlcore.h"
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
        {
            // [JN] Prevent negative values.
            const int total_value = (total - value > 0) ? (total - value) : 0;
            snprintf(buffer, buffer_size, "%d", total_value);
            break;
        }

        case 2: // Percent
        {
            snprintf(buffer, buffer_size, "%d%%", 
                     safe_percent(value, total));
            break;
        }

        default: // Ratio
        {
            snprintf(buffer, buffer_size, "%d/%d", value, total);
            break;
        }
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
// CRL_FixedToString
//  [PN] Formats fixed_t value as string with fractional part (up to 5 digits).
// -----------------------------------------------------------------------------

static void CRL_FixedToString (fixed_t value, char *const buf, size_t buf_size)
{
    const split_fixed_t val = SplitFixed(value);
    
    if (crl_widget_coordsfrac && val.frac)
    {
        const char *const sign = (val.negative && !val.base) ? "-" : "";
        M_snprintf(buf, buf_size, "%s%d.%05d", sign, val.base, val.frac);
    }
    else
    {
        M_snprintf(buf, buf_size, "%d", val.base);
    }
}

// -----------------------------------------------------------------------------
// CRL_AngleToString
//  [PN] Formats angle_t value as string with fractional part (up to 3 digits).
// -----------------------------------------------------------------------------

static void CRL_AngleToString (angle_t value, char *const buf, size_t buf_size)
{
    const split_angle_t val = SplitAngle(value);
    
    if (crl_widget_coordsfrac && val.frac)
    {
        M_snprintf(buf, buf_size, "%d.%03d", val.base, val.frac);
    }
    else
    {
        M_snprintf(buf, buf_size, "%d", val.base);
    }
}

// -----------------------------------------------------------------------------
// Draws CRL stats.
//  [JN] Draw all the widgets and counters.
// -----------------------------------------------------------------------------

// Power-up counters:
int CRL_counter_tome;
int CRL_counter_ring;
int CRL_counter_shadow;
int CRL_counter_wings;
int CRL_counter_torch;

void CRL_StatDrawer (void)
{
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

    // Apply translucency while Save/Load menu is active.
    dp_translucent = savemenuactive;

    // Player coords
    if (crl_widget_coords == 1
    || (crl_widget_coords == 2 && automapactive))
    {
        char x[8] = {0};
        char y[8] = {0};
        char str[128];
        int coord_x_width;

        M_snprintf(x, 8, "X: ");
        MN_DrTextA(x, 0, 30, cr[CR_GRAY]);
        CRL_FixedToString(CRLWidgets.x, str, sizeof(str));
        coord_x_width = MN_TextAWidth(str);
        MN_DrTextA(str, MN_TextAWidth(x), 30, cr[CR_GREEN]);

        M_snprintf(y, 8, " Y: ");
        MN_DrTextA(y, MN_TextAWidth(x) + coord_x_width, 30, cr[CR_GRAY]);
        CRL_FixedToString(CRLWidgets.y, str, sizeof(str));
        MN_DrTextA(str, MN_TextAWidth(x) + coord_x_width + MN_TextAWidth(y), 30, cr[CR_GREEN]);

        MN_DrTextA("ANG:", 0, 40, cr[CR_GRAY]);
        CRL_AngleToString(CRLWidgets.ang, str, sizeof(str));
        MN_DrTextA(str, 32, 40, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            MN_DrTextA("PLT:", 0, 53, CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            MN_DrTextA(plt, 32, 53, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            MN_DrTextA("ANI:", 0, 63, CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            MN_DrTextA(ani, 32, 63, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
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

        // Openings
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numopenings >= CRL_MaxOpenings))
        {
            char opn[64];

            MN_DrTextA("OPN:", 0, 105, CRL_StatColor_Str(CRLData.numopenings, CRL_MaxOpenings));
            M_snprintf(opn, 16, "%d/%d", CRLData.numopenings, CRL_MaxOpenings);
            MN_DrTextA(opn, 32, 105, CRL_StatColor_Val(CRLData.numopenings, CRL_MaxOpenings));
        }

        // Planes (vanilla: 128, doom+: 1024)
        // Show even if only MAX got overflow.
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && (TotalVisPlanes >= CRL_MaxVisPlanes
                                   ||  CRL_MAX_count >= CRL_MaxVisPlanes)))
        {
            char vis[32];
            char max[32];

            MN_DrTextA("PLN:", 0, 115, TotalVisPlanes >= CRL_MaxVisPlanes ? 
                      (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : cr[CR_GRAY]);

            M_snprintf(vis, 32, "%d/%d (MAX: ", TotalVisPlanes, CRL_MaxVisPlanes);
            M_snprintf(max, 32, "%d", CRL_MAX_count);

            // PLN: x/x (MAX:
            MN_DrTextA(vis, 32, 115, TotalVisPlanes >= CRL_MaxVisPlanes ?
                      (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);

            // x
            MN_DrTextA(max, 32 + MN_TextAWidth(vis), 115, TotalVisPlanes >= CRL_MaxVisPlanes ?
                      (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : 
                      CRL_MAX_count >= CRL_MaxVisPlanes ? CRL_Colorize_MAX(crl_widget_maxvp) : cr[CR_GREEN]);

            // )
            MN_DrTextA(")", 32 + MN_TextAWidth(vis) + MN_TextAWidth(max), 115, TotalVisPlanes >= CRL_MaxVisPlanes ?
                       (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
        }
    }

    const int xx  = /*crl_screen_size > 10 && (!automapactive || crl_automap_overlay)*/ automapactive ? 0 : 20;
    const int yy  = automapactive ? 0 : 10;
    const int yy2 = crl_widget_kis ? 0 : 10;

    // Level timer
    if (crl_widget_time == 1
    || (crl_widget_time == 2 && automapactive))
    {
        char stra[8];
        char strb[16];
        const int time = leveltime / TICRATE;
        
        M_snprintf(stra, 8, "TIME ");
        MN_DrTextA(stra, 0, 125 + yy + yy2, cr[CR_GRAY]);
        M_snprintf(strb, 16, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
        MN_DrTextA(strb, MN_TextAWidth(stra), 125 + yy + yy2, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis == 1
    || (crl_widget_kis == 2 && automapactive))
    {
        char str1[8], str2[16];  // kills
        char str3[8], str4[16];  // items
        char str5[8], str6[16];  // secret

        // Kills:
        M_snprintf(str1, 8, "K ");
        MN_DrTextA(str1, xx, 135 + yy, CRL_WidgetColor(widget_kis_str));
        CRL_WidgetKISCount(str2, sizeof(str2), widget_kis_kills);
        MN_DrTextA(str2, MN_TextAWidth(str1) + xx, 135 + yy,
                         CRLWidgets.totalkills == 0 ? cr[CR_GREEN] :
                         CRLWidgets.kills == 0 ? cr[CR_RED] :
                         CRLWidgets.kills < CRLWidgets.totalkills ? cr[CR_YELLOW] : cr[CR_GREEN]);

        // Items:
        if (crl_widget_kis_items == 1 || (crl_widget_kis_items == 2 && automapactive))
        {
        M_snprintf(str3, 8, " I ");
        MN_DrTextA(str3, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) + xx, 135 + yy, CRL_WidgetColor(widget_kis_str));
        CRL_WidgetKISCount(str4, sizeof(str4), widget_kis_items);
        MN_DrTextA(str4, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3) + xx, 135 + yy,
                         CRL_WidgetColor(widget_items));
        }
        else
        {
        str3[0] = '\0';
        str4[0] = '\0';
        }

        // Secrets:
        M_snprintf(str5, 8, " S ");
        MN_DrTextA(str5, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3) +
                         MN_TextAWidth(str4) + xx, 135 + yy, CRL_WidgetColor(widget_kis_str));

        CRL_WidgetKISCount(str6, sizeof(str6), widget_kis_secrets);
        MN_DrTextA(str6, MN_TextAWidth(str1) +
                         MN_TextAWidth(str2) +
                         MN_TextAWidth(str3) +
                         MN_TextAWidth(str4) +
                         MN_TextAWidth(str5) + xx, 135 + yy,
                         CRL_WidgetColor(widget_secret));
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
        // [PN] Parallel arrays for powerup data.
        const char *const labels[5]  = { "TOM:", "RNG:", "SHD:", "WNG:", "TRC:" };
        const int *const counters[5] = { &CRL_counter_tome, &CRL_counter_ring,
                                         &CRL_counter_shadow, &CRL_counter_wings,
                                         &CRL_counter_torch };
        const int thresholds[5]  = { 40, 30, 60, 60, 120 };
        const int y_positions[5] = { 45, 55, 65, 75, 85 };

        for (int i = 0; i < 5; i++)
        {
            if (*counters[i] > 0) // [PN] Show only active powerups
            {
                char str[4];
                const int current_y = y_positions[i];

                // [PN] Draw label (TOM:, RNG:, etc.)
                MN_DrTextA(labels[i], SCREENWIDTH - 32 - MN_TextAWidth(labels[i]), current_y, cr[CR_GRAY]);

                // [PN] Draw timer value
                M_snprintf(str, 4, "%d", *counters[i]);
                MN_DrTextA(str, 293, current_y, CRL_PowerupColor(*counters[i], thresholds[i]));
            }
        }
    }

    dp_translucent = false;
}

// -----------------------------------------------------------------------------
// CRL_DrawFPS.
//  [JN] Draw actual frames per second value.
//  Some MN_TextAWidth adjustments are needed for proper positioning
//  in case of custom font is thinner or thicker.
// -----------------------------------------------------------------------------

void CRL_DrawFPS (void)
{
    char fps[8];
    char fps_str[4];
    int yy = 10;

    // [JN] If demo timer is active and running, shift FPS widget one line down.
    if ((demoplayback && (crl_demo_timer == 1 || crl_demo_timer == 3))
    ||  (demorecording && (crl_demo_timer == 2 || crl_demo_timer == 3)))
    {
        yy += 10;
    }

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, "FPS");

    MN_DrTextA(fps, SCREENWIDTH - 11 - MN_TextAWidth(fps) 
                                     - MN_TextAWidth(fps_str), yy, cr[CR_GRAY]);

    MN_DrTextA(fps_str, SCREENWIDTH - 7 - MN_TextAWidth(fps_str), yy, cr[CR_GRAY]);
}


// =============================================================================
//
//                             Demo enhancements
//
// =============================================================================

// [crispy] demo progress bar and timer widget
int defdemotics = 0, deftotaldemotics;

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
    MN_DrTextA(n, x, 10, cr[CR_LIGHTGRAY]);
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
    byte *color;

    if (player->targetsheathTics <= 0 || !player->targetsheath)
    {
        return;  // No tics or target is dead, nothing to display.
    }

    // Apply translucency while Save/Load menu is active.
    dp_translucent = savemenuactive;

    const int yy = crl_widget_speed ? 10 : 0;
    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);
    color = CRL_HealthColor(player->targetsheath, player->targetsmaxheath);

    switch (crl_widget_health)
    {
        case 1:  // Top
            MN_DrTextACentered(str, 20, color);
            break;
        case 2:  // Top + name
            MN_DrTextACentered(player->targetsname, 10, color);
            MN_DrTextACentered(str, 20, color);
            break;
        case 3:  // Bottom
            MN_DrTextACentered(str, 145 - yy, color);
            break;
        case 4:  // Bottom + name
            MN_DrTextACentered(player->targetsname, 135 - yy, color);
            MN_DrTextACentered(str, 145 - yy, color);
            break;
    }

    dp_translucent = false;
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

    // Apply translucency while Save/Load menu is active.
    dp_translucent = savemenuactive;

    // Calculating speed only every game tic (not every frame)
    // is not possible while D_Display is called after S_UpdateSounds in D_DoomLoop.
    // if (oldgametic < gametic)
    {
        const double dx = (double)(player->mo->x - player->mo->oldx) / FRACUNIT;
        const double dy = (double)(player->mo->y - player->mo->oldy) / FRACUNIT;
        const double dz = (double)(player->mo->z - player->mo->oldz) / FRACUNIT;
        speed = sqrt(dx * dx + dy * dy + dz * dz) * TICRATE;
    }

    M_snprintf(str, sizeof(str), "SPD:");
    M_snprintf(val, sizeof(val), " %.0f", speed);

    const int x_val = (SCREENWIDTH / 2);
    const int x_str = x_val - MN_TextAWidth(str);

    MN_DrTextA(str, x_str, 145, CRL_WidgetColor(widget_speed_str));
    MN_DrTextA(val, x_val, 145, CRL_WidgetColor(widget_speed_val));

    dp_translucent = false;
}
