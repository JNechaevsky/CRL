//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2011-2017 RestlessRodent
// Copyright(C) 2018-2024 Julia Nechaevskaya
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
#include <time.h>

#include "ct_chat.h"
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
    MENU_CRLDISPLAY,
    MENU_CRLSOUND,
    MENU_CRLCONTROLS,
    MENU_CRLKBDBINDS1,
    MENU_CRLKBDBINDS2,
    MENU_CRLKBDBINDS3,
    MENU_CRLKBDBINDS4,
    MENU_CRLKBDBINDS5,
    MENU_CRLKBDBINDS6,
    MENU_CRLKBDBINDS7,
    MENU_CRLKBDBINDS8,
    MENU_CRLKBDBINDS9,
    MENU_CRLMOUSEBINDS,
    MENU_CRLWIDGETS,
    MENU_CRLGAMEPLAY,
    MENU_CRLLIMITS,
    MENU_NONE
} MenuType_t;

typedef struct
{
    ItemType_t type;
    char *text;
    void (*func) (int option);
    int option;
    MenuType_t menu;
    short tics;  // [JN] Menu item timer for glowing effect.
} MenuItem_t;

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

// [JN] Font enum's used by FontType in Menu_t below.
// NoFont is used only in Save/Load menu for allowing to 
// choose slot by pressing number key.
enum {
    NoFont,
    SmallFont,
    BigFont
} FontType_t;

typedef struct
{
    int x;
    int y;
    void (*drawFunc) (void);
    int itemCount;
    MenuItem_t *items;
    int oldItPos;
    int FontType;       // [JN] 0 = no font, 1 = small font, 2 = big font
    boolean ScrollAR;   // [JN] Menu can be scrolled by arrow keys
    boolean ScrollPG;   // [JN] Menu can be scrolled by PGUP/PGDN keys
    MenuType_t prevMenu;
} Menu_t;

// Private Functions

static void InitFonts(void);
static void SetMenu(MenuType_t menu);
static boolean SCNetCheck(int option);
static void SCNetCheck2(int option);
static void SCQuitGame(int option);
static void SCEpisode(int option);
static void SCSkill(int option);
static void SCMouseSensi(int option);
static void SCSfxVolume(int option);
static void SCMusicVolume(int option);
static void SCScreenSize(int option);
static void SCLoadGame(int option);
static void SCSaveCheck(int option);
static void SCSaveGame(int option);
static void SCMessages(int option);
static void SCEndGame(int option);
static void SCInfo(int option);
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

// Public Data

boolean MenuActive;
int InfoType;

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
static char SlotText[SAVES_PER_PAGE][SLOTTEXTLEN + 2];
static char oldSlotText[SLOTTEXTLEN + 2];
static int SlotStatus[SAVES_PER_PAGE];
static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;

// [JN] Show custom titles while performing quick save/load.
static boolean quicksaveTitle = false;
static boolean quickloadTitle = false;

static char *gammalvls[16][32] =
{
    { GAMMALVL05,   "0.50" },
    { GAMMALVL055,  "0.55" },
    { GAMMALVL06,   "0.60" },
    { GAMMALVL065,  "0.65" },
    { GAMMALVL07,   "0.70" },
    { GAMMALVL075,  "0.75" },
    { GAMMALVL08,   "0.80" },
    { GAMMALVL085,  "0.85" },
    { GAMMALVL09,   "0.90" },
    { GAMMALVL095,  "0.95" },
    { GAMMALVL0,    "OFF"  },
    { GAMMALVL1,    "1"    },
    { GAMMALVL2,    "2"    },
    { GAMMALVL3,    "3"    },
    { GAMMALVL4,    "4"    },
    { NULL,         NULL   },
};

static MenuItem_t MainItems[] = {
    {ITT_SETMENU, "NEW GAME", SCNetCheck2, 1, MENU_EPISODE},
    {ITT_SETMENU, "OPTIONS", NULL, 0, MENU_CRLMAIN},
    {ITT_SETMENU, "GAME FILES", NULL, 0, MENU_FILES},
    {ITT_EFUNC, "INFO", SCInfo, 0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME", SCQuitGame, 0, MENU_NONE}
};

static Menu_t MainMenu = {
    110, 56,
    DrawMainMenu,
    5, MainItems,
    0,
    BigFont, false, false,
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
    BigFont, false, false,
    MENU_MAIN
};

static MenuItem_t FilesItems[] = {
    {ITT_SETMENU, "LOAD GAME", SCNetCheck2,  2, MENU_LOAD},
    {ITT_EFUNC, "SAVE GAME", SCSaveCheck, 0, MENU_SAVE}
};

static Menu_t FilesMenu = {
    110, 60,
    DrawFilesMenu,
    2, FilesItems,
    0,
    BigFont, false, false,
    MENU_MAIN
};

// [JN] Allow to chose slot by pressing number key.
// This behavior is same to Doom.
static MenuItem_t LoadItems[] = {
    {ITT_EFUNC, "1", SCLoadGame, 0, MENU_NONE},
    {ITT_EFUNC, "2", SCLoadGame, 1, MENU_NONE},
    {ITT_EFUNC, "3", SCLoadGame, 2, MENU_NONE},
    {ITT_EFUNC, "4", SCLoadGame, 3, MENU_NONE},
    {ITT_EFUNC, "5", SCLoadGame, 4, MENU_NONE},
    {ITT_EFUNC, "6", SCLoadGame, 5, MENU_NONE}
};

static Menu_t LoadMenu = {
    70, 18,
    DrawLoadMenu,
    SAVES_PER_PAGE, LoadItems,
    0,
    NoFont, true, true,
    MENU_FILES
};

// [JN] Allow to chose slot by pressing number key.
// This behavior is same to Doom.
static MenuItem_t SaveItems[] = {
    {ITT_EFUNC, "1", SCSaveGame, 0, MENU_NONE},
    {ITT_EFUNC, "2", SCSaveGame, 1, MENU_NONE},
    {ITT_EFUNC, "3", SCSaveGame, 2, MENU_NONE},
    {ITT_EFUNC, "4", SCSaveGame, 3, MENU_NONE},
    {ITT_EFUNC, "5", SCSaveGame, 4, MENU_NONE},
    {ITT_EFUNC, "6", SCSaveGame, 5, MENU_NONE}
};

static Menu_t SaveMenu = {
    70, 18,
    DrawSaveMenu,
    SAVES_PER_PAGE, SaveItems,
    0,
    NoFont, true, true,
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
    BigFont, false, false,
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
    BigFont, false, false,
    MENU_CRLMAIN
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
    BigFont, false, false,
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

#define ID_MENU_LINEHEIGHT_SMALL  (10)
#define ID_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return SCREENWIDTH - CurrentMenu->x - MN_TextAWidth(text);
}

static player_t *player;

static void DrawCRLMain (void);

static void CRL_Spectating (int option);
static void CRL_Freeze (int option);
static void CRL_NoTarget (int option);
static void CRL_NoMomentum (int option);

static void DrawCRLVideo (void);
static void CRL_UncappedFPS (int option);
static void CRL_LimitFPS (int option);
static void CRL_VSync (int option);
static void CRL_ShowFPS (int option);
static void CRL_PixelScaling (int option);
static void CRL_VisplanesDraw (int option);
static void CRL_HOMDraw (int option);
static void CRL_Gamma (int option);
static void CRL_MenuBgShading (int option);
static void CRL_LevelBrightness (int option);
static void CRL_MsgCritical (int option);
static void CRL_GfxStartup (int option);
static void CRL_EndText (int option);
static void CRL_Colorblind (int option);

static void DrawCRLDisplay (void);
static void CRL_TextShadows (int option);

static void DrawCRLSound (void);
static void CRL_MusicSystem (int option);
static void CRL_SFXMode (int option);
static void CRL_PitchShift (int option);
static void CRL_SFXChannels (int option);

static void DrawCRLControls (void);
static void CRL_Controls_Acceleration (int option);
static void CRL_Controls_Threshold (int option);
static void CRL_Controls_MLook (int option);
static void CRL_Controls_NoVert (int option);

static void DrawCRLKbd1 (void);
static void M_Bind_MoveForward (int option);
static void M_Bind_MoveBackward (int option);
static void M_Bind_TurnLeft (int option);
static void M_Bind_TurnRight (int option);
static void M_Bind_StrafeLeft (int option);
static void M_Bind_StrafeRight (int option);
static void M_Bind_SpeedOn (int option);
static void M_Bind_StrafeOn (int option);
static void M_Bind_180Turn (int option);
static void M_Bind_FireAttack (int option);
static void M_Bind_Use (int option);

static void DrawCRLKbd2 (void);
static void M_Bind_LookUp (int option);
static void M_Bind_LookDown (int option);
static void M_Bind_LookCenter (int option);
static void M_Bind_FlyUp (int option);
static void M_Bind_FlyDown (int option);
static void M_Bind_FlyCenter (int option);
static void M_Bind_InvLeft (int option);
static void M_Bind_InvRight (int option);
static void M_Bind_UseArti (int option);

static void DrawCRLKbd3 (void);
static void M_Bind_CRLmenu (int option);
static void M_Bind_RestartLevel (int option);
static void M_Bind_NextLevel (int option);
static void M_Bind_FastForward (int option);
static void M_Bind_SpectatorMode (int option);
static void M_Bind_CameraUp (int option);
static void M_Bind_CameraDown (int option);
static void M_Bind_FreezeMode (int option);
static void M_Bind_NotargetMode (int option);

static void DrawCRLKbd4 (void);
static void M_Bind_AlwaysRun (int option);
static void M_Bind_MouseLook (int option);
static void M_Bind_VileBomb (int option);
static void M_Bind_ClearMAX (int option);
static void M_Bind_MoveToMAX (int option);
static void M_Bind_IDDQD (int option);
static void M_Bind_IDFA (int option);
static void M_Bind_IDCLIP (int option);
static void M_Bind_IDDT (int option);

static void DrawCRLKbd5 (void);
static void M_Bind_Weapon1 (int option);
static void M_Bind_Weapon2 (int option);
static void M_Bind_Weapon3 (int option);
static void M_Bind_Weapon4 (int option);
static void M_Bind_Weapon5 (int option);
static void M_Bind_Weapon6 (int option);
static void M_Bind_Weapon7 (int option);
static void M_Bind_Weapon8 (int option);
static void M_Bind_PrevWeapon (int option);
static void M_Bind_NextWeapon (int option);

static void DrawCRLKbd6 (void);
static void M_Bind_Quartz (int option);
static void M_Bind_Urn (int option);
static void M_Bind_Bomb (int option);
static void M_Bind_Tome (int option);
static void M_Bind_Ring (int option);
static void M_Bind_Chaosdevice (int option);
static void M_Bind_Shadowsphere (int option);
static void M_Bind_Wings (int option);
static void M_Bind_Torch (int option);
static void M_Bind_Morph (int option);

static void DrawCRLKbd7 (void);
static void M_Bind_ToggleMap (int option);
static void M_Bind_ZoomIn (int option);
static void M_Bind_ZoomOut (int option);
static void M_Bind_MaxZoom (int option);
static void M_Bind_FollowMode (int option);
// static void M_Bind_RotateMode (int option);
// static void M_Bind_OverlayMode (int option);
static void M_Bind_ToggleGrid (int option);

static void DrawCRLKbd8 (void);
static void M_Bind_HelpScreen (int option);
static void M_Bind_SaveGame (int option);
static void M_Bind_LoadGame (int option);
static void M_Bind_SoundVolume (int option);
static void M_Bind_QuickSave (int option);
static void M_Bind_EndGame (int option);
static void M_Bind_ToggleMessages (int option);
static void M_Bind_QuickLoad (int option);
static void M_Bind_QuitGame (int option);
static void M_Bind_ToggleGamma (int option);
static void M_Bind_MultiplayerSpy (int option);

static void DrawCRLKbd9 (void);
static void M_Bind_Pause (int option);
static void M_Bind_SaveScreenshot (int option);
static void M_Bind_FinishDemo (int option);
static void M_Bind_SendMessage (int option);
static void M_Bind_ToPlayer1 (int option);
static void M_Bind_ToPlayer2 (int option);
static void M_Bind_ToPlayer3 (int option);
static void M_Bind_ToPlayer4 (int option);
static void M_Bind_Reset (int option);

static void DrawCRLMouse (void);
static void M_Bind_M_FireAttack (int option);
static void M_Bind_M_MoveForward (int option);
static void M_Bind_M_StrafeOn (int option);
static void M_Bind_M_MoveBackward (int option);
static void M_Bind_M_Use (int option);
static void M_Bind_M_StrafeLeft (int option);
static void M_Bind_M_StrafeRight (int option);
static void M_Bind_M_PrevWeapon (int option);
static void M_Bind_M_NextWeapon (int option);
static void M_Bind_M_InventoryLeft (int option);
static void M_Bind_M_InventoryRight (int option);
static void M_Bind_M_UseArtifact (int option);

static void M_Bind_M_Reset (int option);

static void DrawCRLWidgets (void);
static void CRL_Widget_Render (int option);
static void CRL_Widget_MAX (int option);
static void CRL_Widget_Playstate (int option);
static void CRL_Widget_KIS (int option);
static void CRL_Widget_Time (int option);
static void CRL_Widget_Coords (int option);
static void CRL_Widget_Powerups (int option);
static void CRL_Widget_Health (int option);
static void CRL_Automap_Secrets (int option);

static void DrawCRLGameplay (void);
static void CRL_DefaulSkill (int option);
static void CRL_PistolStart (int option);
static void CRL_ColoredSBar (int option);
static void CRL_RestoreTargets (int option);
static void CRL_DemoTimer (int option);
static void CRL_TimerDirection (int option);
static void CRL_ProgressBar (int option);
static void CRL_InternalDemos (int option);

static void DrawCRLLimits (void);
static void CRL_SaveSizeWarning (int option);
static void CRL_Limits (int option);

// Keyboard binding prototypes
static boolean KbdIsBinding;
static int     keyToBind;

static char   *M_NameBind (int itemSetOn, int key);
static void    M_StartBind (int keynum);
static void    M_CheckBind (int key);
static void    M_DoBind (int keynum, int key);
static void    M_ClearBind (int itemOn);
static byte   *M_ColorizeBind (int itemSetOn, int key);
static void    M_ResetBinds (void);
static void    M_DrawBindKey (int itemNum, int yPos, int keyBind);
static void    M_DrawBindFooter (int y, char *pagenum, boolean drawPages);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static char   *M_NameMouseBind (int CurrentItPosOn, int btn);
static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int itemOn);
static byte   *M_ColorizeMouseBind (int CurrentItPosOn, int btn);
static void    M_DrawBindButton (int itemNum, int yPos, int btnBind);
static void    M_ResetMouseBinds (void);

// Forward declarations for scrolling and remembering last pages.
static Menu_t CRLKbdBinds1;
static Menu_t CRLKbdBinds2;
static Menu_t CRLKbdBinds3;
static Menu_t CRLKbdBinds4;
static Menu_t CRLKbdBinds5;
static Menu_t CRLKbdBinds6;
static Menu_t CRLKbdBinds7;
static Menu_t CRLKbdBinds8;
static Menu_t CRLKbdBinds9;

// Remember last keybindings page.
static int Keybinds_Cur;

static void CRL_Choose_Keybinds (int choice)
{
    SetMenu(Keybinds_Cur);
}

// Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // Save/Load menu:
    if (CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
    {
        if (savepage > 0 && !direction)
        {
            savepage--;
            S_StartSound(NULL, sfx_switch);
        }
        else
        if (savepage < SAVEPAGE_MAX && direction)
        {
            savepage++;
            S_StartSound(NULL, sfx_switch);
        }
        quicksave = -1;
        MN_LoadSlotText();
        return;
    }

    // Keyboard bindings:
    else if (CurrentMenu == &CRLKbdBinds1) SetMenu(direction ? MENU_CRLKBDBINDS2 : MENU_CRLKBDBINDS9);
    else if (CurrentMenu == &CRLKbdBinds2) SetMenu(direction ? MENU_CRLKBDBINDS3 : MENU_CRLKBDBINDS1);
    else if (CurrentMenu == &CRLKbdBinds3) SetMenu(direction ? MENU_CRLKBDBINDS4 : MENU_CRLKBDBINDS2);
    else if (CurrentMenu == &CRLKbdBinds4) SetMenu(direction ? MENU_CRLKBDBINDS5 : MENU_CRLKBDBINDS3);
    else if (CurrentMenu == &CRLKbdBinds5) SetMenu(direction ? MENU_CRLKBDBINDS6 : MENU_CRLKBDBINDS4);
    else if (CurrentMenu == &CRLKbdBinds6) SetMenu(direction ? MENU_CRLKBDBINDS7 : MENU_CRLKBDBINDS5);
    else if (CurrentMenu == &CRLKbdBinds7) SetMenu(direction ? MENU_CRLKBDBINDS8 : MENU_CRLKBDBINDS6);
    else if (CurrentMenu == &CRLKbdBinds8) SetMenu(direction ? MENU_CRLKBDBINDS1 : MENU_CRLKBDBINDS7);
    else if (CurrentMenu == &CRLKbdBinds9) SetMenu(direction ? MENU_CRLKBDBINDS1 : MENU_CRLKBDBINDS8);

    // Play sound.
    S_StartSound(NULL, sfx_switch);
}
// -----------------------------------------------------------------------------

// [JN] Delay before shading.
static int shade_wait;

// [JN] Shade background while in CRL menu.
static void M_ShadeBackground (void)
{
    // Return earlier if shading disabled.
    if (crl_menu_shading)
    {
        for (int y = 0; y < SCREENWIDTH * SCREENHEIGHT; y++)
        {
            I_VideoBuffer[y] = colormaps[crl_menu_shading * 256 + I_VideoBuffer[y]];
        }
    }
}

static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("FLOOR16", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
}

static byte *M_Small_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT2] :
        tics == 4 ? cr[CR_MENU_BRIGHT1] :
        tics == 3 ? NULL :
        tics == 2 ? cr[CR_MENU_DARK1]   :
                    cr[CR_MENU_DARK2]   ;
}

/*
static byte *M_Big_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT3] :
        tics >= 3 ? cr[CR_MENU_BRIGHT2] :
        tics >= 1 ? cr[CR_MENU_BRIGHT1] : NULL;
}
*/

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_YELLOW     4
#define GLOW_ORANGE     5
#define GLOW_LIGHTGRAY  6
#define GLOW_DARKGRAY   7
#define GLOW_BLUE       8
#define GLOW_OLIVE      9
#define GLOW_DARKGREEN  10

#define ITEMONTICS      CurrentMenu->items[CurrentItPos].tics
#define ITEMSETONTICS   CurrentMenu->items[CurrentItPosOn].tics

static byte *M_Item_Glow (const int CurrentItPosOn, const int color)
{
    if (CurrentItPos == CurrentItPosOn)
    {
        return
            color == GLOW_RED ||
            color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]       :
            color == GLOW_GREEN     ? cr[CR_GREEN_BRIGHT5]     :
            color == GLOW_YELLOW    ? cr[CR_YELLOW_BRIGHT5]    :
            color == GLOW_ORANGE    ? cr[CR_ORANGE_HR_BRIGHT5] :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY_BRIGHT5] :
            color == GLOW_DARKGRAY  ? cr[CR_MENU_DARK1]        :
            color == GLOW_BLUE      ? cr[CR_BLUE2_BRIGHT5]     :
            color == GLOW_OLIVE     ? cr[CR_OLIVE_BRIGHT5]     :
            color == GLOW_DARKGREEN ? cr[CR_DARKGREEN_BRIGHT5] :
                                      cr[CR_MENU_BRIGHT5]      ; // GLOW_UNCOLORED
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
        if (color == GLOW_YELLOW)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_YELLOW_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_YELLOW_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_YELLOW_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_YELLOW_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_YELLOW_BRIGHT1] : cr[CR_YELLOW];
        }
        if (color == GLOW_ORANGE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_ORANGE_HR_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_ORANGE_HR_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_ORANGE_HR_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_ORANGE_HR_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_ORANGE_HR_BRIGHT1] : cr[CR_ORANGE_HR];
        }
        if (color == GLOW_LIGHTGRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_LIGHTGRAY_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_LIGHTGRAY_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_LIGHTGRAY_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_LIGHTGRAY_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_LIGHTGRAY_BRIGHT1] : cr[CR_LIGHTGRAY];
        }
        if (color == GLOW_DARKGRAY)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_MENU_DARK1] :
                ITEMSETONTICS == 4 ? cr[CR_MENU_DARK2] :
                ITEMSETONTICS == 3 ? cr[CR_MENU_DARK3] :
                ITEMSETONTICS == 2 ? cr[CR_MENU_DARK4] :
                ITEMSETONTICS == 1 ? cr[CR_MENU_DARK4] : cr[CR_MENU_DARK4];
        }
        if (color == GLOW_BLUE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_BLUE2_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_BLUE2_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_BLUE2_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_BLUE2_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_BLUE2_BRIGHT1] : cr[CR_BLUE2];
        }
        if (color == GLOW_OLIVE)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_OLIVE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_OLIVE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_OLIVE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_OLIVE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_OLIVE_BRIGHT1] : cr[CR_OLIVE];
        }
        if (color == GLOW_DARKGREEN)
        {
            return
                ITEMSETONTICS == 5 ? cr[CR_DARKGREEN_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_DARKGREEN_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_DARKGREEN_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_DARKGREEN_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_DARKGREEN_BRIGHT1] : cr[CR_DARKGREEN];
        }
    }
    return NULL;
}

static byte *M_Cursor_Glow (const int tics)
{
    return
        tics ==  8 || tics ==  7 ? cr[CR_MENU_BRIGHT4] :
        tics ==  6 || tics ==  5 ? cr[CR_MENU_BRIGHT3] :
        tics ==  4 || tics ==  3 ? cr[CR_MENU_BRIGHT2] :
        tics ==  2 || tics ==  1 ? cr[CR_MENU_BRIGHT1] :
        tics == -1 || tics == -2 ? cr[CR_MENU_DARK1]   :
        tics == -3 || tics == -4 ? cr[CR_MENU_DARK2]   :
        tics == -5 || tics == -6 ? cr[CR_MENU_DARK3]   :
        tics == -7 || tics == -8 ? cr[CR_MENU_DARK4]   : NULL;
}

static const int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    switch (direction)
    {
        case 0:
        val--;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val++;
        if (val > max)
            val = capped ? max : min;
        break;
    }
    return val;
}

static const float M_FLOAT_Slider (float val, float min, float max, float step,
                                   int direction, boolean capped)
{
    char buf[9];

    switch (direction)
    {
        case 0:
        val -= step;
        if (val < min) 
            val = capped ? min : max;
        break;

        case 1:
        val += step;
        if (val > max)
            val = capped ? max : min;
        break;
    }

    // [JN] Do a float correction to always get x.xxx000 values:
    sprintf (buf, "%f", val);
    val = (float)atof(buf);
    return val;
}

static int DefSkillColor (const int skill)
{
    return
        skill == 0 ? GLOW_OLIVE     :
        skill == 1 ? GLOW_DARKGREEN :
        skill == 2 ? GLOW_GREEN     :
        skill == 3 ? GLOW_YELLOW    :
        skill == 4 ? GLOW_ORANGE    :
                     GLOW_RED       ;
}

static char *const DefSkillName[5] = 
{
    "WET-NURSE"     ,
    "YELLOWBELLIES" ,
    "BRINGEST"      ,
    "SMITE-MEISTER" ,
    "BLACK PLAGUE"
};

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
    {ITT_SETMENU, "DISPLAY OPTIONS",      NULL,           0, MENU_CRLDISPLAY},
    {ITT_SETMENU, "SOUND OPTIONS",        NULL,           0, MENU_CRLSOUND},
    {ITT_SETMENU, "CONTROL SETTINGS",     NULL,           0, MENU_CRLCONTROLS},
    {ITT_SETMENU, "WIDGETS AND AUTOMAP",  NULL,           0, MENU_CRLWIDGETS},
    {ITT_SETMENU, "GAMEPLAY FEATURES",    NULL,           0, MENU_CRLGAMEPLAY},
    {ITT_SETMENU, "STATIC ENGINE LIMITS", NULL,           0, MENU_CRLLIMITS},
    {ITT_SETMENU, "VANILLA OPTIONS MENU", NULL,           0, MENU_OPTIONS}
};

static Menu_t CRLMain = {
    CRL_MENU_LEFTOFFSET_SML, CRL_MENU_TOPOFFSET,
    DrawCRLMain,
    13, CRLMainItems,
    0,
    SmallFont, false, false,
    MENU_MAIN
};

static void DrawCRLMain (void)
{
    char str[32];

    MN_DrTextACentered("MAIN MENU", 20, cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 30,
               M_Item_Glow(0, crl_spectating ? GLOW_GREEN : GLOW_RED));

    // Freeze
    sprintf(str, !singleplayer ? "N/A" : crl_freeze ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 40,
                 M_Item_Glow(1, !singleplayer ? GLOW_DARKRED :
                             crl_freeze ? GLOW_GREEN : GLOW_RED));

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 50,
                 M_Item_Glow(2, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOTARGET ? GLOW_GREEN : GLOW_RED));

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    MN_DrTextA(str, CRL_MENU_RIGHTOFFSET_SML - MN_TextAWidth(str), 60, 
                 M_Item_Glow(3, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOMOMENTUM ? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered ("SETTINGS", 70, cr[CR_YELLOW]);
}

static void CRL_Spectating (int option)
{
    crl_spectating ^= 1;
}

static void CRL_Freeze (int option)
{
    if (!singleplayer)
    {
        return;
    }
    crl_freeze ^= 1;
}

static void CRL_NoTarget (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_NOTARGET;
}

static void CRL_NoMomentum (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_NOMOMENTUM;
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static MenuItem_t CRLVideoItems[] = {
    { ITT_LRFUNC, "UNCAPPED FRAMERATE",  CRL_UncappedFPS,   0, MENU_NONE },
    { ITT_LRFUNC, "FRAMERATE LIMIT",     CRL_LimitFPS,      0, MENU_NONE },
    { ITT_LRFUNC, "ENABLE VSYNC",        CRL_VSync,         0, MENU_NONE },
    { ITT_LRFUNC, "SHOW FPS COUNTER",    CRL_ShowFPS,       0, MENU_NONE },
    { ITT_LRFUNC, "PIXEL SCALING",       CRL_PixelScaling,  0, MENU_NONE },
    { ITT_LRFUNC, "VISPLANES DRAWING",   CRL_VisplanesDraw, 0, MENU_NONE },
    { ITT_LRFUNC, "HOM EFFECT",          CRL_HOMDraw,       0, MENU_NONE },
    { ITT_EMPTY,  NULL,                  NULL,              0, MENU_NONE },
    { ITT_LRFUNC, "GRAPHICAL STARTUP",   CRL_GfxStartup,    0, MENU_NONE },
    { ITT_LRFUNC, "SHOW ENDTEXT SCREEN", CRL_EndText,       0, MENU_NONE },
    { ITT_LRFUNC, "COLORBLIND",          CRL_Colorblind,    0, MENU_NONE },
};

static Menu_t CRLVideo = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLVideo,
    11, CRLVideoItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLVideo (void)
{
    char str[32];

    MN_DrTextACentered("VIDEO OPTIONS", 20, cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(0, crl_uncapped_fps ? GLOW_GREEN : GLOW_RED));

    // Framerate limit
    sprintf(str, !crl_uncapped_fps ? "35" :
                 crl_fpslimit ? "%d" : "NONE", crl_fpslimit);
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(1, crl_uncapped_fps ? GLOW_GREEN : GLOW_RED));

    // Enable vsync
    sprintf(str, crl_vsync ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(2, crl_vsync ? GLOW_GREEN : GLOW_RED));

    // Show FPS counter
    sprintf(str, crl_showfps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(3, crl_showfps ? GLOW_GREEN : GLOW_RED));

    // Pixel scaling
    sprintf(str, smooth_scaling ? "SMOOTH" : "SHARP");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(4, smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    // Visplanes drawing
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(5, crl_visplanes_drawing ? GLOW_GREEN : GLOW_RED));
    
    // HOM effect
    sprintf(str, crl_hom_effect == 0 ? "OFF" :
                 crl_hom_effect == 1 ? "MULTICOLOR" : "BLACK");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(6, crl_hom_effect ? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered("MISCELLANEOUS", 100, cr[CR_YELLOW]);

    // Graphical startup
    sprintf(str, graphical_startup ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(8, graphical_startup ? GLOW_GREEN : GLOW_RED));

    // Show ENDTEXT screen
    sprintf(str, show_endoom == 1 ? "ALWAYS" :
                 show_endoom == 2 ? "PWAD ONLY" : "NEVER");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(9, show_endoom == 1 ? GLOW_RED : GLOW_GREEN));

    // Colorblind
    sprintf(str, crl_colorblind == 1 ? "PROTANOPIA"    :
                 crl_colorblind == 2 ? "PROTANOMALY"   :
                 crl_colorblind == 3 ? "DEUTERANOPIA"  :
                 crl_colorblind == 4 ? "DEUTERANOMALY" :
                 crl_colorblind == 5 ? "TRITANOPIA"    :
                 crl_colorblind == 6 ? "TRITANOMALY"   :
                 crl_colorblind == 7 ? "ACHROMATOPSIA" :
                 crl_colorblind == 8 ? "ACHROMATOMALY" : "NONE");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(10, crl_colorblind ? GLOW_GREEN : GLOW_RED));

    // Print hint about colorblind type
    if (CurrentItPos == 10 && crl_colorblind)
    {
        MN_DrTextACentered(crl_colorblind == 1 ? "RED-BLIND"    :
                           crl_colorblind == 2 ? "RED-WEAK"     :
                           crl_colorblind == 3 ? "GREEN-BLIND"  :
                           crl_colorblind == 4 ? "GREEN-WEAK"   :
                           crl_colorblind == 5 ? "BLUE-BLIND"   :
                           crl_colorblind == 6 ? "BLUE-WEAK"    :
                           crl_colorblind == 7 ? "MONOCHROMACY" :
                        /* crl_colorblind == 8*/ "BLUE CONE MONOCHROMACY",
                           150, cr[CR_WHITE]);
    }
}

static void CRL_UncappedFPS (int option)
{
    crl_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
}

static void CRL_LimitFPS (int option)
{
    if (!crl_uncapped_fps)
    {
        return;  // Do not allow change value in capped framerate.
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
}

static void CRL_VSync (int option)
{
    crl_vsync ^= 1;
    I_ToggleVsync();
}

static void CRL_ShowFPS (int option)
{
    crl_showfps ^= 1;
}

static void CRL_PixelScaling (int choice)
{
    smooth_scaling ^= 1;
    I_TogglePixelScaling();
}

static void CRL_VisplanesDraw (int option)
{
    crl_visplanes_drawing = M_INT_Slider(crl_visplanes_drawing, 0, 4, option, false);
}

static void CRL_HOMDraw (int option)
{
    crl_hom_effect = M_INT_Slider(crl_hom_effect, 0, 2, option, false);
}

static void CRL_GfxStartup (int option)
{
    graphical_startup ^= 1;
}

static void CRL_EndText (int option)
{
    show_endoom = M_INT_Slider(show_endoom, 0, 2, option, false);
}

static void CRL_Colorblind (int option)
{
    crl_colorblind = M_INT_Slider(crl_colorblind, 0, 8, option, false);

    CRL_ReloadPalette();
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static MenuItem_t CRLDisplayItems[] = {
    { ITT_LRFUNC, "GAMMA-CORRECTION",        CRL_Gamma,           0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                0, MENU_NONE },
    { ITT_LRFUNC, "MENU BACKGROUND SHADING", CRL_MenuBgShading,   0, MENU_NONE },
    { ITT_LRFUNC, "EXTRA LEVEL BRIGHTNESS",  CRL_LevelBrightness, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                      NULL,                0, MENU_NONE },
    { ITT_LRFUNC, "MESSAGES ENABLED",        SCMessages,          0, MENU_NONE },
    { ITT_LRFUNC, "CRITICAL MESSAGE",        CRL_MsgCritical,     0, MENU_NONE },
    { ITT_LRFUNC, "TEXT CAST SHADOWS",       CRL_TextShadows,     0, MENU_NONE },
};

static Menu_t CRLDisplay = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLDisplay,
    9, CRLDisplayItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLDisplay (void)
{
    char str[32];

    MN_DrTextACentered("DISPLAY OPTIONS", 20, cr[CR_YELLOW]);

    // Gamma-correction slider and num
    DrawSlider(&CRLVideo, 1, 8, crl_gamma/2, false);
    MN_DrTextA(gammalvls[crl_gamma][1], 164, 45, M_Item_Glow(0, GLOW_UNCOLORED));

    // Menu background shading
    sprintf(str, crl_menu_shading ? "%d" : "OFF", crl_menu_shading);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(3, crl_menu_shading ? GLOW_GREEN : GLOW_RED));

    // Extra level brightness
    sprintf(str, crl_level_brightness ? "%d" : "OFF", crl_level_brightness);
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(4, crl_level_brightness ? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered("MESSAGES SETTINGS", 80, cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, showMessages ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(6, showMessages ? GLOW_GREEN : GLOW_RED));

    // Critical message style
    sprintf(str, crl_msg_critical ? "BLINKING" : "STATIC");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(7, crl_msg_critical ? GLOW_GREEN : GLOW_RED));
    // Show nice preview-reminder :)
    if (CurrentItPos == 7)
    {
        CRL_SetMessageCritical("CRITICAL MESSAGES ARE ALWAYS ENABLED!", "", 2);
    }

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(8, crl_text_shadows ? GLOW_GREEN : GLOW_RED));
}

static void CRL_Gamma (int option)
{
	shade_wait = I_GetTime() + TICRATE;
   
    crl_gamma = M_INT_Slider(crl_gamma, 0, 14, option, true);

    CRL_ReloadPalette();
}

static void CRL_MenuBgShading (int option)
{
    crl_menu_shading = M_INT_Slider(crl_menu_shading, 0, 24, option, true);
}

static void CRL_LevelBrightness (int option)
{
    crl_level_brightness = M_INT_Slider(crl_level_brightness, 0, 8, option, true);
}

static void CRL_MsgCritical (int option)
{
    crl_msg_critical ^= 1;
}

static void CRL_TextShadows (int option)
{
    crl_text_shadows ^= 1;
}

// -----------------------------------------------------------------------------
// Sound options
// -----------------------------------------------------------------------------

static MenuItem_t CRLSoundItems[] = {
    { ITT_LRFUNC, "SFX VOLUME",           SCSfxVolume,        MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE },
    { ITT_LRFUNC, "MUSIC VOLUME",         SCMusicVolume,      MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE },
    { ITT_EMPTY,  NULL,                   NULL,            0, MENU_NONE },
    { ITT_LRFUNC, "MUSIC PLAYBACK",       CRL_MusicSystem, 0, MENU_NONE },
    { ITT_LRFUNC, "SOUNDS EFFECTS MODE",  CRL_SFXMode,     0, MENU_NONE },
    { ITT_LRFUNC, "PITCH-SHIFTED SOUNDS", CRL_PitchShift,  0, MENU_NONE },
    { ITT_LRFUNC, "NUMBER OF SFX TO MIX", CRL_SFXChannels, 0, MENU_NONE },
};

static Menu_t CRLSound = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLSound,
    11, CRLSoundItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLSound (void)
{
    char str[32];

    MN_DrTextACentered("SOUND OPTIONS", 20, cr[CR_YELLOW]);

    DrawSlider(&CRLSound, 1, 16, snd_MaxVolume, false);
    sprintf(str,"%d", snd_MaxVolume);
    MN_DrTextA(str, 228, 45, M_Item_Glow(0, GLOW_UNCOLORED));

    DrawSlider(&CRLSound, 4, 16, snd_MusicVolume, false);
    sprintf(str,"%d", snd_MusicVolume);
    MN_DrTextA(str, 228, 75, M_Item_Glow(3, GLOW_UNCOLORED));

    MN_DrTextACentered("SOUND SYSTEM", 90, cr[CR_YELLOW]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ?  "GUS (EMULATED)" :
                 snd_musicdevice == 8 ?  "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                         "UNKNOWN");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(7, snd_musicdevice ? GLOW_GREEN : GLOW_RED));

    // Sound effects mode
    sprintf(str, crl_monosfx ? "MONO" : "STEREO");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(8, crl_monosfx ? GLOW_RED : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(9, snd_pitchshift ? GLOW_GREEN : GLOW_RED));

    // Number of SFX to mix
    sprintf(str, "%i", snd_Channels);
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(10, snd_Channels == 8 ? GLOW_GREEN :
                               snd_Channels == 1 ? GLOW_RED : GLOW_DARKGREEN));

    // Inform that music system is not hot-swappable. :(
    if (CurrentItPos == 7)
    {
        MN_DrTextACentered("CHANGE WILL REQUIRE RESTART OF THE PROGRAM", 142, cr[CR_GRAY]);
    }
}

static void CRL_MusicSystem (int option)
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
}

static void CRL_SFXMode (int option)
{
    crl_monosfx ^= 1;
}

static void CRL_PitchShift (int option)
{
    snd_pitchshift ^= 1;
}

static void CRL_SFXChannels (int option)
{
    // [JN] Note: cap minimum channels to 2, not 1.
    // Only one channel produces a strange effect, 
    // as if there were no channels at all.
    snd_Channels = M_INT_Slider(snd_Channels, 2, 16, option, true);
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static MenuItem_t CRLControlsItems[] = {
    {ITT_EFUNC,   "KEYBOARD BINDINGS",       CRL_Choose_Keybinds,       0, MENU_NONE},
    {ITT_SETMENU, "MOUSE BINDINGS",          NULL,                      0, MENU_CRLMOUSEBINDS},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_LRFUNC,  "SENSIVITY",               SCMouseSensi,              0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_LRFUNC,  "ACCELERATION",            CRL_Controls_Acceleration, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_LRFUNC,  "ACCELERATION THRESHOLD",  CRL_Controls_Threshold,    0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_LRFUNC,  "MOUSE LOOK",              CRL_Controls_MLook,        0, MENU_NONE},
    {ITT_LRFUNC,  "VERTICAL MOUSE MOVEMENT", CRL_Controls_NoVert,       0, MENU_NONE}
};

static Menu_t CRLControls = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLControls,
    14, CRLControlsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLControls (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("BINDINGS", 20, cr[CR_YELLOW]);

    MN_DrTextACentered("MOUSE CONFIGURATION", 50, cr[CR_YELLOW]);

    DrawSlider(&CRLControls, 4, 10, mouseSensitivity, false);
    sprintf(str,"%d", mouseSensitivity);
    MN_DrTextA(str, 180, 75, M_Item_Glow(3, mouseSensitivity > 9 ?
                                         GLOW_GREEN : GLOW_UNCOLORED));

    DrawSlider(&CRLControls, 7, 12, mouse_acceleration * 2, false);
    sprintf(str,"%.1f", mouse_acceleration);
    MN_DrTextA(str, 196, 105, M_Item_Glow(6, GLOW_UNCOLORED));

    DrawSlider(&CRLControls, 10, 14, mouse_threshold / 2, false);
    sprintf(str,"%d", mouse_threshold);
    MN_DrTextA(str, 212, 135, M_Item_Glow(9, GLOW_UNCOLORED));

    // Mouse look
    sprintf(str, crl_mouselook ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 150,
               M_Item_Glow(12, crl_mouselook ? GLOW_GREEN : GLOW_RED));

    // Vertical mouse movement
    sprintf(str, novert ? "OFF" : "ON");
    MN_DrTextA(str, M_ItemRightAlign(str), 160,
               M_Item_Glow(13, novert ? GLOW_RED : GLOW_GREEN));
}

static void CRL_Controls_Acceleration (int option)
{
    mouse_acceleration = M_FLOAT_Slider(mouse_acceleration, 1.000000f, 5.000000f, 0.100000f, option, true);
}

static void CRL_Controls_Threshold (int option)
{
    mouse_threshold = M_INT_Slider(mouse_threshold, 0, 32, option, true);
}

static void CRL_Controls_MLook (int option)
{
    crl_mouselook ^= 1;
    if (!crl_mouselook)
    {
        players[consoleplayer].centering = true;
    }
}

static void CRL_Controls_NoVert (int option)
{
    novert ^= 1;
}

// -----------------------------------------------------------------------------
// Keybinds 1
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds1Items[] = {
    { ITT_EFUNC, "MOVE FORWARD",    M_Bind_MoveForward,  0, MENU_NONE },
    { ITT_EFUNC, "MOVE BACKWARD",   M_Bind_MoveBackward, 0, MENU_NONE },
    { ITT_EFUNC, "TURN LEFT",       M_Bind_TurnLeft,     0, MENU_NONE },
    { ITT_EFUNC, "TURN RIGHT",      M_Bind_TurnRight,    0, MENU_NONE },
    { ITT_EFUNC, "STRAFE LEFT",     M_Bind_StrafeLeft,   0, MENU_NONE },
    { ITT_EFUNC, "STRAFE RIGHT",    M_Bind_StrafeRight,  0, MENU_NONE },
    { ITT_EFUNC, "SPEED ON",        M_Bind_SpeedOn,      0, MENU_NONE },
    { ITT_EFUNC, "STRAFE ON",       M_Bind_StrafeOn,     0, MENU_NONE },
    { ITT_EFUNC, "180 DEGREE TURN", M_Bind_180Turn,      0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,                0, MENU_NONE },
    { ITT_EFUNC, "FIRE/ATTACK",     M_Bind_FireAttack,   0, MENU_NONE },
    { ITT_EFUNC, "USE",             M_Bind_Use,          0, MENU_NONE },
};

static Menu_t CRLKbdBinds1 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd1,
    12, CRLKbsBinds1Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd1 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS1;

    M_FillBackground();

    MN_DrTextACentered("MOVEMENT", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_up);
    M_DrawBindKey(1, 40, key_down);
    M_DrawBindKey(2, 50, key_left);
    M_DrawBindKey(3, 60, key_right);
    M_DrawBindKey(4, 70, key_strafeleft);
    M_DrawBindKey(5, 80, key_straferight);
    M_DrawBindKey(6, 90, key_speed);
    M_DrawBindKey(7, 100, key_strafe);
    M_DrawBindKey(8, 110, key_180turn);

    MN_DrTextACentered("ACTION", 120, cr[CR_YELLOW]);

    M_DrawBindKey(10, 130, key_fire);
    M_DrawBindKey(11, 140, key_use);

    M_DrawBindFooter(170, "1", true);
}

static void M_Bind_MoveForward (int option)
{
    M_StartBind(100);  // key_up
}

static void M_Bind_MoveBackward (int option)
{
    M_StartBind(101);  // key_down
}

static void M_Bind_TurnLeft (int option)
{
    M_StartBind(102);  // key_left
}

static void M_Bind_TurnRight (int option)
{
    M_StartBind(103);  // key_right
}

static void M_Bind_StrafeLeft (int option)
{
    M_StartBind(104);  // key_strafeleft
}

static void M_Bind_StrafeRight (int option)
{
    M_StartBind(105);  // key_straferight
}

static void M_Bind_SpeedOn (int option)
{
    M_StartBind(106);  // key_speed
}

static void M_Bind_StrafeOn (int option)
{
    M_StartBind(107);  // key_strafe
}

static void M_Bind_180Turn (int choice)
{
    M_StartBind(108);  // key_180turn
}

static void M_Bind_FireAttack (int option)
{
    M_StartBind(109);  // key_fire
}

static void M_Bind_Use (int option)
{
    M_StartBind(110);  // key_use
}

// -----------------------------------------------------------------------------
// Keybinds 2
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds2Items[] = {
    { ITT_EFUNC, "LOOK UP",         M_Bind_LookUp,     0, MENU_NONE },
    { ITT_EFUNC, "LOOK DOWN",       M_Bind_LookDown,   0, MENU_NONE },
    { ITT_EFUNC, "CENTER VIEW",     M_Bind_LookCenter, 0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,              0, MENU_NONE },
    { ITT_EFUNC, "FLY UP",          M_Bind_FlyUp,      0, MENU_NONE },
    { ITT_EFUNC, "FLY DOWN",        M_Bind_FlyDown,    0, MENU_NONE },
    { ITT_EFUNC, "STOP FLYING",     M_Bind_FlyCenter,  0, MENU_NONE },
    { ITT_EMPTY, NULL,              NULL,              0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY LEFT",  M_Bind_InvLeft,    0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY RIGHT", M_Bind_InvRight,   0, MENU_NONE },
    { ITT_EFUNC, "USE ARTIFACT",    M_Bind_UseArti,    0, MENU_NONE },
};

static Menu_t CRLKbdBinds2 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd2,
    11, CRLKbsBinds2Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd2 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS2;

    M_FillBackground();

    MN_DrTextACentered("VIEW", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_lookup);
    M_DrawBindKey(1, 40, key_lookdown);
    M_DrawBindKey(2, 50, key_lookcenter);

    MN_DrTextACentered("FLYING", 60, cr[CR_YELLOW]);

    M_DrawBindKey(4, 70, key_flyup);
    M_DrawBindKey(5, 80, key_flydown);
    M_DrawBindKey(6, 90, key_flycenter);

    MN_DrTextACentered("INVENTORY", 100, cr[CR_YELLOW]);

    M_DrawBindKey(8, 110, key_invleft);
    M_DrawBindKey(9, 120, key_invright);
    M_DrawBindKey(10, 130, key_useartifact);

    M_DrawBindFooter(170, "2", true);
}

static void M_Bind_LookUp (int option)
{
    M_StartBind(200);  // key_lookup
}

static void M_Bind_LookDown (int option)
{
    M_StartBind(201);  // key_lookdown
}

static void M_Bind_LookCenter (int option)
{
    M_StartBind(202);  // key_lookcenter
}

static void M_Bind_FlyUp (int option)
{
    M_StartBind(203);  // key_flyup
}

static void M_Bind_FlyDown (int option)
{
    M_StartBind(204);  // key_flydown
}

static void M_Bind_FlyCenter (int option)
{
    M_StartBind(205);  // key_flycenter
}

static void M_Bind_InvLeft (int option)
{
    M_StartBind(206);  // key_invleft
}

static void M_Bind_InvRight (int option)
{
    M_StartBind(207);  // key_invright
}

static void M_Bind_UseArti (int option)
{
    M_StartBind(208);  // key_useartifact
}

// -----------------------------------------------------------------------------
// Keybinds 3
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds3Items[] = {
    {ITT_EFUNC, "MAIN CRL MENU",      M_Bind_CRLmenu,       0, MENU_NONE},
    {ITT_EFUNC, "RESTART LEVEL/DEMO", M_Bind_RestartLevel,  0, MENU_NONE},
    {ITT_EFUNC, "GO TO NEXT LEVEL",   M_Bind_NextLevel,     0, MENU_NONE},
    {ITT_EFUNC, "DEMO FAST-FORWARD",  M_Bind_FastForward,   0, MENU_NONE},
    {ITT_EMPTY, NULL,                 NULL,                 0, MENU_NONE},
    {ITT_EFUNC, "SPECTATOR MODE",     M_Bind_SpectatorMode, 0, MENU_NONE},
    {ITT_EFUNC, "MOVE CAMERA UP",     M_Bind_CameraUp,      0, MENU_NONE},
    {ITT_EFUNC, "MOVE CAMERA DOWN",   M_Bind_CameraDown,    0, MENU_NONE},
    {ITT_EFUNC, "FREEZE MODE",        M_Bind_FreezeMode,    0, MENU_NONE},
    {ITT_EFUNC, "NOTARGET MODE",      M_Bind_NotargetMode,  0, MENU_NONE}
};

static Menu_t CRLKbdBinds3 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd3,
    10, CRLKbsBinds3Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd3 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS3;

    M_FillBackground();

    MN_DrTextACentered("CRL CONTROLS", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_crl_menu);
    M_DrawBindKey(1, 40, key_crl_reloadlevel);
    M_DrawBindKey(2, 50, key_crl_nextlevel);
    M_DrawBindKey(3, 60, key_crl_demospeed);

    MN_DrTextACentered("GAME MODES", 70, cr[CR_YELLOW]);

    M_DrawBindKey(5, 80, key_crl_spectator);
    M_DrawBindKey(6, 90, key_crl_cameraup);
    M_DrawBindKey(7, 100, key_crl_cameradown);
    M_DrawBindKey(8, 110, key_crl_freeze);
    M_DrawBindKey(9, 120, key_crl_notarget);

    M_DrawBindFooter(170, "3", true);
}

static void M_Bind_CRLmenu (int option)
{
    M_StartBind(300);  // key_crl_menu
}

static void M_Bind_RestartLevel (int option)
{
    M_StartBind(301);  // key_crl_reloadlevel
}

static void M_Bind_NextLevel (int option)
{
    M_StartBind(302);  // key_crl_nextlevel
}

static void M_Bind_FastForward (int option)
{
    M_StartBind(303);  // key_crl_demospeed
}

static void M_Bind_SpectatorMode (int option)
{
    M_StartBind(304);  // key_crl_spectator
}

static void M_Bind_CameraUp (int option)
{
    M_StartBind(305);  // key_crl_cameraup
}

static void M_Bind_CameraDown (int option)
{
    M_StartBind(306);  // key_crl_cameradown
}

static void M_Bind_FreezeMode (int option)
{
    M_StartBind(307);  // key_crl_freeze
}

static void M_Bind_NotargetMode (int option)
{
    M_StartBind(308);  // key_crl_notarget
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds4Items[] = {
    {ITT_EFUNC, "ALWAYS RUN",     M_Bind_AlwaysRun, 0, MENU_NONE},
    {ITT_EFUNC, "MOUSE LOOK",     M_Bind_MouseLook, 0, MENU_NONE},
    {ITT_EFUNC, "ARCH-VILE JUMP", M_Bind_VileBomb,  0, MENU_NONE},
    {ITT_EMPTY, NULL,             NULL,             0, MENU_NONE},
    {ITT_EFUNC, "CLEAR MAX",      M_Bind_ClearMAX,  0, MENU_NONE},
    {ITT_EFUNC, "MOVE TO MAX",    M_Bind_MoveToMAX, 0, MENU_NONE},
    {ITT_EMPTY, NULL,             NULL,             0, MENU_NONE},
    {ITT_EFUNC, "QUICKEN",        M_Bind_IDDQD,     0, MENU_NONE},
    {ITT_EFUNC, "RAMBO",          M_Bind_IDFA,      0, MENU_NONE},
    {ITT_EFUNC, "KITTY",          M_Bind_IDCLIP,    0, MENU_NONE},
    {ITT_EFUNC, "RAVMAP",         M_Bind_IDDT,      0, MENU_NONE}
};

static Menu_t CRLKbdBinds4 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd4,
    11, CRLKbsBinds4Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd4 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS4;

    M_FillBackground();

    MN_DrTextACentered("ADVANCED MOVEMENT", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_crl_autorun);
    M_DrawBindKey(1, 40, key_crl_mlook);
    M_DrawBindKey(2, 50, key_crl_vilebomb);

    MN_DrTextACentered("VISPLANES MAX VALUE", 60, cr[CR_YELLOW]);

    M_DrawBindKey(4, 70, key_crl_clearmax);
    M_DrawBindKey(5, 80, key_crl_movetomax);

    MN_DrTextACentered("CHEAT SHORTCUTS", 90, cr[CR_YELLOW]);

    M_DrawBindKey(7, 100, key_crl_iddqd);
    M_DrawBindKey(8, 110, key_crl_idfa);
    M_DrawBindKey(9, 120, key_crl_idclip);
    M_DrawBindKey(10, 130, key_crl_iddt);

    M_DrawBindFooter(170, "4", true);
}

static void M_Bind_AlwaysRun (int option)
{
    M_StartBind(400);  // key_crl_autorun
}

static void M_Bind_MouseLook (int option)
{
    M_StartBind(401);  // key_crl_mlook
}

static void M_Bind_VileBomb (int option)
{
    M_StartBind(402);  // key_crl_vilebomb
}

static void M_Bind_ClearMAX (int option)
{
    M_StartBind(403);  // key_crl_clearmax
}

static void M_Bind_MoveToMAX (int option)
{
    M_StartBind(404);  // key_crl_movetomax
}

static void M_Bind_IDDQD (int option)
{
    M_StartBind(405);  // key_crl_iddqd
}

static void M_Bind_IDFA (int option)
{
    M_StartBind(406);  // key_crl_idfa
}

static void M_Bind_IDCLIP (int option)
{
    M_StartBind(407);  // key_crl_idclip
}

static void M_Bind_IDDT (int option)
{
    M_StartBind(408);  // key_crl_iddt
}

// -----------------------------------------------------------------------------
// Keybinds 5
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds5Items[] = {
    {ITT_EFUNC, "WEAPON 1",        M_Bind_Weapon1,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 2",        M_Bind_Weapon2,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 3",        M_Bind_Weapon3,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 4",        M_Bind_Weapon4,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 5",        M_Bind_Weapon5,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 6",        M_Bind_Weapon6,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 7",        M_Bind_Weapon7,    0, MENU_NONE},
    {ITT_EFUNC, "WEAPON 8",        M_Bind_Weapon8,    0, MENU_NONE},
    {ITT_EFUNC, "PREVIOUS WEAPON", M_Bind_PrevWeapon, 0, MENU_NONE},
    {ITT_EFUNC, "NEXT WEAPON",     M_Bind_NextWeapon, 0, MENU_NONE}
};

static Menu_t CRLKbdBinds5 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd5,
    10, CRLKbsBinds5Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd5 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS5;

    M_FillBackground();

    MN_DrTextACentered("WEAPONS", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_weapon1);
    M_DrawBindKey(1, 40, key_weapon2);
    M_DrawBindKey(2, 50, key_weapon3);
    M_DrawBindKey(3, 60, key_weapon4);
    M_DrawBindKey(4, 70, key_weapon5);
    M_DrawBindKey(5, 80, key_weapon6);
    M_DrawBindKey(6, 90, key_weapon7);
    M_DrawBindKey(7, 100, key_weapon8);
    M_DrawBindKey(8, 110, key_prevweapon);
    M_DrawBindKey(9, 120, key_nextweapon);

    M_DrawBindFooter(170, "5", true);
}

static void M_Bind_Weapon1 (int option)
{
    M_StartBind(500);  // key_weapon1
}

static void M_Bind_Weapon2 (int option)
{
    M_StartBind(501);  // key_weapon2
}

static void M_Bind_Weapon3 (int option)
{
    M_StartBind(502);  // key_weapon3
}

static void M_Bind_Weapon4 (int option)
{
    M_StartBind(503);  // key_weapon4
}

static void M_Bind_Weapon5 (int option)
{
    M_StartBind(504);  // key_weapon5
}

static void M_Bind_Weapon6 (int option)
{
    M_StartBind(505);  // key_weapon6
}

static void M_Bind_Weapon7 (int option)
{
    M_StartBind(506);  // key_weapon7
}

static void M_Bind_Weapon8 (int option)
{
    M_StartBind(507);  // key_weapon8
}

static void M_Bind_PrevWeapon (int option)
{
    M_StartBind(508);  // key_prevweapon
}

static void M_Bind_NextWeapon (int option)
{
    M_StartBind(509);  // key_nextweapon
}

// -----------------------------------------------------------------------------
// Keybinds 6
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds6Items[] = {
    {ITT_EFUNC, "QUARTZ FLASK",          M_Bind_Quartz,       0, MENU_NONE},
    {ITT_EFUNC, "MYSTIC URN",            M_Bind_Urn,          0, MENU_NONE},
    {ITT_EFUNC, "TIMEBOMB",              M_Bind_Bomb,         0, MENU_NONE},
    {ITT_EFUNC, "TOME OF POWER",         M_Bind_Tome,         0, MENU_NONE},
    {ITT_EFUNC, "RING OF INVINCIBILITY", M_Bind_Ring,         0, MENU_NONE},
    {ITT_EFUNC, "CHAOS DEVICE",          M_Bind_Chaosdevice,  0, MENU_NONE},
    {ITT_EFUNC, "SHADOWSPHERE",          M_Bind_Shadowsphere, 0, MENU_NONE},
    {ITT_EFUNC, "WINGS OF WRATH",        M_Bind_Wings,        0, MENU_NONE},
    {ITT_EFUNC, "TORCH",                 M_Bind_Torch,        0, MENU_NONE},
    {ITT_EFUNC, "MORPH OVUM",            M_Bind_Morph,        0, MENU_NONE}
};

static Menu_t CRLKbdBinds6 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd6,
    10, CRLKbsBinds6Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd6 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS6;

    M_FillBackground();

    MN_DrTextACentered("ARTIFACTS", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_arti_quartz);
    M_DrawBindKey(1, 40, key_arti_urn);
    M_DrawBindKey(2, 50, key_arti_bomb);
    M_DrawBindKey(3, 60, key_arti_tome);
    M_DrawBindKey(4, 70, key_arti_ring);
    M_DrawBindKey(5, 80, key_arti_chaosdevice);
    M_DrawBindKey(6, 90, key_arti_shadowsphere);
    M_DrawBindKey(7, 100, key_arti_wings);
    M_DrawBindKey(8, 110, key_arti_torch);
    M_DrawBindKey(9, 120, key_arti_morph);

    M_DrawBindFooter(170, "6", true);
}

static void M_Bind_Quartz (int option)
{
    M_StartBind(600);  // key_arti_quartz
}

static void M_Bind_Urn (int option)
{
    M_StartBind(601);  // key_arti_urn
}

static void M_Bind_Bomb (int option)
{
    M_StartBind(602);  // key_arti_bomb
}

static void M_Bind_Tome (int option)
{
    M_StartBind(603);  // key_arti_tome
}

static void M_Bind_Ring (int option)
{
    M_StartBind(604);  // key_arti_ring
}

static void M_Bind_Chaosdevice (int option)
{
    M_StartBind(605);  // key_arti_chaosdevice
}

static void M_Bind_Shadowsphere (int option)
{
    M_StartBind(606);  // key_arti_shadowsphere
}

static void M_Bind_Wings (int option)
{
    M_StartBind(607);  // key_arti_wings
}

static void M_Bind_Torch (int option)
{
    M_StartBind(608);  // key_arti_torch
}

static void M_Bind_Morph (int option)
{
    M_StartBind(609);  // key_arti_morph
}

// -----------------------------------------------------------------------------
// Keybinds 7
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds7Items[] = {
    {ITT_EFUNC, "TOGGLE MAP",       M_Bind_ToggleMap,   0, MENU_NONE},
    {ITT_EFUNC, "ZOOM IN",          M_Bind_ZoomIn,      0, MENU_NONE},
    {ITT_EFUNC, "ZOOM OUT",         M_Bind_ZoomOut,     0, MENU_NONE},
    {ITT_EFUNC, "MAXIMUM ZOOM OUT", M_Bind_MaxZoom,     0, MENU_NONE},
    {ITT_EFUNC, "FOLLOW MODE",      M_Bind_FollowMode,  0, MENU_NONE},
//  {ITT_EFUNC, "ROTATE MODE",      M_Bind_RotateMode,  0, MENU_NONE},
//  {ITT_EFUNC, "OVERLAY MODE",     M_Bind_OverlayMode, 0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE GRID",      M_Bind_ToggleGrid,  0, MENU_NONE}
};

static Menu_t CRLKbdBinds7 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd7,
    6, CRLKbsBinds7Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd7 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS7;

    M_FillBackground();

    MN_DrTextACentered("AUTOMAP", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_map_toggle);
    M_DrawBindKey(1, 40, key_map_zoomin);
    M_DrawBindKey(2, 50, key_map_zoomout);
    M_DrawBindKey(3, 60, key_map_maxzoom);
    M_DrawBindKey(4, 70, key_map_follow);
//  M_DrawBindKey(5, 80, key_crl_map_rotate);
//  M_DrawBindKey(6, 90, key_crl_map_overlay);
    M_DrawBindKey(5, 80, key_map_grid);

    M_DrawBindFooter(170, "7", true);
}

static void M_Bind_ToggleMap (int option)
{
    M_StartBind(700);  // key_map_toggle
}

static void M_Bind_ZoomIn (int option)
{
    M_StartBind(701);  // key_map_zoomin
}

static void M_Bind_ZoomOut (int option)
{
    M_StartBind(702);  // key_map_zoomout
}

static void M_Bind_MaxZoom (int option)
{
    M_StartBind(703);  // key_map_maxzoom
}

static void M_Bind_FollowMode (int option)
{
    M_StartBind(704);  // key_map_follow
}

/*
static void M_Bind_RotateMode (int option)
{
    M_StartBind(705);  // key_crl_map_rotate
}

static void M_Bind_OverlayMode (int option)
{
    M_StartBind(706);  // key_crl_map_overlay
}
*/

static void M_Bind_ToggleGrid (int option)
{
    M_StartBind(705);  // key_map_grid
}

// -----------------------------------------------------------------------------
// Keybinds 8
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds8Items[] = {
    {ITT_EFUNC, "HELP SCREEN",     M_Bind_HelpScreen,     0, MENU_NONE},
    {ITT_EFUNC, "SAVE GAME",       M_Bind_SaveGame,       0, MENU_NONE},
    {ITT_EFUNC, "LOAD GAME",       M_Bind_LoadGame,       0, MENU_NONE},
    {ITT_EFUNC, "SOUND VOLUME",    M_Bind_SoundVolume,    0, MENU_NONE},
    {ITT_EFUNC, "QUICK SAVE",      M_Bind_QuickSave,      0, MENU_NONE},
    {ITT_EFUNC, "END GAME",        M_Bind_EndGame,        0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE MESSAGES", M_Bind_ToggleMessages, 0, MENU_NONE},
    {ITT_EFUNC, "QUICK LOAD",      M_Bind_QuickLoad,      0, MENU_NONE},
    {ITT_EFUNC, "QUIT GAME",       M_Bind_QuitGame,       0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE GAMMA",    M_Bind_ToggleGamma,    0, MENU_NONE},
    {ITT_EFUNC, "MULTIPLAYER SPY", M_Bind_MultiplayerSpy, 0, MENU_NONE}
};

static Menu_t CRLKbdBinds8 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd8,
    11, CRLKbsBinds8Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd8 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS8;

    M_FillBackground();

    MN_DrTextACentered("FUNCTION KEYS", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_menu_help);
    M_DrawBindKey(1, 40, key_menu_save);
    M_DrawBindKey(2, 50, key_menu_load);
    M_DrawBindKey(3, 60, key_menu_volume);
    M_DrawBindKey(4, 70, key_menu_qsave);
    M_DrawBindKey(5, 80, key_menu_endgame);
    M_DrawBindKey(6, 90, key_menu_messages);
    M_DrawBindKey(7, 100, key_menu_qload);
    M_DrawBindKey(8, 110, key_menu_quit);
    M_DrawBindKey(9, 120, key_menu_gamma);
    M_DrawBindKey(10, 130, key_spy);

    M_DrawBindFooter(170, "8", true);
}

static void M_Bind_HelpScreen (int option)
{
    M_StartBind(800);  // key_menu_help
}

static void M_Bind_SaveGame (int option)
{
    M_StartBind(801);  // key_menu_save
}

static void M_Bind_LoadGame (int option)
{
    M_StartBind(802);  // key_menu_load
}

static void M_Bind_SoundVolume (int option)
{
    M_StartBind(803);  // key_menu_volume
}

static void M_Bind_QuickSave (int option)
{
    M_StartBind(804);  // key_menu_qsave
}

static void M_Bind_EndGame (int option)
{
    M_StartBind(805);  // key_menu_endgame
}

static void M_Bind_ToggleMessages (int option)
{
    M_StartBind(806);  // key_menu_messages
}

static void M_Bind_QuickLoad (int option)
{
    M_StartBind(807);  // key_menu_qload
}

static void M_Bind_QuitGame (int option)
{
    M_StartBind(808);  // key_menu_quit
}

static void M_Bind_ToggleGamma (int option)
{
    M_StartBind(809);  // key_menu_gamma
}

static void M_Bind_MultiplayerSpy (int option)
{
    M_StartBind(810);  // key_spy
}

// -----------------------------------------------------------------------------
// Keybinds 9
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds9Items[] = {
    {ITT_EFUNC, "PAUSE GAME",            M_Bind_Pause,          0, MENU_NONE},
    {ITT_EFUNC, "SAVE A SCREENSHOT",     M_Bind_SaveScreenshot, 0, MENU_NONE},
    {ITT_EFUNC, "FINISH DEMO RECORDING", M_Bind_FinishDemo,     0, MENU_NONE},
    {ITT_EMPTY, NULL,                    NULL,                  0, MENU_NONE},
    {ITT_EFUNC, "SEND MESSAGE",          M_Bind_SendMessage,    0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 1",         M_Bind_ToPlayer1,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 2",         M_Bind_ToPlayer2,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 3",         M_Bind_ToPlayer3,      0, MENU_NONE},
    {ITT_EFUNC, "- TO PLAYER 4",         M_Bind_ToPlayer4,      0, MENU_NONE},
    {ITT_EMPTY, NULL,                    NULL,                  0, MENU_NONE},
    {ITT_EFUNC, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,      0, MENU_NONE},
};

static Menu_t CRLKbdBinds9 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd9,
    11, CRLKbsBinds9Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd9 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS9;

    M_FillBackground();

    MN_DrTextACentered("SHORTCUT KEYS", 20, cr[CR_YELLOW]);

    M_DrawBindKey(0, 30, key_pause);
    M_DrawBindKey(1, 40, key_menu_screenshot);
    M_DrawBindKey(2, 50, key_demo_quit);

    MN_DrTextACentered("MULTIPLAYER", 60, cr[CR_YELLOW]);

    M_DrawBindKey(4, 70, key_multi_msg);
    M_DrawBindKey(5, 80, key_multi_msgplayer[0]);
    M_DrawBindKey(6, 90, key_multi_msgplayer[1]);
    M_DrawBindKey(7, 100, key_multi_msgplayer[2]);
    M_DrawBindKey(8, 110, key_multi_msgplayer[3]);

    MN_DrTextACentered("RESET", 120, cr[CR_YELLOW]);

    M_DrawBindFooter(170, "9", true);
}

static void M_Bind_Pause (int option)
{
    M_StartBind(900);  // key_pause
}

static void M_Bind_SaveScreenshot (int option)
{
    M_StartBind(901);  // key_menu_screenshot
}

static void M_Bind_FinishDemo (int option)
{
    M_StartBind(902);  // key_demo_quit
}

static void M_Bind_SendMessage (int option)
{
    M_StartBind(903);  // key_multi_msg
}

static void M_Bind_ToPlayer1 (int option)
{
    M_StartBind(904);  // key_multi_msgplayer[0]
}

static void M_Bind_ToPlayer2 (int option)
{
    M_StartBind(905);  // key_multi_msgplayer[1]
}

static void M_Bind_ToPlayer3 (int option)
{
    M_StartBind(906);  // key_multi_msgplayer[2]
}

static void M_Bind_ToPlayer4 (int option)
{
    M_StartBind(907);  // key_multi_msgplayer[3]
}

static void M_Bind_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 6;      // [JN] keybinds reset
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static MenuItem_t CRLMouseItems[] = {
    { ITT_EFUNC, "FIRE/ATTACK",               M_Bind_M_FireAttack,     0, MENU_NONE },
    { ITT_EFUNC, "MOVE FORWARD",              M_Bind_M_MoveForward,    0, MENU_NONE },
    { ITT_EFUNC, "STRAFE ON",                 M_Bind_M_StrafeOn,       0, MENU_NONE },
    { ITT_EFUNC, "MOVE BACKWARD",             M_Bind_M_MoveBackward,   0, MENU_NONE },
    { ITT_EFUNC, "USE",                       M_Bind_M_Use,            0, MENU_NONE },
    { ITT_EFUNC, "STRAFE LEFT",               M_Bind_M_StrafeLeft,     0, MENU_NONE },
    { ITT_EFUNC, "STRAFE RIGHT",              M_Bind_M_StrafeRight,    0, MENU_NONE },
    { ITT_EFUNC, "PREV WEAPON",               M_Bind_M_PrevWeapon,     0, MENU_NONE },
    { ITT_EFUNC, "NEXT WEAPON",               M_Bind_M_NextWeapon,     0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY LEFT",            M_Bind_M_InventoryLeft,  0, MENU_NONE },
    { ITT_EFUNC, "INVENTORY RIGHT",           M_Bind_M_InventoryRight, 0, MENU_NONE },
    { ITT_EFUNC, "USE ARTIFACT",              M_Bind_M_UseArtifact,    0, MENU_NONE },
    { ITT_EMPTY, NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EFUNC, "RESET BINDINGS TO DEFAULT", M_Bind_M_Reset,          0, MENU_NONE },
};

static Menu_t CRLMouseBinds = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLMouse,
    14, CRLMouseItems,
    0,
    SmallFont, false, false,
    MENU_CRLCONTROLS
};

static void DrawCRLMouse (void)
{
    M_FillBackground();

    MN_DrTextACentered("MOUSE BINDINGS", 20, cr[CR_YELLOW]);

    M_DrawBindButton(0, 30, mousebfire);
    M_DrawBindButton(1, 40, mousebforward);
    M_DrawBindButton(2, 50, mousebstrafe);
    M_DrawBindButton(3, 60, mousebbackward);
    M_DrawBindButton(4, 70, mousebuse);
    M_DrawBindButton(5, 80, mousebstrafeleft);
    M_DrawBindButton(6, 90, mousebstraferight);
    M_DrawBindButton(7, 100, mousebprevweapon);
    M_DrawBindButton(8, 110, mousebnextweapon);
    M_DrawBindButton(9, 120, mousebinvleft);
    M_DrawBindButton(10, 130, mousebinvright);
    M_DrawBindButton(11, 140, mousebuseartifact);

    MN_DrTextACentered("RESET", 150, cr[CR_YELLOW]);

    M_DrawBindFooter(180, NULL, false);
}

static void M_Bind_M_FireAttack (int option)
{
    M_StartMouseBind(1000);  // mousebfire
}

static void M_Bind_M_MoveForward (int option)
{
    M_StartMouseBind(1001);  // mousebforward
}

static void M_Bind_M_StrafeOn (int option)
{
    M_StartMouseBind(1002);  // mousebstrafe
}

static void M_Bind_M_MoveBackward (int option)
{
    M_StartMouseBind(1003);  // mousebbackward
}

static void M_Bind_M_Use (int option)
{
    M_StartMouseBind(1004);  // mousebuse
}

static void M_Bind_M_StrafeLeft (int option)
{
    M_StartMouseBind(1005);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int option)
{
    M_StartMouseBind(1006);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int option)
{
    M_StartMouseBind(1007);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int option)
{
    M_StartMouseBind(1008);  // mousebnextweapon
}

static void M_Bind_M_InventoryLeft (int option)
{
    M_StartMouseBind(1009);  // mousebinvleft
}

static void M_Bind_M_InventoryRight (int option)
{
    M_StartMouseBind(1010);  // mousebinvright
}

static void M_Bind_M_UseArtifact (int option)
{
    M_StartMouseBind(1011);  // mousebuseartifact
}

static void M_Bind_M_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 7;      // [JN] mouse binds reset
}

// -----------------------------------------------------------------------------
// Widgets and Automap
// -----------------------------------------------------------------------------

static MenuItem_t CRLWidgetsItems[] = {
    { ITT_LRFUNC, "RENDER COUNTERS",     CRL_Widget_Render,    0, MENU_NONE },
    { ITT_LRFUNC, "MAX OVERFLOW STYLE",  CRL_Widget_MAX,       0, MENU_NONE },
    { ITT_LRFUNC, "PLAYSTATE COUNTERS",  CRL_Widget_Playstate, 0, MENU_NONE },
    { ITT_LRFUNC, "KIS STATS",           CRL_Widget_KIS,       0, MENU_NONE },
    { ITT_LRFUNC, "LEVEL TIME",          CRL_Widget_Time,      0, MENU_NONE },
    { ITT_LRFUNC, "PLAYER COORDS",       CRL_Widget_Coords,    0, MENU_NONE },
    { ITT_LRFUNC, "POWERUP TIMERS",      CRL_Widget_Powerups,  0, MENU_NONE },
    { ITT_LRFUNC, "TARGET'S HEALTH",     CRL_Widget_Health,    0, MENU_NONE },
    { ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE },
    { ITT_LRFUNC, "MARK SECRET SECTORS", CRL_Automap_Secrets,  0, MENU_NONE },
    { ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE },
    { ITT_EMPTY,  NULL,                  NULL,                 0, MENU_NONE },
};

static Menu_t CRLWidgetsMap = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLWidgets,
    11, CRLWidgetsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLWidgets (void)
{
    char str[32];

    MN_DrTextACentered("WIDGETS", 20, cr[CR_YELLOW]);

    // Render counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(0, crl_widget_render == 1 ? GLOW_GREEN :
                              crl_widget_render == 2 ? GLOW_DARKGREEN : GLOW_RED));

    // MAX overflow style
    sprintf(str, crl_widget_maxvp == 1 ? "BLINKING 1" :
                 crl_widget_maxvp == 2 ? "BLINKING 2" : "STATIC");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(1, crl_widget_maxvp == 1 ? (gametic &  8 ? GLOW_YELLOW : GLOW_GREEN) :
                              crl_widget_maxvp == 2 ? (gametic & 16 ? GLOW_YELLOW : GLOW_GREEN) : GLOW_YELLOW));

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(2, crl_widget_playstate == 1 ? GLOW_GREEN :
                              crl_widget_playstate == 2 ? GLOW_DARKGREEN : GLOW_RED));

    // K/I/S stats
    sprintf(str, crl_widget_kis == 1 ? "ON" :
                 crl_widget_kis == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(3, crl_widget_kis ? GLOW_GREEN : GLOW_RED));

    // Level time
    sprintf(str, crl_widget_time == 1 ? "ON" : 
                 crl_widget_time == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(4, crl_widget_time ? GLOW_GREEN : GLOW_RED));

    // Player coords
    sprintf(str, crl_widget_coords == 1 ? "ON" :
                 crl_widget_coords == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(5, crl_widget_coords ? GLOW_GREEN : GLOW_RED));

    // Powerup timers
    sprintf(str, crl_widget_powerups ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(6, crl_widget_powerups ? GLOW_GREEN : GLOW_RED));

    // Target's health
    sprintf(str, crl_widget_health == 1 ? "TOP" :
                 crl_widget_health == 2 ? "TOP+NAME" :
                 crl_widget_health == 3 ? "BOTTOM" :
                 crl_widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(7, crl_widget_health ? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered("AUTOMAP", 110, cr[CR_YELLOW]);

    // Level time
    sprintf(str, crl_automap_secrets ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(9, crl_automap_secrets ? GLOW_GREEN : GLOW_RED));
}

static void CRL_Widget_Render (int option)
{
    crl_widget_render = M_INT_Slider(crl_widget_render, 0, 2, option, false);
}

static void CRL_Widget_MAX (int option)
{
    crl_widget_maxvp = M_INT_Slider(crl_widget_maxvp, 0, 2, option, false);
}

static void CRL_Widget_Playstate (int option)
{
    crl_widget_playstate = M_INT_Slider(crl_widget_playstate, 0, 2, option, false);
}

static void CRL_Widget_KIS (int option)
{
    crl_widget_kis = M_INT_Slider(crl_widget_kis, 0, 2, option, false);
}

static void CRL_Widget_Time (int option)
{
    crl_widget_time = M_INT_Slider(crl_widget_time, 0, 2, option, false);
}

static void CRL_Widget_Coords (int option)
{
    crl_widget_coords = M_INT_Slider(crl_widget_coords, 0, 2, option, false);
}

static void CRL_Widget_Powerups (int option)
{
    crl_widget_powerups ^= 1;
}

static void CRL_Widget_Health (int option)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 4, option, false);
}

static void CRL_Automap_Secrets (int option)
{
    crl_automap_secrets ^= 1;
}

// -----------------------------------------------------------------------------
// Static engine limits
// -----------------------------------------------------------------------------

static MenuItem_t CRLGameplayItems[] = {
    {ITT_LRFUNC, "DEFAULT SKILL LEVEL",     CRL_DefaulSkill,    0, MENU_NONE},
    {ITT_LRFUNC, "WAND START GAME MODE",    CRL_PistolStart,    0, MENU_NONE},
    {ITT_LRFUNC, "COLORED STATUS BAR",      CRL_ColoredSBar,    0, MENU_NONE},
    {ITT_LRFUNC, "RESTORE MONSTER TARGETS", CRL_RestoreTargets, 0, MENU_NONE},
    {ITT_EMPTY,  NULL,                      NULL,               0, MENU_NONE},
    {ITT_LRFUNC, "SHOW DEMO TIMER",         CRL_DemoTimer,      0, MENU_NONE},
    {ITT_LRFUNC, "TIMER DIRECTION",         CRL_TimerDirection, 0, MENU_NONE},
    {ITT_LRFUNC, "SHOW PROGRESS BAR",       CRL_ProgressBar,    0, MENU_NONE},
    {ITT_LRFUNC, "PLAY INTERNAL DEMOS",     CRL_InternalDemos,  0, MENU_NONE}
};

static Menu_t CRLGameplay = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLGameplay,
    9, CRLGameplayItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLGameplay (void)
{
    char str[32];

    MN_DrTextACentered("GAMEPLAY FEATURES", 20, cr[CR_YELLOW]);

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[crl_default_skill]);
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(0, DefSkillColor(crl_default_skill)));

    // Wand start game mode
    sprintf(str, crl_pistol_start ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(1, crl_pistol_start ? GLOW_GREEN : GLOW_RED));

    // Colored status bar
    sprintf(str, crl_colored_stbar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(2, crl_colored_stbar? GLOW_GREEN : GLOW_RED));

    // Restore monster targets
    sprintf(str, crl_restore_targets ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(3, crl_restore_targets? GLOW_GREEN : GLOW_RED));

    MN_DrTextACentered("DEMOS", 70, cr[CR_YELLOW]);

    // Show Demo timer
    sprintf(str, crl_demo_timer == 1 ? "PLAYBACK" : 
                 crl_demo_timer == 2 ? "RECORDING" : 
                 crl_demo_timer == 3 ? "ALWAYS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(5, crl_demo_timer ? GLOW_GREEN : GLOW_RED));

    // Timer direction
    sprintf(str, crl_demo_timerdir ? "BACKWARD" : "FORWARD");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(6, crl_demo_timer ? GLOW_GREEN : GLOW_RED));

    // Show progress bar
    sprintf(str, crl_demo_bar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(7, crl_demo_bar? GLOW_GREEN : GLOW_RED));

    // Play internal demos
    sprintf(str, crl_internal_demos ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(8, crl_internal_demos? GLOW_GREEN : GLOW_RED));
}

static void CRL_DefaulSkill (int option)
{
    crl_default_skill = M_INT_Slider(crl_default_skill, 0, 4, option, false);
    SkillMenu.oldItPos = crl_default_skill;
}

static void CRL_PistolStart (int option)
{
    crl_pistol_start ^= 1;
}

static void CRL_ColoredSBar (int option)
{
    crl_colored_stbar ^= 1;
}

static void CRL_RestoreTargets (int option)
{
    crl_restore_targets ^= 1;
}

static void CRL_DemoTimer (int choice)
{
    crl_demo_timer = M_INT_Slider(crl_demo_timer, 0, 3, choice, false);
}

static void CRL_TimerDirection (int choice)
{
    crl_demo_timerdir ^= 1;
}

static void CRL_ProgressBar (int option)
{
    crl_demo_bar ^= 1;
}

static void CRL_InternalDemos (int option)
{
    crl_internal_demos ^= 1;
}

// -----------------------------------------------------------------------------
// Static engine limits
// -----------------------------------------------------------------------------

static MenuItem_t CRLLimitsItems[] = {
    {ITT_LRFUNC, "SAVE GAME LIMIT WARNING",    CRL_SaveSizeWarning, 0, MENU_NONE},
    {ITT_LRFUNC, "RENDER LIMITS LEVEL",        CRL_Limits,          0, MENU_NONE}
};

static Menu_t CRLLimits = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLLimits,
    2, CRLLimitsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLLimits (void)
{
    char str[32];

    MN_DrTextACentered("STATIC ENGINE LIMITS", 20, cr[CR_YELLOW]);

    // Save game limit warning
    sprintf(str, vanilla_savegame_limit ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(0, vanilla_savegame_limit ? GLOW_GREEN : GLOW_RED));

    // Level of the limits
    sprintf(str, crl_vanilla_limits ? "VANILLA" : "HERETIC-PLUS");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(1, crl_vanilla_limits ? GLOW_RED : GLOW_GREEN));

    MN_DrTextA("MAXVISPLANES", CRL_MENU_LEFTOFFSET_SML + 16, 60, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXDRAWSEGS", CRL_MENU_LEFTOFFSET_SML + 16, 70, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXVISSPRITES", CRL_MENU_LEFTOFFSET_SML + 16, 80, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXOPENINGS", CRL_MENU_LEFTOFFSET_SML + 16, 90, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXPLATS", CRL_MENU_LEFTOFFSET_SML + 16, 100, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXLINEANIMS", CRL_MENU_LEFTOFFSET_SML + 16, 110, cr[CR_MENU_DARK2]);

    if (crl_vanilla_limits)
    {
        MN_DrTextA("128", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"), 60, cr[CR_RED]);
        MN_DrTextA("256", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("256"), 70, cr[CR_RED]);
        MN_DrTextA("128", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"), 80, cr[CR_RED]);
        MN_DrTextA("20480", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("20480"), 90, cr[CR_RED]);
        MN_DrTextA("30", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("30"), 100, cr[CR_RED]);
        MN_DrTextA("64", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("64"), 110, cr[CR_RED]);
    }
    else
    {
        MN_DrTextA("1024", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"), 60, cr[CR_GREEN]);
        MN_DrTextA("2048", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("2048"), 70, cr[CR_GREEN]);
        MN_DrTextA("1024", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"), 80, cr[CR_GREEN]);
        MN_DrTextA("65536", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("65536"), 90, cr[CR_GREEN]);
        MN_DrTextA("7680", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("7680"), 100, cr[CR_GREEN]);
        MN_DrTextA("16384", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("16384"), 110, cr[CR_GREEN]);
    }
}

static void CRL_SaveSizeWarning (int option)
{
    vanilla_savegame_limit ^= 1;
}

static void CRL_Limits (int option)
{
    crl_vanilla_limits ^= 1;

    // [JN] CRL - re-define static engine limits.
    CRL_SetStaticLimits("HERETIC+");
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
    &CRLDisplay,
    &CRLSound,
    &CRLControls,
    &CRLKbdBinds1,
    &CRLKbdBinds2,
    &CRLKbdBinds3,
    &CRLKbdBinds4,
    &CRLKbdBinds5,
    &CRLKbdBinds6,
    &CRLKbdBinds7,
    &CRLKbdBinds8,
    &CRLKbdBinds9,
    &CRLMouseBinds,
    &CRLWidgetsMap,
    &CRLGameplay,
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
    SkullBaseLump = W_GetNumForName(DEH_String("M_SKL00"));

    // [JN] CRL - player is always local, "console" player.
    player = &players[consoleplayer];

    if (gamemode == retail)
    {                           // Add episodes 4 and 5 to the menu
        EpisodeMenu.itemCount = 5;
        EpisodeMenu.y -= ITEM_HEIGHT;
    }

    // [crispy] apply default difficulty
    SkillMenu.oldItPos = crl_default_skill;

    // [JN] Apply default first page of Keybinds menu.
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS1;
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

// [crispy] Check if printable character is existing in FONTA/FONTB sets
// and do a replacement or case correction if needed.

enum {
    big_font, small_font
} fontsize_t;

static const char MN_CheckValidChar (char ascii_index, int have_cursor)
{
    if ((ascii_index > 'Z' + have_cursor && ascii_index < 'a') || ascii_index > 'z')
    {
        // Replace "\]^_`" and "{|}~" with spaces,
        // allow "[" (cursor symbol) only in small fonts.
        return ' ';
    }
    else if (ascii_index >= 'a' && ascii_index <= 'z')
    {
        // Force lowercase "a...z" characters to uppercase "A...Z".
        return ascii_index + 'A' - 'a';
    }
    else
    {
        // Valid char, do not modify it's ASCII index.
        return ascii_index;
    }
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
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

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
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

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
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

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
        c = MN_CheckValidChar(c, small_font); // [crispy] check for valid characters

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

void MN_DrTextB(const char *text, int x, int y, byte *table)
{
    char c;
    patch_t *p;

    dp_translation = table;

    while ((c = *text++) != 0)
    {
        c = MN_CheckValidChar(c, big_font); // [crispy] check for valid characters

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

    dp_translation = NULL;
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
        c = MN_CheckValidChar(c, big_font); // [crispy] check for valid characters

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

    // [JN] Don't go any farther with effects while active info screens.
    
    if (InfoType)
    {
        return;
    }

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

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

char *QuitEndMsg[] = {
    "ARE YOU SURE YOU WANT TO QUIT?",
    "ARE YOU SURE YOU WANT TO END THE GAME?",
    "DO YOU WANT TO QUICKSAVE THE GAME NAMED",
    "DO YOU WANT TO QUICKLOAD THE GAME NAMED",
    "DO YOU WANT TO DELETE THE GAME NAMED",        // [crispy] typeofask 5 (delete a savegame)
    "RESET KEYBOARD BINDINGS TO DEFAULT VALUES?",  // [JN] typeofask 6 (reset keyboard binds)
    "RESET MOUSE BINDINGS TO DEFAULT VALUES?",     // [JN] typeofask 7 (reset mouse binds)
};

void MN_Drawer(void)
{
    int i;
    int x;
    int y;
    MenuItem_t *item;
    const char *message;
    const char *selName;

    if (MenuActive || typeofask)
    {
        // Temporary unshade while changing certain settings.
        if (shade_wait < I_GetTime())
        {
            M_ShadeBackground();
        }
        // Always redraw status bar background.
        SB_ForceRedraw();
    }

    if (MenuActive == false)
    {
        if (askforquit)
        {
            message = DEH_String(QuitEndMsg[typeofask - 1]);

            // [JN] Keep backgound filling while asking for 
            // reset and inform about Y or N pressing.
            if (typeofask == 6 || typeofask == 7)
            {
                M_FillBackground();
                MN_DrTextACentered("PRESS Y OR N.", 100, NULL);
            }

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
            if (typeofask == 5)
            {
                MN_DrTextA(SlotText[CurrentItPos], 160 -
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
                MN_DrTextA(DEH_String("?"), 160 +
                           MN_TextAWidth(SlotText[CurrentItPos]) / 2, 90, NULL);
            }
        }
        return;
    }
    else
    {
        if (InfoType)
        {
            MN_DrawInfo();
            return;
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
            if (item->type != ITT_EMPTY && item->text)
            {
                if (CurrentMenu->FontType == SmallFont)
                {
                    MN_DrTextA(DEH_String(item->text), x, y, M_Small_Line_Glow(CurrentMenu->items[i].tics));
                }
                else
                if (CurrentMenu->FontType == BigFont)
                {
                    MN_DrTextB(DEH_String(item->text), x, y, NULL/*M_Big_Line_Glow(CurrentMenu->items[i].tics)*/);
                }
                // [JN] Else, don't draw file slot names (1, 2, 3, ...) in Save/Load menus.
            }
            y += CurrentMenu->FontType == SmallFont ? ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;
            item++;
        }
        
        if (CurrentMenu->FontType == SmallFont)
        {
            y = CurrentMenu->y + (CurrentItPos * ID_MENU_LINEHEIGHT_SMALL);
            MN_DrTextA("*", x - ID_MENU_CURSOR_OFFSET, y, M_Cursor_Glow(cursor_tics));
        }
        else
        {
            y = CurrentMenu->y + (CurrentItPos * ITEM_HEIGHT) + SELECTOR_YOFFSET;
            selName = DEH_String(MenuTime & 16 ? "M_SLCTR1" : "M_SLCTR2");
            V_DrawShadowedPatchRavenOptional(x + SELECTOR_XOFFSET, y,
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
}

// [crispy] support additional pages of savegames
static void DrawSaveLoadBottomLine(const Menu_t *menu)
{
    char pagestr[16];
    static short width;
    const int y = menu->y + ITEM_HEIGHT * SAVES_PER_PAGE;

    if (!width)
    {
        const patch_t *const p = W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE);
        width = SHORT(p->width);
    }
    if (savepage > 0)
        MN_DrTextA("PGUP", menu->x + 1, y, cr[CR_MENU_DARK4]);
    if (savepage < SAVEPAGE_MAX)
        MN_DrTextA("PGDN", menu->x + width - MN_TextAWidth("PGDN"), y, cr[CR_MENU_DARK4]);

    M_snprintf(pagestr, sizeof(pagestr), "PAGE %d/%d", savepage + 1, SAVEPAGE_MAX + 1);
    MN_DrTextA(pagestr, SCREENWIDTH / 2 - MN_TextAWidth(pagestr) / 2, y, cr[CR_MENU_DARK4]);

    // [JN] Print "modified" (or created initially) time of savegame file.
    if (SlotStatus[CurrentItPos] && !FileMenuKeySteal)
    {
        struct stat filestat;
        char filedate[32];

        stat(SV_Filename(CurrentItPos), &filestat);
// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
        strftime(filedate, sizeof(filedate), "%x %X", localtime(&filestat.st_mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        MN_DrTextACentered(filedate, y + 10, cr[CR_MENU_DARK4]);
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawLoadMenu
//
//---------------------------------------------------------------------------

static void DrawLoadMenu(void)
{
    const char *title;

    title = DEH_String(quickloadTitle ? "QUICK LOAD GAME" : "LOAD GAME");

    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&LoadMenu);
    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 1, NULL);
    DrawSaveLoadBottomLine(&LoadMenu);
}

//---------------------------------------------------------------------------
//
// PROC DrawSaveMenu
//
//---------------------------------------------------------------------------

static void DrawSaveMenu(void)
{
    const char *title;

    title = DEH_String(quicksaveTitle ? "QUICK SAVE GAME" : "SAVE GAME");

    if (!slottextloaded)
    {
        MN_LoadSlotText();
    }
    DrawFileSlots(&SaveMenu);
    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 1, NULL);
    DrawSaveLoadBottomLine(&SaveMenu);
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
        int retval;
        filename = SV_Filename(i);
        fp = M_fopen(filename, "rb+");
	free(filename);

        if (!fp)
        {
            SlotText[i][0] = 0; // empty the string
            SlotStatus[i] = 0;
            continue;
        }
        retval = fread(&SlotText[i], 1, SLOTTEXTLEN, fp);
        fclose(fp);
        SlotStatus[i] = retval == SLOTTEXTLEN;
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
        V_DrawShadowedPatchRavenOptional(x, y, W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE), "M_FSLOT");
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
    if (showMessages)
    {
        MN_DrTextB(DEH_String("ON"), 196, 50, NULL);
    }
    else
    {
        MN_DrTextB(DEH_String("OFF"), 196, 50, NULL);
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
    DrawSlider(&Options2Menu, 1, 9, crl_screen_size - 3, true);
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
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T START A NEW GAME IN NETPLAY!", true, NULL);
            break;
        case 2:
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T LOAD A GAME IN NETPLAY!", true, NULL);
            break;
        case 3:                // end game
            CT_SetMessage(&players[consoleplayer],
                          "YOU CAN'T END A GAME IN NETPLAY!", true, NULL);
            break;
    }
    MenuActive = false;
    return false;
}

static void SCNetCheck2(int option)
{
    SCNetCheck(option);
}

//---------------------------------------------------------------------------
//
// PROC SCQuitGame
//
//---------------------------------------------------------------------------

static void SCQuitGame(int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 1;              //quit game
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
}

//---------------------------------------------------------------------------
//
// PROC SCEndGame
//
//---------------------------------------------------------------------------

static void SCEndGame(int option)
{
    if (demoplayback || netgame)
    {
        return;
    }
    if (SCNetCheck(3))
    {
        MenuActive = false;
        askforquit = true;
        typeofask = 2;              //endgame
        if (!netgame && !demoplayback)
        {
            paused = true;
        }
    }
}

//---------------------------------------------------------------------------
//
// PROC SCMessages
//
//---------------------------------------------------------------------------

static void SCMessages(int option)
{
    showMessages ^= 1;
    if (showMessages)
    {
        CT_SetMessage(&players[consoleplayer], DEH_String("MESSAGES ON"), true, NULL);
    }
    else
    {
        CT_SetMessage(&players[consoleplayer], DEH_String("MESSAGES OFF"), true, NULL);
    }
    S_StartSound(NULL, sfx_chat);
}

//---------------------------------------------------------------------------
//
// PROC SCLoadGame
//
//---------------------------------------------------------------------------

static void SCLoadGame(int option)
{
    char *filename;

    if (!SlotStatus[option])
    {                           // slot's empty...don't try and load
        return;
    }

    filename = SV_Filename(option);
    G_LoadGame(filename);
    free(filename);

    MN_DeactivateMenu();
    if (quickload == -1)
    {
        quickload = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
}

static void SCDeleteGame(int option)
{
    char *filename;

    if (!SlotStatus[option])
    {
        return;
    }

    filename = SV_Filename(option);
    remove(filename);
    free(filename);

    CurrentMenu->oldItPos = CurrentItPos;  // [JN] Do not reset cursor position.
    MN_LoadSlotText();
}

//---------------------------------------------------------------------------
//
// PROC SCSaveGame
//
//---------------------------------------------------------------------------

// [crispy] override savegame name if it already starts with a map identifier
static boolean StartsWithMapIdentifier (char *str)
{
    if (strlen(str) >= 4 &&
        toupper(str[0]) == 'E' && isdigit(str[1]) &&
        toupper(str[2]) == 'M' && isdigit(str[3]))
    {
        return true;
    }

    return false;
}

// [JN] Check if Save Game menu should be accessable.
static void SCSaveCheck(int option)
{
    if (!usergame)
    {
        CT_SetMessage(&players[consoleplayer],
                      "YOU CAN'T SAVE IF YOU AREN'T PLAYING", true, NULL);
    }
    else
    {
        SetMenu(MENU_SAVE);
    }
}

static void SCSaveGame(int option)
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
        // [crispy] generate a default save slot name when the user saves to an empty slot
        if (!oldSlotText[0] || StartsWithMapIdentifier(oldSlotText))
          M_snprintf(ptr, sizeof(oldSlotText), "E%dM%d", gameepisode, gamemap);
        while (*ptr)
        {
            ptr++;
        }
        *ptr = '[';
        *(ptr + 1) = 0;
        SlotStatus[option]++;
        currentSlot = option;
        slotptr = ptr - SlotText[option];
        return;
    }
    else
    {
        G_SaveGame(option, SlotText[option]);
        FileMenuKeySteal = false;
        I_StopTextInput();
        MN_DeactivateMenu();
    }
    if (quicksave == -1)
    {
        quicksave = option + 1;
        players[consoleplayer].message = NULL;
        players[consoleplayer].messageTics = 1;
    }
}

//---------------------------------------------------------------------------
//
// PROC SCEpisode
//
//---------------------------------------------------------------------------

static void SCEpisode(int option)
{
    if (gamemode == shareware && option > 1)
    {
        CT_SetMessage(&players[consoleplayer],
                      "ONLY AVAILABLE IN THE REGISTERED VERSION", true, NULL);
    }
    else
    {
        MenuEpisode = option;
        SetMenu(MENU_SKILL);
    }
}

//---------------------------------------------------------------------------
//
// PROC SCSkill
//
//---------------------------------------------------------------------------

static void SCSkill(int option)
{
    G_DeferedInitNew(option, MenuEpisode, 1);
    MN_DeactivateMenu();
}

//---------------------------------------------------------------------------
//
// PROC SCMouseSensi
//
//---------------------------------------------------------------------------

static void SCMouseSensi(int option)
{
    // [crispy] extended range
    mouseSensitivity = M_INT_Slider(mouseSensitivity, 0, 255, option, true);
}

//---------------------------------------------------------------------------
//
// PROC SCSfxVolume
//
//---------------------------------------------------------------------------

static void SCSfxVolume(int option)
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
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static void SCMusicVolume(int option)
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
}

//---------------------------------------------------------------------------
//
// PROC SCScreenSize
//
//---------------------------------------------------------------------------

static void SCScreenSize(int option)
{
    if (option == RIGHT_DIR)
    {
        if (crl_screen_size < 11)
        {
            crl_screen_size++;
        }
    }
    else if (crl_screen_size > 3)
    {
        crl_screen_size--;
    }
    R_SetViewSize(crl_screen_size, detailLevel);
}

//---------------------------------------------------------------------------
//
// PROC SCInfo
//
//---------------------------------------------------------------------------

static void SCInfo(int option)
{
    InfoType = 1;
    S_StartSound(NULL, sfx_dorcls);
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
}

//---------------------------------------------------------------------------
// [crispy] reload current level / go to next level
// adapted from prboom-plus/src/e6y.c:369-449
//---------------------------------------------------------------------------

static int G_ReloadLevel (void)
{
    int result = false;

    if (gamestate == GS_LEVEL)
    {
        // [crispy] restart demos from the map they were started
        if (demorecording)
        {
            gamemap = startmap;
        }
        G_DeferedInitNew(gameskill, gameepisode, gamemap);
        result = true;
    }

    return result;
}

static int G_GotoNextLevel(void)
{
    byte heretic_next[6][9] = {
    {12, 13, 14, 15, 16, 19, 18, 21, 17},
    {22, 23, 24, 29, 26, 27, 28, 31, 25},
    {32, 33, 34, 39, 36, 37, 38, 41, 35},
    {42, 43, 44, 49, 46, 47, 48, 51, 45},
    {52, 53, 59, 55, 56, 57, 58, 61, 54},
    {62, 63, 11, 11, 11, 11, 11, 11, 11}, // E6M4-E6M9 shouldn't be accessible
    };

    int changed = false;

    if (gamemode == shareware)
        heretic_next[0][7] = 11;

    if (gamemode == registered)
        heretic_next[2][7] = 11;

    if (gamestate == GS_LEVEL)
    {
        int epsd, map;

        epsd = heretic_next[gameepisode-1][gamemap-1] / 10;
        map = heretic_next[gameepisode-1][gamemap-1] % 10;

        G_DeferedInitNew(gameskill, epsd, map);
        changed = true;
    }

    return changed;
}

static void MN_ReturnToMenu (void)
{
	Menu_t *cur = CurrentMenu;
	MN_ActivateMenu();
	CurrentMenu = cur;
	CurrentItPos = CurrentMenu->oldItPos;
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
        if (event->type == ev_mouse && mousewait < I_GetTime()
        && !event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
        {
            // [crispy] novert disables controlling the menus with the mouse
            // [JN] Not needed, as menu is fully controllable by mouse wheel and buttons.
            /*
            if (!novert)
            {
                mousey += event->data3;
            }
            */

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
                mousewait = I_GetTime() + 5;
            }

            if (event->data1 & 2)
            {
                if (FileMenuKeySteal)
                {
                    key = KEY_ESCAPE;
                    FileMenuKeySteal = false;
                }
                else
                {
                    key = key_menu_back;
                }
                mousewait = I_GetTime() + 5;
            }

            // [crispy] scroll menus with mouse wheel
            // [JN] Buttons hardcoded to wheel so we won't mix it up with inventory scrolling.
            if (/*mousebprevweapon >= 0 &&*/ event->data1 & (1 << 4/*mousebprevweapon*/))
            {
                key = key_menu_down;
                mousewait = I_GetTime() + 1;
            }
            else
            if (/*mousebnextweapon >= 0 &&*/ event->data1 & (1 << 3/*mousebnextweapon*/))
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
        }
        S_StartSound(NULL, sfx_dorcls);
        return (true);          //make the info screen eat the keypress
    }

    // [JN] Handle keyboard bindings:
    if (KbdIsBinding)
    {
        if (event->type == ev_mouse)
        {
            // Reject mouse buttons, but keep binding active.
            return false;
        }

        if (key == KEY_ESCAPE)
        {
            // Pressing ESC will cancel binding and leave key unchanged.
            keyToBind = 0;
            KbdIsBinding = false;
            return false;
        }
        else
        {
            M_CheckBind(key);
            M_DoBind(keyToBind, key);
            keyToBind = 0;
            KbdIsBinding = false;
            return true;
        }
    }

    // [JN] Disallow keyboard pressing and stop binding
    // while mouse binding is active.
    if (MouseIsBinding)
    {
        if (event->type != ev_mouse)
        {
            btnToBind = 0;
            MouseIsBinding = false;
            return false;
        }
    }


    if ((ravpic && key == KEY_F1) ||
        (key != 0 && key == key_menu_screenshot))
    {
        G_ScreenShot();
        // [JN] Audible feedback.
        S_StartSound(NULL, sfx_itemup);
        return (true);
    }

    if (askforquit)
    {
        if (key == key_menu_confirm
        // [JN] Allow to confirm quit (1) and end game (2) by pressing Enter.
        || (key == key_menu_forward && (typeofask == 1 || typeofask == 2))
        || (event->type == ev_mouse && event->data1 & 1))  // [JN] Confirm by left mouse button.
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
                                 ("PLAYPAL", PU_CACHE));
                    D_StartTitle();     // go to intro/demo mode.
                    break;

                case 3:
                    CT_SetMessage(&players[consoleplayer],
                                  "QUICKSAVING....", false, NULL);
                    FileMenuKeySteal = true;
                    SCSaveGame(quicksave - 1);
                    break;

                case 4:
                    CT_SetMessage(&players[consoleplayer],
                                  "QUICKLOADING....", false, NULL);
                    SCLoadGame(quickload - 1);
                    break;

                case 5:
                    SCDeleteGame(CurrentItPos);
                    MN_ReturnToMenu();
                    break;

                case 6: // [JN] Reset keybinds.
                    M_ResetBinds();
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    MN_ReturnToMenu();
                    break;

                case 7: // [JN] Reset mouse binds.
                    M_ResetMouseBinds();
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    MN_ReturnToMenu();
                    break;

                default:
                    break;
            }

            askforquit = false;
            typeofask = 0;

            return true;
        }
        else
        if (key == key_menu_abort || key == KEY_ESCAPE
        || (event->type == ev_mouse && event->data1 & 2))  // [JN] Cancel by right mouse button.
        {
            // [JN] Do not close reset menus after canceling.
            if (typeofask == 6 || typeofask == 7)
            {
                if (!netgame && !demoplayback)
                {
                    paused = true;
                }
                MenuActive = true;
                askforquit = false;
                typeofask = 0;
            }
            else
            {
                players[consoleplayer].messageTics = 1;  //set the msg to be cleared
                askforquit = false;
                typeofask = 0;
                paused = false;
                return true;
            }
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
                quicksaveTitle = false;  // [JN] "Save game" title.
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
                quickloadTitle = false;  // [JN] "Load game" title.
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
                    // [JN] "Quick save game" title instead of message.
                    quicksaveTitle = true;
                }
                else
                {
                    // [JN] Do not ask for quick save confirmation.
                    S_StartSound(NULL, sfx_dorcls);
                    FileMenuKeySteal = true;
                    SCSaveGame(quicksave - 1);
                }
            }
            return true;
        }
        else if (key == key_menu_endgame)         // F7 (end game)
        {
            if (SCNetCheck(3))
            {
                if (gamestate == GS_LEVEL && !demoplayback)
                {
                    S_StartSound(NULL, sfx_chat);
                    SCEndGame(0);
                }
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
            if (SCNetCheck(2))
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
                    // [JN] "Quick load game" title instead of message.
                    quickloadTitle = true;
                }
                else
                {
                    // [JN] Do not ask for quick load confirmation.
                    SCLoadGame(quickload - 1);
                }
            }
            return true;
        }
        else if (key == key_menu_quit)            // F10 (quit)
        {
            // [JN] Allow to invoke quit in any game state.
            //if (gamestate == GS_LEVEL)
            {
                SCQuitGame(0);
                S_StartSound(NULL, sfx_chat);
            }
            return true;
        }
        // [crispy] those two can be considered as shortcuts for the IDCLEV cheat
        // and should be treated as such, i.e. add "if (!netgame)"
        else if (!netgame && key != 0 && key == key_crl_reloadlevel)
        {
            if (G_ReloadLevel())
            return true;
        }
        else if (!netgame && key != 0 && key == key_crl_nextlevel)
        {
            if (G_GotoNextLevel())
            return true;
        }

    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma)           // F11 (gamma correction)
    {
        crl_gamma = M_INT_Slider(crl_gamma, 0, 14, 1 /*right*/, false);
        CT_SetMessage(&players[consoleplayer], gammalvls[crl_gamma][0], false, NULL);
        CRL_ReloadPalette();
        return true;
    }

    if (!MenuActive)
    {
        // [JN] Open Heretic/CRL menu only by pressing it's keys to allow 
        // certain CRL features to be toggled. This behavior is same to Doom.
        if (key == key_menu_activate || key == key_crl_menu)
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
            // [JN] Go to previous-left menu by pressing Left Arrow.
            if (CurrentMenu->ScrollAR)
            {
                M_ScrollPages(false);
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
            // [JN] Go to next-right menu by pressing Right Arrow.
            if (CurrentMenu->ScrollAR)
            {
                M_ScrollPages(true);
            }
            return (true);
        }
        // [JN] Go to previous-left menu by pressing Page Up key.
        else if (key == KEY_PGUP)
        {
            if (CurrentMenu->ScrollPG)
            {
                M_ScrollPages(false);
            }
            return (true);
        }
        // [JN] Go to next-right menu by pressing Page Down key.
        else if (key == KEY_PGDN)
        {
            if (CurrentMenu->ScrollPG)
            {
                M_ScrollPages(true);
            }
            return (true);
        }
        else if (key == key_menu_forward)    // Activate item (enter)
        {
            if (item->type == ITT_SETMENU)
            {
                if (item->func != NULL)
                {
                    item->func(item->option);
                }
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
                    item->func(item->option);
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
            if (CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
            {
                if (SlotStatus[CurrentItPos])
                {
                    MenuActive = false;
                    askforquit = true;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                    typeofask = 5;
                    S_StartSound(NULL, sfx_chat);
                }
            }
            // [JN] ...or clear key bind.
            else
            if (CurrentMenu == &CRLKbdBinds1 || CurrentMenu == &CRLKbdBinds2
            ||  CurrentMenu == &CRLKbdBinds3 || CurrentMenu == &CRLKbdBinds4
            ||  CurrentMenu == &CRLKbdBinds5 || CurrentMenu == &CRLKbdBinds6
            ||  CurrentMenu == &CRLKbdBinds7 || CurrentMenu == &CRLKbdBinds8
            ||  CurrentMenu == &CRLKbdBinds9)
            {
                M_ClearBind(CurrentItPos);
            }
            // [JN] ...or clear mouse bind.
            else if (CurrentMenu == &CRLMouseBinds)
            {
                M_ClearMouseBind(CurrentItPos);
            }
            return (true);
        }
        else if (charTyped != 0)
        {
            // Jump to menu item based on first letter:

            // [JN] Allow multiple jumps over menu items with
            // same first letters. This behavior is same to Doom.
            for (i = CurrentItPos + 1; i < CurrentMenu->itemCount; i++)
            {
                if (CurrentMenu->items[i].text)
                {
                    if (toupper(charTyped)
                        == toupper(DEH_String(CurrentMenu->items[i].text)[0]))
                    {
                        CurrentItPos = i;
                        S_StartSound(NULL, sfx_switch);
                        return (true);
                    }
                }
            }
            for (i = 0; i <= CurrentItPos; i++)
            {
                if (CurrentMenu->items[i].text)
                {
                    if (toupper(charTyped)
                        == toupper(DEH_String(CurrentMenu->items[i].text)[0]))
                    {
                        CurrentItPos = i;
                        S_StartSound(NULL, sfx_switch);
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
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
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
    y = menu->y + 2 + (item * (bigspacing ? ITEM_HEIGHT : ID_MENU_LINEHEIGHT_SMALL));
    V_DrawShadowedPatchRavenOptional(x - 32, y, W_CacheLumpName(DEH_String("M_SLDLT"), PU_CACHE), "M_SLDLT");
    for (x2 = x, count = width; count--; x2 += 8)
    {
        V_DrawShadowedPatchRavenOptional(x2, y, W_CacheLumpName(DEH_String(count & 1 ? "M_SLDMD1" : "M_SLDMD2"), PU_CACHE),
                                                      count & 1 ? "M_SLDMD1" : "M_SLDMD2");
    }
    V_DrawShadowedPatchRavenOptional(x2, y, W_CacheLumpName(DEH_String("M_SLDRT"), PU_CACHE), "M_SLDRT");

    // [JN] Prevent gem go out of slider bounds.
    if (slot > width - 1)
    {
        slot = width - 1;
    }

    V_DrawPatch(x + 4 + slot * 8, y + 7,
                W_CacheLumpName(DEH_String("M_SLDKB"), PU_CACHE), "M_SLDKB");
}


// =============================================================================
//
//                        [JN] Keyboard binding routines.
//                    Drawing, coloring, checking and binding.
//
// =============================================================================


// -----------------------------------------------------------------------------
// M_NameBind
//  [JN] Convert Doom key number into printable string.
// -----------------------------------------------------------------------------

static struct {
    int key;
    char *name;
} key_names[] = KEY_NAMES_ARRAY_RAVEN;

static char *M_NameBind (int CurrentItPosOn, int key)
{
    if (CurrentItPos == CurrentItPosOn && KbdIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        for (int i = 0; i < arrlen(key_names); ++i)
        {
            if (key_names[i].key == key)
            {
                return key_names[i].name;
            }
        }
    }
    return "---";  // Means empty
}

// -----------------------------------------------------------------------------
// M_StartBind
//  [JN] Indicate that key binding is started (KbdIsBinding), and
//  pass internal number (keyToBind) for binding a new key.
// -----------------------------------------------------------------------------

static void M_StartBind (int keynum)
{
    KbdIsBinding = true;
    keyToBind = keynum;
}

// -----------------------------------------------------------------------------
// M_CheckBind
//  [JN] Check if pressed key is already binded, clear previous bind if found.
// -----------------------------------------------------------------------------

static void M_CheckBind (int key)
{
    // Page 1
    if (key_up == key)               key_up               = 0;
    if (key_down == key)             key_down             = 0;
    if (key_left == key)             key_left             = 0;
    if (key_right == key)            key_right            = 0;
    if (key_strafeleft == key)       key_strafeleft       = 0;
    if (key_straferight == key)      key_straferight      = 0;
    if (key_speed == key)            key_speed            = 0;
    if (key_strafe == key)           key_strafe           = 0;
    if (key_180turn == key)          key_180turn          = 0;
    if (key_fire == key)             key_fire             = 0;
    if (key_use == key)              key_use              = 0;

    // Page 2
    if (key_lookup == key)           key_lookup           = 0;
    if (key_lookdown == key)         key_lookdown         = 0;
    if (key_lookcenter == key)       key_lookcenter       = 0;
    if (key_flyup == key)            key_flyup            = 0;
    if (key_flydown == key)          key_flydown          = 0;
    if (key_flycenter == key)        key_flycenter        = 0;
    if (key_invleft == key)          key_invleft          = 0;
    if (key_invright == key)         key_invright         = 0;
    if (key_useartifact == key)      key_useartifact      = 0;

    // Page 3
    if (key_crl_menu == key)         key_crl_menu         = 0;
    if (key_crl_reloadlevel == key)  key_crl_reloadlevel  = 0;
    if (key_crl_nextlevel == key)    key_crl_nextlevel    = 0;
    if (key_crl_demospeed == key)    key_crl_demospeed    = 0;
    if (key_crl_spectator == key)    key_crl_spectator    = 0;
    if (key_crl_cameraup == key)     key_crl_cameraup     = 0;
    if (key_crl_cameradown == key)   key_crl_cameradown   = 0;
    if (key_crl_freeze == key)       key_crl_freeze       = 0;
    if (key_crl_notarget == key)     key_crl_notarget     = 0;

    // Page 4
    if (key_crl_autorun == key)      key_crl_autorun      = 0;
    if (key_crl_mlook == key)        key_crl_mlook        = 0;
    if (key_crl_vilebomb == key)     key_crl_vilebomb     = 0;
    if (key_crl_clearmax == key)     key_crl_clearmax     = 0;
    if (key_crl_movetomax == key)    key_crl_movetomax    = 0;
    if (key_crl_iddqd == key)        key_crl_iddqd        = 0;
    if (key_crl_idfa == key)         key_crl_idfa         = 0;
    if (key_crl_idclip == key)       key_crl_idclip       = 0;
    if (key_crl_iddt == key)         key_crl_iddt         = 0;

    // Page 5
    if (key_weapon1 == key)          key_weapon1          = 0;
    if (key_weapon2 == key)          key_weapon2          = 0;
    if (key_weapon3 == key)          key_weapon3          = 0;
    if (key_weapon4 == key)          key_weapon4          = 0;
    if (key_weapon5 == key)          key_weapon5          = 0;
    if (key_weapon6 == key)          key_weapon6          = 0;
    if (key_weapon7 == key)          key_weapon7          = 0;
    if (key_weapon8 == key)          key_weapon8          = 0;
    if (key_prevweapon == key)       key_prevweapon       = 0;
    if (key_nextweapon == key)       key_nextweapon       = 0;

    // Page 6
    if (key_arti_quartz == key)      key_arti_quartz      = 0;
    if (key_arti_urn == key)         key_arti_urn         = 0;
    if (key_arti_bomb == key)        key_arti_bomb        = 0;
    if (key_arti_tome == key)        key_arti_tome        = 0;
    if (key_arti_ring == key)        key_arti_ring        = 0;
    if (key_arti_chaosdevice == key) key_arti_chaosdevice = 0;
    if (key_arti_shadowsphere == key) key_arti_shadowsphere = 0;
    if (key_arti_wings == key)       key_arti_wings       = 0;
    if (key_arti_torch == key)       key_arti_torch       = 0;
    if (key_arti_morph == key)       key_arti_morph       = 0;

    // Page 7
    if (key_map_toggle == key)       key_map_toggle       = 0;
    if (key_map_zoomin == key)       key_map_zoomin       = 0;
    if (key_map_zoomout == key)      key_map_zoomout      = 0;
    if (key_map_maxzoom == key)      key_map_maxzoom      = 0;
    if (key_map_follow == key)       key_map_follow       = 0;
//  if (key_crl_map_rotate == key)   key_crl_map_rotate   = 0;
//  if (key_crl_map_overlay == key)  key_crl_map_overlay  = 0;
    if (key_map_grid == key)         key_map_grid         = 0;

    // Page 8
    if (key_menu_help == key)        key_menu_help        = 0;
    if (key_menu_save == key)        key_menu_save        = 0;
    if (key_menu_load == key)        key_menu_load        = 0;
    if (key_menu_volume == key)      key_menu_volume      = 0;
    if (key_menu_qsave == key)       key_menu_qsave       = 0;
    if (key_menu_endgame == key)     key_menu_endgame     = 0;
    if (key_menu_messages == key)    key_menu_messages    = 0;
    if (key_menu_qload == key)       key_menu_qload       = 0;
    if (key_menu_quit == key)        key_menu_quit        = 0;
    if (key_menu_gamma == key)       key_menu_gamma       = 0;
    if (key_spy == key)              key_spy              = 0;

    // Page 9
    if (key_pause == key)              key_pause           = 0;
    if (key_menu_screenshot == key)    key_menu_screenshot = 0;
    if (key_demo_quit == key)          key_demo_quit       = 0;
    if (key_multi_msg == key)          key_multi_msg       = 0;
    if (key_multi_msgplayer[0] == key) key_multi_msgplayer[0] = 0;
    if (key_multi_msgplayer[1] == key) key_multi_msgplayer[1] = 0;
    if (key_multi_msgplayer[2] == key) key_multi_msgplayer[2] = 0;
    if (key_multi_msgplayer[3] == key) key_multi_msgplayer[3] = 0;
}

// -----------------------------------------------------------------------------
// M_DoBind
//  [JN] By catching internal bind number (keynum), do actual binding
//  of pressed key (key) to real keybind.
// -----------------------------------------------------------------------------

static void M_DoBind (int keynum, int key)
{
    switch (keynum)
    {
        // Page 1
        case 100:  key_up = key;                break;
        case 101:  key_down = key;              break;
        case 102:  key_left = key;              break;
        case 103:  key_right = key;             break;
        case 104:  key_strafeleft = key;        break;
        case 105:  key_straferight = key;       break;
        case 106:  key_speed = key;             break;
        case 107:  key_strafe = key;            break;
        case 108:  key_180turn = key;           break;
        case 109:  key_fire = key;              break;
        case 110:  key_use = key;               break;

        // Page 2  
        case 200:  key_lookup = key;            break;
        case 201:  key_lookdown = key;          break;
        case 202:  key_lookcenter = key;        break;
        case 203:  key_flyup = key;             break;
        case 204:  key_flydown = key;           break;
        case 205:  key_flycenter = key;         break;
        case 206:  key_invleft = key;           break;
        case 207:  key_invright = key;          break;
        case 208:  key_useartifact = key;       break;

        // Page 3  
        case 300:  key_crl_menu = key;          break;
        case 301:  key_crl_reloadlevel = key;   break;
        case 302:  key_crl_nextlevel = key;     break;
        case 303:  key_crl_demospeed = key;     break;
        case 304:  key_crl_spectator = key;     break;
        case 305:  key_crl_cameraup = key;      break;
        case 306:  key_crl_cameradown = key;    break;
        case 307:  key_crl_freeze = key;        break;
        case 308:  key_crl_notarget = key;      break;

        // Page 4  
        case 400:  key_crl_autorun = key;       break;
        case 401:  key_crl_mlook = key;         break;
        case 402:  key_crl_vilebomb = key;      break;
        case 403:  key_crl_clearmax = key;      break;
        case 404:  key_crl_movetomax = key;     break;
        case 405:  key_crl_iddqd = key;         break;
        case 406:  key_crl_idfa = key;          break;
        case 407:  key_crl_idclip = key;        break;
        case 408:  key_crl_iddt = key;          break;

        // Page 5  
        case 500:  key_weapon1 = key;           break;
        case 501:  key_weapon2 = key;           break;
        case 502:  key_weapon3 = key;           break;
        case 503:  key_weapon4 = key;           break;
        case 504:  key_weapon5 = key;           break;
        case 505:  key_weapon6 = key;           break;
        case 506:  key_weapon7 = key;           break;
        case 507:  key_weapon8 = key;           break;
        case 508:  key_prevweapon = key;        break;
        case 509:  key_nextweapon = key;        break;

        // Page 6  
        case 600:  key_arti_quartz = key;       break;
        case 601:  key_arti_urn = key;          break;
        case 602:  key_arti_bomb = key;         break;
        case 603:  key_arti_tome = key;         break;
        case 604:  key_arti_ring = key;         break;
        case 605:  key_arti_chaosdevice = key;  break;
        case 606:  key_arti_shadowsphere = key; break;
        case 607:  key_arti_wings = key;        break;
        case 608:  key_arti_torch = key;        break;
        case 609:  key_arti_morph = key;        break;

        // Page 7
        if (CurrentMenu == &CRLKbdBinds7)
        {
        case 700:  key_map_toggle = key;        break;
        case 701:  key_map_zoomin = key;        break;
        case 702:  key_map_zoomout = key;       break;
        case 703:  key_map_maxzoom = key;       break;
        case 704:  key_map_follow = key;        break;
//      case 705:  key_crl_map_rotate = key;    break;
//      case 706:  key_crl_map_overlay = key;   break;
        case 705:  key_map_grid = key;          break;
        }

        // Page 8
        case 800:  key_menu_help = key;         break;
        case 801:  key_menu_save = key;         break;
        case 802:  key_menu_load = key;         break;
        case 803:  key_menu_volume = key;       break;
        case 804:  key_menu_qsave = key;        break;
        case 805:  key_menu_endgame = key;      break;
        case 806:  key_menu_messages = key;     break;
        case 807:  key_menu_qload = key;        break;
        case 808:  key_menu_quit = key;         break;
        case 809:  key_menu_gamma = key;        break;
        case 810:  key_spy = key;               break;

        // Page 9
        case 900:  key_pause = key;              break;
        case 901:  key_menu_screenshot = key;    break;
        case 902:  key_demo_quit = key;          break;
        case 903:  key_multi_msg = key;          break;
        if (CurrentMenu == &CRLKbdBinds9)
        {
        case 904:  key_multi_msgplayer[0] = key; break;
        case 905:  key_multi_msgplayer[1] = key; break;
        case 906:  key_multi_msgplayer[2] = key; break;
        case 907:  key_multi_msgplayer[3] = key; break;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ClearBind
//  [JN] Clear key bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------

static void M_ClearBind (int CurrentItPos)
{
    if (CurrentMenu == &CRLKbdBinds1)
    {
        switch (CurrentItPos)
        {
            case 0:   key_up = 0;               break;
            case 1:   key_down = 0;             break;
            case 2:   key_left = 0;             break;
            case 3:   key_right = 0;            break;
            case 4:   key_strafeleft = 0;       break;
            case 5:   key_straferight = 0;      break;
            case 6:   key_speed = 0;            break;
            case 7:   key_strafe = 0;           break;
            case 8:   key_180turn = 0;          break;
            // Action title
            case 10:  key_fire = 0;             break;
            case 11:  key_use = 0;              break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds2)
    {
        switch (CurrentItPos)
        {
            case 0:   key_lookup = 0;           break;
            case 1:   key_lookdown = 0;         break;
            case 2:   key_lookcenter = 0;       break;
            // Flying title
            case 4:   key_flyup = 0;            break;
            case 5:   key_flydown = 0;          break;
            case 6:   key_flycenter = 0;        break;
            // Inventory title
            case 8:   key_invleft = 0;          break;
            case 9:   key_invright = 0;         break;
            case 10:  key_useartifact = 0;      break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds3)
    {
        switch (CurrentItPos)
        {
            case 0:   key_crl_menu = 0;         break;
            case 1:   key_crl_reloadlevel = 0;  break;
            case 2:   key_crl_nextlevel = 0;    break;
            case 3:   key_crl_demospeed = 0;    break;
            // Game modes title
            case 5:   key_crl_spectator = 0;    break;
            case 6:   key_crl_cameraup = 0;     break;
            case 7:   key_crl_cameradown = 0;   break;
            case 8:   key_crl_freeze = 0;       break;
            case 9:   key_crl_notarget = 0;     break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds4)
    {
        switch (CurrentItPos)
        {
            case 0:   key_crl_autorun = 0;      break;
            case 1:   key_crl_mlook = 0;        break;
            case 2:   key_crl_vilebomb = 0;     break;
            // Visplanes max value title
            case 4:   key_crl_clearmax = 0;     break;
            case 5:   key_crl_movetomax = 0;    break;
            // Visplanes max value title
            case 7:   key_crl_iddqd = 0;        break;
            case 8:   key_crl_idfa = 0;         break;
            case 9:   key_crl_idclip = 0;       break;
            case 10:  key_crl_iddt = 0;         break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds5)
    {
        switch (CurrentItPos)
        {
            case 0:   key_weapon1 = 0;          break;
            case 1:   key_weapon1 = 0;          break;
            case 2:   key_weapon1 = 0;          break;
            case 3:   key_weapon1 = 0;          break;
            case 4:   key_weapon1 = 0;          break;
            case 5:   key_weapon1 = 0;          break;
            case 6:   key_weapon1 = 0;          break;
            case 7:   key_weapon1 = 0;          break;
            case 8:   key_prevweapon = 0;       break;
            case 9:   key_nextweapon = 0;       break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds6)
    {
        switch (CurrentItPos)
        {
            case 0:   key_arti_quartz = 0;      break;
            case 1:   key_arti_urn = 0;         break;
            case 2:   key_arti_bomb = 0;        break;
            case 3:   key_arti_tome = 0;        break;
            case 4:   key_arti_ring = 0;        break;
            case 5:   key_arti_chaosdevice = 0; break;
            case 6:   key_arti_shadowsphere = 0; break;
            case 7:   key_arti_wings = 0;       break;
            case 8:   key_arti_torch = 0;       break;
            case 9:   key_arti_morph = 0;       break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds7)
    {
        switch (CurrentItPos)
        {
            case 0:   key_map_toggle = 0;       break;
            case 1:   key_map_zoomin = 0;       break;
            case 2:   key_map_zoomout = 0;      break;
            case 3:   key_map_maxzoom = 0;      break;
            case 4:   key_map_follow = 0;       break;
//          case 5:   key_crl_map_rotate = 0;   break;
//          case 6:   key_crl_map_overlay = 0;  break;
            case 5:   key_map_grid = 0;         break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds8)
    {
        switch (CurrentItPos)
        {
            case 0:   key_menu_help = 0;        break;
            case 1:   key_menu_save = 0;        break;
            case 2:   key_menu_load = 0;        break;
            case 3:   key_menu_volume = 0;      break;
            case 4:   key_menu_qsave = 0;       break;
            case 5:   key_menu_endgame = 0;     break;
            case 6:   key_menu_messages = 0;    break;
            case 7:   key_menu_qload = 0;       break;
            case 8:   key_menu_quit = 0;        break;
            case 9:   key_menu_gamma = 0;       break;
            case 10:  key_spy = 0;              break;
        }
    }
    if (CurrentMenu == &CRLKbdBinds9)
    {
        switch (CurrentItPos)
        {
            case 0:   key_pause = 0;              break;
            case 1:   key_menu_screenshot = 0;    break;
            case 2:   key_demo_quit = 0;          break;
            // Multiplayer title
            case 4:   key_multi_msg = 0;          break;
            case 5:   key_multi_msgplayer[0] = 0; break;
            case 6:   key_multi_msgplayer[1] = 0; break;
            case 7:   key_multi_msgplayer[2] = 0; break;
            case 8:   key_multi_msgplayer[3] = 0; break;

        }
    }
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all keyboard binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetBinds (void)
{
    // Page 1
    key_up = 'w';
    key_down = 's'; 
    key_left = KEY_LEFTARROW;
    key_right = KEY_RIGHTARROW;
    key_strafeleft = 'a';
    key_straferight = 'd';
    key_speed = KEY_RSHIFT; 
    key_strafe = KEY_RALT;
    key_180turn = 0;
    key_fire = KEY_RCTRL;
    key_use = ' ';

    // Page 2
    key_lookup = KEY_PGDN;
    key_lookdown = KEY_DEL;
    key_lookcenter = KEY_END;
    key_flyup = KEY_PGUP;
    key_flydown = KEY_INS;
    key_flycenter = KEY_HOME;
    key_invleft = '[';
    key_invright = ']';
    key_useartifact = KEY_ENTER;

    // Page 3
    key_crl_menu = '`';
    key_crl_reloadlevel = 0;
    key_crl_nextlevel = 0;
    key_crl_demospeed = 0;
    key_crl_spectator = 0;
    key_crl_cameraup = 0;
    key_crl_cameradown = 0;
    key_crl_freeze = 0;
    key_crl_notarget = 0;

    // Page 4
    key_crl_autorun = KEY_CAPSLOCK;
    key_crl_mlook = 0;
    key_crl_vilebomb = 0;
    key_crl_clearmax = 0;
    key_crl_movetomax = 0;
    key_crl_iddqd = 0;
    key_crl_idfa = 0;
    key_crl_idclip = 0;
    key_crl_iddt = 0;

    // Page 5
    key_weapon1 = '1';
    key_weapon2 = '2';
    key_weapon3 = '3';
    key_weapon4 = '4';
    key_weapon5 = '5';
    key_weapon6 = '6';
    key_weapon7 = '7';
    key_weapon8 = '8';
    key_prevweapon = 0;
    key_nextweapon = 0;

    // Page 6
    key_arti_quartz = 0;
    key_arti_urn = 0;
    key_arti_bomb = 0;
    key_arti_tome = 127;
    key_arti_ring = 0;
    key_arti_chaosdevice = 0;
    key_arti_shadowsphere = 0;
    key_arti_wings = 0;
    key_arti_torch = 0;
    key_arti_morph = 0;

    // Page 7
    key_map_toggle = KEY_TAB;
    key_map_zoomin = '=';
    key_map_zoomout = '-';
    key_map_maxzoom = '0';
    key_map_follow = 'f';
    key_crl_map_rotate = 'r';
    key_crl_map_overlay = 'o';
    key_map_grid = 'g';

    // Page 8
    key_menu_help = KEY_F1;
    key_menu_save = KEY_F2;
    key_menu_load = KEY_F3;
    key_menu_volume = KEY_F4;
    key_menu_qsave = KEY_F6;
    key_menu_endgame = KEY_F7;
    key_menu_messages = KEY_F8;
    key_menu_qload = KEY_F9;
    key_menu_quit = KEY_F10;
    key_menu_gamma = KEY_F11;
    key_spy = KEY_F12;

    // Page 9
    key_pause = KEY_PAUSE;
    key_menu_screenshot = KEY_PRTSCR;
    key_demo_quit = 'q';
    key_multi_msg = 't';
    key_multi_msgplayer[0] = 'g';
    key_multi_msgplayer[1] = 'i';
    key_multi_msgplayer[2] = 'b';
    key_multi_msgplayer[3] = 'r';
}

// -----------------------------------------------------------------------------
// M_ColorizeBind
//  [JN] Do key bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeBind (int CurrentItPosOn, int key)
{
    if (CurrentItPos == CurrentItPosOn && KbdIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        if (key == 0)
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

// -----------------------------------------------------------------------------
// M_DrawBindKey
//  [JN] Do keyboard bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindKey (int itemNum, int yPos, int keyBind)
{
    MN_DrTextA(M_NameBind(itemNum, keyBind),
               M_ItemRightAlign(M_NameBind(itemNum, keyBind)),
               yPos,
               M_ColorizeBind(itemNum, keyBind));
}

// -----------------------------------------------------------------------------
// M_DrawBindFooter
//  [JN] Draw footer in key binding pages with numeration.
// -----------------------------------------------------------------------------

static void M_DrawBindFooter (int y, char *pagenum, boolean drawPages)
{
    const int newline = 10;

    MN_DrTextACentered("PRESS ENTER TO BIND, DEL TO CLEAR", y, cr[CR_GRAY]);
    
    if (drawPages)
    {
        MN_DrTextA("PGUP", CRL_MENU_LEFTOFFSET, y + newline, cr[CR_GRAY]);
        MN_DrTextACentered(M_StringJoin("PAGE ", pagenum, "/9", NULL), y + newline, cr[CR_GRAY]);
        MN_DrTextA("PGDN", M_ItemRightAlign("PGDN"), y + newline, cr[CR_GRAY]);
    }
}


// =============================================================================
//
//                          [JN] Mouse binding routines.
//                    Drawing, coloring, checking and binding.
//
// =============================================================================


// -----------------------------------------------------------------------------
// M_NameBind
//  [JN] Draw mouse button number as printable string.
// -----------------------------------------------------------------------------

static char *M_NameMouseBind (int CurrentItPosOn, int btn)
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
    if (mousebinvleft == btn)     mousebinvleft     = -1;
    if (mousebinvright == btn)    mousebinvright    = -1;
    if (mousebuseartifact == btn) mousebuseartifact = -1;
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
        case 1009:  mousebinvleft = btn;      break;
        case 1010:  mousebinvright = btn;     break;
        case 1011:  mousebuseartifact = btn;  break;
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
        case 0:   mousebfire = -1;         break;
        case 1:   mousebforward = -1;      break;
        case 2:   mousebstrafe = -1;       break;
        case 3:   mousebbackward = -1;     break;
        case 4:   mousebuse = -1;          break;
        case 5:   mousebstrafeleft = -1;   break;
        case 6:   mousebstraferight = -1;  break;
        case 7:   mousebprevweapon = -1;   break;
        case 8:   mousebnextweapon = -1;   break;
        case 9:   mousebinvleft = -1;      break;
        case 10:  mousebinvright = -1;     break;
        case 11:  mousebuseartifact = -1;  break;
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

// -----------------------------------------------------------------------------
// M_DrawBindButton
//  [JN] Do mouse button bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindButton (int itemNum, int yPos, int btnBind)
{
    MN_DrTextA(M_NameMouseBind(itemNum, btnBind),
               M_ItemRightAlign(M_NameMouseBind(itemNum, btnBind)),
               yPos,
               M_ColorizeMouseBind(itemNum, btnBind));
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all mouse binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    mousebfire = 0;
    mousebforward = 2;
    mousebstrafe = 1;
    mousebbackward = -1;
    mousebuse = -1;
    mousebstrafeleft = -1;
    mousebstraferight = -1;
    mousebprevweapon = 4;
    mousebnextweapon = 3;
    mousebinvleft = -1;
    mousebinvright = -1;
    mousebuseartifact = -1;
}
