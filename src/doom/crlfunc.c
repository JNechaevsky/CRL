//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2022 Ryan Krafnick
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


#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "i_timer.h"
#include "i_swap.h"
#include "i_video.h"
#include "v_trans.h"
#include "v_video.h"
#include "doomstat.h"
#include "d_englsh.h"
#include "dsda_font.h"
#include "mn_menu.h"
#include "m_misc.h"
#include "p_local.h"

#include "crlvars.h"
#include "crlfunc.h"

// [JN] Widget anchor helpers, defined below with the widget text drawers.
static void WidgetTextPatchAt (int cx, int cy, const char *string, byte *table);
static void WidgetTextDSDAAt (int cx, int cy, const char *text, byte *table);
static void CRL_WidgetTextPair (int x_game, int y_game, int right_anchored,
                                int bottom_anchor,
                                const char *s1, byte *c1,
                                const char *s2, byte *c2);
static void CRL_WidgetTextColumns (int label_right, int y_game,
                                   const char *s1, byte *c1,
                                   const char *s2, byte *c2);


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

char ID_Level_Time[64];

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

byte *const CRL_StatColor_Str (const int val1, const int val2)
{
    return
        val1 == val2 ? cr[CR_LIGHTGRAY] :
        val1 >= val2 ? (gametic & 8 ? cr[CR_GRAY] : cr[CR_LIGHTGRAY]) : 
                       cr[CR_GRAY];
}

byte *const CRL_StatColor_Val (const int val1, const int val2)
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
            const int total_value = total - value;
            snprintf(buffer, buffer_size, "%d", total_value);
            break;
        }

        case 2: // Percent
        {
            snprintf(buffer, buffer_size, "%d%%", safe_percent(value, total));
            break;
        }

        default: // Ratio
        {
            snprintf(buffer, buffer_size, "%d/%d", value, total);
            break;
        }
    }
}

// [JN] Format time string for the level/total time widget.
void ID_FormatWidgetTime (char *buf, size_t bufsize, int ticks, int mode)
{
    const int hours   = ticks / (3600 * TICRATE);
    const int mins    = (ticks / (60 * TICRATE)) % 60;
    const int sec     = (ticks / TICRATE) % 60;
    const int csec    = ((ticks % TICRATE) * 100 + TICRATE / 2) / TICRATE;
    const int show_cs = (mode == 3 || mode == 4);

    if (hours)
    {
        if (show_cs)
            M_snprintf(buf, bufsize, "%02i:%02i:%02i.%02i", hours, mins, sec, csec);
        else
            M_snprintf(buf, bufsize, "%02i:%02i:%02i", hours, mins, sec);
    }
    else
    {
        if (show_cs)
            M_snprintf(buf, bufsize, "%02i:%02i.%02i", mins, sec, csec);
        else
            M_snprintf(buf, bufsize, "%02i:%02i", mins, sec);
    }
}

// -----------------------------------------------------------------------------
// Clear/MoveTo
// [PN/JN] Actions for MAX values of render counters.
// -----------------------------------------------------------------------------

void CRL_Clear_ALL_MAX (void)
{
    CRL_Clear_PLN_MAX();
    CRL_Clear_SSG_MAX();
    CRL_Clear_SEG_MAX();
    CRL_Clear_OPN_MAX();
}

void CRL_Clear_SSG_MAX (void) { CRL_MAX_ssg = (CRL_Render_max_t) { 0 }; }
void CRL_Clear_SEG_MAX (void) { CRL_MAX_seg = (CRL_Render_max_t) { 0 }; }
void CRL_Clear_OPN_MAX (void) { CRL_MAX_opn = (CRL_Render_max_t) { 0 }; }
void CRL_Clear_PLN_MAX (void) { CRL_MAX_pln = (CRL_Render_max_t) { 0 }; }

void CRL_MoveTo_SSG_MAX (void) { CRL_MoveTo_Render_MAX(&CRL_MAX_ssg); }
void CRL_MoveTo_SEG_MAX (void) { CRL_MoveTo_Render_MAX(&CRL_MAX_seg); }
void CRL_MoveTo_OPN_MAX (void) { CRL_MoveTo_Render_MAX(&CRL_MAX_opn); }
void CRL_MoveTo_PLN_MAX (void) { CRL_MoveTo_Render_MAX(&CRL_MAX_pln); }

void CRL_Get_Render_MAX (CRL_Render_max_t *max)
{
    player_t *player = &players[displayplayer];

    max->x = viewx;
    max->y = viewy;
    max->z = crl_spectating ? LerpFixed(CRL_camera_oldz, CRL_camera_z) - VIEWHEIGHT
                            : LerpFixed(player->mo->oldz, player->mo->z);
    max->ang = viewangle;
}

void CRL_MoveTo_Render_MAX (const CRL_Render_max_t *max)
{
    // Prevent jumping to X0/Y0 coords if count equals zero.
    if (max->count <= 0)
        return;

    player_t *player = &players[displayplayer];

    // Define subsector we will move on.
    subsector_t* ss = R_PointInSubsector(max->x, max->y);

    // Supress interpolation for next frame.
    player->mo->interp = -1;    
    // Unset player from subsector and/or block links.
    P_UnsetThingPosition(player->mo);
    // Set new position.
    player->mo->x = max->x;
    player->mo->y = max->y;
    player->mo->z = max->z;
    player->viewz = player->mo->z + player->viewheight;
    // Supress any horizontal and vertical momentums.
    player->mo->momx = player->mo->momy = player->mo->momz = 0;
    // Set angle and heights.
    player->mo->angle = max->ang;
    player->mo->floorz = ss->sector->interpfloorheight;
    player->mo->ceilingz = ss->sector->interpceilingheight;
    // Set new position in subsector and/or block links.
    P_SetThingPosition(player->mo);
}

byte *const CRL_Colorize_MAX (int style)
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

void CRL_FixedToString (fixed_t value, char *const buf, size_t buf_size)
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

void CRL_AngleToString (angle_t value, char *const buf, size_t buf_size)
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
// CRL_GetTotalVisPlanes
//  [PN] Total amount of visplanes used during the current frame.
// -----------------------------------------------------------------------------

int CRL_GetTotalVisPlanes (void)
{
    return CRLData.numcheckplanes + CRLData.numfindplanes;
}

// -----------------------------------------------------------------------------
// CRL_CounterValue_SPR
//  [PN] Sprites: "current/max".
// -----------------------------------------------------------------------------

void CRL_CounterValue_SPR (char *const value, size_t value_size)
{
    M_snprintf(value, value_size, "%d/%d", CRLData.numsprites, CRL_MaxVisSprites);
}

// -----------------------------------------------------------------------------
// CRL_CounterValue_SSG
//  [PN] Solid segments: "current/max (MAX: N)" and the peak value.
// -----------------------------------------------------------------------------

void CRL_CounterValue_SSG (char *const value, size_t value_size, char *const max, size_t max_size)
{
    M_snprintf(value, value_size, "%d/%d (MAX: ", CRLData.numsolidsegs, CRL_MAX_SOLIDSEGS);
    M_snprintf(max, max_size, "%d", CRL_MAX_ssg.count);
}

// -----------------------------------------------------------------------------
// CRL_CounterValue_SEG
//  [PN] Segments: "current/max (MAX: N)" and the peak value.
// -----------------------------------------------------------------------------

void CRL_CounterValue_SEG (char *const value, size_t value_size, char *const max, size_t max_size)
{
    M_snprintf(value, value_size, "%d/%d (MAX: ", CRLData.numsegs, CRL_MaxDrawSegs);
    M_snprintf(max, max_size, "%d", CRL_MAX_seg.count);
}

// -----------------------------------------------------------------------------
// CRL_CounterValue_OPN
//  [PN] Openings: "current/max (MAX: N)" and the peak value.
// -----------------------------------------------------------------------------

void CRL_CounterValue_OPN (char *const value, size_t value_size, char *const max, size_t max_size)
{
    M_snprintf(value, value_size, "%d/%d (MAX: ", CRLData.numopenings, CRL_MaxOpenings);
    M_snprintf(max, max_size, "%d", CRL_MAX_opn.count);
}

// -----------------------------------------------------------------------------
// CRL_CounterValue_PLN
//  [PN] Visplanes: "current/max (MAX: N)" and the peak value.
// -----------------------------------------------------------------------------

void CRL_CounterValue_PLN (char *const value, size_t value_size, char *const max, size_t max_size)
{
    const int TotalVisPlanes = CRL_GetTotalVisPlanes();

    M_snprintf(value, value_size, "%d/%d (MAX: ", TotalVisPlanes, CRL_MaxVisPlanes);
    M_snprintf(max, max_size, "%d", CRL_MAX_pln.count);
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
    int xx = crl_widget_font ? 20 : 32;
    int yy = 0;
    int yy2 = 0;

    const int CRL_current_pln_count = (int)(lastvisplane - visplanes);
    const int TotalVisPlanes = CRL_GetTotalVisPlanes();

    // [PN] Remember each render counter's peak and its player position.
    if (CRL_current_pln_count > CRL_MAX_pln.count)
    {
        CRL_MAX_pln.count = CRL_current_pln_count;
        CRL_Get_Render_MAX(&CRL_MAX_pln);
    }
    if (CRLData.numsolidsegs > CRL_MAX_ssg.count)
    {
        CRL_MAX_ssg.count = CRLData.numsolidsegs;
        CRL_Get_Render_MAX(&CRL_MAX_ssg);
    }
    if (CRLData.numsegs > CRL_MAX_seg.count)
    {
        CRL_MAX_seg.count = CRLData.numsegs;
        CRL_Get_Render_MAX(&CRL_MAX_seg);
    }
    if (CRLData.numopenings > CRL_MAX_opn.count)
    {
        CRL_MAX_opn.count = CRLData.numopenings;
        CRL_Get_Render_MAX(&CRL_MAX_opn);
    }

    // Even if extended HUD is disabled, we still
    // have to count MAX values in the code above.
    if (!crl_extended_hud)
        return;

    // Player coords
    if (crl_widget_coords == 1
    || (crl_widget_coords == 2 && automapactive))
    {
        char str[128];

        fontfunc(0, 25, "X:", cr[CR_GRAY]);
        CRL_FixedToString(CRLWidgets.x, str, sizeof(str));
        fontfunc(16, 25, str, cr[CR_GREEN]);

        fontfunc(0, 34, "Y:", cr[CR_GRAY]);
        CRL_FixedToString(CRLWidgets.y, str, sizeof(str));
        fontfunc(16, 34, str, cr[CR_GREEN]);

        fontfunc(0, 43, "Z:", cr[CR_GRAY]);
        CRL_FixedToString(CRLWidgets.z, str, sizeof(str));
        fontfunc(16, 43, str, cr[CR_GREEN]);

        fontfunc(0, 52, "ANG:", cr[CR_GRAY]);
        CRL_AngleToString(CRLWidgets.ang, str, sizeof(str));
        fontfunc(xx, 52, str, cr[CR_GREEN]);
    }

    if (crl_widget_playstate)
    {
        int yy1 = 0;

        // Icon of Sin spitter targets (32 max)
        if ((crl_widget_playstate == 1 
        ||  (crl_widget_playstate == 2 && CRL_brain_counter > 32)) && CRL_brain_counter)
        {
            char brn[32];

            fontfunc(0, 61, "BRN:", CRL_StatColor_Str(CRL_brain_counter, 32));
            M_snprintf(brn, 16, "%d/32", CRL_brain_counter);
            fontfunc(xx, 61, brn, CRL_StatColor_Val(CRL_brain_counter, 32));
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

            fontfunc(0, 70+yy1, "BTN:", CRL_StatColor_Str(CRL_buttons_counter, 16));
            M_snprintf(btn, 16, "%d/16", CRL_buttons_counter);
            fontfunc(xx, 70+yy1, btn, CRL_StatColor_Val(CRL_buttons_counter, 16));
        }

        // Plats (30 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_plats_counter > CRL_MaxPlats))
        {
            char plt[32];

            fontfunc(0, 79+yy1, "PLT:", CRL_StatColor_Str(CRL_plats_counter, CRL_MaxPlats));
            M_snprintf(plt, 16, "%d/%d", CRL_plats_counter, CRL_MaxPlats);
            fontfunc(xx, 79+yy1, plt, CRL_StatColor_Val(CRL_plats_counter, CRL_MaxPlats));
        }

        // Animated lines (64 max)
        if (crl_widget_playstate == 1
        || (crl_widget_playstate == 2 && CRL_lineanims_counter > CRL_MaxAnims))
        {
            char ani[32];

            fontfunc(0, 88+yy1, "ANI:", CRL_StatColor_Str(CRL_lineanims_counter, CRL_MaxAnims));
            M_snprintf(ani, 16, "%d/%d", CRL_lineanims_counter, CRL_MaxAnims);
            fontfunc(xx, 88+yy1, ani, CRL_StatColor_Val(CRL_lineanims_counter, CRL_MaxAnims));
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

    // [PN] Render counters
    if (crl_widget_render)
    {
        byte *const flash_gray = (gametic & 8) ? cr[CR_GRAY] : cr[CR_LIGHTGRAY];
        byte *const flash_red  = (gametic & 8) ? cr[CR_RED]  : cr[CR_YELLOW];

        const struct
        {
            const char *const label;
            int y;
            int current;
            int limit;
            int max_count;
            void (*get_counter)(char *, size_t, char *, size_t);
        } items[] = {
            { "SSG:", 106, CRLData.numsolidsegs, CRL_MAX_SOLIDSEGS, CRL_MAX_ssg.count, CRL_CounterValue_SSG },
            { "SEG:", 115, CRLData.numsegs,      CRL_MaxDrawSegs,   CRL_MAX_seg.count, CRL_CounterValue_SEG },
            { "OPN:", 124, CRLData.numopenings,  CRL_MaxOpenings,   CRL_MAX_opn.count, CRL_CounterValue_OPN },
            { "PLN:", 133, TotalVisPlanes,       CRL_MaxVisPlanes,  CRL_MAX_pln.count, CRL_CounterValue_PLN }
        };

        // Sprites
        if (crl_widget_render == 1
        || (crl_widget_render == 2 && CRLData.numsprites >= CRL_MaxVisSprites))
        {
            char spr[32];

            CRL_CounterValue_SPR(spr, sizeof(spr));

            fontfunc(0, 97 + yy, "SPR:", CRL_StatColor_Str(CRLData.numsprites, CRL_MaxVisSprites));
            fontfunc(xx, 97 + yy, spr, CRL_StatColor_Val(CRLData.numsprites, CRL_MaxVisSprites));
        }

        // Solidsegs, Segments, Openings, Planes
        for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++)
        {
            const int current_overflow = items[i].current >= items[i].limit;
            const int max_overflow = items[i].max_count >= items[i].limit;
            const int should_render = (crl_widget_render == 1)
                                   || (crl_widget_render == 2 && (current_overflow || max_overflow));

            if (!should_render)
            {
                continue;
            }

            char val_str[32];
            char max_str[32];

            items[i].get_counter(val_str, sizeof(val_str), max_str, sizeof(max_str));

            byte *const label_color = current_overflow ? flash_gray : cr[CR_GRAY];
            byte *const val_color   = current_overflow ? flash_red : cr[CR_GREEN];
            byte *const max_color   = current_overflow ? flash_red :
                                         (max_overflow ? CRL_Colorize_MAX(crl_widget_maxblink) : cr[CR_GREEN]);

            fontfunc(0, items[i].y + yy, items[i].label, label_color);
            fontfunc(xx, items[i].y + yy, val_str, val_color);

            const int val_width = widthfunc(val_str);
            fontfunc(xx + val_width, items[i].y + yy, max_str, max_color);

            const int max_width = widthfunc(max_str);
            fontfunc(xx + val_width + max_width, items[i].y + yy, ")", val_color);
        }
    }

    if (crl_widget_kis)
    {
        yy2 = 9;
    }

    // Level / DeathMatch timer
    if ( crl_widget_time == 1 || crl_widget_time == 3
    || ((crl_widget_time == 2 || crl_widget_time == 4) && automapactive))
    {
        char stra[8];
        const int yy3 = automapactive ? 0 : 9;

        sprintf(stra, "TIME ");
        fontfunc(0, 151 - yy2 + yy3, stra, cr[CR_GRAY]);
        fontfunc(0 + widthfunc(stra), 151 - yy2 + yy3, ID_Level_Time, cr[CR_LIGHTGRAY]);
    }

    // K/I/S stats
    if (crl_widget_kis == 1
    || (crl_widget_kis == 2 && automapactive))
    {
        const int yy4 = automapactive ? 8 : -1;

        if (!deathmatch)
        {
            char str1[8], str2[16];  // kills
            char str3[8], str4[16];  // items
            char str5[8], str6[16];  // secret

            // Kills:
            sprintf(str1, "K ");
            fontfunc(0, 159 - yy4, str1, CRL_WidgetColor(widget_kis_str));
            CRL_WidgetKISCount(str2, sizeof(str2), widget_kis_kills);
            fontfunc(0 + widthfunc(str1), 159 - yy4, str2, CRL_WidgetColor(widget_kills));

            // Items:
            if (crl_widget_kis_items == 1 || (crl_widget_kis_items == 2 && automapactive))
            {
            sprintf(str3, " I ");
            fontfunc(widthfunc(str1) + 
                        widthfunc(str2), 159 - yy4, str3, CRL_WidgetColor(widget_kis_str));
            CRL_WidgetKISCount(str4, sizeof(str4), widget_kis_items);
            fontfunc(widthfunc(str1) +
                        widthfunc(str2) +
                        widthfunc(str3), 159 - yy4, str4, CRL_WidgetColor(widget_items));
            }
            else
            {
            str3[0] = '\0';
            str4[0] = '\0';
            }

            // Secret:
            sprintf(str5, " S ");
            fontfunc(widthfunc(str1) +
                        widthfunc(str2) +
                        widthfunc(str3) +
                        widthfunc(str4), 159 - yy4, str5, CRL_WidgetColor(widget_kis_str));

            CRL_WidgetKISCount(str6, sizeof(str6), widget_kis_secrets);
            fontfunc(widthfunc(str1) +
                        widthfunc(str2) + 
                        widthfunc(str3) +
                        widthfunc(str4) +
                        widthfunc(str5), 159 - yy4, str6, CRL_WidgetColor(widget_secret));
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
                fontfunc(0, 159 - yy4, str1, CRL_WidgetColor(widget_plyr1));

                sprintf(str2, "%d ", CRLWidgets.frags_g);
                fontfunc(widthfunc(str1), 159 - yy4, str2, CRL_WidgetColor(widget_plyr1));
            }
            // Indigo
            if (playeringame[1])
            {
                sprintf(str3, "I ");
                fontfunc(widthfunc(str1) +
                            widthfunc(str2),
                            159 - yy4, str3, CRL_WidgetColor(widget_plyr2));

                sprintf(str4, "%d ", CRLWidgets.frags_i);
                fontfunc(widthfunc(str1) +
                            widthfunc(str2) +
                            widthfunc(str3),
                            159 - yy4, str4, CRL_WidgetColor(widget_plyr2));
            }
            // Brown
            if (playeringame[2])
            {
                sprintf(str5, "B ");
                fontfunc(widthfunc(str1) +
                            widthfunc(str2) +
                            widthfunc(str3) +
                            widthfunc(str4),
                            159 - yy4, str5, CRL_WidgetColor(widget_plyr3));

                sprintf(str6, "%d ", CRLWidgets.frags_b);
                fontfunc(widthfunc(str1) +
                            widthfunc(str2) +
                            widthfunc(str3) +
                            widthfunc(str4) +
                            widthfunc(str5),
                            159 - yy4, str6, CRL_WidgetColor(widget_plyr3));
            }
            // Red
            if (playeringame[3])
            {
                sprintf(str7, "R ");
                fontfunc(widthfunc(str1) +
                            widthfunc(str2) +
                            widthfunc(str3) +
                            widthfunc(str4) +
                            widthfunc(str5) +
                            widthfunc(str6),
                            159 - yy4, str7, CRL_WidgetColor(widget_plyr4));

                sprintf(str8, "%d ", CRLWidgets.frags_r);
                fontfunc(widthfunc(str1) +
                            widthfunc(str2) +
                            widthfunc(str3) +
                            widthfunc(str4) +
                            widthfunc(str5) +
                            widthfunc(str6) +
                            widthfunc(str7),
                            159 - yy4, str8, CRL_WidgetColor(widget_plyr4));
            }
        }
    }

    // Powerup timers.
    if (crl_widget_powerups)
    {
        if (CRL_invul_counter)
        {
            char invl[4];

            M_snprintf(invl, 4, "%d", CRL_invul_counter);
            CRL_WidgetTextColumns(292, 106, "INVL:", cr[CR_GRAY],
                                  invl, CRL_PowerupColor(CRL_invul_counter, 30));
        }

        if (CRL_invis_counter)
        {
            char invs[4];

            M_snprintf(invs, 4, "%d", CRL_invis_counter);
            CRL_WidgetTextColumns(292, 115, "INVS:", cr[CR_GRAY],
                                  invs, CRL_PowerupColor(CRL_invis_counter, 60));
        }

        if (CRL_rad_counter)
        {
            char rad[4];

            M_snprintf(rad, 4, "%d", CRL_rad_counter);
            CRL_WidgetTextColumns(292, 124, "RAD:", cr[CR_GRAY],
                                  rad, CRL_PowerupColor(CRL_rad_counter, 60));
        }

        if (CRL_amp_counter)
        {
            char amp[4];

            M_snprintf(amp, 4, "%d", CRL_amp_counter);
            CRL_WidgetTextColumns(292, 133, "AMP:", cr[CR_GRAY],
                                  amp, CRL_PowerupColor(CRL_amp_counter, 120));
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
    char fps_str[8];
    int yy = 9;

    // [JN] If demo timer is active and running, shift FPS widget one line down.
    // [PN] The drop is scaled with the widget size, so FPS always sits exactly
    // one shrunken line below the timer instead of drifting away from it.
    if ((demoplayback && (crl_demo_timer == 1 || crl_demo_timer == 3))
    ||  (demorecording && (crl_demo_timer == 2 || crl_demo_timer == 3)))
    {
        const int k = I_WidgetPixelScale();
        yy += k ? 9 * k / WIDGET_CANVAS_FACTOR : 9;
    }

    sprintf(fps, "%d", CRL_fps);
    sprintf(fps_str, " FPS");

    // [PN] One shrink-locked pair pinned to its right edge.
    CRL_WidgetTextPair(SCREENWIDTH - 7, yy, 1, 0, fps, cr[CR_GRAY], fps_str, cr[CR_GRAY]);
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

    const int x = crl_widget_font ? 15 : 0;
    const int xx = 237 + x + (hours > 0 ? 0 : 20);

    // [PN] Right edge pinned, so the timer stays put at any widget scale.
    CRL_WidgetTextPair(xx + widthfunc(n), 9, 1, 0, n, cr[CR_LIGHTGRAY], NULL, NULL);
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
    // [JN] Row gap shrinks with the widget scale, like the stat block rows.
    const int k = I_WidgetPixelScale();
    const int gap = k ? 9 * k / WIDGET_CANVAS_FACTOR : 9;
    sprintf(str, "%d/%d", player->targetsheath, player->targetsmaxheath);
    color = CRL_HealthColor(player->targetsheath, player->targetsmaxheath);

    switch (crl_widget_health)
    {
        case 1:  // Top
            fontcenteredfunc(18, str, color);
            break;
        case 2:  // Top + name
            fontcenteredfunc(9, player->targetsname, color);
            fontcenteredfunc(9 + gap, str, color);
            break;
        case 3:  // Bottom
            fontcenteredfunc(152 - yy, str, color);
            break;
        case 4:  // Bottom + name
            fontcenteredfunc(152 - yy - gap, player->targetsname, color);
            fontcenteredfunc(152 - yy, str, color);
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

    // [JN] One shrink-locked pair, centered at any widget scale.
    CRL_WidgetTextPair(160 /*SCREENWIDTH / 2*/, 151, 0, 1, str,
                       CRL_WidgetColor(widget_speed_str),
                       val, CRL_WidgetColor(widget_speed_val));
}

// =============================================================================
//
//                              Powerup Shortcuts
//
// =============================================================================

#define FULL_CHEAT_CHECK                                                    \
                                                                            \
player_t *const plr = &players[consoleplayer];                              \
                                                                            \
if (demorecording)                                                          \
{                                                                           \
    CRL_SetMessage(plr, CRL_POWERUP_NA_R, false, NULL);                     \
    return;                                                                 \
}                                                                           \
if (demoplayback)                                                           \
{                                                                           \
    CRL_SetMessage(plr, CRL_POWERUP_NA_P, false, NULL);                     \
    return;                                                                 \
}                                                                           \
if (netgame)                                                                \
{                                                                           \
    CRL_SetMessage(plr, CRL_POWERUP_NA_N, false, NULL);                     \
    return;                                                                 \
}

void CRL_Give_200_Health (void)
{
    FULL_CHEAT_CHECK;
    plr->health = plr->mo->health = 200;
    CRL_SetMessage(plr, "200% HEALTH", false, NULL);
}

void CRL_Give_200_Armor (void)
{
    FULL_CHEAT_CHECK;
    plr->armorpoints = 200;
    plr->armortype = 2;
    CRL_SetMessage(plr, "200% ARMOR", false, NULL);
}

void CRL_Reset_Health (void)
{
    FULL_CHEAT_CHECK;
    plr->health = plr->mo->health = 100;
    CRL_SetMessage(plr, "HEALTH RESET", false, NULL);
}

void CRL_Reset_Armor (void)
{
    FULL_CHEAT_CHECK;
    plr->armorpoints = plr->armortype = 0;
    CRL_SetMessage(plr, "ARMOR RESET", false, NULL);
}

void CRL_Give_Powerup (const powertype_t power)
{
    FULL_CHEAT_CHECK;

    switch (power)
    {
        case pw_invulnerability:
            plr->powers[pw_invulnerability] =
            plr->powers[pw_invulnerability] ? 0 : INVULNTICS;
            plr->fixedcolormap = plr->powers[pw_infrared] ? 1 :
                                 plr->powers[pw_invulnerability] ? 32 : 0;
            CRL_SetMessage(plr, plr->powers[pw_invulnerability] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        case pw_strength:
            plr->powers[pw_strength] =
            plr->powers[pw_strength] ? 0 : 1;
            CRL_SetMessage(plr, plr->powers[pw_strength] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        case pw_invisibility:
            plr->powers[pw_invisibility] =
            plr->powers[pw_invisibility] ? 0 : INVISTICS;
            plr->mo->flags ^= MF_SHADOW;
            CRL_SetMessage(plr, plr->powers[pw_invisibility] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        case pw_ironfeet:
            plr->powers[pw_ironfeet] =
            plr->powers[pw_ironfeet] ? 0 : IRONTICS;
            CRL_SetMessage(plr, plr->powers[pw_ironfeet] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        case pw_allmap:
            plr->powers[pw_allmap] ^= 1;
            CRL_SetMessage(plr, plr->powers[pw_allmap] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        case pw_infrared:
            plr->powers[pw_infrared] =
            plr->powers[pw_infrared] ? 0 : INFRATICS;
            plr->fixedcolormap = plr->powers[pw_invulnerability] ? 32 :
                                 plr->powers[pw_infrared] ? 1 : 0;
            CRL_SetMessage(plr, plr->powers[pw_infrared] ?
                            CRL_POWERUP_ON : CRL_POWERUP_OFF, false, NULL);
            break;

        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// [PN] Widget-overlay text drawers. The canvas is a fixed 4x screen size and
// I_WidgetPixelScale() picks how many canvas pixels one glyph pixel becomes
// (2/3/4 options = 75/50/25 percent; 0/1 keep the legacy in-buffer path).
// Origins and advances use that same scale, so the full-size layout is
// reproduced as one proportionally smaller whole, anchored bottom-left:
// the widget block keeps its full-size bottom edge while shrinking.
// -----------------------------------------------------------------------------

// [JN] Bottom edge of the widget block in 320-space: the K/I/S line
// (or TIME when K is off) sits here, 9 px higher while automap is active.
static int CRL_WidgetAnchorY (void)
{
    return 160 - (automapactive ? 9 : 0);
}

// [JN] Canvas y for a left-column widget row: rows at or above the block
// anchor shrink up to it, rows below (chat lines) keep their absolute spot.
static int CRL_WidgetRowY (int y_game)
{
    const int k = I_WidgetPixelScale();
    const int a = CRL_WidgetAnchorY();

    return (y_game <= a) ? y_game * k + (WIDGET_CANVAS_FACTOR - k) * a
                         : y_game * WIDGET_CANVAS_FACTOR;
}

// [PN] Draw a widget string in canvas coordinates with the active font.
static void WidgetTextAt (int cx, int cy, const char *s, byte *table)
{
    if (crl_widget_font)
    {
        WidgetTextDSDAAt(cx, cy, s, table);
    }
    else
    {
        WidgetTextPatchAt(cx, cy, s, table);
    }
}

static void WidgetTextPatchAt (int cx, int cy, const char *string, byte *table)
{
    const int k = I_WidgetPixelScale();
    const int start_cx = cx; // [PN] line start for CR and LF
    int canvas_w;

    I_WidgetCanvas(&canvas_w, NULL);

    dp_translation = table;

    for ( ; *string ; string++)
    {
        int c = *string;

        if (c == '\n')
        {
            cx = start_cx;
            cy += 12 * k;
            continue;
        }

        if (c == '\r')
        {
            cx = start_cx;
            cy += 9 * k;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4 * k;
            continue;
        }

        const int w = SHORT (hu_font[c]->width);

        if (cx + w * k > canvas_w)
        {
            break;
        }

        V_DrawPatchWidget (cx, cy, hu_font[c], true);
        cx += w * k;
    }

    dp_translation = NULL;
}

void CRL_WidgetTextPatch (int x, int y, const char *string, byte *table)
{
    const int k = I_WidgetPixelScale();

    if (k == 0)
    {
        M_WriteText(x, y, string, table); // [PN] overlay off: legacy path
        return;
    }

    WidgetTextPatchAt(x * k, CRL_WidgetRowY(y), string, table);
}

void CRL_WidgetTextPatchCentered (int y, const char *string, byte *table)
{
    int canvas_w;

    if (I_WidgetPixelScale() == 0 || I_WidgetCanvas(&canvas_w, NULL) == NULL)
    {
        M_WriteTextCentered(y, string, table);
        return;
    }

    WidgetTextPatchAt((canvas_w - widthfunc(string) * I_WidgetPixelScale()) / 2,
                      y * WIDGET_CANVAS_FACTOR, string, table);
}

static void WidgetTextDSDAAt (int cx, int cy, const char *text, byte *table)
{
    const int k = I_WidgetPixelScale();

    dp_translation = table;

    for (const char *ch = text; *ch; ch++)
    {
        const int c = toupper((unsigned char) *ch);
        const patch_t *patch;

        if (c < 33 || c > 126)
        {
            cx += 4 * k;  // Space or non-printable character
            continue;
        }

        patch = dsda_font[c - HU_FONTSTART];

        V_DrawPatchWidget (cx, cy, patch, false);
        cx += patch->width * k;
    }

    dp_translation = NULL;
}

void CRL_WidgetTextDSDA (int x, int y, const char *text, byte *table)
{
    const int k = I_WidgetPixelScale();

    if (k == 0)
    {
        DSDA_DrawText(x, y, text, table); // [PN] overlay off: legacy path
        return;
    }

    WidgetTextDSDAAt(x * k, CRL_WidgetRowY(y), text, table);
}

void CRL_WidgetTextDSDACentered (int y, const char *text, byte *table)
{
    int canvas_w;

    if (I_WidgetPixelScale() == 0 || I_WidgetCanvas(&canvas_w, NULL) == NULL)
    {
        DSDA_DrawTextCentered(y, text, table);
        return;
    }

    WidgetTextDSDAAt((canvas_w - DSDA_StringWidth(text) * I_WidgetPixelScale()) / 2,
                     y * WIDGET_CANVAS_FACTOR, text, table);
}

// -----------------------------------------------------------------------------
// CRL_WidgetTextPair
//  [JN] Draw "label value" as one shrink-locked widget: the value follows
//  the label at the scaled advance, so the pair never scatters. x_game is
//  the pair center, or its right edge when right_anchored.
// -----------------------------------------------------------------------------

static void CRL_WidgetTextPair (int x_game, int y_game, int right_anchored,
                                int bottom_anchor,
                                const char *s1, byte *c1,
                                const char *s2, byte *c2)
{
    const int k = I_WidgetPixelScale();
    const int w1 = widthfunc(s1);
    const int total = w1 + (s2 ? widthfunc(s2) : 0);

    if (k == 0)
    {
        const int x = right_anchored ? x_game - total
                                     : x_game - total / 2;

        fontfunc(x, y_game, s1, c1);

        if (s2)
        {
            fontfunc(x + w1, y_game, s2, c2);
        }
        return;
    }

    const int cx = right_anchored ? x_game * WIDGET_CANVAS_FACTOR - total * k
                                  : x_game * WIDGET_CANVAS_FACTOR - total * k / 2;
    // [PN] Top-pinned widgets (FPS, demo timer) keep their absolute y;
    // block widgets shrink toward the bottom anchor like the rest.
    const int cy = bottom_anchor ? CRL_WidgetRowY(y_game)
                                 : y_game * WIDGET_CANVAS_FACTOR;

    WidgetTextAt(cx, cy, s1, c1);

    if (s2)
    {
        WidgetTextAt(cx + w1 * k, cy, s2, c2);
    }
}

// -----------------------------------------------------------------------------
// CRL_WidgetTextColumns
//  [JN] Two-column widget:
//   s1 right-aligns its edge to label_right,
//   s2 left-aligns 4 px later; both columns stay pinned at any widget scale.
// -----------------------------------------------------------------------------

static void CRL_WidgetTextColumns (int label_right, int y_game,
                                   const char *s1, byte *c1,
                                   const char *s2, byte *c2)
{
    const int k = I_WidgetPixelScale();
    const int w1 = widthfunc(s1);

    if (k == 0)
    {
        fontfunc(label_right - w1, y_game, s1, c1);
        fontfunc(label_right + 4, y_game, s2, c2);
        return;
    }

    const int cy = CRL_WidgetRowY(y_game);
    const int lx = label_right * WIDGET_CANVAS_FACTOR - w1 * k;
    const int rx = (label_right + 4) * WIDGET_CANVAS_FACTOR;

    WidgetTextAt(lx, cy, s1, c1);
    WidgetTextAt(rx, cy, s2, c2);
}
