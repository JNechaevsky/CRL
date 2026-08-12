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
// F_finale.c

#include <ctype.h>

#include "doomdef.h"
#include "deh_str.h"
#include "i_swap.h"
#include "i_video.h"
#include "s_sound.h"
#include "v_video.h"
#include "am_map.h"

#include "crlcore.h"

static int finalestage;                // 0 = text, 1 = art screen
static int finalecount;
static int finaleendcount;

// [JN] Do screen wipe only once after text skipping.
static boolean finale_wipe_done;

#define TEXTSPEED       3
#define TEXTWAIT        250
#define	TEXTEND         25

static const char *finaletext;
static const char *finaleflat;

static int FontABaseLump;

// [JN] Externalized F_DemonScroll variables to allow repeated scrolling.
static int yval = 0;
static int nextscroll = 0;


/*
=======================
=
= F_StartFinale
=
=======================
*/

void F_StartFinale(void)
{
    gameaction = ga_nothing;
    gamestate = GS_FINALE;
    automapactive = false;
    finale_wipe_done = false;
    players[consoleplayer].cheatTics = 1;
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageCenteredTics = 1;
    players[consoleplayer].messageCentered = NULL;

    switch (gameepisode)
    {
        case 1:
            finaleflat = DEH_String("FLOOR25");
            finaletext = DEH_String(E1TEXT);
            break;
        case 2:
            finaleflat = DEH_String("FLATHUH1");
            finaletext = DEH_String(E2TEXT);
            break;
        case 3:
            finaleflat = DEH_String("FLTWAWA2");
            finaletext = DEH_String(E3TEXT);
            break;
        case 4:
            finaleflat = DEH_String("FLOOR28");
            finaletext = DEH_String(E4TEXT);
            break;
        case 5:
            finaleflat = DEH_String("FLOOR08");
            finaletext = DEH_String(E5TEXT);
            break;
    }

    finalestage = 0;
    finalecount = 0;
    // [JN] Count intermission/finale text lenght. Once it's fully printed, 
    // no extra "attack/use" button pressing is needed for skipping.
    finaleendcount = strlen(finaletext) * TEXTSPEED + TEXTEND;
    FontABaseLump = W_GetNumForName(DEH_String("FONTA_S")) + 1;
    // [JN] Reset F_DemonScroll variables.
    yval = 0;
    nextscroll = 0;

//      S_ChangeMusic(mus_victor, true);
    S_StartSong(mus_cptd, true);
}



boolean F_Responder(const event_t *event)
{
    if (event->type != ev_keydown)
    {
        return false;
    }
    if (finalestage == 1 && gameepisode == 2)
    {                           // we're showing the water pic, make any key kick to demo mode
        finalestage++;
        /*
        memset((byte *) 0xa0000, 0, SCREENWIDTH * SCREENHEIGHT);
        memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT);
        I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
        */
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// F_HandleDoubleSkip
//  [PN] Helper function for double-skip logic.
// -----------------------------------------------------------------------------

static void F_HandleDoubleSkip(void)
{
    player_t *const player = &players[consoleplayer];
    
    // Arrays for buttons and their corresponding state fields
    const int buttons[] = {BT_ATTACK, BT_USE};
    boolean *const state_fields[] = {&player->attackdown, &player->usedown};
    
    for (int i = 0; i < 2; i++)
    {
        const boolean old_state = *state_fields[i];
        
        if (player->cmd.buttons & buttons[i] && !MenuActive && !finalestage)
        {
            if (!old_state)
            {
                if (finalecount >= finaleendcount)
                {
                    finalestage = 1;
                }
                finalecount += finaleendcount;
            }
            *state_fields[i] = true;
        }
        else
        {
            *state_fields[i] = false;
        }
    }
}

/*
=======================
=
= F_Ticker
=
=======================
*/

void F_Ticker(void)
{
    // [JN] If we are in single player mode, allow double skipping of
    // finale texts. The first skip is printing all the text,
    // the second is advancing to next state.
    if (singleplayer)
    {
        // [JN] Make PAUSE working properly on text screen
        if (paused)
        {
            return;
        }

        // [JN] Check for skipping. Allow double-press skiping, 
        // but don't skip immediately.
        if (finalecount > 10)
        {
            // [JN] Don't allow skipping by pressing PAUSE button.
            if (players[consoleplayer].cmd.buttons == (BT_SPECIAL | BTS_PAUSE))
            {
                return;
            }

            // [PN] Double-skip by pressing "attack" or "use" button.
            F_HandleDoubleSkip();
        }

        // [JN] Force a wipe after skipping text screen.
        if (finalestage && !finale_wipe_done)
        {
            finale_wipe_done = true;
            wipegamestate = -1;
        }

        // Advance animation.
        finalecount++;
    }
    //
    // [JN] Standard Heretic routine, safe for network game and demos.
    //
    else
    {
    finalecount++;
    if (!finalestage
        && finalecount > strlen(finaletext) * TEXTSPEED + TEXTWAIT)
    {
        finalecount = 0;
        if (!finalestage)
        {
            finalestage = 1;
        }

//              wipegamestate = -1;             // force a wipe
/*
		if (gameepisode == 3)
			S_StartMusic (mus_bunny);
*/
    }
    }
}


/*
=======================
=
= F_TextWrite
=
=======================
*/


static void F_TextWrite(void)
{
    byte *src, *dest;
    int count;
    const char *ch;
    int c;
    int cx, cy;
    patch_t *w;

//
// erase the entire screen to a tiled background
//
    src = W_CacheLumpName(finaleflat, PU_CACHE);
    dest = I_VideoBuffer;

    // [crispy] use unified flat filling function
    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);


//
// draw some of the text onto the screen
//
    cx = 20;
    cy = 5;
    ch = finaletext;

    count = (finalecount - 10) / TEXTSPEED;
    if (count < 0)
        count = 0;
    for (; count; count--)
    {
        c = *ch++;
        if (!c)
            break;
        if (c == '\n')
        {
            cx = 20;
            cy += 9;
            continue;
        }

        c = toupper(c);
        if (c < 33)
        {
            cx += 5;
            continue;
        }

        w = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
        if (cx + SHORT(w->width) > SCREENWIDTH)
            break;
        V_DrawShadowedPatchRavenOptional(cx, cy, w, "NULL"); // [JN] TODO - proper name
        cx += SHORT(w->width);
    }

}

/*
==================
=
= F_DemonScroll
=
==================
*/

static void F_DemonScroll(void)
{
    byte *p1, *p2;

    if (finalecount < nextscroll)
    {
        return;
    }
    p1 = W_CacheLumpName(DEH_String("FINAL1"), PU_LEVEL);
    p2 = W_CacheLumpName(DEH_String("FINAL2"), PU_LEVEL);
    if (finalecount < 70)
    {
        memcpy(I_VideoBuffer, p1, SCREENHEIGHT * SCREENWIDTH);
        nextscroll = finalecount;
        return;
    }
    if (yval < 64000)
    {
        memcpy(I_VideoBuffer, p2 + SCREENHEIGHT * SCREENWIDTH - yval, yval);
        memcpy(I_VideoBuffer + yval, p1, SCREENHEIGHT * SCREENWIDTH - yval);
        yval += SCREENWIDTH;
        nextscroll = finalecount + 3;
    }
    else
    {                           //else, we'll just sit here and wait, for now
        memcpy(I_VideoBuffer, p2, SCREENWIDTH * SCREENHEIGHT);
    }
}

/*
==================
=
= F_DrawUnderwater
=
==================
*/

static void F_DrawUnderwater(void)
{
    static boolean underwawa = false;
    const char *lumpname;
    byte *palette;

    // The underwater screen has its own palette, which is rather annoying.
    // The palette doesn't correspond to the normal palette. Because of
    // this, we must regenerate the lookup tables used in the video scaling
    // code.

    switch (finalestage)
    {
        case 1:
            if (!underwawa)
            {
                underwawa = true;
                V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
                lumpname = DEH_String("E2PAL");
                palette = W_CacheLumpName(lumpname, PU_STATIC);
                I_SetPalette(palette);
                W_ReleaseLumpName(lumpname);
            }
            // [PN] Redraw every frame: after crossfade, the wipe buffer can
            // still contain faint remnants of finale text if we don't refresh
            // the underwater background continuously.
            V_DrawRawScreen(W_CacheLumpName(DEH_String("E2END"), PU_CACHE));
            paused = false;
            MenuActive = false;
            askforquit = false;

            break;
        case 2:
            if (underwawa)
            {
                lumpname = DEH_String("PLAYPAL");
                palette = W_CacheLumpName(lumpname, PU_STATIC);
                I_SetPalette(palette);
                W_ReleaseLumpName(lumpname);
                underwawa = false;
            }
            V_DrawRawScreen(W_CacheLumpName(DEH_String("TITLE"), PU_CACHE));
            //D_StartTitle(); // go to intro/demo mode.
    }
}

/*
=======================
=
= F_Drawer
=
=======================
*/

void F_Drawer(void)
{
    if (!finalestage)
        F_TextWrite();
    else
    {
        switch (gameepisode)
        {
            case 1:
                if (gamemode == shareware)
                {
                    V_DrawRawScreen(W_CacheLumpName("ORDER", PU_CACHE));
                }
                else
                {
                    V_DrawRawScreen(W_CacheLumpName("CREDIT", PU_CACHE));
                }
                break;
            case 2:
                F_DrawUnderwater();
                break;
            case 3:
                F_DemonScroll();
                break;
            case 4:            // Just show credits screen for extended episodes
            case 5:
                V_DrawRawScreen(W_CacheLumpName("CREDIT", PU_CACHE));
                break;
        }
    }
}
