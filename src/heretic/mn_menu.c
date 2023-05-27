//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

// MN_menu.c

#include <stdlib.h>
#include <ctype.h>

#include "deh_str.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "i_input.h"
#include "i_system.h"
#include "i_swap.h"
#include "i_timer.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "r_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"

#include "crlcore.h"
#include "crlvars.h"

// Macros

#define LEFT_DIR 0
#define RIGHT_DIR 1
#define ITEM_HEIGHT 20
#define SELECTOR_XOFFSET (-28)
#define SELECTOR_YOFFSET (-1)
#define SLOTTEXTLEN     16
#define ASCII_CURSOR '['

// Types

typedef enum
{
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC,
    ITT_SETMENU,
    ITT_INERT
} ItemType_t;

typedef enum
{
    MENU_MAIN,
    MENU_EPISODE,
    MENU_SKILL,
    MENU_OPTIONS,
    MENU_OPTIONS2,
    MENU_FILES,
    MENU_LOAD,
    MENU_SAVE,
    MENU_CRLMAIN,
    MENU_CRLVIDEO,
    MENU_CRLSOUND,
    MENU_CRLCONTROLS,
    MENU_CRLKBDBINDS1, // pitto
    MENU_CRLMOUSEBINDS,
    MENU_CRLWIDGETS,
    // MENU_CRLGAMEPLAY,
    MENU_CRLLIMITS,
    MENU_NONE
} MenuType_t;

typedef struct
{
    ItemType_t type;
    char *text;
    boolean(*func) (int option);
    int option;
    MenuType_t menu;
    short tics;  // [JN] Menu item timer for glowing effect.
} MenuItem_t;

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

typedef struct
{
    int x;
    int y;
    void (*drawFunc) (void);
    int itemCount;
    MenuItem_t *items;
    int oldItPos;
    boolean smallFont;  // [JN] If true, use small font
    MenuType_t prevMenu;
} Menu_t;

// Private Functions

static void InitFonts(void);
static void SetMenu(MenuType_t menu);
static boolean SCNetCheck(int option);
static boolean SCQuitGame(int option);
static boolean SCEpisode(int option);
static boolean SCSkill(int option);
static boolean SCMouseSensi(int option);
static boolean SCSfxVolume(int option);
static boolean SCMusicVolume(int option);
static boolean SCScreenSize(int option);
static boolean SCLoadGame(int option);
static boolean SCSaveGame(int option);
static boolean SCMessages(int option);
static boolean SCEndGame(int option);
static boolean SCInfo(int option);
static void DrawMainMenu(void);
static void DrawEpisodeMenu(void);
static void DrawSkillMenu(void);
static void DrawOptionsMenu(void);
static void DrawOptions2Menu(void);
static void DrawFileSlots(Menu_t * menu);
static void DrawFilesMenu(void);
static void MN_DrawInfo(void);
static void DrawLoadMenu(void);
static void DrawSaveMenu(void);
static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing);
void MN_LoadSlotText(void);

// External Data

extern int detailLevel;
extern int screenblocks;

// Public Data

boolean MenuActive;
int InfoType;
boolean messageson;

// Private Data

static int FontABaseLump;
static int FontBBaseLump;
static int SkullBaseLump;
static Menu_t *CurrentMenu;
static int CurrentItPos;
static int MenuEpisode;
static int MenuTime;
static boolean soundchanged;

boolean askforquit;
static int typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static char SlotText[6][SLOTTEXTLEN + 2];
static char oldSlotText[SLOTTEXTLEN + 2];
static int SlotStatus[6];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

static char gammamsg[15][32] =
{
    GAMMALVL05,
    GAMMALVL055,
    GAMMALVL06,
    GAMMALVL065,
    GAMMALVL07,
    GAMMALVL075,
    GAMMALVL08,
    GAMMALVL085,
    GAMMALVL09,
    GAMMALVL095,
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

static MenuItem_t MainItems[] = {
    {ITT_EFUNC, "NEW GAME", SCNetCheck, 1, MENU_EPISODE},
    {ITT_SETMENU, "OPTIONS", NULL, 0, MENU_OPTIONS},
    {ITT_SETMENU, "GAME FILES", NULL, 0, MENU_FILES},
    {ITT_EFUNC, "INFO", SCInfo, 0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME", SCQuitGame, 0, MENU_NONE}
};

static Menu_t MainMenu = {
    110, 56,
    DrawMainMenu,
    5, MainItems,
    0,
    false,
    MENU_NONE
};

static MenuItem_t EpisodeItems[] = {
    {ITT_EFUNC, "CITY OF THE DAMNED", SCEpisode, 1, MENU_NONE},
    {ITT_EFUNC, "HELL'S MAW", SCEpisode, 2, MENU_NONE},
    {ITT_EFUNC, "THE DOME OF D'SPARIL", SCEpisode, 3, MENU_NONE},
    {ITT_EFUNC, "THE OSSUARY", SCEpisode, 4, MENU_NONE},
    {ITT_EFUNC, "THE STAGNANT DEMESNE", SCEpisode, 5, MENU_NONE}
};

static Menu_t EpisodeMenu = {
    80, 50,
    DrawEpisodeMenu,
    3, EpisodeItems,
    0,
    false,
    MENU_MAIN
};

static MenuItem_t FilesItems[] = {
    {ITT_EFUNC, "LOAD GAME", SCNetCheck, 2, MENU_LOAD},
    {ITT_SETMENU, "SAVE GAME", NULL, 0, MENU_SAVE}
};

static Menu_t FilesMenu = {
    110, 60,
    DrawFilesMenu,
    2, FilesItems,
    0,
    false,
    MENU_MAIN
};

static MenuItem_t LoadItems[] = {
    {ITT_EFUNC, NULL, SCLoadGame, 0, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 1, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 2, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 3, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 4, MENU_NONE},
    {ITT_EFUNC, NULL, SCLoadGame, 5, MENU_NONE}
};

static Menu_t LoadMenu = {
    70, 30,
    DrawLoadMenu,
    6, LoadItems,
    0,
    false,
    MENU_FILES
};

static MenuItem_t SaveItems[] = {
    {ITT_EFUNC, NULL, SCSaveGame, 0, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 1, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 2, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 3, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 4, MENU_NONE},
    {ITT_EFUNC, NULL, SCSaveGame, 5, MENU_NONE}
};

static Menu_t SaveMenu = {
    70, 30,
    DrawSaveMenu,
    6, SaveItems,
    0,
    false,
    MENU_FILES
};

static MenuItem_t SkillItems[] = {
    {ITT_EFUNC, "THOU NEEDETH A WET-NURSE", SCSkill, sk_baby, MENU_NONE},
    {ITT_EFUNC, "YELLOWBELLIES-R-US", SCSkill, sk_easy, MENU_NONE},
    {ITT_EFUNC, "BRINGEST THEM ONETH", SCSkill, sk_medium, MENU_NONE},
    {ITT_EFUNC, "THOU ART A SMITE-MEISTER", SCSkill, sk_hard, MENU_NONE},
    {ITT_EFUNC, "BLACK PLAGUE POSSESSES THEE",
     SCSkill, sk_nightmare, MENU_NONE}
};

static Menu_t SkillMenu = {
    38, 30,
    DrawSkillMenu,
    5, SkillItems,
    2,
    false,
    MENU_EPISODE
};

static MenuItem_t OptionsItems[] = {
    {ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE},
    {ITT_EFUNC, "MESSAGES : ", SCMessages, 0, MENU_NONE},
    {ITT_LRFUNC, "MOUSE SENSITIVITY", SCMouseSensi, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_SETMENU, "MORE...", NULL, 0, MENU_OPTIONS2}
};

static Menu_t OptionsMenu = {
    88, 30,
    DrawOptionsMenu,
    5, OptionsItems,
    0,
    false,
    MENU_MAIN
};

static MenuItem_t Options2Items[] = {
    {ITT_LRFUNC, "SCREEN SIZE", SCScreenSize, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_LRFUNC, "SFX VOLUME", SCSfxVolume, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_LRFUNC, "MUSIC VOLUME", SCMusicVolume, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE}
};

static Menu_t Options2Menu = {
    90, 20,
    DrawOptions2Menu,
    6, Options2Items,
    0,
    false,
    MENU_OPTIONS
};

// =============================================================================
// [JN] CRL custom menu
// =============================================================================

#define CRL_MENU_TOPOFFSET     (30)
#define CRL_MENU_LEFTOFFSET    (48)
#define CRL_MENU_RIGHTOFFSET   (SCREENWIDTH - CRL_MENU_LEFTOFFSET)

#define CRL_MENU_LEFTOFFSET_SML    (72)
#define CRL_MENU_RIGHTOFFSET_SML   (SCREENWIDTH - CRL_MENU_LEFTOFFSET_SML)

#define ITEM_HEIGHT_SMALL        10
#define SELECTOR_XOFFSET_SMALL  -10

static player_t *player;

static byte *M_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT2] :
        tics == 4 ? cr[CR_MENU_BRIGHT1] :
        tics == 3 ? NULL :
        tics == 2 ? cr[CR_MENU_DARK1]   :
                    cr[CR_MENU_DARK2]   ;
        /*            
        tics == 1 ? cr[CR_MENU_DARK2]  :
                    cr[CR_MENU_DARK3]  ;
        */
}

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_DARKGREEN  4

#define ITEMONTICS      CurrentMenu->items[CurrentItPos].tics
#define ITEMSETONTICS   CurrentMenu->items[CurrentItPosOn].tics

static byte *M_Item_Glow (const int CurrentItPosOn, const int color, const int tics)
{
    if (CurrentItPos == CurrentItPosOn)
    {
        return
            color == GLOW_RED   || color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]   :
            color == GLOW_GREEN || color == GLOW_DARKGREEN ? cr[CR_GREEN_BRIGHT5] :
                                                             cr[CR_MENU_BRIGHT5]  ; // GLOW_UNCOLORED
    }
    else
    {
        if (color == GLOW_UNCOLORED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_MENU_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_MENU_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_MENU_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_MENU_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
        }
        if (color == GLOW_RED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        if (color == GLOW_DARKRED)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_DARK1] :
                ITEMSETONTICS == 4 ? cr[CR_RED_DARK2] :
                ITEMSETONTICS == 3 ? cr[CR_RED_DARK3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_DARK4] :
                ITEMSETONTICS == 1 ? cr[CR_RED_DARK5] : cr[CR_DARKRED];
        }
        if (color == GLOW_GREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
        if (color == GLOW_DARKGREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_DARKGREEN];
        }
    }
    return NULL;
}

static byte *M_Cursor_Glow (const int tics)
{
    return
        tics >  6 ? cr[CR_MENU_BRIGHT3] :
        tics >  4 ? cr[CR_MENU_BRIGHT2] :
        tics >  2 ? cr[CR_MENU_BRIGHT1] :
        tics > -1 ? NULL                :
        tics > -3 ? cr[CR_MENU_DARK1]   :
        tics > -5 ? cr[CR_MENU_DARK2]   :
        tics > -7 ? cr[CR_MENU_DARK3]   :
        tics > -9 ? cr[CR_MENU_DARK4]   : NULL;
}

static const int M_INT_Slider (int val, int min, int max, int direction)
{
    switch (direction)
    {
        case 0:
        val--;
        if (val < min) 
            val = max;
        break;

        case 1:
        val++;
        if (val > max)
            val = min;
        break;
    }
    return val;
}

static void DrawCRLMain (void);
static boolean CRL_Spectating (int option);
static boolean CRL_Freeze (int option);
static boolean CRL_NoTarget (int option);
static boolean CRL_NoMomentum (int option);

static void DrawCRLVideo (void);
static boolean CRL_UncappedFPS (int option);
static boolean CRL_LimitFPS (int option);
static boolean CRL_VSync (int option);
static boolean CRL_ShowFPS (int option);
static boolean CRL_VisplanesDraw (int option);
static boolean CRL_HOMDraw (int option);
static boolean CRL_Gamma (int option);
static boolean CRL_TextShadows (int option);
static boolean CRL_GfxStartup (int option);
static boolean CRL_EndText (int option);

static void DrawCRLSound (void);
static boolean CRL_MusicSystem (int option);
static boolean CRL_SFXMode (int option);
static boolean CRL_PitchShift (int option);
static boolean CRL_SFXChannels (int option);

static void DrawCRLControls (void);
static boolean CRL_Controls_Acceleration (int option);
static boolean CRL_Controls_Threshold (int option);
static boolean CRL_Controls_MLook (int option);
static boolean CRL_Controls_NoVert (int option);

static void DrawCRLMouse (void);
static boolean M_Bind_M_FireAttack (int option);
static boolean M_Bind_M_MoveForward (int option);
static boolean M_Bind_M_StrafeOn (int option);
static boolean M_Bind_M_MoveBackward (int option);
static boolean M_Bind_M_Use (int option);
static boolean M_Bind_M_StrafeLeft (int option);
static boolean M_Bind_M_StrafeRight (int option);
static boolean M_Bind_M_PrevWeapon (int option);
static boolean M_Bind_M_NextWeapon (int option);


static void DrawCRLWidgets (void);
static boolean CRL_Widget_Coords (int option);
static boolean CRL_Widget_Playstate (int option);
static boolean CRL_Widget_Render (int option);
static boolean CRL_Widget_KIS (int option);
static boolean CRL_Widget_Time (int option);
static boolean CRL_Widget_Powerups (int option);
static boolean CRL_Widget_Health (int option);

static void DrawCRLLimits (void);
static boolean CRL_ZMalloc (int option);
static boolean CRL_SaveSizeWarning (int option);
static boolean CRL_DemoSizeWarning (int option);
static boolean CRL_Limits (int option);

// Keyboard binding prototypes
static boolean KbdIsBinding;
static int     keyToBind;

static void DrawCRLKbd1 (void);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static char   *M_MouseBtnDrawer (int CurrentItPosOn, int btn);
static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int itemOn);
static byte   *M_ColorizeMouseBind (int CurrentItPosOn, int btn);

// -----------------------------------------------------------------------------

// [JN] Delay before shading.
// static int shade_wait;
// [JN] Shade background while in CRL menu.
static void M_ShadeBackground (void)
{
    for (int y = 0; y < SCREENWIDTH * SCREENHEIGHT; y++)
    {
        I_VideoBuffer[y] = colormaps[12 * 256 + I_VideoBuffer[y]];
    }
    SB_state = -1;  // Refresh the statbar.
}

// -----------------------------------------------------------------------------
// Main CRL Menu
// -----------------------------------------------------------------------------

static MenuItem_t CRLMainItems[] = {
    {ITT_LRFUNC,  "SPECTATOR MODE",       CRL_Spectating, 0, MENU_NONE},
    {ITT_LRFUNC,  "FREEZE MODE",          CRL_Freeze,     0, MENU_NONE},
    {ITT_LRFUNC,  "NO TARGET MODE",       CRL_NoTarget,   0, MENU_NONE},
    {ITT_LRFUNC,  "NO MOMENTUM MODE",     CRL_NoMomentum, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                   NULL,           0, MENU_NONE},
    {ITT_SETMENU, "VIDEO OPTIONS",        NULL,           0, MENU_CRLVIDEO},
    {ITT_SETMENU, "SOUND OPTIONS",        NULL,           0, MENU_CRLSOUND},
    {ITT_SETMENU, "CONTROL SETTINGS",     NULL,           0, MENU_CRLCONTROLS},
    {ITT_SETMENU, "WIDGETS AND AUTOMAP",  NULL,           0, MENU_CRLWIDGETS},
    {ITT_SETMENU, "GAMEPLAY FEATURES",    NULL,           0, MENU_NONE},
    {ITT_SETMENU, "STATIC ENGINE LIMITS", NULL,           0, MENU_CRLLIMITS}

};

static Menu_t CRLMain = {
    CRL_MENU_LEFTOFFSET_SML, CRL_MENU_TOPOFFSET,
    DrawCRLMain,
    11, CRLMainItems,
    0,
    true,
    MENU_NONE
};

static void DrawCRLMain (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("MAIN MENU", 20, cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 30,
               M_Item_Glow(0, crl_spectating ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Freeze
    sprintf(str, !singleplayer ? "N/A" : crl_freeze ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 40,
                 M_Item_Glow(1, !singleplayer ? GLOW_DARKRED :
                             crl_freeze ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 50,
                 M_Item_Glow(2, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOTARGET ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 60, 
                 M_Item_Glow(3, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOMOMENTUM ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    MN_DrTextACentered ("SETTINGS", 70, cr[CR_YELLOW]);
}

static boolean CRL_Spectating (int option)
{
    crl_spectating ^= 1;
    return true;
}

static boolean CRL_Freeze (int option)
{
    if (!singleplayer)
    {
        return false;
    }
    crl_freeze ^= 1;
    return true;
}

static boolean CRL_NoTarget (int choice)
{
    if (!singleplayer)
    {
        return false;
    }

    player->cheats ^= CF_NOTARGET;
    return true;
}

static boolean CRL_NoMomentum (int choice)
{
    if (!singleplayer)
    {
        return false;
    }

    player->cheats ^= CF_NOMOMENTUM;
    return true;
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static MenuItem_t CRLVideoItems[] = {
    {ITT_LRFUNC, "UNCAPPED FRAMERATE",  CRL_UncappedFPS,   0, MENU_NONE},
    {ITT_LRFUNC, "FRAMERATE LIMIT",     CRL_LimitFPS,      0, MENU_NONE},
    {ITT_LRFUNC, "ENABLE VSYNC",        CRL_VSync,         0, MENU_NONE},
    {ITT_LRFUNC, "SHOW FPS COUNTER",    CRL_ShowFPS,       0, MENU_NONE},
    {ITT_LRFUNC, "VISPLANES DRAWING",   CRL_VisplanesDraw, 0, MENU_NONE},
    {ITT_LRFUNC, "HOM EFFECT",          CRL_HOMDraw,       0, MENU_NONE},
    {ITT_LRFUNC, "GAMMA-CORRECTION",    CRL_Gamma,         0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,              0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,              0, MENU_NONE},
    {ITT_LRFUNC, "TEXT CASTS SHADOWS",  CRL_TextShadows,   0, MENU_NONE},
    {ITT_LRFUNC, "GRAPHICAL STARTUP",   CRL_GfxStartup,    0, MENU_NONE},
    {ITT_LRFUNC, "SHOW ENDTEXT SCREEN", CRL_EndText,       0, MENU_NONE}
};

static Menu_t CRLVideo = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLVideo,
    12, CRLVideoItems,
    0,
    true,
    MENU_CRLMAIN
};

static void DrawCRLVideo (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("VIDEO OPTIONS", 20, cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 30,
               M_Item_Glow(0, crl_uncapped_fps ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Framerate limit
    sprintf(str, !crl_uncapped_fps ? "35" :
                 crl_fpslimit ? "%d" : "NONE", crl_fpslimit);
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 40,
               M_Item_Glow(1, crl_uncapped_fps ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Enable vsync
    sprintf(str, crl_vsync ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 50,
               M_Item_Glow(2, crl_vsync ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Show FPS counter
    sprintf(str, crl_showfps ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 60,
               M_Item_Glow(3, crl_showfps ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Visplanes drawing
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 70,
               M_Item_Glow(4, crl_visplanes_drawing ? GLOW_GREEN : GLOW_RED, ITEMONTICS));
    
    // HOM effect
    sprintf(str, crl_hom_effect == 0 ? "OFF" :
                 crl_hom_effect == 1 ? "MULTICOLOR" : "BLACK");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 80,
               M_Item_Glow(5, crl_hom_effect ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Gamma-correction slider and num
    DrawSlider(&CRLVideo, 7, 8, crl_gamma/2, false);
    MN_DrTextA(crl_gamma ==  0 ? "0.50" :
               crl_gamma ==  1 ? "0.55" :
               crl_gamma ==  2 ? "0.60" :
               crl_gamma ==  3 ? "0.65" :
               crl_gamma ==  4 ? "0.70" :
               crl_gamma ==  5 ? "0.75" :
               crl_gamma ==  6 ? "0.80" :
               crl_gamma ==  7 ? "0.85" :
               crl_gamma ==  8 ? "0.90" :
               crl_gamma ==  9 ? "0.95" :
               crl_gamma == 10 ? "OFF"  :
               crl_gamma == 11 ? "1"    :
               crl_gamma == 12 ? "2"    :
               crl_gamma == 13 ? "3"    : "4",
               164, 105, M_Item_Glow(6, GLOW_UNCOLORED, ITEMONTICS));

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 120,
               M_Item_Glow(9, crl_text_shadows ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Graphical startup
    sprintf(str, graphical_startup ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 130,
               M_Item_Glow(10, graphical_startup ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Show ENDTEXT screen
    sprintf(str, show_endoom ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 140,
               M_Item_Glow(11, show_endoom ? GLOW_GREEN : GLOW_RED, ITEMONTICS));
}

static boolean CRL_UncappedFPS (int option)
{
    crl_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
    return true;
}

static boolean CRL_LimitFPS (int option)
{
    if (!crl_uncapped_fps)
    {
        return false;  // Do not allow change value in capped framerate.
    }
    
    switch (option)
    {
        case 0:
            if (crl_fpslimit)
                crl_fpslimit--;

            if (crl_fpslimit < TICRATE)
                crl_fpslimit = 0;

            break;
        case 1:
            if (crl_fpslimit < 501)
                crl_fpslimit++;

            if (crl_fpslimit < TICRATE)
                crl_fpslimit = TICRATE;

        default:
            break;
    }
    return true;
}

static boolean CRL_VSync (int option)
{
    crl_vsync ^= 1;
    I_ToggleVsync();
    return true;
}

static boolean CRL_ShowFPS (int option)
{
    crl_showfps ^= 1;
    return true;
}

static boolean CRL_VisplanesDraw (int option)
{
    crl_visplanes_drawing = M_INT_Slider(crl_visplanes_drawing, 0, 4, option);
    return true;
}

static boolean CRL_HOMDraw (int option)
{
    crl_hom_effect = M_INT_Slider(crl_hom_effect, 0, 2, option);
    return true;
}

static boolean CRL_Gamma (int option)
{
    switch (option)
    {
        case 0:
            if (crl_gamma)
                crl_gamma--;
            break;
        case 1:
            if (crl_gamma < 14)
                crl_gamma++;
        default:
            break;
    }
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE), false); // [JN] TODO - colorblind
    return true;
}

static boolean CRL_TextShadows (int option)
{
    crl_text_shadows ^= 1;
    return true;
}

static boolean CRL_GfxStartup (int option)
{
    graphical_startup ^= 1;
    return true;
}

static boolean CRL_EndText (int option)
{
    show_endoom ^= 1;
    return true;
}

// -----------------------------------------------------------------------------
// Sound options
// -----------------------------------------------------------------------------

static MenuItem_t CRLSoundItems[] = {
    {ITT_LRFUNC, "SFX VOLUME",           SCSfxVolume,        MENU_NONE},
    {ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE},
    {ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE},
    {ITT_LRFUNC, "MUSIC VOLUME",         SCMusicVolume,      MENU_NONE},
    {ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE},
    {ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE},
    {ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE},
    {ITT_LRFUNC, "MUSIC PLAYBACK",       CRL_MusicSystem, 0, MENU_NONE},
    {ITT_LRFUNC,  "SOUNDS EFFECTS MODE", CRL_SFXMode,     0, MENU_NONE},
    {ITT_LRFUNC, "PITCH-SHIFTED SOUNDS", CRL_PitchShift,  0, MENU_NONE},
    {ITT_LRFUNC, "NUMBER OF SFX TO MIX", CRL_SFXChannels, 0, MENU_NONE}
};

static Menu_t CRLSound = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLSound,
    11, CRLSoundItems,
    0,
    true,
    MENU_CRLMAIN
};

static void DrawCRLSound (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("SOUND OPTIONS", 20, cr[CR_YELLOW]);

    DrawSlider(&CRLSound, 1, 16, snd_MaxVolume, false);

    DrawSlider(&CRLSound, 4, 16, snd_MusicVolume, false);

    MN_DrTextACentered("SOUND SYSTEM", 90, cr[CR_YELLOW]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ?  "GUS (EMULATED)" :
                 snd_musicdevice == 8 ?  "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                         "UNKNOWN");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 100,
               M_Item_Glow(7, snd_musicdevice ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Sound effects mode
    sprintf(str, crl_monosfx ? "MONO" : "STEREO");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 110,
               M_Item_Glow(8, crl_monosfx ? GLOW_RED : GLOW_GREEN, ITEMONTICS));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 120,
               M_Item_Glow(9, snd_pitchshift ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Number of SFX to mix
    sprintf(str, "%i", snd_Channels);
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 130,
               M_Item_Glow(10, snd_Channels == 8 ? GLOW_GREEN :
                               snd_Channels == 1 ? GLOW_RED : GLOW_DARKGREEN, ITEMONTICS));

    // Inform that music system is not hot-swappable. :(
    if (CurrentItPos == 7)
    {
        MN_DrTextACentered("CHANGE WILL REQUIRE RESTART OF THE PROGRAM", 142, cr[CR_GREEN]);
    }
}

static boolean CRL_MusicSystem (int option)
{
    switch (option)
    {
        case 0:
            if (snd_musicdevice == 0)
            {
                snd_musicdevice = 5;    // Set to GUS
            }
            else if (snd_musicdevice == 5)
#ifdef HAVE_FLUIDSYNTH
            {
                snd_musicdevice = 11;    // Set to FluidSynth
            }
            else if (snd_musicdevice == 11)
#endif // HAVE_FLUIDSYNTH
            {
                snd_musicdevice = 8;    // Set to Native MIDI
            }
            else if (snd_musicdevice == 8)
            {
                snd_musicdevice = 3;    // Set to OPL3
                snd_dmxoption = "-opl3";
            }
            else if (snd_musicdevice == 3  && !strcmp(snd_dmxoption, "-opl3"))
            {
                snd_musicdevice = 3;    // Set to OPL2
                snd_dmxoption = "";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, ""))
            {
                snd_musicdevice = 0;    // Disable
            }
            break;
        case 1:
            if (snd_musicdevice == 0)
            {
                snd_musicdevice  = 3;   // Set to OPL2
                snd_dmxoption = "";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, ""))
            {
                snd_musicdevice  = 3;   // Set to OPL3
                snd_dmxoption = "-opl3";
            }
            else if (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3"))
            {
                snd_musicdevice  = 8;   // Set to Native MIDI
            }
            else if (snd_musicdevice == 8)
#ifdef HAVE_FLUIDSYNTH
            {
                snd_musicdevice  = 11;   // Set to FluidSynth
            }
            else if (snd_musicdevice == 11)
#endif // HAVE_FLUIDSYNTH
            {
                snd_musicdevice  = 5;   // Set to GUS
            }
            else if (snd_musicdevice == 5)
            {
                snd_musicdevice  = 0;   // Disable
            }
            break;
        default:
            {
                break;
            }
    }
    return true;
}

static boolean CRL_SFXMode (int option)
{
    crl_monosfx ^= 1;
    return true;
}

static boolean CRL_PitchShift (int option)
{
    snd_pitchshift ^= 1;
    return true;
}

static boolean CRL_SFXChannels (int option)
{
    switch (option)
    {
        case 0:
            if (snd_Channels > 1)
                snd_Channels--;
            break;
        case 1:
            if (snd_Channels < 16)
                snd_Channels++;
        default:
            break;
    }
    return true;
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static MenuItem_t CRLControlsItems[] = {
    {ITT_SETMENU, "KEYBOARD BINDINGS",       NULL,         0, MENU_CRLKBDBINDS1},
    {ITT_SETMENU, "MOUSE BINDINGS",          NULL,         0, MENU_CRLMOUSEBINDS},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_LRFUNC,  "SENSIVITY",               SCMouseSensi, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_LRFUNC,  "ACCELERATION",            CRL_Controls_Acceleration, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_LRFUNC,  "ACCELERATION THRESHOLD",  CRL_Controls_Threshold, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,         0, MENU_NONE},
    {ITT_LRFUNC,  "MOUSE LOOK",              CRL_Controls_MLook, 0, MENU_NONE},
    {ITT_LRFUNC,  "VERTICAL MOUSE MOVEMENT", CRL_Controls_NoVert, 0, MENU_NONE}
};

static Menu_t CRLControls = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLControls,
    14, CRLControlsItems,
    0,
    true,
    MENU_CRLMAIN
};

static void DrawCRLControls (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("BINDINGS", 20, cr[CR_YELLOW]);

    MN_DrTextACentered("MOUSE CONFIGURATION", 50, cr[CR_YELLOW]);

    DrawSlider(&CRLControls, 4, 10, mouseSensitivity, false);
    DrawSlider(&CRLControls, 7, 12, mouse_acceleration * 2, false);
    DrawSlider(&CRLControls, 10, 14, mouse_threshold / 2, false);

    // Mouse look
    sprintf(str, crl_mouselook ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 150,
               M_Item_Glow(12, crl_mouselook ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Vertical mouse movement
    sprintf(str, novert ? "OFF" : "ON");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 160,
               M_Item_Glow(13, novert ? GLOW_RED : GLOW_GREEN, ITEMONTICS));
}

static boolean CRL_Controls_Acceleration (int option)
{
    char buf[9];

    switch (option)
    {   // 1.0 ... 5.0
        case 0:
        if (mouse_acceleration > 1.0f)
            mouse_acceleration -= 0.1f;
        break;

        case 1:
        if (mouse_acceleration < 5.0f)
            mouse_acceleration += 0.1f;
        break;
    }

    // [JN] Do a float correction to always get x.x00000 values:
    sprintf (buf, "%f", mouse_acceleration);
    mouse_acceleration = (float) atof(buf);
    return true;
}

static boolean CRL_Controls_Threshold (int option)
{
    switch (option)
    {   // 0 ... 32
        case 0:
        if (mouse_threshold)
            mouse_threshold--;
        break;
        case 1:
        if (mouse_threshold < 32)
            mouse_threshold++;
        break;
    }
    return true;
}

static boolean CRL_Controls_MLook (int option)
{
    crl_mouselook ^= 1;
    if (!crl_mouselook)
    {
        players[consoleplayer].centering = true;
    }
    return true;
}

static boolean CRL_Controls_NoVert (int option)
{
    novert ^= 1;
    return true;
}

// -----------------------------------------------------------------------------
// Keyboard bindings 1
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds1Items[] = {
    {ITT_EFUNC, "MOVE FORWARD",  NULL, 0, MENU_NONE},
    {ITT_EFUNC, "MOVE BACKWARD", NULL, 0, MENU_NONE},
    {ITT_EFUNC, "TURN LEFT",     NULL, 0, MENU_NONE},
    {ITT_EFUNC, "TURN RIGHT",    NULL, 0, MENU_NONE},
    {ITT_EFUNC, "STRAFE LEFT",   NULL, 0, MENU_NONE},
    {ITT_EFUNC, "STRAFE RIGHT",  NULL, 0, MENU_NONE},
    {ITT_EFUNC, "SPEED ON",      NULL, 0, MENU_NONE},
    {ITT_EFUNC, "STRAFE ON",     NULL, 0, MENU_NONE},
    {ITT_EMPTY, NULL,            NULL, 0, MENU_NONE},
    {ITT_EFUNC, "FIRE/ATTACK",   NULL, 0, MENU_NONE},
    {ITT_EFUNC, "USE",           NULL, 0, MENU_NONE}
};

static Menu_t CRLKbsBinds1 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd1,
    11, CRLKbsBinds1Items,
    0,
    true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd1 (void)
{
    // static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("MOVEMENT", 20, cr[CR_YELLOW]);
    
    MN_DrTextACentered("ACTION", 110, cr[CR_YELLOW]);
    
    MN_DrTextACentered("PRESS ENTER TO BIND, DEL TO CLEAR", 140, cr[CR_GRAY]);
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static MenuItem_t CRLMouseItems[] = {
    {ITT_EFUNC, "FIRE/ATTACK",   M_Bind_M_FireAttack,   0, MENU_NONE},
    {ITT_EFUNC, "MOVE FORWARD",  M_Bind_M_MoveForward,  0, MENU_NONE},
    {ITT_EFUNC, "STRAFE ON",     M_Bind_M_StrafeOn,     0, MENU_NONE},
    {ITT_EFUNC, "MOVE BACKWARD", M_Bind_M_MoveBackward, 0, MENU_NONE},
    {ITT_EFUNC, "USE",           M_Bind_M_Use,          0, MENU_NONE},
    {ITT_EFUNC, "STRAFE LEFT",   M_Bind_M_StrafeLeft,   0, MENU_NONE},
    {ITT_EFUNC, "STRAFE RIGHT",  M_Bind_M_StrafeRight,  0, MENU_NONE},
    {ITT_EFUNC, "PREV WEAPON",   M_Bind_M_PrevWeapon,   0, MENU_NONE},
    {ITT_EFUNC, "NEXT WEAPON",   M_Bind_M_NextWeapon,   0, MENU_NONE}
};

static Menu_t CRLMouseBinds = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLMouse,
    9, CRLMouseItems,
    0,
    true,
    MENU_CRLCONTROLS
};

static void DrawCRLMouse (void)
{
    M_ShadeBackground();

    MN_DrTextACentered("MOUSE BINDINGS", 20, cr[CR_YELLOW]);

    MN_DrTextA(M_MouseBtnDrawer(0, mousebfire), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(0, mousebfire)), 30, M_ColorizeMouseBind(0, mousebfire));
    MN_DrTextA(M_MouseBtnDrawer(1, mousebforward), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(1, mousebforward)), 40, M_ColorizeMouseBind(1, mousebforward));
    MN_DrTextA(M_MouseBtnDrawer(2, mousebstrafe), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(2, mousebstrafe)), 50, M_ColorizeMouseBind(2, mousebstrafe));
    MN_DrTextA(M_MouseBtnDrawer(3, mousebbackward), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(3, mousebbackward)), 60, M_ColorizeMouseBind(3, mousebbackward));
    MN_DrTextA(M_MouseBtnDrawer(4, mousebuse), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(4, mousebuse)), 70, M_ColorizeMouseBind(4, mousebuse));
    MN_DrTextA(M_MouseBtnDrawer(5, mousebstrafeleft), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(5, mousebstrafeleft)), 80, M_ColorizeMouseBind(5, mousebstrafeleft));
    MN_DrTextA(M_MouseBtnDrawer(6, mousebstraferight), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(6, mousebstraferight)), 90, M_ColorizeMouseBind(6, mousebstraferight));
    MN_DrTextA(M_MouseBtnDrawer(7, mousebprevweapon), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(7, mousebprevweapon)), 100, M_ColorizeMouseBind(7, mousebprevweapon));
    MN_DrTextA(M_MouseBtnDrawer(8, mousebnextweapon), CRL_MENU_RIGHTOFFSET - MN_TextAWidth(M_MouseBtnDrawer(8, mousebnextweapon)), 110, M_ColorizeMouseBind(8, mousebnextweapon));

    MN_DrTextACentered("PRESS ENTER TO BIND, DEL TO CLEAR", 130, cr[CR_GRAY]);
}

static boolean M_Bind_M_FireAttack (int option)
{
    M_StartMouseBind(1000);  // mousebfire
    return true;
}

static boolean M_Bind_M_MoveForward (int option)
{
    M_StartMouseBind(1001);  // mousebforward
    return true;
}

static boolean M_Bind_M_StrafeOn (int option)
{
    M_StartMouseBind(1002);  // mousebstrafe
    return true;
}

static boolean M_Bind_M_MoveBackward (int option)
{
    M_StartMouseBind(1003);  // mousebbackward
    return true;
}

static boolean M_Bind_M_Use (int option)
{
    M_StartMouseBind(1004);  // mousebuse
    return true;
}

static boolean M_Bind_M_StrafeLeft (int option)
{
    M_StartMouseBind(1005);  // mousebstrafeleft
    return true;
}

static boolean M_Bind_M_StrafeRight (int option)
{
    M_StartMouseBind(1006);  // mousebstraferight
    return true;
}

static boolean M_Bind_M_PrevWeapon (int option)
{
    M_StartMouseBind(1007);  // mousebprevweapon
    return true;
}

static boolean M_Bind_M_NextWeapon (int option)
{
    M_StartMouseBind(1008);  // mousebnextweapon
    return true;
}

// -----------------------------------------------------------------------------
// Widgets and Automap
// -----------------------------------------------------------------------------

static MenuItem_t CRLWidgetsItems[] = {
    {ITT_LRFUNC, "PLAYER COORDS",       CRL_Widget_Coords,    0, MENU_NONE},
    {ITT_LRFUNC, "PLAYSTATE COUNTERS",  CRL_Widget_Playstate, 0, MENU_NONE},
    {ITT_LRFUNC, "RENDER COUNTERS",     CRL_Widget_Render,    0, MENU_NONE},
    {ITT_LRFUNC, "KIS STATS",           CRL_Widget_KIS,       0, MENU_NONE},
    {ITT_LRFUNC, "LEVEL TIME",          CRL_Widget_Time,      0, MENU_NONE},
    {ITT_LRFUNC, "POWERUP TIMERS",      CRL_Widget_Powerups,  0, MENU_NONE},
    {ITT_LRFUNC, "TARGET'S HEALTH",     CRL_Widget_Health,    0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE},
    {ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE}
};

static Menu_t CRLWidgetsMap = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLWidgets,
    12, CRLWidgetsItems,
    0,
    true,
    MENU_CRLMAIN
};

static void DrawCRLWidgets (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("WIDGETS", 20, cr[CR_YELLOW]);

    // Player coords
    sprintf(str, crl_widget_coords ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 30,
               M_Item_Glow(0, crl_widget_coords ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 40,
               M_Item_Glow(1, crl_widget_playstate == 1 ? GLOW_GREEN :
                              crl_widget_playstate == 2 ? GLOW_DARKGREEN : GLOW_RED, ITEMONTICS));

    // Render counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 50,
               M_Item_Glow(2, crl_widget_render == 1 ? GLOW_GREEN :
                              crl_widget_render == 2 ? GLOW_DARKGREEN : GLOW_RED, ITEMONTICS));

    // K/I/S stats
    sprintf(str, crl_widget_kis ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 60,
               M_Item_Glow(3, crl_widget_kis ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Level time
    sprintf(str, crl_widget_time ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 70,
               M_Item_Glow(4, crl_widget_time ? GLOW_GREEN : GLOW_RED, ITEMONTICS));
}

static boolean CRL_Widget_Coords (int option)
{
    crl_widget_coords ^= 1;
    return true;
}

static boolean CRL_Widget_Playstate (int option)
{
    crl_widget_playstate = M_INT_Slider(crl_widget_playstate, 0, 2, option);
    return true;
}

static boolean CRL_Widget_Render (int option)
{
    crl_widget_render = M_INT_Slider(crl_widget_render, 0, 2, option);
    return true;
}

static boolean CRL_Widget_KIS (int option)
{
    crl_widget_kis ^= 1;
    return true;
}

static boolean CRL_Widget_Time (int option)
{
    crl_widget_time ^= 1;
    return true;
}

static boolean CRL_Widget_Powerups (int option)
{
    crl_widget_powerups ^= 1;
    return true;
}

static boolean CRL_Widget_Health (int option)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 4, option);
    return true;
}

// -----------------------------------------------------------------------------
// Static engine limits
// -----------------------------------------------------------------------------

static MenuItem_t CRLLimitsItems[] = {
    {ITT_LRFUNC, "PREVENT Z[MALLOC ERRORS",    CRL_ZMalloc,         0, MENU_NONE},
    {ITT_LRFUNC, "SAVE GAME LIMIT WARNING",    CRL_SaveSizeWarning, 0, MENU_NONE},
    {ITT_LRFUNC, "DEMO LENGHT LIMIT WARNING",  CRL_DemoSizeWarning, 0, MENU_NONE},
    {ITT_LRFUNC, "RENDER LIMITS LEVEL",        CRL_Limits,          0, MENU_NONE}
};

static Menu_t CRLLimits = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLLimits,
    4, CRLLimitsItems,
    0,
    true,
    MENU_CRLMAIN
};

static void DrawCRLLimits (void)
{
    static char str[32];

    M_ShadeBackground();

    MN_DrTextACentered("STATIC ENGINE LIMITS", 20, cr[CR_YELLOW]);

    // Prevent Z_Malloc errors
    sprintf(str, crl_prevent_zmalloc ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 30,
               M_Item_Glow(0, crl_prevent_zmalloc ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Save game limit warning
    sprintf(str, vanilla_savegame_limit ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 40,
               M_Item_Glow(1, vanilla_savegame_limit ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Demo lenght limit warning
    sprintf(str, vanilla_demo_limit ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 50,
               M_Item_Glow(2, vanilla_demo_limit ? GLOW_GREEN : GLOW_RED, ITEMONTICS));

    // Level of the limits
    sprintf(str, crl_vanilla_limits ? "VANILLA" : "HERETIC-PLUS");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET - MN_TextAWidth(str), 60,
               M_Item_Glow(3, crl_vanilla_limits ? GLOW_RED : GLOW_GREEN, ITEMONTICS));

    MN_DrTextA("MAXVISPLANES", CRL_MENU_LEFTOFFSET_SML + 16, 80, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXDRAWSEGS", CRL_MENU_LEFTOFFSET_SML + 16, 90, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXVISSPRITES", CRL_MENU_LEFTOFFSET_SML + 16, 100, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXOPENINGS", CRL_MENU_LEFTOFFSET_SML + 16, 110, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXPLATS", CRL_MENU_LEFTOFFSET_SML + 16, 120, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXLINEANIMS", CRL_MENU_LEFTOFFSET_SML + 16, 130, cr[CR_MENU_DARK2]);

    if (crl_vanilla_limits)
    {
        MN_DrTextA("128", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"), 80, cr[CR_RED]);
        MN_DrTextA("256", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("256"), 90, cr[CR_RED]);
        MN_DrTextA("128", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"), 100, cr[CR_RED]);
        MN_DrTextA("20480", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("20480"), 110, cr[CR_RED]);
        MN_DrTextA("30", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("30"), 120, cr[CR_RED]);
        MN_DrTextA("64", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("64"), 130, cr[CR_RED]);
    }
    else
    {
        MN_DrTextA("1024", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"), 80, cr[CR_GREEN]);
        MN_DrTextA("2048", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("2048"), 90, cr[CR_GREEN]);
        MN_DrTextA("1024", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"), 100, cr[CR_GREEN]);
        MN_DrTextA("65536", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("65536"), 110, cr[CR_GREEN]);
        MN_DrTextA("7680", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("7680"), 120, cr[CR_GREEN]);
        MN_DrTextA("16384", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("16384"), 130, cr[CR_GREEN]);
    }
}

static boolean CRL_ZMalloc (int option)
{
    crl_prevent_zmalloc ^= 1;
    return true;
}

static boolean CRL_SaveSizeWarning (int option)
{
    vanilla_savegame_limit ^= 1;
    return true;
}

static boolean CRL_DemoSizeWarning (int option)
{
    vanilla_demo_limit ^= 1;
    return true;
}

static boolean CRL_Limits (int option)
{
    crl_vanilla_limits ^= 1;

    // [JN] CRL - re-define static engine limits.
    CRL_SetStaticLimits("HERETIC+");
    return true;
}



static Menu_t *Menus[] = {
    &MainMenu,
    &EpisodeMenu,
    &SkillMenu,
    &OptionsMenu,
    &Options2Menu,
    &FilesMenu,
    &LoadMenu,
    &SaveMenu,
    // [JN] CRL menu
    &CRLMain,
    &CRLVideo,
    &CRLSound,
    &CRLControls,
    &CRLKbsBinds1, // pitto
    &CRLMouseBinds,
    &CRLWidgetsMap,
    // &CRLGameplay,
    &CRLLimits,
};



//---------------------------------------------------------------------------
//
// PROC MN_Init
//
//---------------------------------------------------------------------------

void MN_Init(void)
{
    InitFonts();
    MenuActive = false;
    messageson = true;
    SkullBaseLump = W_GetNumForName(DEH_String("M_SKL00"));

    // [JN] CRL - player is always local, "console" player.
    player = &players[consoleplayer];

    if (gamemode == retail)
    {                           // Add episodes 4 and 5 to the menu
        EpisodeMenu.itemCount = 5;
        EpisodeMenu.y -= ITEM_HEIGHT;
    }
}

//---------------------------------------------------------------------------
//
// PROC InitFonts
//
//---------------------------------------------------------------------------

static void InitFonts(void)
{
    FontABaseLump = W_GetNumForName(DEH_String("FONTA_S")) + 1;
    FontBBaseLump = W_GetNumForName(DEH_String("FONTB_S")) + 1;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextA
//
// Draw text using font A.
//
//---------------------------------------------------------------------------

void MN_DrTextA (const char *text, int x, int y, byte *table)
{
    char c;
    patch_t *p;

    dp_translation = table;

    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            x += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchRavenOptional(x, y, p, "NULL"); // [JN] TODO - patch name
            x += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextAWidth
//
// Returns the pixel width of a string using font A.
//
//---------------------------------------------------------------------------

int MN_TextAWidth(const char *text)
{
    char c;
    int width;
    patch_t *p;

    width = 0;
    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            width += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            width += SHORT(p->width) - 1;
        }
    }
    return (width);
}

void MN_DrTextACentered (const char *text, int y, byte *table)
{
    char c;
    int cx;
    patch_t *p;

    cx = 160 - MN_TextAWidth(text) / 2;
    
    dp_translation = table;

    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            cx += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchRavenOptional(cx, y, p, "NULL"); // [JN] TODO - patch name
            cx += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// M_WriteTextCritical
// [JN] Write a two line strings.
// -----------------------------------------------------------------------------

void MN_DrTextACritical (const char *text1, const char *text2, int y, byte *table)
{
    char c;
    int cx1, cx2;
    patch_t *p;

    cx1 = 160 - MN_TextAWidth(text1) / 2;
    cx2 = 160 - MN_TextAWidth(text2) / 2;
    
    dp_translation = table;

    while ((c = *text1++) != 0)
    {
        if (c < 33)
        {
            cx1 += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchRavenOptional(cx1, y, p, "NULL"); // [JN] TODO - patch name
            cx1 += SHORT(p->width) - 1;
        }
    }

    while ((c = *text2++) != 0)
    {
        if (c < 33)
        {
            cx2 += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontABaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchRavenOptional(cx2, y+10, p, "NULL"); // [JN] TODO - patch name
            cx2 += SHORT(p->width) - 1;
        }
    }

    dp_translation = NULL;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrTextB
//
// Draw text using font B.
//
//---------------------------------------------------------------------------

void MN_DrTextB(const char *text, int x, int y)
{
    char c;
    patch_t *p;

    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            x += 8;
        }
        else
        {
            p = W_CacheLumpNum(FontBBaseLump + c - 33, PU_CACHE);
            V_DrawShadowedPatchRavenOptional(x, y, p, "NULL"); // [JN] TODO - patch name
            x += SHORT(p->width) - 1;
        }
    }
}

//---------------------------------------------------------------------------
//
// FUNC MN_TextBWidth
//
// Returns the pixel width of a string using font B.
//
//---------------------------------------------------------------------------

int MN_TextBWidth(const char *text)
{
    char c;
    int width;
    patch_t *p;

    width = 0;
    while ((c = *text++) != 0)
    {
        if (c < 33)
        {
            width += 5;
        }
        else
        {
            p = W_CacheLumpNum(FontBBaseLump + c - 33, PU_CACHE);
            width += SHORT(p->width) - 1;
        }
    }
    return (width);
}

//---------------------------------------------------------------------------
//
// PROC MN_Ticker
//
//---------------------------------------------------------------------------

void MN_Ticker(void)
{
    if (MenuActive == false)
    {
        return;
    }
    MenuTime++;

    // [JN] Menu glowing animation:
    
    if (!cursor_direction && ++cursor_tics == 8)
    {
        // Brightening
        cursor_direction = true;
    }
    else
    if (cursor_direction && --cursor_tics == -8)
    {
        // Darkening
        cursor_direction = false;
    }

    // [JN] Menu item fading effect:

    if (CurrentMenu->smallFont)
    {
        for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
        {
            if (CurrentItPos == i)
            {
                // Keep menu item bright
                CurrentMenu->items[i].tics = 5;
            }
            else
            {
                // Decrease tics for glowing effect
                CurrentMenu->items[i].tics--;
            }
        }
    }

}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

char *QuitEndMsg[] = {
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED"
};

void MN_Drawer(void)
{
    int i;
    int x;
    int y;
    MenuItem_t *item;
    const char *message;
    const char *selName;

    if (MenuActive == false)
    {
        if (askforquit)
        {
            message = DEH_String(QuitEndMsg[typeofask - 1]);

            MN_DrTextA(message, 160 - MN_TextAWidth(message) / 2, 80, NULL);
            if (typeofask == 3)
            {
                MN_DrTextA(SlotText[quicksave - 1], 160 -
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[quicksave - 1]) / 2, 90, NULL);
            }
            if (typeofask == 4)
            {
                MN_DrTextA(SlotText[quickload - 1], 160 -
                           MN_TextAWidth(SlotText[quickload - 1]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[quickload - 1]) / 2, 90, NULL);
            }
            UpdateState |= I_FULLSCRN;
        }
        return;
    }
    else
    {
        UpdateState |= I_FULLSCRN;
        if (InfoType)
        {
            MN_DrawInfo();
            return;
        }
        if (screenblocks < 10)
        {
            BorderNeedRefresh = true;
        }
        if (CurrentMenu->drawFunc != NULL)
        {
            CurrentMenu->drawFunc();
        }
        x = CurrentMenu->x;
        y = CurrentMenu->y;
        item = CurrentMenu->items;
        for (i = 0; i < CurrentMenu->itemCount; i++)
        {
            if (CurrentMenu->smallFont)
            {
                if (item->type != ITT_EMPTY && item->text)
                {
                    if (CurrentItPos == i)
                    {
                        // [JN] Highlight menu item on which the cursor is positioned.
                        MN_DrTextA(DEH_String(item->text), x, y, cr[CR_MENU_BRIGHT2]);
                    }
                    else
                    {
                        // [JN] Apply fading effect in MN_Ticker.
                        MN_DrTextA(DEH_String(item->text), x, y, M_Line_Glow(CurrentMenu->items[i].tics));
                    }
                }
                y += ITEM_HEIGHT_SMALL;
            }
            else
            {
                if (item->type != ITT_EMPTY && item->text)
                {
                    MN_DrTextB(DEH_String(item->text), x, y);
                }
                y += ITEM_HEIGHT;
            }
            item++;
        }
        
        if (CurrentMenu->smallFont)
        {
            y = CurrentMenu->y + (CurrentItPos * ITEM_HEIGHT_SMALL);
            MN_DrTextA("*", x + SELECTOR_XOFFSET_SMALL, y, M_Cursor_Glow(cursor_tics));
        }
        else
        {
            y = CurrentMenu->y + (CurrentItPos * ITEM_HEIGHT) + SELECTOR_YOFFSET;
            selName = DEH_String(MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2");
            V_DrawPatch(x + SELECTOR_XOFFSET, y,
                        W_CacheLumpName(selName, PU_CACHE), selName);
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMainMenu
//
//---------------------------------------------------------------------------

static void DrawMainMenu(void)
{
    int frame;

    frame = (MenuTime / 3) % 18;
    V_DrawPatch(88, 0, W_CacheLumpName(DEH_String("M_HTIC"), PU_CACHE), "M_HTIC");
    V_DrawPatch(40, 10, W_CacheLumpNum(SkullBaseLump + (17 - frame),
                                       PU_CACHE), "NULL"); // [JN] TODO - patch name
    V_DrawPatch(232, 10, W_CacheLumpNum(SkullBaseLump + frame, PU_CACHE), "NULL"); // [JN] TODO - patch name
}

//---------------------------------------------------------------------------
//
// PROC DrawEpisodeMenu
//
//---------------------------------------------------------------------------

static void DrawEpisodeMenu(void)
{
}

//---------------------------------------------------------------------------
//
// PROC DrawSkillMenu
//
//---------------------------------------------------------------------------

static void DrawSkillMenu(void)
{
}

//---------------------------------------------------------------------------
//
// PROC DrawFilesMenu
//
//---------------------------------------------------------------------------

static void DrawFilesMenu(void)
{
// clear out the quicksave/quickload stuff
    quicksave = 0;
    quickload = 0;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageTics = 1;
}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

static void DrawLoadMenu(void)
{
    const char *title;

    title = DEH_String("LOAD GAME");

    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 10);
    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&LoadMenu);
}

//---------------------------------------------------------------------------
//
// PROC DrawSaveMenu
//
//---------------------------------------------------------------------------

static void DrawSaveMenu(void)
{
    const char *title;

    title = DEH_String("SAVE GAME");

    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 10);
    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&SaveMenu);
}

//===========================================================================
//
// MN_LoadSlotText
//
//              Loads in the text message for each slot
//===========================================================================

void MN_LoadSlotText(void)
{
    FILE *fp;
    int i;
    char *filename;

    for (i = 0; i < 6; i++)
    {
        filename = SV_Filename(i);
        fp = fopen(filename, "rb+");
	free(filename);

        if (!fp)
        {
            SlotText[i][0] = 0; // empty the string
            SlotStatus[i] = 0;
            continue;
        }
        fread(&SlotText[i], SLOTTEXTLEN, 1, fp);
        fclose(fp);
        SlotStatus[i] = 1;
    }
    slottextloaded = true;
}

//---------------------------------------------------------------------------
//
// PROC DrawFileSlots
//
//---------------------------------------------------------------------------

static void DrawFileSlots(Menu_t * menu)
{
    int i;
    int x;
    int y;

    x = menu->x;
    y = menu->y;
    for (i = 0; i < 6; i++)
    {
        V_DrawPatch(x, y, W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE), "M_FSLOT");
        if (SlotStatus[i])
        {
            MN_DrTextA(SlotText[i], x + 5, y + 5, NULL);
        }
        y += ITEM_HEIGHT;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawOptionsMenu
//
//---------------------------------------------------------------------------

static void DrawOptionsMenu(void)
{
    if (messageson)
    {
        MN_DrTextB(DEH_String("ON"), 196, 50);
    }
    else
    {
        MN_DrTextB(DEH_String("OFF"), 196, 50);
    }
    DrawSlider(&OptionsMenu, 3, 10, mouseSensitivity, true);
}

//---------------------------------------------------------------------------
//
// PROC DrawOptions2Menu
//
//---------------------------------------------------------------------------

static void DrawOptions2Menu(void)
{
    DrawSlider(&Options2Menu, 1, 9, screenblocks - 3, true);
    DrawSlider(&Options2Menu, 3, 16, snd_MaxVolume, true);
    DrawSlider(&Options2Menu, 5, 16, snd_MusicVolume, true);
}

//---------------------------------------------------------------------------
//
// PROC SCNetCheck
//
//---------------------------------------------------------------------------

static boolean SCNetCheck(int option)
{
    if (!netgame)
    {                           // okay to go into the menu
        return true;
    }
    switch (option)
    {
        case 1:
            P_SetMessage(&players[consoleplayer],
                         "YOU CAN'T START A NEW GAME IN NETPLAY!", true);
            break;
        case 2:
            P_SetMessage(&players[consoleplayer],
                         "YOU CAN'T LOAD A GAME IN NETPLAY!", true);
            break;
        default:
            break;
    }
    MenuActive = false;
    return false;
}

//---------------------------------------------------------------------------
//
// PROC SCQuitGame
//
//---------------------------------------------------------------------------

static boolean SCQuitGame(int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 1;              //quit game
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCEndGame
//
//---------------------------------------------------------------------------

static boolean SCEndGame(int option)
{
    if (demoplayback || netgame)
    {
        return false;
    }
    MenuActive = false;
    askforquit = true;
    typeofask = 2;              //endgame
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMessages
//
//---------------------------------------------------------------------------

static boolean SCMessages(int option)
{
    messageson ^= 1;
    if (messageson)
    {
        P_SetMessage(&players[consoleplayer], DEH_String("MESSAGES ON"), true);
    }
    else
    {
        P_SetMessage(&players[consoleplayer], DEH_String("MESSAGES OFF"), true);
    }
    S_StartSound(NULL, sfx_chat);
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static boolean SCLoadGame(int option)
{
    char *filename;

    if (!SlotStatus[option])
    {                           // slot's empty...don't try and load
        return false;
    }

    filename = SV_Filename(option);
    G_LoadGame(filename);
    free(filename);

    MN_DeactivateMenu();
    BorderNeedRefresh = true;
    if (quickload == -1)
    {
        quickload = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

static boolean SCSaveGame(int option)
{
    char *ptr;

    if (!FileMenuKeySteal)
    {
        int x, y;

        FileMenuKeySteal = true;
        // We need to activate the text input interface to type the save
        // game name:
        x = SaveMenu.x + 1;
        y = SaveMenu.y + 1 + option * ITEM_HEIGHT;
        I_StartTextInput(x, y, x + 190, y + ITEM_HEIGHT - 2);

        M_StringCopy(oldSlotText, SlotText[option], sizeof(oldSlotText));
        ptr = SlotText[option];
        while (*ptr)
        {
            ptr++;
        }
        *ptr = '[';
        *(ptr + 1) = 0;
        SlotStatus[option]++;
        currentSlot = option;
        slotptr = ptr - SlotText[option];
        return false;
    }
    else
    {
        G_SaveGame(option, SlotText[option]);
        FileMenuKeySteal = false;
        I_StopTextInput();
        MN_DeactivateMenu();
    }
    BorderNeedRefresh = true;
    if (quicksave == -1)
    {
        quicksave = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCEpisode
//
//---------------------------------------------------------------------------

static boolean SCEpisode(int option)
{
    if (gamemode == shareware && option > 1)
    {
        P_SetMessage(&players[consoleplayer],
                     "ONLY AVAILABLE IN THE REGISTERED VERSION", true);
    }
    else
    {
        MenuEpisode = option;
        SetMenu(MENU_SKILL);
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static boolean SCSkill(int option)
{
    G_DeferedInitNew(option, MenuEpisode, 1);
    MN_DeactivateMenu();
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMouseSensi
//
//---------------------------------------------------------------------------

static boolean SCMouseSensi(int option)
{
    if (option == RIGHT_DIR)
    {
        if (mouseSensitivity < 9)
        {
            mouseSensitivity++;
        }
    }
    else if (mouseSensitivity)
    {
        mouseSensitivity--;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCSfxVolume
//
//---------------------------------------------------------------------------

static boolean SCSfxVolume(int option)
{
    if (option == RIGHT_DIR)
    {
        if (snd_MaxVolume < 15)
        {
            snd_MaxVolume++;
        }
    }
    else if (snd_MaxVolume)
    {
        snd_MaxVolume--;
    }
    S_SetMaxVolume(false);      // don't recalc the sound curve, yet
    soundchanged = true;        // we'll set it when we leave the menu
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static boolean SCMusicVolume(int option)
{
    if (option == RIGHT_DIR)
    {
        if (snd_MusicVolume < 15)
        {
            snd_MusicVolume++;
        }
    }
    else if (snd_MusicVolume)
    {
        snd_MusicVolume--;
    }
    S_SetMusicVolume();
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static boolean SCScreenSize(int option)
{
    if (option == RIGHT_DIR)
    {
        if (screenblocks < 11)
        {
            screenblocks++;
        }
    }
    else if (screenblocks > 3)
    {
        screenblocks--;
    }
    R_SetViewSize(screenblocks, detailLevel);
    return true;
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static boolean SCInfo(int option)
{
    InfoType = 1;
    S_StartSound(NULL, sfx_dorcls);
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    return true;
}

//---------------------------------------------------------------------------
//
// FUNC MN_Responder
//
//---------------------------------------------------------------------------

boolean MN_Responder(event_t * event)
{
    int charTyped;
    int key;
    int i;
    MenuItem_t *item;
    extern void D_StartTitle(void);
    extern void G_CheckDemoStatus(void);
    char *textBuffer;
    static int mousewait = 0;
    static int mousey = 0;
    static int lasty = 0;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (event->type == ev_quit
         || (event->type == ev_keydown
          && (event->data1 == key_menu_activate
           || event->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (event->type == ev_quit)
    {
        // First click on close = bring up quit confirm message.
        // Second click = confirm quit.

        if (!MenuActive && askforquit && typeofask == 1)
        {
            G_CheckDemoStatus();
            I_Quit();
        }
        else
        {
            SCQuitGame(0);
            S_StartSound(NULL, sfx_chat);
        }
        return true;
    }

    // key is the key pressed, ch is the actual character typed
  
    charTyped = 0;
    key = -1;

    // Allow the menu to be activated from a joystick button if a button
    // is bound for joybmenu.
    if (event->type == ev_joystick)
    {
        if (joybmenu >= 0 && (event->data1 & (1 << joybmenu)) != 0)
        {
            MN_ActivateMenu();
            return true;
        }
    }
    else
    {
        // [JN] Allow menu control by mouse.
        if (event->type == ev_mouse && mousewait < I_GetTime())
        {
            // [crispy] novert disables controlling the menus with the mouse
            if (!novert)
            {
                mousey += event->data3;
            }

            if (mousey < lasty - 30)
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 5;
                mousey = lasty -= 30;
            }
            else if (mousey > lasty + 30)
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 5;
                mousey = lasty += 30;
            }

            // [JN] Handle mouse bindings before going any farther.
            // Catch only button pressing events, i.e. event->data1.
            if (MouseIsBinding && event->data1)
            {
                M_CheckMouseBind(SDL_mouseButton);
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 15;
                return true;
            }

            if (event->data1 & 1)
            {
                key = key_menu_forward;
                mousewait = I_GetTime() + 15;
            }

            if (event->data1 & 2)
            {
                key = key_menu_back;
                mousewait = I_GetTime() + 15;
            }

            // [crispy] scroll menus with mouse wheel
            if (mousebprevweapon >= 0 && event->data1 & (1 << mousebprevweapon))
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 1;
            }
            else
            if (mousebnextweapon >= 0 && event->data1 & (1 << mousebnextweapon))
            {
                key = key_menu_up;
                mousewait = I_GetTime() + 1;
            }
        }
        else
        {
            if (event->type == ev_keydown)
            {
                key = event->data1;
                charTyped = event->data2;
            }
        }
    }

    if (key == -1)
    {
        return false;
    }

    if (InfoType)
    {
        if (gamemode == shareware)
        {
            InfoType = (InfoType + 1) % 5;
        }
        else
        {
            InfoType = (InfoType + 1) % 4;
        }
        if (key == KEY_ESCAPE)
        {
            InfoType = 0;
        }
        if (!InfoType)
        {
            paused = false;
            MN_DeactivateMenu();
            SB_state = -1;      //refresh the statbar
            BorderNeedRefresh = true;
        }
        S_StartSound(NULL, sfx_dorcls);
        return (true);          //make the info screen eat the keypress
    }

    if ((ravpic && key == KEY_F1) ||
        (key != 0 && key == key_menu_screenshot))
    {
        G_ScreenShot();
        return (true);
    }

    if (askforquit)
    {
        if (key == key_menu_confirm)
        {
            switch (typeofask)
            {
                case 1:
                    G_CheckDemoStatus();
                    I_Quit();
                    return false;

                case 2:
                    players[consoleplayer].messageTics = 0;
                    //set the msg to be cleared
                    players[consoleplayer].message = NULL;
                    paused = false;
                    I_SetPalette(W_CacheLumpName
                                 ("PLAYPAL", PU_CACHE), false); // [JN] TODO - colorblind
                    D_StartTitle();     // go to intro/demo mode.
                    break;

                case 3:
                    P_SetMessage(&players[consoleplayer],
                                 "QUICKSAVING....", false);
                    FileMenuKeySteal = true;
                    SCSaveGame(quicksave - 1);
                    BorderNeedRefresh = true;
                    break;

                case 4:
                    P_SetMessage(&players[consoleplayer],
                                 "QUICKLOADING....", false);
                    SCLoadGame(quickload - 1);
                    BorderNeedRefresh = true;
                    break;

                default:
                    break;
            }

            askforquit = false;
            typeofask = 0;

            return true;
        }
        else if (key == key_menu_abort || key == KEY_ESCAPE)
        {
            players[consoleplayer].messageTics = 1;  //set the msg to be cleared
            askforquit = false;
            typeofask = 0;
            paused = false;
            UpdateState |= I_FULLSCRN;
            BorderNeedRefresh = true;
            return true;
        }

        return false;           // don't let the keys filter thru
    }

    if (!MenuActive && !chatmodeon)
    {
        if (key == key_menu_decscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(LEFT_DIR);
            S_StartSound(NULL, sfx_keyup);
            BorderNeedRefresh = true;
            UpdateState |= I_FULLSCRN;
            return (true);
        }
        else if (key == key_menu_incscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(RIGHT_DIR);
            S_StartSound(NULL, sfx_keyup);
            BorderNeedRefresh = true;
            UpdateState |= I_FULLSCRN;
            return (true);
        }
        else if (key == key_menu_help)           // F1
        {
            SCInfo(0);      // start up info screens
            MenuActive = true;
            return (true);
        }
        else if (key == key_menu_save)           // F2 (save game)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                MenuActive = true;
                FileMenuKeySteal = false;
                MenuTime = 0;
                CurrentMenu = &SaveMenu;
                CurrentItPos = CurrentMenu->oldItPos;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                S_StartSound(NULL, sfx_dorcls);
                slottextloaded = false;     //reload the slot text, when needed
            }
            return true;
        }
        else if (key == key_menu_load)           // F3 (load game)
        {
            if (SCNetCheck(2))
            {
                MenuActive = true;
                FileMenuKeySteal = false;
                MenuTime = 0;
                CurrentMenu = &LoadMenu;
                CurrentItPos = CurrentMenu->oldItPos;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                S_StartSound(NULL, sfx_dorcls);
                slottextloaded = false;     //reload the slot text, when needed
            }
            return true;
        }
        else if (key == key_menu_volume)         // F4 (volume)
        {
            MenuActive = true;
            FileMenuKeySteal = false;
            MenuTime = 0;
            CurrentMenu = &Options2Menu;
            CurrentItPos = CurrentMenu->oldItPos;
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            S_StartSound(NULL, sfx_dorcls);
            slottextloaded = false; //reload the slot text, when needed
            return true;
        }
        else if (key == key_menu_detail)          // F5 (detail)
        {
            // F5 isn't used in Heretic. (detail level)
            return true;
        }
        else if (key == key_menu_qsave)           // F6 (quicksave)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                if (!quicksave || quicksave == -1)
                {
                    MenuActive = true;
                    FileMenuKeySteal = false;
                    MenuTime = 0;
                    CurrentMenu = &SaveMenu;
                    CurrentItPos = CurrentMenu->oldItPos;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    S_StartSound(NULL, sfx_dorcls);
                    slottextloaded = false; //reload the slot text, when needed
                    quicksave = -1;
                    P_SetMessage(&players[consoleplayer],
                                 "CHOOSE A QUICKSAVE SLOT", true);
                }
                else
                {
                    askforquit = true;
                    typeofask = 3;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    S_StartSound(NULL, sfx_chat);
                }
            }
            return true;
        }
        else if (key == key_menu_endgame)         // F7 (end game)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                S_StartSound(NULL, sfx_chat);
                SCEndGame(0);
            }
            return true;
        }
        else if (key == key_menu_messages)        // F8 (toggle messages)
        {
            SCMessages(0);
            return true;
        }
        else if (key == key_menu_qload)           // F9 (quickload)
        {
            if (!quickload || quickload == -1)
            {
                MenuActive = true;
                FileMenuKeySteal = false;
                MenuTime = 0;
                CurrentMenu = &LoadMenu;
                CurrentItPos = CurrentMenu->oldItPos;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                S_StartSound(NULL, sfx_dorcls);
                slottextloaded = false;     //reload the slot text, when needed
                quickload = -1;
                P_SetMessage(&players[consoleplayer],
                             "CHOOSE A QUICKLOAD SLOT", true);
            }
            else
            {
                askforquit = true;
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                typeofask = 4;
                S_StartSound(NULL, sfx_chat);
            }
            return true;
        }
        else if (key == key_menu_quit)            // F10 (quit)
        {
            if (gamestate == GS_LEVEL)
            {
                SCQuitGame(0);
                S_StartSound(NULL, sfx_chat);
            }
            return true;
        }
        else if (key == key_menu_gamma)           // F11 (gamma correction)
        {
            if (++crl_gamma > 14)
            {
                crl_gamma = 0;
            }
            P_SetMessage(&players[consoleplayer], gammamsg[crl_gamma], false);
            I_SetPalette((byte *) W_CacheLumpName("PLAYPAL", PU_CACHE), false); // [JN] TODO - colorblind
            return true;
        }

    }

    if (!MenuActive)
    {
        if (key == key_menu_activate || key == key_crl_menu 
        // [JN] Open Heretic/CRL menu only by pressing it's keys to allow 
        // certain CRL features to be toggled. This behavior is same to Doom.
        /*||  gamestate == GS_DEMOSCREEN || demoplayback*/)
        {
            MN_ActivateMenu();

            // [JN] Spawn CRL menu
            if (key == key_crl_menu)
            {
                CurrentMenu = &CRLMain;
                CurrentItPos = CurrentMenu->oldItPos;
            }

            return (true);
        }
        return (false);
    }
    else
    {
        // [JN] Deactivate CRL menu by pressing ~ key again.
        if (key == key_crl_menu)
        {
            MN_DeactivateMenu();
            return (true);
        }
    }

    if (!FileMenuKeySteal)
    {
        item = &CurrentMenu->items[CurrentItPos];

        if (key == key_menu_down)            // Next menu item
        {
            do
            {
                if (CurrentItPos + 1 > CurrentMenu->itemCount - 1)
                {
                    CurrentItPos = 0;
                }
                else
                {
                    CurrentItPos++;
                }
            }
            while (CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
            S_StartSound(NULL, sfx_switch);
            return (true);
        }
        else if (key == key_menu_up)         // Previous menu item
        {
            do
            {
                if (CurrentItPos == 0)
                {
                    CurrentItPos = CurrentMenu->itemCount - 1;
                }
                else
                {
                    CurrentItPos--;
                }
            }
            while (CurrentMenu->items[CurrentItPos].type == ITT_EMPTY);
            S_StartSound(NULL, sfx_switch);
            return (true);
        }
        else if (key == key_menu_left)       // Slider left
        {
            if (item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(LEFT_DIR);
                S_StartSound(NULL, sfx_keyup);
            }
            return (true);
        }
        else if (key == key_menu_right)      // Slider right
        {
            if (item->type == ITT_LRFUNC && item->func != NULL)
            {
                item->func(RIGHT_DIR);
                S_StartSound(NULL, sfx_keyup);
            }
            return (true);
        }
        else if (key == key_menu_forward)    // Activate item (enter)
        {
            if (item->type == ITT_SETMENU)
            {
                SetMenu(item->menu);
            }
            else if (item->func != NULL)
            {
                CurrentMenu->oldItPos = CurrentItPos;
                if (item->type == ITT_LRFUNC)
                {
                    item->func(RIGHT_DIR);
                }
                else if (item->type == ITT_EFUNC)
                {
                    if (item->func(item->option))
                    {
                        if (item->menu != MENU_NONE)
                        {
                            SetMenu(item->menu);
                        }
                    }
                }
            }
            S_StartSound(NULL, sfx_dorcls);
            return (true);
        }
        else if (key == key_menu_activate)     // Toggle menu
        {
            MN_DeactivateMenu();
            return (true);
        }
        else if (key == key_menu_back)         // Go back to previous menu
        {
            S_StartSound(NULL, sfx_switch);
            if (CurrentMenu->prevMenu == MENU_NONE)
            {
                MN_DeactivateMenu();
            }
            else
            {
                SetMenu(CurrentMenu->prevMenu);
            }
            return (true);
        }
        // [crispy] delete a savegame
        else if (key == key_menu_del)
        {
            // [JN] ...or clear mouse bind.
            if (CurrentMenu == &CRLMouseBinds)
            {
                M_ClearMouseBind(CurrentItPos);
            }
            return (true);
        }
        else if (charTyped != 0)
        {
            // Jump to menu item based on first letter:

            for (i = 0; i < CurrentMenu->itemCount; i++)
            {
                if (CurrentMenu->items[i].text)
                {
                    if (toupper(charTyped)
                        == toupper(DEH_String(CurrentMenu->items[i].text)[0]))
                    {
                        CurrentItPos = i;
                        return (true);
                    }
                }
            }
        }

        return (false);
    }
    else
    {
        // Editing file names
        // When typing a savegame name, we use the fully shifted and
        // translated input value from event->data3.
        charTyped = event->data3;

        textBuffer = &SlotText[currentSlot][slotptr];
        if (key == KEY_BACKSPACE)
        {
            if (slotptr)
            {
                *textBuffer-- = 0;
                *textBuffer = ASCII_CURSOR;
                slotptr--;
            }
            return (true);
        }
        if (key == KEY_ESCAPE)
        {
            memset(SlotText[currentSlot], 0, SLOTTEXTLEN + 2);
            M_StringCopy(SlotText[currentSlot], oldSlotText,
                         sizeof(SlotText[currentSlot]));
            SlotStatus[currentSlot]--;
            MN_DeactivateMenu();
            return (true);
        }
        if (key == KEY_ENTER)
        {
            SlotText[currentSlot][slotptr] = 0; // clear the cursor
            item = &CurrentMenu->items[CurrentItPos];
            CurrentMenu->oldItPos = CurrentItPos;
            if (item->type == ITT_EFUNC)
            {
                item->func(item->option);
                if (item->menu != MENU_NONE)
                {
                    SetMenu(item->menu);
                }
            }
            return (true);
        }
        if (slotptr < SLOTTEXTLEN && key != KEY_BACKSPACE)
        {
            if (isalpha(charTyped))
            {
                *textBuffer++ = toupper(charTyped);
                *textBuffer = ASCII_CURSOR;
                slotptr++;
                return (true);
            }
            if (isdigit(charTyped) || charTyped == ' '
              || charTyped == ',' || charTyped == '.' || charTyped == '-'
              || charTyped == '!')
            {
                *textBuffer++ = charTyped;
                *textBuffer = ASCII_CURSOR;
                slotptr++;
                return (true);
            }
        }
        return (true);
    }
    return (false);
}

//---------------------------------------------------------------------------
//
// PROC MN_ActivateMenu
//
//---------------------------------------------------------------------------

void MN_ActivateMenu(void)
{
    if (MenuActive)
    {
        return;
    }
    if (paused)
    {
        S_ResumeSound();
    }
    MenuActive = true;
    FileMenuKeySteal = false;
    MenuTime = 0;
    CurrentMenu = &MainMenu;
    CurrentItPos = CurrentMenu->oldItPos;
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    S_StartSound(NULL, sfx_dorcls);
    slottextloaded = false;     //reload the slot text, when needed
}

//---------------------------------------------------------------------------
//
// PROC MN_DeactivateMenu
//
//---------------------------------------------------------------------------

void MN_DeactivateMenu(void)
{
    if (CurrentMenu != NULL)
    {
        CurrentMenu->oldItPos = CurrentItPos;
    }
    MenuActive = false;
    if (FileMenuKeySteal)
    {
        I_StopTextInput();
    }
    if (!netgame)
    {
        paused = false;
    }
    S_StartSound(NULL, sfx_dorcls);
    if (soundchanged)
    {
        S_SetMaxVolume(true);   //recalc the sound curve
        soundchanged = false;
    }
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageTics = 1;
}

//---------------------------------------------------------------------------
//
// PROC MN_DrawInfo
//
//---------------------------------------------------------------------------

void MN_DrawInfo(void)
{
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE), false); // [JN] TODO - colorblind
    V_DrawRawScreen(W_CacheLumpNum(W_GetNumForName("TITLE") + InfoType,
                                   PU_CACHE));
//      V_DrawPatch(0, 0, W_CacheLumpNum(W_GetNumForName("TITLE")+InfoType,
//              PU_CACHE));
}


//---------------------------------------------------------------------------
//
// PROC SetMenu
//
//---------------------------------------------------------------------------

static void SetMenu(MenuType_t menu)
{
    CurrentMenu->oldItPos = CurrentItPos;
    CurrentMenu = Menus[menu];
    CurrentItPos = CurrentMenu->oldItPos;
}

//---------------------------------------------------------------------------
//
// PROC DrawSlider
//
//---------------------------------------------------------------------------

static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing)
{
    int x;
    int y;
    int x2;
    int count;

    x = menu->x + 24;
    y = menu->y + 2 + (item * (bigspacing ? ITEM_HEIGHT : ITEM_HEIGHT_SMALL));
    V_DrawPatch(x - 32, y, W_CacheLumpName(DEH_String("M_SLDLT"), PU_CACHE), "M_SLDLT");
    for (x2 = x, count = width; count--; x2 += 8)
    {
        V_DrawPatch(x2, y, W_CacheLumpName(DEH_String(count & 1 ? "M_SLDMD1"
                                           : "M_SLDMD2"), PU_CACHE), "NULL"); // [JN] TODO - patch names
    }
    V_DrawPatch(x2, y, W_CacheLumpName(DEH_String("M_SLDRT"), PU_CACHE), "M_SLDRT");
    V_DrawPatch(x + 4 + slot * 8, y + 7,
                W_CacheLumpName(DEH_String("M_SLDKB"), PU_CACHE), "M_SLDKB");
}


// =============================================================================
//
//                          [JN] Mouse binding routines.
//                    Drawing, coloring, checking and binding.
//
// =============================================================================


// -----------------------------------------------------------------------------
// M_KeyDrawer
//  [JN] Draw mouse button number as printable string.
// -----------------------------------------------------------------------------

static char *M_MouseBtnDrawer (int CurrentItPosOn, int btn)
{
    if (CurrentItPos == CurrentItPosOn && MouseIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        switch (btn)
        {
            case -1:  return  "---";            break;  // Means empty
            case  0:  return  "LEFT BUTTON";    break;
            case  1:  return  "RIGHT BUTTON";   break;
            case  2:  return  "MIDDLE BUTTON";  break;
            case  3:  return  "BUTTON #4";      break;
            case  4:  return  "BUTTON #5";      break;
            case  5:  return  "BUTTON #6";      break;
            case  6:  return  "BUTTON #7";      break;
            case  7:  return  "BUTTON #8";      break;
            case  8:  return  "BUTTON #9";      break;
            default:  return  "UNKNOWN";        break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_StartMouseBind
//  [JN] Indicate that mouse button binding is started (MouseIsBinding), and
//  pass internal number (btnToBind) for binding a new button.
// -----------------------------------------------------------------------------

static void M_StartMouseBind (int btn)
{
    MouseIsBinding = true;
    btnToBind = btn;
}

// -----------------------------------------------------------------------------
// M_CheckMouseBind
//  [JN] Check if pressed button is already binded, clear previous bind if found.
// -----------------------------------------------------------------------------

static void M_CheckMouseBind (int btn)
{
    if (mousebfire == btn)        mousebfire        = -1;
    if (mousebforward == btn)     mousebforward     = -1;
    if (mousebstrafe == btn)      mousebstrafe      = -1;
    if (mousebbackward == btn)    mousebbackward    = -1;
    if (mousebuse == btn)         mousebuse         = -1;
    if (mousebstrafeleft == btn)  mousebstrafeleft  = -1;
    if (mousebstraferight == btn) mousebstraferight = -1;
    if (mousebprevweapon == btn)  mousebprevweapon  = -1;
    if (mousebnextweapon == btn)  mousebnextweapon  = -1;
}

// -----------------------------------------------------------------------------
// M_DoMouseBind
//  [JN] By catching internal bind number (btnnum), do actual binding
//  of pressed button (btn) to real mouse bind.
// -----------------------------------------------------------------------------

static void M_DoMouseBind (int btnnum, int btn)
{
    switch (btnnum)
    {
        case 1000:  mousebfire = btn;         break;
        case 1001:  mousebforward = btn;      break;
        case 1002:  mousebstrafe = btn;       break;
        case 1003:  mousebbackward = btn;     break;
        case 1004:  mousebuse = btn;          break;
        case 1005:  mousebstrafeleft = btn;   break;
        case 1006:  mousebstraferight = btn;  break;
        case 1007:  mousebprevweapon = btn;   break;
        case 1008:  mousebnextweapon = btn;   break;
        default:                              break;
    }
}

// -----------------------------------------------------------------------------
// M_ClearMouseBind
//  [JN] Clear mouse bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------

static void M_ClearMouseBind (int itemOn)
{
    switch (itemOn)
    {
        case 0:  mousebfire = -1;         break;
        case 1:  mousebforward = -1;      break;
        case 2:  mousebstrafe = -1;       break;
        case 3:  mousebbackward = -1;     break;
        case 4:  mousebuse = -1;          break;
        case 5:  mousebstrafeleft = -1;   break;
        case 6:  mousebstraferight = -1;  break;
        case 7:  mousebprevweapon = -1;   break;
        case 8:  mousebnextweapon = -1;   break;
    }
}

// -----------------------------------------------------------------------------
// M_ColorizeMouseBind
//  [JN] Do mouse bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeMouseBind (int CurrentItPosOn, int btn)
{
    if (CurrentItPos == CurrentItPosOn && MouseIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        if (btn == -1)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_RED_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_RED_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_RED_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_RED_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_RED_BRIGHT1] : cr[CR_RED];
        }
        else
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_GREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_GREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_GREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_GREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_GREEN_BRIGHT1] : cr[CR_GREEN];
        }
    }
}