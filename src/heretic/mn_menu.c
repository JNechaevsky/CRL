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

// MN_menu.c

#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "ct_chat.h"
#include "deh_str.h"
#include "doomdef.h"
#include "doomkeys.h"
#include "gusconf.h"
#include "i_input.h"
#include "i_system.h"
#include "i_swap.h"
#include "i_timer.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "v_savepreview.h"
#include "r_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "g_rewind.h"
#include "am_map.h"

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

// [PN] Save/Load preview area (original 320x200 coordinate space).
#define SAVE_PREVIEW_X 236
#define SAVE_PREVIEW_Y 23

// Types

typedef enum
{
    ITT_EMPTY,
    ITT_EFUNC,
    ITT_LRFUNC1,    // Multichoice function: increase by wheel up, decrease by wheel down
    ITT_LRFUNC2,    // Multichoice function: decrease by wheel up, increase by wheel down
    ITT_SETMENU,
    ITT_SLDR,       // Slider line.
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
    MENU_CRLAUTOMAP,
    MENU_CRLGAMEPLAY,
    MENU_MISC_1,
    MENU_MISC_2,
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
    int itemCount;      // [PN] Automatic count via ITEMCOUNT() macro below
    MenuItem_t *items;
    int oldItPos;
    int FontType;       // [JN] 0 = no font, 1 = small font, 2 = big font
    boolean ScrollAR;   // [JN] Menu can be scrolled by arrow keys
    boolean ScrollPG;   // [JN] Menu can be scrolled by PGUP/PGDN keys
    MenuType_t prevMenu;
} Menu_t;

#define ITEMCOUNT(items) (sizeof(items) / sizeof((items)[0]))

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
static void DrawSavePreview(const Menu_t *menu);
static void DrawSavePreviewBorder(int x, int y, int w, int h);
static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing, int itemPos);
static void MN_DeactivateMenu(void);
static void MN_LoadSlotText(void);

inline static void M_ID_MenuMouseControl (void);
inline static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max);

// Public Data

boolean MenuActive;
int InfoType;

// Private Data

static int FontABaseLump;
static int FontBBaseLump;
static int SkullBaseLump;
static Menu_t *CurrentMenu;
static int CurrentItPos;    // -1 = no selection
static int MenuEpisode;
static int MenuTime;

boolean askforquit;
static int typeofask;
static boolean FileMenuKeySteal;
static boolean slottextloaded;
static char SlotText[SAVES_PER_PAGE][SLOTTEXTLEN + 2];
static byte SlotPreview[SAVES_PER_PAGE][V_SAVEPREVIEW_SIZE];
static char oldSlotText[SLOTTEXTLEN + 2];
static int SlotStatus[SAVES_PER_PAGE];
static boolean SlotPreviewStatus[SAVES_PER_PAGE];

typedef struct
{
    byte skill;
    byte episode;
    byte map;
    int leveltime;
    boolean present;
} savegame_meta_t;

static savegame_meta_t SlotMeta[SAVES_PER_PAGE];
static const char *const SaveSkillShortName[5] = { "WN", "YB", "BR", "SM", "BP" };

static int slotptr;
static int currentSlot;
static int quicksave;
static int quickload;
static boolean MenuWasPaused;

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
    ITEMCOUNT(MainItems), MainItems,
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
    ITEMCOUNT(FilesItems), FilesItems,
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
    34, 18,
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
    34, 18,
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
    ITEMCOUNT(SkillItems), SkillItems,
    2,
    BigFont, false, false,
    MENU_EPISODE
};

static MenuItem_t OptionsItems[] = {
    {ITT_EFUNC, "END GAME", SCEndGame, 0, MENU_NONE},
    {ITT_LRFUNC1, "MESSAGES : ", SCMessages, 0, MENU_NONE},
    {ITT_SLDR, "MOUSE SENSITIVITY", SCMouseSensi, 0, MENU_NONE},
    {ITT_EMPTY, NULL, NULL, 0, MENU_NONE},
    {ITT_SETMENU, "MORE...", NULL, 0, MENU_OPTIONS2}
};

static Menu_t OptionsMenu = {
    88, 30,
    DrawOptionsMenu,
    ITEMCOUNT(OptionsItems), OptionsItems,
    0,
    BigFont, false, false,
    MENU_CRLMAIN
};

static MenuItem_t Options2Items[] = {
    { ITT_SLDR,   "SFX VOLUME",   SCSfxVolume,   0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
    { ITT_SLDR,   "MUSIC VOLUME", SCMusicVolume, 0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
    { ITT_SLDR,   "SCREEN SIZE",  SCScreenSize,  0, MENU_NONE },
    { ITT_EMPTY,  NULL,           NULL,          0, MENU_NONE },
};

static Menu_t Options2Menu = {
    72, 20,
    DrawOptions2Menu,
    ITEMCOUNT(Options2Items), Options2Items,
    0,
    BigFont, false, false,
    MENU_OPTIONS
};

// =============================================================================
// [JN] CRL custom menu
// =============================================================================

#define CRL_MENU_TOPOFFSET     (20)
#define CRL_MENU_LEFTOFFSET    (48)
#define CRL_MENU_LEFTOFFSET_BIG    (32)
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
static void CRL_Buddha (int option);
static void CRL_NoTarget (int option);
static void CRL_NoMomentum (int option);
static void CRL_GameSpeed (int choice);

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
static void CRL_ScreenWipe (int option);
static void CRL_EndText (int option);

static void DrawCRLDisplay (void);
static void CRL_TextShadows (int option);

static void DrawCRLSound (void);
static void CRL_MusicSystem (int option);
static void CRL_SFXMode (int option);
static void CRL_PitchShift (int option);
static void CRL_SFXChannels (int option);
static void M_CRL_MuteInactive (int choice);

static void DrawCRLControls (void);
static void CRL_Controls_Sensivity_y(int option);
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
static void M_Bind_PrevLevel (int option);
static void M_Bind_RestartLevel (int option);
static void M_Bind_NextLevel (int option);
static void M_Bind_FastForward (int option);
static void M_Bind_ExtendedHUD (int choice);
static void M_Bind_SpectatorMode (int option);
static void M_Bind_CameraUp (int option);
static void M_Bind_CameraDown (int option);
static void M_Bind_CameraMoveTo (int option);
static void M_Bind_FreezeMode (int option);
static void M_Bind_BuddhaMode (int option);
static void M_Bind_NotargetMode (int option);
static void M_Bind_NomomentumMode (int option);

static void DrawCRLKbd4 (void);
static void M_Bind_AlwaysRun (int option);
static void M_Bind_MouseLook (int option);
static void M_Bind_NoVert (int option);
static void M_Bind_VileBomb (int option);
static void M_Bind_VileFly (int option);
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
static void M_Bind_RotateMode (int option);
static void M_Bind_OverlayMode (int option);
static void M_Bind_SndPropMode (int option);
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
static void M_Bind_M_SpeedOn (int option);
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
static void CRL_Widget_KIS_Format (int option);
static void CRL_Widget_KIS_Items (int option);
static void CRL_Widget_Time (int option);
static void CRL_Widget_Coords (int option);
static void CRL_Widget_CoordsFrac (int option);
static void CRL_Widget_Speed (int option);
static void CRL_Widget_Powerups (int option);
static void CRL_Widget_Health (int option);

static void DrawCRLAutomap (void);
static void CRL_Automap_TexturedBg (int choice);
static void CRL_Automap_ScrollBg (int choice);
static void CRL_Automap_Rotate (int option);
static void CRL_Automap_Overlay (int option);
static void CRL_Automap_Shading (int option);
static void CRL_Automap_Pan (int option);
static void CRL_Automap_Secrets (int option);
static void CRL_Automap_SndProp (int option);

static void DrawCRLGameplay (void);
static void CRL_DefaulSkill (int option);
static void CRL_PistolStart (int option);
static void CRL_RevealedSecrets (int option);
static void CRL_RestoreTargets (int option);
static void CRL_OnDeathAction (int option);
static void CRL_ColoredSBar (int option);
static void CRL_AmmoWidget (int option);
static void CRL_AmmoWidgetTranslucent (int option);
static void CRL_AmmoWidgetColors (int option);
static void CRL_DemoTimer (int option);
static void CRL_TimerDirection (int option);
static void CRL_ProgressBar (int option);
static void CRL_InternalDemos (int option);

static void DrawCRLMisc_1 (void);
static void CRL_Invul (int option);
static void CRL_PalFlash (int option);
static void CRL_MoveBob (int option);
static void CRL_WeaponBob (int option);
static void CRL_Colorblind (int option);
static void CRL_AutoloadWAD (int option);
static void CRL_AutoloadDEH (int option);
static void CRL_Hightlight (int option);
static void CRL_MenuEscKey (int option);
static void CRL_ConfirmQuit (int option);
static void CRL_MenuCapFps (int option);

static void DrawCRLMisc_2 (void);
static void CRL_Misc_RewindEnable (int option);
static void CRL_Misc_RewindInterwal (int option);
static void CRL_Misc_RewindDepth (int option);
static void CRL_Misc_RewindTimeout (int option);
static void CRL_Misc_ShotFormat (int option);
static void CRL_Misc_ShotSetup (int option);

static void M_ScrollMisc (int option);

static void DrawCRLLimits (void);
static void CRL_UnknownLineWarning (int option);
static void CRL_SaveSizeWarning (int option);
static void CRL_Limits (int option);

// Keyboard binding prototypes
static boolean KbdIsBinding;
static int     keyToBind;

static char   *M_MakeBindName (int CurrentItPosOn, int key, int type);
static char   *M_NameBind (int CurrentItPosOn, int key1, int key2, int type);
static void    M_StartBind (int keynum);
static void    M_CheckBind (int key);
static void    M_DoBind (int keynum, int key);
static void    M_ClearBind (int CurrentItPos);
static byte   *M_ColorizeBind (int itemSetOn, int key1, int key2);
static void    M_ResetBinds (void);
static void    M_DrawBindKey (int itemNum, int yPos, int key1, int key2);
static void    M_DrawBindFooter (char *pagenum, boolean drawPages);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int CurrentItPos);
static byte   *M_ColorizeMouseBind (int itemSetOn, int btn1, int btn2);
static void    M_DrawBindButton (int itemNum, int yPos, int btn1, int btn2);
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

// Remember last misc settings page.
static int Misc_Cur;

static void M_Choose_CRL_Misc (int choice)
{
    SetMenu(Misc_Cur);
}

// [JN/PN] Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // "sfx_switch" sound will be played only if menu will be changed.
    int nextMenu = 0;

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
    else if (CurrentMenu == &CRLKbdBinds1) nextMenu = (direction ? MENU_CRLKBDBINDS2 : MENU_CRLKBDBINDS9);
    else if (CurrentMenu == &CRLKbdBinds2) nextMenu = (direction ? MENU_CRLKBDBINDS3 : MENU_CRLKBDBINDS1);
    else if (CurrentMenu == &CRLKbdBinds3) nextMenu = (direction ? MENU_CRLKBDBINDS4 : MENU_CRLKBDBINDS2);
    else if (CurrentMenu == &CRLKbdBinds4) nextMenu = (direction ? MENU_CRLKBDBINDS5 : MENU_CRLKBDBINDS3);
    else if (CurrentMenu == &CRLKbdBinds5) nextMenu = (direction ? MENU_CRLKBDBINDS6 : MENU_CRLKBDBINDS4);
    else if (CurrentMenu == &CRLKbdBinds6) nextMenu = (direction ? MENU_CRLKBDBINDS7 : MENU_CRLKBDBINDS5);
    else if (CurrentMenu == &CRLKbdBinds7) nextMenu = (direction ? MENU_CRLKBDBINDS8 : MENU_CRLKBDBINDS6);
    else if (CurrentMenu == &CRLKbdBinds8) nextMenu = (direction ? MENU_CRLKBDBINDS1 : MENU_CRLKBDBINDS7);
    else if (CurrentMenu == &CRLKbdBinds9) nextMenu = (direction ? MENU_CRLKBDBINDS1 : MENU_CRLKBDBINDS8);

    // If a new menu was set up, play the navigation sound.
    if (nextMenu)
    {
        SetMenu(nextMenu);
        S_StartSound(NULL, sfx_switch);
    }
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
    switch (crl_menu_highlight)
    {
        case 1:
            return
            tics == 5 ? cr[CR_MENU_BRIGHT2] : cr[CR_MENU_DARK2];

        case 2:
            return
            tics == 5 ? cr[CR_MENU_BRIGHT2] :
            tics == 4 ? cr[CR_MENU_BRIGHT1] :
            tics == 3 ? NULL                :
            tics == 2 ? cr[CR_MENU_DARK1]   :
            tics == 1 ? cr[CR_MENU_DARK2]   : cr[CR_MENU_DARK2];
    }

    return cr[CR_MENU_DARK2];
}

static byte *M_Big_Line_Glow (const int tics)
{
    switch (crl_menu_highlight)
    {
        case 1: 
            return
            tics == 5 ? cr[CR_MENU_BRIGHT3] : NULL;

        case 2:
            return
            tics == 5 ? cr[CR_MENU_BRIGHT3] :
            tics >= 3 ? cr[CR_MENU_BRIGHT2] :
            tics >= 1 ? cr[CR_MENU_BRIGHT1] : NULL;
    }

    return NULL;
}

static void M_Reset_Line_Glow (void)
{
    for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
    {
        CurrentMenu->items[i].tics = 0;
    }

    // [JN] If menu is controlled by mouse, reset "last on" position
    // so this item won't blink upon reentering to the current menu.
    if (menu_mouse_allow)
    {
        CurrentMenu->oldItPos = -1;
    }
}

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
    if (!crl_menu_highlight)
    {
        return
            color == GLOW_RED       ? cr[CR_RED] :
            color == GLOW_DARKRED   ? cr[CR_DARKRED] :
            color == GLOW_GREEN     ? cr[CR_GREEN] :
            color == GLOW_YELLOW    ? cr[CR_YELLOW] :
            color == GLOW_ORANGE    ? cr[CR_ORANGE] :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY] :
            color == GLOW_BLUE      ? cr[CR_BLUE2] :
            color == GLOW_OLIVE     ? cr[CR_OLIVE] :
            color == GLOW_DARKGREEN ? cr[CR_DARKGREEN] :
                                      NULL; // color == GLOW_UNCOLORED
    }

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
    if (!crl_menu_highlight)
    return MenuTime & 16 ? NULL : cr[CR_MENU_DARK4];

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

static int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    const int old_val = val;

    // [PN] Adjust the slider value based on direction and handle min/max limits
    val += (direction == -1) ?  0 :     // [JN] Routine "-1" just reintializes value.
           (direction ==  0) ? -1 : 1;  // Otherwise, move either left "0" or right "1".

    if (val < min)
        val = capped ? min : max;
    else
    if (val > max)
        val = capped ? max : min;

    // [JN] Play sound only if value was really changed.
    // [PN] For line items (ITT_LRFUNC1/2), sound is handled in MN_Responder
    // to keep keyboard/mouse/wheel behavior consistent and avoid doubles.
    if (old_val != val)
    {
        if (!(MenuActive && CurrentMenu != NULL
        && CurrentItPos >= 0 && CurrentItPos < CurrentMenu->itemCount
        && (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
        ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2)))
        {
            S_StartSound(NULL, sfx_keyup);
        }
    }

    return val;
}

static float M_FLOAT_Slider (float val, float min, float max, float step,
                             int direction, boolean capped)
{
    const float old_val = val;

    // [PN] Adjust value based on direction
    val += (direction == -1) ? 0 :            // [JN] Routine "-1" just reintializes value.
           (direction ==  0) ? -step : step;  // Otherwise, move either left "0" or right "1".

    // [PN] Handle min/max limits
    if (val < min)
        val = capped ? min : max;
    else
    if (val > max)
        val = capped ? max : min;

    // [PN/JN] Do a float correction to get x.xxx000 values
    val = roundf(val * 1000.0f) / 1000.0f;

    // [JN] Play sound only if value was really changed.
    // [PN] For line items (ITT_LRFUNC1/2), sound is handled in MN_Responder
    // to keep keyboard/mouse/wheel behavior consistent and avoid doubles.
    if (old_val != val)
    {
        if (!(MenuActive && CurrentMenu != NULL
        && CurrentItPos >= 0 && CurrentItPos < CurrentMenu->itemCount
        && (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
        ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2)))
        {
            S_StartSound(NULL, sfx_keyup);
        }
    }

    return val;
}

static void M_DrawScrollPages (int x, int y, int itemOnGlow, const char *pagenum)
{
    char str[32];

    MN_DrTextA("SCROLL PAGES", x, y,
               M_Item_Glow(14, GLOW_LIGHTGRAY));

    M_snprintf(str, 32, "PAGE %s", pagenum);

    MN_DrTextA(str, M_ItemRightAlign(str), y,
               M_Item_Glow(14, GLOW_LIGHTGRAY));
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
    {ITT_LRFUNC1, "SPECTATOR MODE",       CRL_Spectating,    0, MENU_NONE},
    {ITT_LRFUNC1, "FREEZE MODE",          CRL_Freeze,        0, MENU_NONE},
    {ITT_LRFUNC1, "BUDDHA MODE",          CRL_Buddha,        0, MENU_NONE},
    {ITT_LRFUNC1, "NO TARGET MODE",       CRL_NoTarget,      0, MENU_NONE},
    {ITT_LRFUNC1, "NO MOMENTUM MODE",     CRL_NoMomentum,    0, MENU_NONE},
    {ITT_LRFUNC1, "GAME SPEED",           CRL_GameSpeed,     0, MENU_NONE},
    {ITT_EMPTY,   NULL,                   NULL,              0, MENU_NONE},
    {ITT_SETMENU, "VIDEO OPTIONS",        NULL,              0, MENU_CRLVIDEO},
    {ITT_SETMENU, "DISPLAY OPTIONS",      NULL,              0, MENU_CRLDISPLAY},
    {ITT_SETMENU, "SOUND OPTIONS",        NULL,              0, MENU_CRLSOUND},
    {ITT_SETMENU, "CONTROL SETTINGS",     NULL,              0, MENU_CRLCONTROLS},
    {ITT_SETMENU, "WIDGETS SETTINGS",     NULL,              0, MENU_CRLWIDGETS},
    {ITT_SETMENU, "AUTOMAP SETTINGS",     NULL,              0, MENU_CRLAUTOMAP},
    {ITT_SETMENU, "GAMEPLAY FEATURES",    NULL,              0, MENU_CRLGAMEPLAY},
    {ITT_EFUNC,   "MISC FEATURES",        M_Choose_CRL_Misc, 0, MENU_NONE},
    {ITT_SETMENU, "LIMITS AND WARNINGS",  NULL,              0, MENU_CRLLIMITS},
    {ITT_SETMENU, "VANILLA OPTIONS MENU", NULL,              0, MENU_OPTIONS}
};

static Menu_t CRLMain = {
    CRL_MENU_LEFTOFFSET_SML, CRL_MENU_TOPOFFSET,
    DrawCRLMain,
    ITEMCOUNT(CRLMainItems), CRLMainItems,
    0,
    SmallFont, false, false,
    MENU_MAIN
};

static void DrawCRLMain (void)
{
    char str[32];

    MN_DrTextACentered("MAIN CRL MENU", 10, cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_spectating ? GLOW_GREEN : GLOW_DARKRED));

    // Freeze
    sprintf(str, !singleplayer ? "N/A" : crl_freeze ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
                 M_Item_Glow(1, !singleplayer ? GLOW_DARKRED :
                             crl_freeze ? GLOW_GREEN : GLOW_DARKRED));

    // Buddha
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_BUDDHA ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
                 M_Item_Glow(2, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_BUDDHA ? GLOW_GREEN : GLOW_DARKRED));

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
                 M_Item_Glow(3, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOTARGET ? GLOW_GREEN : GLOW_DARKRED));

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 60, 
                 M_Item_Glow(4, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOMOMENTUM ? GLOW_GREEN : GLOW_DARKRED));

    // Game speed
    sprintf(str, netgame ? "N/A" : "%d%%", crl_game_speed);
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
                M_Item_Glow(5, netgame || crl_game_speed == 100 ? GLOW_DARKRED :
                            crl_game_speed < 100 ? GLOW_YELLOW : GLOW_GREEN));

    MN_DrTextACentered ("SETTINGS", 80, cr[CR_YELLOW]);
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

static void CRL_Buddha (int option)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_BUDDHA;
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

static void CRL_GameSpeed (int choice)
{
    G_CRL_ChangeGameSpeed(choice == 0 ? -1 : 1, false);
}

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static MenuItem_t CRLVideoItems[] = {
    { ITT_LRFUNC1, "UNCAPPED FRAMERATE",  CRL_UncappedFPS,   0, MENU_NONE },
    { ITT_LRFUNC1, "FRAMERATE LIMIT",     CRL_LimitFPS,      0, MENU_NONE },
    { ITT_LRFUNC2, "ENABLE VSYNC",        CRL_VSync,         0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW FPS COUNTER",    CRL_ShowFPS,       0, MENU_NONE },
    { ITT_LRFUNC2, "PIXEL SCALING",       CRL_PixelScaling,  0, MENU_NONE },
    { ITT_LRFUNC2, "VISPLANES DRAWING",   CRL_VisplanesDraw, 0, MENU_NONE },
    { ITT_LRFUNC1, "HOM EFFECT",          CRL_HOMDraw,       0, MENU_NONE },
    { ITT_EMPTY,   NULL,                  NULL,              0, MENU_NONE },
    { ITT_LRFUNC2, "GRAPHICAL STARTUP",   CRL_GfxStartup,    0, MENU_NONE },
    { ITT_LRFUNC2, "SCREEN WIPE EFFECT",  CRL_ScreenWipe,    0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW ENDTEXT SCREEN", CRL_EndText,       0, MENU_NONE },
};

static Menu_t CRLVideo = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLVideo,
    ITEMCOUNT(CRLVideoItems), CRLVideoItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLVideo (void)
{
    char str[32];

    MN_DrTextACentered("VIDEO OPTIONS", 10, cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !crl_uncapped_fps ? "35" :
                 crl_fpslimit ? "%d" : "NONE", crl_fpslimit);
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, crl_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Enable vsync
    sprintf(str, crl_vsync ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, crl_vsync ? GLOW_DARKRED : GLOW_YELLOW));

    // Show FPS counter
    sprintf(str, crl_showfps ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (CurrentItPos == 3 && !crl_extended_hud)
    {
        MN_DrTextACentered("HIDDEN WHILE EXTENDED HUD IS OFF", 130, cr[CR_GRAY]);
    }

    // Pixel scaling
    sprintf(str, smooth_scaling ? "SMOOTH" : "SHARP");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    // Visplanes drawing
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, crl_visplanes_drawing ? GLOW_GREEN : GLOW_DARKRED));
    
    // HOM effect
    sprintf(str, crl_hom_effect == 0 ? "OFF" :
                 crl_hom_effect == 1 ? "MULTICOLOR" : "BLACK");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, crl_hom_effect ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("MISCELLANEOUS", 90, cr[CR_YELLOW]);

    // Graphical startup
    sprintf(str, graphical_startup == 1 ? "FAST" :
                 graphical_startup == 2 ? "SLOW" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, graphical_startup == 1 ? GLOW_DARKRED :
                              graphical_startup == 2 ? GLOW_YELLOW : GLOW_GREEN));

    // Screen wipe effect
    sprintf(str, crl_screenwipe == 1 ? "CROSSFADE" :
                 crl_screenwipe == 2 ? "MELT" :
                 crl_screenwipe == 3 ? "FAST MELT": "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, crl_screenwipe == 1 ? GLOW_GREEN :
                              crl_screenwipe == 2 ? GLOW_YELLOW :
                              crl_screenwipe == 3 ? GLOW_ORANGE : GLOW_DARKRED));

    // Show ENDTEXT screen
    sprintf(str, show_endoom == 1 ? "ALWAYS" :
                 show_endoom == 2 ? "PWAD ONLY" : "NEVER");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, show_endoom == 1 ? GLOW_DARKRED :
                               show_endoom == 2 ? GLOW_YELLOW : GLOW_GREEN));
}

static void CRL_UncappedFPS (int option)
{
    crl_uncapped_fps ^= 1;
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
    graphical_startup = M_INT_Slider(graphical_startup, 0, 2, option, false);
}

static void CRL_ScreenWipe (int choice)
{
    crl_screenwipe = M_INT_Slider(crl_screenwipe, 0, 3, choice, false);
}

static void CRL_EndText (int option)
{
    show_endoom = M_INT_Slider(show_endoom, 0, 2, option, false);
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static MenuItem_t CRLDisplayItems[] = {
    { ITT_SLDR,    "GAMMA-CORRECTION",        CRL_Gamma,           0, MENU_NONE },
    { ITT_EMPTY,   NULL,                      NULL,                0, MENU_NONE },
    { ITT_EMPTY,   NULL,                      NULL,                0, MENU_NONE },
    { ITT_LRFUNC1, "MENU BACKGROUND SHADING", CRL_MenuBgShading,   0, MENU_NONE },
    { ITT_LRFUNC1, "EXTRA LEVEL BRIGHTNESS",  CRL_LevelBrightness, 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                      NULL,                0, MENU_NONE },
    { ITT_LRFUNC1, "MESSAGES ENABLED",        SCMessages,          0, MENU_NONE },
    { ITT_LRFUNC2, "CRITICAL MESSAGE",        CRL_MsgCritical,     0, MENU_NONE },
    { ITT_LRFUNC1, "TEXT CAST SHADOWS",       CRL_TextShadows,     0, MENU_NONE },
};

static Menu_t CRLDisplay = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET + 10,
    DrawCRLDisplay,
    ITEMCOUNT(CRLDisplayItems), CRLDisplayItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLDisplay (void)
{
    char str[32];

    MN_DrTextACentered("DISPLAY OPTIONS", 20, cr[CR_YELLOW]);

    // Gamma-correction slider and num
    DrawSlider(&CRLDisplay, 1, 15, crl_gamma, false, 0);
    M_ID_HandleSliderMouseControl(70, 40, 124, &crl_gamma, false, 0, 15);
    MN_DrTextA(gammalvls[crl_gamma][1], 221, 45, M_Item_Glow(0, GLOW_UNCOLORED));

    // Menu background shading
    sprintf(str, crl_menu_shading ? "%d" : "OFF", crl_menu_shading);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(3, crl_menu_shading ? GLOW_GREEN : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, crl_level_brightness ? "%d" : "OFF", crl_level_brightness);
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(4, crl_level_brightness ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("MESSAGES SETTINGS", 80, cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, showMessages ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(6, showMessages ? GLOW_DARKRED : GLOW_GREEN));

    // Critical message style
    sprintf(str, crl_msg_critical ? "BLINKING" : "STATIC");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(7, crl_msg_critical ? GLOW_GREEN : GLOW_DARKRED));
    // Show nice preview-reminder :)
    if (CurrentItPos == 7)
    {
        CRL_SetMessageCritical("CRITICAL MESSAGES ARE ALWAYS ENABLED!", "", 2);
    }

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(8, crl_text_shadows ? GLOW_GREEN : GLOW_DARKRED));
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
    { ITT_SLDR,    "SFX VOLUME",           SCSfxVolume,           MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,               0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,               0, MENU_NONE },
    { ITT_SLDR,    "MUSIC VOLUME",         SCMusicVolume,         MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,               0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,               0, MENU_NONE },
    { ITT_EMPTY,   NULL,                   NULL,               0, MENU_NONE },
    { ITT_LRFUNC2, "MUSIC PLAYBACK",       CRL_MusicSystem,    0, MENU_NONE },
    { ITT_LRFUNC1, "SOUNDS EFFECTS MODE",  CRL_SFXMode,        0, MENU_NONE },
    { ITT_LRFUNC2, "PITCH-SHIFTED SOUNDS", CRL_PitchShift,     0, MENU_NONE },
    { ITT_LRFUNC1, "NUMBER OF SFX TO MIX", CRL_SFXChannels,    0, MENU_NONE },
    { ITT_LRFUNC1, "MUTE INACTIVE WINDOW", M_CRL_MuteInactive, 0, MENU_NONE },
};

static Menu_t CRLSound = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLSound,
    ITEMCOUNT(CRLSoundItems), CRLSoundItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLSound (void)
{
    char str[32];

    MN_DrTextACentered("SOUND OPTIONS", 10, cr[CR_YELLOW]);

    DrawSlider(&CRLSound, 1, 16, snd_MaxVolume, false, 0);
    M_ID_HandleSliderMouseControl(70, 30, 134, &snd_MaxVolume, false, 0, 15);
    sprintf(str,"%d", snd_MaxVolume);
    MN_DrTextA(str, 228, 35, M_Item_Glow(0, GLOW_UNCOLORED));

    DrawSlider(&CRLSound, 4, 16, snd_MusicVolume, false, 3);
    M_ID_HandleSliderMouseControl(70, 60, 134, &snd_MusicVolume, false, 0, 15);
    sprintf(str,"%d", snd_MusicVolume);
    MN_DrTextA(str, 228, 65, M_Item_Glow(3, GLOW_UNCOLORED));

    MN_DrTextACentered("SOUND SYSTEM", 80, cr[CR_YELLOW]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ?  "GUS (EMULATED)" :
                 snd_musicdevice == 8 ?  "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                         "UNKNOWN");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, snd_musicdevice ? GLOW_GREEN : GLOW_DARKRED));

    // Sound effects mode
    sprintf(str, crl_monosfx ? "MONO" : "STEREO");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, crl_monosfx ? GLOW_GREEN : GLOW_DARKRED));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, snd_pitchshift ? GLOW_DARKRED : GLOW_GREEN));

    // Number of SFX to mix
    sprintf(str, "%i", snd_Channels);
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, snd_Channels == 8 ? GLOW_DARKRED :
                               snd_Channels <= 2 ? GLOW_DARKGREEN : GLOW_GREEN));

    if (CurrentItPos == 7)
    {
        if (snd_musicdevice == 5 && strcmp(gus_patch_path, "") == 0)
        {
            MN_DrTextACentered("\"GUS[PATCH[PATH\" VARIABLE IS NOT SET", 150, cr[CR_GRAY]);
        }
    }

    // Mute inactive window
    sprintf(str, crl_mute_inactive ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
                 M_Item_Glow(11, crl_mute_inactive ? GLOW_GREEN : GLOW_DARKRED));
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

    // [PN] Hot-swap music system
    S_ShutDown();
    I_InitSound(heretic);
    I_InitMusic();
    S_SetMusicVolume();

    // [JN] Enforce music replay while changing music system.
    mus_force_replay = true;

    if (mus_song != -1)
    {
        S_StartSong(mus_song, true);
    }
    else
    {
        S_StartSong((gameepisode - 1) * 9 + gamemap - 1, true);
    }

    mus_force_replay = false;
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

static void M_CRL_MuteInactive (int choice)
{
    crl_mute_inactive ^= 1;
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static MenuItem_t CRLControlsItems[] = {
    {ITT_EFUNC,   "KEYBOARD BINDINGS",       CRL_Choose_Keybinds,       0, MENU_NONE},
    {ITT_SETMENU, "MOUSE BINDINGS",          NULL,                      0, MENU_CRLMOUSEBINDS},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_SLDR,    "HORIZONTAL SENSITIVITY",  SCMouseSensi,              0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_SLDR,    "VERTICAL SENSITIVITY",    CRL_Controls_Sensivity_y,  0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_SLDR,    "ACCELERATION",            CRL_Controls_Acceleration, 0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_SLDR,    "ACCELERATION THRESHOLD",  CRL_Controls_Threshold,    0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_EMPTY,   NULL,                      NULL,                      0, MENU_NONE},
    {ITT_LRFUNC1, "MOUSE LOOK",              CRL_Controls_MLook,        0, MENU_NONE},
    {ITT_LRFUNC1, "VERTICAL MOUSE MOVEMENT", CRL_Controls_NoVert,       0, MENU_NONE}
};

static Menu_t CRLControls = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLControls,
    ITEMCOUNT(CRLControlsItems), CRLControlsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLControls (void)
{
    char str[32];

    M_FillBackground();

    MN_DrTextACentered("BINDINGS", 10, cr[CR_YELLOW]);

    MN_DrTextACentered("MOUSE CONFIGURATION", 40, cr[CR_YELLOW]);

    DrawSlider(&CRLControls, 4, 16, mouseSensitivity, false, 3);
    M_ID_HandleSliderMouseControl(66, 60, 132, &mouseSensitivity, false, 0, 15);
    sprintf(str,"%d", mouseSensitivity);
    MN_DrTextA(str, 227, 65, M_Item_Glow(3, mouseSensitivity == 255 ? GLOW_YELLOW :
                                            mouseSensitivity  >  15 ? GLOW_GREEN : GLOW_LIGHTGRAY));

    DrawSlider(&CRLControls, 7, 16, mouse_sensitivity_y, false, 6);
    M_ID_HandleSliderMouseControl(66, 90, 132, &mouse_sensitivity_y, false, 0, 15);
    sprintf(str,"%d", mouse_sensitivity_y);
    MN_DrTextA(str, 227, 95, M_Item_Glow(6, mouse_sensitivity_y == 255 ? GLOW_YELLOW :
                                            mouse_sensitivity_y  >  15 ? GLOW_GREEN : GLOW_LIGHTGRAY));

    DrawSlider(&CRLControls, 10, 7, (mouse_acceleration * 1.8f) - 2, false, 9);
    M_ID_HandleSliderMouseControl(66, 120, 60, &mouse_acceleration, true, 0, 6);
    sprintf(str,"%.1f", mouse_acceleration);
    MN_DrTextA(str, 155, 125, M_Item_Glow(9, GLOW_LIGHTGRAY));

    DrawSlider(&CRLControls, 13, 15, mouse_threshold / 2.2f, false, 12);
    M_ID_HandleSliderMouseControl(66, 150, 124, &mouse_threshold, false, 0, 32);
    sprintf(str,"%d", mouse_threshold);
    MN_DrTextA(str, 219, 155, M_Item_Glow(12, GLOW_LIGHTGRAY));

    // Mouse look
    sprintf(str, crl_mouselook ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 170,
               M_Item_Glow(15, crl_mouselook ? GLOW_GREEN : GLOW_DARKRED));

    // Vertical mouse movement
    sprintf(str, novert ? "OFF" : "ON");
    MN_DrTextA(str, M_ItemRightAlign(str), 180,
               M_Item_Glow(16, novert ? GLOW_DARKRED : GLOW_GREEN));
}

static void CRL_Controls_Sensivity_y (int choice)
{
    // [crispy] extended range
    mouse_sensitivity_y = M_INT_Slider(mouse_sensitivity_y, 0, 255, choice, true);
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
    ITEMCOUNT(CRLKbsBinds1Items), CRLKbsBinds1Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd1 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS1;

    M_FillBackground();

    MN_DrTextACentered("MOVEMENT", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_up, key_up2);
    M_DrawBindKey(1, 30, key_down, key_down2);
    M_DrawBindKey(2, 40, key_left, key_left2);
    M_DrawBindKey(3, 50, key_right, key_right2);
    M_DrawBindKey(4, 60, key_strafeleft, key_strafeleft2);
    M_DrawBindKey(5, 70, key_straferight, key_straferight2);
    M_DrawBindKey(6, 80, key_speed, key_speed2);
    M_DrawBindKey(7, 90, key_strafe, key_strafe2);
    M_DrawBindKey(8, 100, key_180turn, key_180turn2);

    MN_DrTextACentered("ACTION", 110, cr[CR_YELLOW]);

    M_DrawBindKey(10, 120, key_fire, key_fire2);
    M_DrawBindKey(11, 130, key_use, key_use2);

    M_DrawBindFooter("1", true);
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
    ITEMCOUNT(CRLKbsBinds2Items), CRLKbsBinds2Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd2 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS2;

    M_FillBackground();

    MN_DrTextACentered("VIEW", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_lookup, key_lookup2);
    M_DrawBindKey(1, 30, key_lookdown, key_lookdown2);
    M_DrawBindKey(2, 40, key_lookcenter, key_lookcenter2);

    MN_DrTextACentered("FLYING", 50, cr[CR_YELLOW]);

    M_DrawBindKey(4, 60, key_flyup, key_flyup2);
    M_DrawBindKey(5, 70, key_flydown, key_flydown2);
    M_DrawBindKey(6, 80, key_flycenter, key_flycenter2);

    MN_DrTextACentered("INVENTORY", 90, cr[CR_YELLOW]);

    M_DrawBindKey(8, 100, key_invleft, key_invleft2);
    M_DrawBindKey(9, 110, key_invright, key_invright2);
    M_DrawBindKey(10, 120, key_useartifact, key_useartifact2);

    M_DrawBindFooter("2", true);
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
    { ITT_EFUNC, "MAIN CRL MENU",        M_Bind_CRLmenu,        0, MENU_NONE },
    { ITT_EFUNC, "GO TO PREVIOUS LEVEL", M_Bind_PrevLevel,      0, MENU_NONE },
    { ITT_EFUNC, "RESTART LEVEL/DEMO",   M_Bind_RestartLevel,   0, MENU_NONE },
    { ITT_EFUNC, "GO TO NEXT LEVEL",     M_Bind_NextLevel,      0, MENU_NONE },
    { ITT_EFUNC, "DEMO FAST-FORWARD",    M_Bind_FastForward,    0, MENU_NONE },
    { ITT_EFUNC, "TOGGLE EXTENDED HUD",  M_Bind_ExtendedHUD,    0, MENU_NONE },
    { ITT_EMPTY, NULL,                   NULL,                  0, MENU_NONE },
    { ITT_EFUNC, "SPECTATOR MODE",       M_Bind_SpectatorMode,  0, MENU_NONE },
    { ITT_EFUNC, "- MOVE CAMERA UP",     M_Bind_CameraUp,       0, MENU_NONE },
    { ITT_EFUNC, "- MOVE CAMERA DOWN",   M_Bind_CameraDown,     0, MENU_NONE },
    { ITT_EFUNC, "- MOVE TO CAMERA POS", M_Bind_CameraMoveTo,   0, MENU_NONE },
    { ITT_EFUNC, "FREEZE MODE",          M_Bind_FreezeMode,     0, MENU_NONE },
    { ITT_EFUNC, "BUDDHA MODE",          M_Bind_BuddhaMode,     0, MENU_NONE },
    { ITT_EFUNC, "NO TARGET MODE",       M_Bind_NotargetMode,   0, MENU_NONE },
    { ITT_EFUNC, "NO MOMENTUM MODE",     M_Bind_NomomentumMode, 0, MENU_NONE }
};

static Menu_t CRLKbdBinds3 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd3,
    ITEMCOUNT(CRLKbsBinds3Items), CRLKbsBinds3Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd3 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS3;

    M_FillBackground();

    MN_DrTextACentered("CRL CONTROLS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_crl_menu, key_crl_menu2);
    M_DrawBindKey(1, 30, key_crl_prevlevel, key_crl_prevlevel2);
    M_DrawBindKey(2, 40, key_crl_reloadlevel, key_crl_reloadlevel2);
    M_DrawBindKey(3, 50, key_crl_nextlevel, key_crl_nextlevel2);
    M_DrawBindKey(4, 60, key_crl_demospeed, key_crl_demospeed2);
    M_DrawBindKey(5, 70, key_crl_extendedhud, key_crl_extendedhud2);

    MN_DrTextACentered("GAME MODES", 80, cr[CR_YELLOW]);

    M_DrawBindKey(7, 90, key_crl_spectator, key_crl_spectator2);
    M_DrawBindKey(8, 100, key_crl_cameraup, key_crl_cameraup2);
    M_DrawBindKey(9, 110, key_crl_cameradown, key_crl_cameradown2);
    M_DrawBindKey(10, 120, key_crl_cameramoveto, key_crl_cameramoveto2);
    M_DrawBindKey(11, 130, key_crl_freeze, key_crl_freeze2);
    M_DrawBindKey(12, 140, key_crl_buddha, key_crl_buddha2);
    M_DrawBindKey(13, 150, key_crl_notarget, key_crl_notarget2);
    M_DrawBindKey(14, 160, key_crl_nomomentum, key_crl_nomomentum2);

    M_DrawBindFooter("3", true);
}

static void M_Bind_CRLmenu (int option)
{
    M_StartBind(300);  // key_crl_menu
}

static void M_Bind_PrevLevel (int choice)
{
    M_StartBind(301);  // key_crl_prevlevel
}

static void M_Bind_RestartLevel (int option)
{
    M_StartBind(302);  // key_crl_reloadlevel
}

static void M_Bind_NextLevel (int option)
{
    M_StartBind(303);  // key_crl_nextlevel
}

static void M_Bind_FastForward (int option)
{
    M_StartBind(304);  // key_crl_demospeed
}

static void M_Bind_ExtendedHUD (int choice)
{
    M_StartBind(305);  // key_crl_extendedhud
}

static void M_Bind_SpectatorMode (int option)
{
    M_StartBind(306);  // key_crl_spectator
}

static void M_Bind_CameraUp (int option)
{
    M_StartBind(307);  // key_crl_cameraup
}

static void M_Bind_CameraDown (int option)
{
    M_StartBind(308);  // key_crl_cameradown
}

static void M_Bind_CameraMoveTo (int choice)
{
    M_StartBind(309);  // key_crl_cameramoveto
}

static void M_Bind_FreezeMode (int option)
{
    M_StartBind(310);  // key_crl_freeze
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(311);  // key_crl_buddha
}

static void M_Bind_NotargetMode (int option)
{
    M_StartBind(312);  // key_crl_notarget
}

static void M_Bind_NomomentumMode (int choice)
{
    M_StartBind(313);  // key_crl_nomomentum
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static MenuItem_t CRLKbsBinds4Items[] = {
    { ITT_EFUNC, "ALWAYS RUN",             M_Bind_AlwaysRun, 0, MENU_NONE},
    { ITT_EFUNC, "MOUSE LOOK",             M_Bind_MouseLook, 0, MENU_NONE},
    { ITT_EFUNC, "VERTICAL MOUSE MOVEMENT", M_Bind_NoVert,   0, MENU_NONE},
    { ITT_EFUNC, "ARCH-VILE JUMP (PRESS)", M_Bind_VileBomb,  0, MENU_NONE},
    { ITT_EFUNC, "ARCH-VILE JUMP (HOLD)",  M_Bind_VileFly,   0, MENU_NONE},
    { ITT_EMPTY, NULL,                     NULL,             0, MENU_NONE},
    { ITT_EFUNC, "CLEAR MAX",              M_Bind_ClearMAX,  0, MENU_NONE},
    { ITT_EFUNC, "MOVE TO MAX",            M_Bind_MoveToMAX, 0, MENU_NONE},
    { ITT_EMPTY, NULL,                     NULL,             0, MENU_NONE},
    { ITT_EFUNC, "QUICKEN",                M_Bind_IDDQD,     0, MENU_NONE},
    { ITT_EFUNC, "RAMBO",                  M_Bind_IDFA,      0, MENU_NONE},
    { ITT_EFUNC, "KITTY",                  M_Bind_IDCLIP,    0, MENU_NONE},
    { ITT_EFUNC, "RAVMAP",                 M_Bind_IDDT,      0, MENU_NONE}
};

static Menu_t CRLKbdBinds4 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd4,
    ITEMCOUNT(CRLKbsBinds4Items), CRLKbsBinds4Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd4 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS4;

    M_FillBackground();

    MN_DrTextACentered("ADVANCED MOVEMENT", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_crl_autorun, key_crl_autorun2);
    M_DrawBindKey(1, 30, key_crl_mlook, key_crl_mlook2);
    M_DrawBindKey(2, 40, key_crl_novert, key_crl_novert2);
    M_DrawBindKey(3, 50, key_crl_vilebomb, key_crl_vilebomb2);
    M_DrawBindKey(4, 60, key_crl_vilefly, key_crl_vilefly2);

    MN_DrTextACentered("VISPLANES MAX VALUE", 70, cr[CR_YELLOW]);

    M_DrawBindKey(6, 80, key_crl_clearmax, key_crl_clearmax2);
    M_DrawBindKey(7, 90, key_crl_movetomax, key_crl_movetomax2);

    MN_DrTextACentered("CHEAT SHORTCUTS", 100, cr[CR_YELLOW]);

    M_DrawBindKey(9, 110, key_crl_iddqd, key_crl_iddqd2);
    M_DrawBindKey(10, 120, key_crl_idfa, key_crl_idfa2);
    M_DrawBindKey(11, 130, key_crl_idclip, key_crl_idclip2);
    M_DrawBindKey(12, 140, key_crl_iddt, key_crl_iddt2);

    M_DrawBindFooter("4", true);
}

static void M_Bind_AlwaysRun (int option)
{
    M_StartBind(400);  // key_crl_autorun
}

static void M_Bind_MouseLook (int option)
{
    M_StartBind(401);  // key_crl_mlook
}

static void M_Bind_NoVert (int option)
{
    M_StartBind(402);  // key_crl_novert
}

static void M_Bind_VileBomb (int option)
{
    M_StartBind(403);  // key_crl_vilebomb
}

static void M_Bind_VileFly (int option)
{
    M_StartBind(404);  // key_crl_vilefly
}

static void M_Bind_ClearMAX (int option)
{
    M_StartBind(405);  // key_crl_clearmax
}

static void M_Bind_MoveToMAX (int option)
{
    M_StartBind(406);  // key_crl_movetomax
}

static void M_Bind_IDDQD (int option)
{
    M_StartBind(407);  // key_crl_iddqd
}

static void M_Bind_IDFA (int option)
{
    M_StartBind(408);  // key_crl_idfa
}

static void M_Bind_IDCLIP (int option)
{
    M_StartBind(409);  // key_crl_idclip
}

static void M_Bind_IDDT (int option)
{
    M_StartBind(410);  // key_crl_iddt
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
    ITEMCOUNT(CRLKbsBinds5Items), CRLKbsBinds5Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd5 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS5;

    M_FillBackground();

    MN_DrTextACentered("WEAPONS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_weapon1, key_weapon1_2);
    M_DrawBindKey(1, 30, key_weapon2, key_weapon2_2);
    M_DrawBindKey(2, 40, key_weapon3, key_weapon3_2);
    M_DrawBindKey(3, 50, key_weapon4, key_weapon4_2);
    M_DrawBindKey(4, 60, key_weapon5, key_weapon5_2);
    M_DrawBindKey(5, 70, key_weapon6, key_weapon6_2);
    M_DrawBindKey(6, 80, key_weapon7, key_weapon7_2);
    M_DrawBindKey(7, 90, key_weapon8, key_weapon8_2);
    M_DrawBindKey(8, 100, key_prevweapon, key_prevweapon2);
    M_DrawBindKey(9, 110, key_nextweapon, key_nextweapon2);

    M_DrawBindFooter("5", true);
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
    ITEMCOUNT(CRLKbsBinds6Items), CRLKbsBinds6Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd6 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS6;

    M_FillBackground();

    MN_DrTextACentered("ARTIFACTS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_arti_quartz, key_arti_quartz2);
    M_DrawBindKey(1, 30, key_arti_urn, key_arti_urn2);
    M_DrawBindKey(2, 40, key_arti_bomb, key_arti_bomb2);
    M_DrawBindKey(3, 50, key_arti_tome, key_arti_tome2);
    M_DrawBindKey(4, 60, key_arti_ring, key_arti_ring2);
    M_DrawBindKey(5, 70, key_arti_chaosdevice, key_arti_chaosdevice2);
    M_DrawBindKey(6, 80, key_arti_shadowsphere, key_arti_shadowsphere2);
    M_DrawBindKey(7, 90, key_arti_wings, key_arti_wings2);
    M_DrawBindKey(8, 100, key_arti_torch, key_arti_torch2);
    M_DrawBindKey(9, 110, key_arti_morph, key_arti_morph2);

    M_DrawBindFooter("6", true);
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
    {ITT_EFUNC, "TOGGLE MAP",             M_Bind_ToggleMap,   0, MENU_NONE},
    {ITT_EFUNC, "ZOOM IN",                M_Bind_ZoomIn,      0, MENU_NONE},
    {ITT_EFUNC, "ZOOM OUT",               M_Bind_ZoomOut,     0, MENU_NONE},
    {ITT_EFUNC, "MAXIMUM ZOOM OUT",       M_Bind_MaxZoom,     0, MENU_NONE},
    {ITT_EFUNC, "FOLLOW MODE",            M_Bind_FollowMode,  0, MENU_NONE},
    {ITT_EFUNC, "ROTATE MODE",            M_Bind_RotateMode,  0, MENU_NONE},
    {ITT_EFUNC, "OVERLAY MODE",           M_Bind_OverlayMode, 0, MENU_NONE},
    {ITT_EFUNC, "SOUND PROPAGATION MODE", M_Bind_SndPropMode, 0, MENU_NONE},
    {ITT_EFUNC, "TOGGLE GRID",            M_Bind_ToggleGrid,  0, MENU_NONE}
};

static Menu_t CRLKbdBinds7 = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLKbd7,
    ITEMCOUNT(CRLKbsBinds7Items), CRLKbsBinds7Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd7 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS7;

    M_FillBackground();

    MN_DrTextACentered("AUTOMAP", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_map_toggle, key_map_toggle2);
    M_DrawBindKey(1, 30, key_map_zoomin, key_map_zoomin2);
    M_DrawBindKey(2, 40, key_map_zoomout, key_map_zoomout2);
    M_DrawBindKey(3, 50, key_map_maxzoom, key_map_maxzoom2);
    M_DrawBindKey(4, 60, key_map_follow, key_map_follow2);
    M_DrawBindKey(5, 80, key_crl_map_rotate, key_crl_map_rotate2);
    M_DrawBindKey(6, 90, key_crl_map_overlay, key_crl_map_overlay2);
    M_DrawBindKey(5, 70, key_crl_map_sndprop, key_crl_map_sndprop2);
    M_DrawBindKey(6, 80, key_map_grid, key_map_grid2);

    M_DrawBindFooter("7", true);
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

static void M_Bind_RotateMode (int option)
{
    M_StartBind(705);  // key_crl_map_rotate
}

static void M_Bind_OverlayMode (int option)
{
    M_StartBind(706);  // key_crl_map_overlay
}

static void M_Bind_SndPropMode (int option)
{
    M_StartBind(707);  // key_crl_map_sndprop
}

static void M_Bind_ToggleGrid (int option)
{
    M_StartBind(708);  // key_map_grid
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
    ITEMCOUNT(CRLKbsBinds8Items), CRLKbsBinds8Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd8 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS8;

    M_FillBackground();

    MN_DrTextACentered("FUNCTION KEYS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_menu_help, key_menu_help2);
    M_DrawBindKey(1, 30, key_menu_save, key_menu_save2);
    M_DrawBindKey(2, 40, key_menu_load, key_menu_load2);
    M_DrawBindKey(3, 50, key_menu_volume, key_menu_volume2);
    M_DrawBindKey(4, 60, key_menu_qsave, key_menu_qsave2);
    M_DrawBindKey(5, 70, key_menu_endgame, key_menu_endgame2);
    M_DrawBindKey(6, 80, key_menu_messages, key_menu_messages2);
    M_DrawBindKey(7, 90, key_menu_qload, key_menu_qload2);
    M_DrawBindKey(8, 100, key_menu_quit, key_menu_quit2);
    M_DrawBindKey(9, 110, key_menu_gamma, key_menu_gamma2);
    M_DrawBindKey(10, 120, key_spy, key_spy2);

    M_DrawBindFooter("8", true);
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
    ITEMCOUNT(CRLKbsBinds9Items), CRLKbsBinds9Items,
    0,
    SmallFont, true, true,
    MENU_CRLCONTROLS
};

static void DrawCRLKbd9 (void)
{
    Keybinds_Cur = (MenuType_t)MENU_CRLKBDBINDS9;

    M_FillBackground();

    MN_DrTextACentered("SHORTCUT KEYS", 10, cr[CR_YELLOW]);

    M_DrawBindKey(0, 20, key_pause, key_pause2);
    M_DrawBindKey(1, 30, key_menu_screenshot, key_menu_screenshot2);
    M_DrawBindKey(2, 40, key_demo_quit, key_demo_quit2);

    MN_DrTextACentered("MULTIPLAYER", 50, cr[CR_YELLOW]);

    M_DrawBindKey(4, 60, key_multi_msg, key_multi_msg2);
    M_DrawBindKey(5, 70, key_multi_msgplayer[0], key_multi_msgplayer2[0]);
    M_DrawBindKey(6, 80, key_multi_msgplayer[1], key_multi_msgplayer2[1]);
    M_DrawBindKey(7, 90, key_multi_msgplayer[2], key_multi_msgplayer2[2]);
    M_DrawBindKey(8, 100, key_multi_msgplayer[3], key_multi_msgplayer2[3]);

    MN_DrTextACentered("RESET", 110, cr[CR_YELLOW]);

    M_DrawBindFooter("9", true);
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
    { ITT_EFUNC, "SPEED ON",                  M_Bind_M_SpeedOn,        0, MENU_NONE },
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
    ITEMCOUNT(CRLMouseItems), CRLMouseItems,
    0,
    SmallFont, false, false,
    MENU_CRLCONTROLS
};

static void DrawCRLMouse (void)
{
    M_FillBackground();

    MN_DrTextACentered("MOUSE BINDINGS", 10, cr[CR_YELLOW]);

    M_DrawBindButton(0, 20, mousebfire, mousebfire2);
    M_DrawBindButton(1, 30, mousebforward, mousebforward2);
    M_DrawBindButton(2, 40, mousebspeed, mousebspeed2);
    M_DrawBindButton(3, 50, mousebstrafe, mousebstrafe2);
    M_DrawBindButton(4, 60, mousebbackward, mousebbackward2);
    M_DrawBindButton(5, 70, mousebuse, mousebuse2);
    M_DrawBindButton(6, 80, mousebstrafeleft, mousebstrafeleft2);
    M_DrawBindButton(7, 90, mousebstraferight, mousebstraferight2);
    M_DrawBindButton(8, 100, mousebprevweapon, mousebprevweapon2);
    M_DrawBindButton(9, 110, mousebnextweapon, mousebnextweapon2);
    M_DrawBindButton(10, 120, mousebinvleft, mousebinvleft2);
    M_DrawBindButton(11, 130, mousebinvright, mousebinvright2);
    M_DrawBindButton(12, 140, mousebuseartifact, mousebuseartifact2);

    MN_DrTextACentered("RESET", 150, cr[CR_YELLOW]);

    M_DrawBindFooter(NULL, false);
}

static void M_Bind_M_FireAttack (int option)
{
    M_StartMouseBind(1000);  // mousebfire
}

static void M_Bind_M_MoveForward (int option)
{
    M_StartMouseBind(1001);  // mousebforward
}

static void M_Bind_M_SpeedOn (int option)
{
    M_StartMouseBind(1002);  // mousebspeed
}

static void M_Bind_M_StrafeOn (int option)
{
    M_StartMouseBind(1003);  // mousebstrafe
}

static void M_Bind_M_MoveBackward (int option)
{
    M_StartMouseBind(1004);  // mousebbackward
}

static void M_Bind_M_Use (int option)
{
    M_StartMouseBind(1005);  // mousebuse
}

static void M_Bind_M_StrafeLeft (int option)
{
    M_StartMouseBind(1006);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int option)
{
    M_StartMouseBind(1007);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int option)
{
    M_StartMouseBind(1008);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int option)
{
    M_StartMouseBind(1009);  // mousebnextweapon
}

static void M_Bind_M_InventoryLeft (int option)
{
    M_StartMouseBind(1010);  // mousebinvleft
}

static void M_Bind_M_InventoryRight (int option)
{
    M_StartMouseBind(1011);  // mousebinvright
}

static void M_Bind_M_UseArtifact (int option)
{
    M_StartMouseBind(1012);  // mousebuseartifact
}

static void M_Bind_M_Reset (int option)
{
    MenuActive = false;
    askforquit = true;
    typeofask = 7;      // [JN] mouse binds reset
}

// -----------------------------------------------------------------------------
// Widgets
// -----------------------------------------------------------------------------

static MenuItem_t CRLWidgetsItems[] = {
    { ITT_LRFUNC2, "RENDER COUNTERS",        CRL_Widget_Render,    0, MENU_NONE },
    { ITT_LRFUNC2, "MAX OVERFLOW STYLE",     CRL_Widget_MAX,       0, MENU_NONE },
    { ITT_LRFUNC2, "PLAYSTATE COUNTERS",     CRL_Widget_Playstate, 0, MENU_NONE },
    { ITT_LRFUNC2, "KIS STATS",              CRL_Widget_KIS,       0, MENU_NONE },
    { ITT_LRFUNC2, "- STATS FORMAT",         CRL_Widget_KIS_Format,0, MENU_NONE },
    { ITT_LRFUNC2, "- SHOW ITEMS",           CRL_Widget_KIS_Items, 0, MENU_NONE },
    { ITT_LRFUNC2, "LEVEL TIME",             CRL_Widget_Time,      0, MENU_NONE },
    { ITT_LRFUNC2, "PLAYER COORDS",          CRL_Widget_Coords,    0, MENU_NONE },
    { ITT_LRFUNC2, "- SHOW FRACTIONS",       CRL_Widget_CoordsFrac,0, MENU_NONE },
    { ITT_LRFUNC2, "PLAYER SPEED",           CRL_Widget_Speed,     0, MENU_NONE },
    { ITT_LRFUNC2, "POWERUP TIMERS",         CRL_Widget_Powerups,  0, MENU_NONE },
    { ITT_LRFUNC2, "TARGET'S HEALTH",        CRL_Widget_Health,    0, MENU_NONE },
};

static Menu_t CRLWidgetsMenu = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLWidgets,
    ITEMCOUNT(CRLWidgetsItems), CRLWidgetsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLWidgets (void)
{
    char str[32];

    MN_DrTextACentered("WIDGETS", 10, cr[CR_YELLOW]);

    // Render counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_widget_render == 1 ? GLOW_GREEN :
                              crl_widget_render == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // MAX overflow style
    sprintf(str, crl_widget_maxvp == 1 ? "BLINKING 1" :
                 crl_widget_maxvp == 2 ? "BLINKING 2" : "STATIC");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, crl_widget_maxvp == 1 ? (gametic &  8 ? GLOW_YELLOW : GLOW_GREEN) :
                              crl_widget_maxvp == 2 ? (gametic & 16 ? GLOW_YELLOW : GLOW_GREEN) : GLOW_YELLOW));

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, crl_widget_playstate == 1 ? GLOW_GREEN :
                              crl_widget_playstate == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // K/I/S stats
    sprintf(str, crl_widget_kis == 1 ? "ON" :
                 crl_widget_kis == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_widget_kis ? GLOW_GREEN : GLOW_DARKRED));

    // Stats format
    sprintf(str, crl_widget_kis_format == 1 ? "REMAINING" :
                 crl_widget_kis_format == 2 ? "PERCENT" : "RATIO");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
                M_Item_Glow(4, crl_widget_kis_format ? GLOW_GREEN : GLOW_DARKRED));

    // Show items
    sprintf(str, crl_widget_kis_items == 1 ? "ON" :
                 crl_widget_kis_items == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, crl_widget_kis_items ? GLOW_GREEN : GLOW_DARKRED));

    // Level time
    sprintf(str, crl_widget_time == 1 ? "ON" : 
                 crl_widget_time == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, crl_widget_time ? GLOW_GREEN : GLOW_DARKRED));

    // Player coords
    sprintf(str, crl_widget_coords == 1 ? "ON" :
                 crl_widget_coords == 2 ? "AUTOMAP" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, crl_widget_coords ? GLOW_GREEN : GLOW_DARKRED));

    // Show fractions
    sprintf(str, crl_widget_coordsfrac ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, crl_widget_coordsfrac ? GLOW_GREEN : GLOW_DARKRED));

    // Player speed
    sprintf(str, crl_widget_speed ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, crl_widget_speed ? GLOW_GREEN : GLOW_DARKRED));

    // Powerup timers
    sprintf(str, crl_widget_powerups ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, crl_widget_powerups ? GLOW_GREEN : GLOW_DARKRED));

    // Target's health
    sprintf(str, crl_widget_health == 1 ? "TOP" :
                 crl_widget_health == 2 ? "TOP+NAME" :
                 crl_widget_health == 3 ? "BOTTOM" :
                 crl_widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, crl_widget_health ? GLOW_GREEN : GLOW_DARKRED));
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

static void CRL_Widget_KIS_Format (int option)
{
    crl_widget_kis_format = M_INT_Slider(crl_widget_kis_format, 0, 2, option, false);
}

static void CRL_Widget_KIS_Items (int option)
{
    crl_widget_kis_items = M_INT_Slider(crl_widget_kis_items, 0, 2, option, false);
}

static void CRL_Widget_Time (int option)
{
    crl_widget_time = M_INT_Slider(crl_widget_time, 0, 2, option, false);
}

static void CRL_Widget_Coords (int option)
{
    crl_widget_coords = M_INT_Slider(crl_widget_coords, 0, 2, option, false);
}

static void CRL_Widget_CoordsFrac (int option)
{
    crl_widget_coordsfrac ^= 1;
}

static void CRL_Widget_Speed (int option)
{
    crl_widget_speed ^= 1;
}

static void CRL_Widget_Powerups (int option)
{
    crl_widget_powerups ^= 1;
}

static void CRL_Widget_Health (int option)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 4, option, false);
}

// -----------------------------------------------------------------------------
// Automap settings
// -----------------------------------------------------------------------------

static MenuItem_t CRLAutomapItems[] = {
    { ITT_LRFUNC2, "BACKGROUND STYLE",       CRL_Automap_TexturedBg, 0, MENU_NONE },
    { ITT_LRFUNC2, "SCROLL BACKGROUND"  ,    CRL_Automap_ScrollBg,   0, MENU_NONE },
    { ITT_LRFUNC2, "ROTATE MODE",            CRL_Automap_Rotate,     0, MENU_NONE },
    { ITT_LRFUNC2, "OVERLAY MODE",           CRL_Automap_Overlay,    0, MENU_NONE },
    { ITT_LRFUNC1, "OVERLAY SHADING LEVEL",  CRL_Automap_Shading,    0, MENU_NONE },
    { ITT_LRFUNC1, "MOUSE PANNING MODE",     CRL_Automap_Pan,        0, MENU_NONE },
    { ITT_LRFUNC2, "MARK SECRET SECTORS",    CRL_Automap_Secrets,    0, MENU_NONE },
    { ITT_LRFUNC2, "SOUND PROPAGATION MODE", CRL_Automap_SndProp,    0, MENU_NONE },
};

static Menu_t CRLAutomap = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLAutomap,
    ITEMCOUNT(CRLAutomapItems), CRLAutomapItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLAutomap (void)
{
    char str[32];

    MN_DrTextACentered("AUTOMAP", 10, cr[CR_YELLOW]);

    // Background style
    sprintf(str, crl_automap_textured_bg ? "TEXTURED" : "BLACK");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_automap_textured_bg ? GLOW_DARKRED : GLOW_GREEN));

    // Scroll background
    sprintf(str, crl_automap_scroll_bg ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, crl_automap_scroll_bg ? GLOW_DARKRED : GLOW_GREEN));

    // Rotate mode
    sprintf(str, crl_automap_rotate ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, crl_automap_rotate ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay mode
    sprintf(str, crl_automap_overlay ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_automap_overlay ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay shading level
    sprintf(str,"%d", crl_automap_shading);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, !crl_automap_overlay ? GLOW_DARKRED :
                               crl_automap_shading ==  0 ? GLOW_DARKRED :
                               crl_automap_shading == 12 ? GLOW_YELLOW : GLOW_GREEN));

    // Mouse panning mode
    sprintf(str, crl_automap_mouse_pan ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 70,
               M_Item_Glow(5, crl_automap_mouse_pan ? GLOW_GREEN : GLOW_DARKRED));

    // Mark secret sectors
    sprintf(str, crl_automap_secrets == 1 ? "REVEALED" :
                 crl_automap_secrets == 2 ? "ALWAYS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, crl_automap_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Sound propagation mode
    sprintf(str, crl_automap_sndprop ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, crl_automap_sndprop ? GLOW_GREEN : GLOW_DARKRED));
}

static void CRL_Automap_TexturedBg (int choice)
{
    crl_automap_textured_bg ^= 1;
    // [JN] Reinitialize pointer to antialiased tables for line drawing.
    AM_initOverlayMode();
}

static void CRL_Automap_ScrollBg (int choice)
{
    crl_automap_scroll_bg ^= 1;
}

static void CRL_Automap_Rotate (int option)
{
    crl_automap_rotate ^= 1;
}

static void CRL_Automap_Overlay (int option)
{
    crl_automap_overlay ^= 1;
}

static void CRL_Automap_Shading (int option)
{
    crl_automap_shading = M_INT_Slider(crl_automap_shading, 0, 12, option, true);
}

static void CRL_Automap_Pan (int option)
{
    crl_automap_mouse_pan ^= 1;
}

static void CRL_Automap_Secrets (int option)
{
    crl_automap_secrets = M_INT_Slider(crl_automap_secrets, 0, 2, option, false);
}

static void CRL_Automap_SndProp (int option)
{
    crl_automap_sndprop ^= 1;
}

// -----------------------------------------------------------------------------
// Gameplay features
// -----------------------------------------------------------------------------

static MenuItem_t CRLGameplayItems[] = {
    { ITT_LRFUNC2, "DEFAULT SKILL LEVEL",      CRL_DefaulSkill,           0, MENU_NONE },
    { ITT_LRFUNC2, "WAND START GAME MODE",     CRL_PistolStart,           0, MENU_NONE },
    { ITT_LRFUNC2, "REPORT REVEALED SECRETS",  CRL_RevealedSecrets,       0, MENU_NONE },
    { ITT_LRFUNC2, "RESTORE MONSTER TARGETS",  CRL_RestoreTargets,        0, MENU_NONE },
    { ITT_LRFUNC2, "ON DEATH ACTION",          CRL_OnDeathAction,         0, MENU_NONE },
    { ITT_EMPTY,   NULL,                       NULL,                      0, MENU_NONE },
    { ITT_LRFUNC2, "COLORED STATUS BAR",       CRL_ColoredSBar,           0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW AMMO WIDGET",         CRL_AmmoWidget,            0, MENU_NONE },
    { ITT_LRFUNC1, "AMMO WIDGET TRANSLUCENCY", CRL_AmmoWidgetTranslucent, 0, MENU_NONE },
    { ITT_LRFUNC2, "AMMO WIDGET COLORING",     CRL_AmmoWidgetColors,      0, MENU_NONE },
    { ITT_EMPTY,   NULL,                       NULL,                      0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW DEMO TIMER",          CRL_DemoTimer,             0, MENU_NONE },
    { ITT_LRFUNC1, "TIMER DIRECTION",          CRL_TimerDirection,        0, MENU_NONE },
    { ITT_LRFUNC2, "SHOW PROGRESS BAR",        CRL_ProgressBar,           0, MENU_NONE },
    { ITT_LRFUNC2, "PLAY INTERNAL DEMOS",      CRL_InternalDemos,         0, MENU_NONE }
};

static Menu_t CRLGameplay = {
    CRL_MENU_LEFTOFFSET_BIG, CRL_MENU_TOPOFFSET,
    DrawCRLGameplay,
    ITEMCOUNT(CRLGameplayItems), CRLGameplayItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLGameplay (void)
{
    char str[32];

    MN_DrTextACentered("GAMEPLAY FEATURES", 10, cr[CR_YELLOW]);

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[crl_default_skill]);
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, DefSkillColor(crl_default_skill)));

    // Wand start game mode
    sprintf(str, crl_pistol_start ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, crl_pistol_start ? GLOW_GREEN : GLOW_DARKRED));

    // Report revealed secrets
    sprintf(str, crl_revealed_secrets == 1 ? "TOP" :
                 crl_revealed_secrets == 2 ? "CENTERED" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, crl_revealed_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Restore monster targets
    sprintf(str, crl_restore_targets ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_restore_targets ? GLOW_GREEN : GLOW_DARKRED));

    // On death action
    sprintf(str, crl_death_use_action == 1 ? "LAST SAVE" :
                 crl_death_use_action == 2 ? "NOTHING" : "DEFAULT");
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, crl_death_use_action ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("STATUS BAR", 70, cr[CR_YELLOW]);

    // Colored status bar
    sprintf(str, crl_colored_stbar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, crl_colored_stbar ? GLOW_GREEN : GLOW_DARKRED));

    // Ammo widget
    sprintf(str, crl_ammo_widget == 1 ? "BRIEF" :
                 crl_ammo_widget == 2 ? "FULL" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, crl_ammo_widget ? GLOW_GREEN : GLOW_DARKRED));

    // Ammo widget translucency
    sprintf(str, crl_ammo_widget_translucent ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 100,
               M_Item_Glow(8, !crl_ammo_widget ? GLOW_DARKRED :
                               crl_ammo_widget_translucent ? GLOW_GREEN : GLOW_DARKRED));

    // Ammo widget colors
    sprintf(str, crl_ammo_widget_colors == 1 ? "AMMO+WEAPONS" :
                 crl_ammo_widget_colors == 2 ? "AMMO ONLY" :
                 crl_ammo_widget_colors == 3 ? "WEAPONS ONLY" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, !crl_ammo_widget ? GLOW_DARKRED :
                               crl_ammo_widget_colors ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("DEMOS", 120, cr[CR_YELLOW]);

    // Show Demo timer
    sprintf(str, crl_demo_timer == 1 ? "PLAYBACK" : 
                 crl_demo_timer == 2 ? "RECORDING" : 
                 crl_demo_timer == 3 ? "ALWAYS" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Timer direction
    sprintf(str, crl_demo_timerdir ? "BACKWARD" : "FORWARD");
    MN_DrTextA(str, M_ItemRightAlign(str), 140,
               M_Item_Glow(12, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Show progress bar
    sprintf(str, crl_demo_bar ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 150,
               M_Item_Glow(13, crl_demo_bar ? GLOW_GREEN : GLOW_DARKRED));

    // Play internal demos
    sprintf(str, crl_internal_demos ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 160,
               M_Item_Glow(14, crl_internal_demos ? GLOW_DARKRED : GLOW_GREEN));
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

static void CRL_RevealedSecrets (int choice)
{
    crl_revealed_secrets = M_INT_Slider(crl_revealed_secrets, 0, 2, choice, false);
}

static void CRL_RestoreTargets (int option)
{
    crl_restore_targets ^= 1;
}

static void CRL_OnDeathAction (int choice)
{
    crl_death_use_action = M_INT_Slider(crl_death_use_action, 0, 2, choice, false);
}

static void CRL_ColoredSBar (int option)
{
    crl_colored_stbar ^= 1;
}

static void CRL_AmmoWidget (int choice)
{
    crl_ammo_widget = M_INT_Slider(crl_ammo_widget, 0, 2, choice, false);
}

static void CRL_AmmoWidgetTranslucent (int choice)
{
    crl_ammo_widget_translucent = M_INT_Slider(crl_ammo_widget_translucent, 0, 1, choice, false);
}

static void CRL_AmmoWidgetColors (int choice)
{
    crl_ammo_widget_colors = M_INT_Slider(crl_ammo_widget_colors, 0, 3, choice, false);
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

    // [JN] Redraw status bar to possibly 
    // clean up remainings of progress bar.
    SB_state = -1;
}

static void CRL_InternalDemos (int option)
{
    crl_internal_demos ^= 1;
}

// -----------------------------------------------------------------------------
// Misc features 1
// -----------------------------------------------------------------------------

static MenuItem_t CRLMiscItems_1[] = {
    { ITT_LRFUNC1, "INVULNERABILITY EFFECT",    CRL_Invul,       0, MENU_NONE },
    { ITT_LRFUNC2, "PALETTE FLASH EFFECTS",     CRL_PalFlash,    0, MENU_NONE },
    { ITT_LRFUNC1, "MOVEMENT BOBBING",          CRL_MoveBob,     0, MENU_NONE },
    { ITT_LRFUNC1, "WEAPON BOBBING",            CRL_WeaponBob,   0, MENU_NONE },
    { ITT_LRFUNC2, "COLORBLIND",                CRL_Colorblind,  0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,            0, MENU_NONE },
    { ITT_LRFUNC2, "AUTOLOAD WAD FILES",        CRL_AutoloadWAD, 0, MENU_NONE },
    { ITT_LRFUNC2, "AUTOLOAD DEH FILES",        CRL_AutoloadDEH, 0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,            0, MENU_NONE },
    { ITT_LRFUNC2, "HIGHLIGHTING EFFECT",       CRL_Hightlight,  0, MENU_NONE },
    { ITT_LRFUNC1, "ESC KEY BEHAVIOUR",         CRL_MenuEscKey,  0, MENU_NONE },
    { ITT_LRFUNC1, "QUIT CONFIRMATION",         CRL_ConfirmQuit, 0, MENU_NONE },
    { ITT_LRFUNC1, "CAP FRAMERATE IN THE MENU", CRL_MenuCapFps,  0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,            0, MENU_NONE },
    { ITT_LRFUNC2, "", /* < SCROLL PAGES >*/    M_ScrollMisc,    0, MENU_NONE },
};

static Menu_t CRLMisc_1 = {
    CRL_MENU_LEFTOFFSET_BIG, CRL_MENU_TOPOFFSET,
    DrawCRLMisc_1,
    ITEMCOUNT(CRLMiscItems_1), CRLMiscItems_1,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLMisc_1 (void)
{
    char str[32];
    const char *bobpercent[] = {
        "OFF","5%","10%","15%","20%","25%","30%","35%","40%","45%","50%",
        "55%","60%","65%","70%","75%","80%","85%","90%","95%","100%"
    };
    const char *colorblind_name[] = {
        "NONE","PROTANOPIA","PROTANOMALY","DEUTERANOPIA","DEUTERANOMALY",
        "TRITANOPIA","TRITANOMALY","ACHROMATOPSIA","ACHROMATOMALY"
    };
    Misc_Cur = (MenuType_t)MENU_MISC_1;

    MN_DrTextACentered("ACCESSIBILITY", 10, cr[CR_YELLOW]);

    // Invulnerability effect
    sprintf(str, crl_a11y_invul ? "GRAYSCALE" : "DEFAULT");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_a11y_invul ? GLOW_GREEN : GLOW_DARKRED));

    // Palette flash effects
    sprintf(str, crl_a11y_pal_flash == 1 ? "HALVED" :
                 crl_a11y_pal_flash == 2 ? "QUARTERED" :
                 crl_a11y_pal_flash == 3 ? "OFF" : "DEFAULT");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, crl_a11y_pal_flash == 1 ? GLOW_YELLOW :
                              crl_a11y_pal_flash == 2 ? GLOW_ORANGE :
                              crl_a11y_pal_flash == 3 ? GLOW_RED : GLOW_DARKRED));

    // Movement bobbing
    sprintf(str, "%s", bobpercent[crl_a11y_move_bob]);
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, crl_a11y_move_bob == 20 ? GLOW_DARKRED :
                              crl_a11y_move_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Weapon bobbing
    sprintf(str, "%s", bobpercent[crl_a11y_weapon_bob]);
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_a11y_weapon_bob == 20 ? GLOW_DARKRED :
                              crl_a11y_weapon_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Colorblind
    sprintf(str, "%s", colorblind_name[crl_colorblind]);
    MN_DrTextA(str, M_ItemRightAlign(str), 60,
               M_Item_Glow(4, crl_colorblind ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("AUTOLOAD", 70, cr[CR_YELLOW]);

    // Autoload WAD files
    sprintf(str, crl_autoload_wad == 1 ? "IWAD ONLY" :
                 crl_autoload_wad == 2 ? "IWAD AND PWAD" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 80,
               M_Item_Glow(6, crl_autoload_wad == 1 ? GLOW_YELLOW :
                              crl_autoload_wad == 2 ? GLOW_GREEN : GLOW_DARKRED));

    // Autoload DEH files
    sprintf(str, crl_autoload_deh == 1 ? "IWAD ONLY" :
                 crl_autoload_deh == 2 ? "IWAD AND PWAD" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 90,
               M_Item_Glow(7, crl_autoload_deh == 1 ? GLOW_YELLOW :
                              crl_autoload_deh == 2 ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("MENU SETTINGS", 100, cr[CR_YELLOW]);

    // Highlighting effect
    sprintf(str, crl_menu_highlight == 1 ? "STATIC" :
                 crl_menu_highlight == 2 ? "ANIMATED" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 110,
               M_Item_Glow(9, crl_menu_highlight == 1 ? GLOW_YELLOW :
                              crl_menu_highlight == 2 ? GLOW_GREEN : GLOW_DARKRED));

    // ESC key behaviour
    sprintf(str, crl_menu_esc_key ? "GO BACK" : "CLOSE MENU" );
    MN_DrTextA(str, M_ItemRightAlign(str), 120,
               M_Item_Glow(10, crl_menu_esc_key ? GLOW_GREEN : GLOW_DARKRED));

    // Quit confirmation
    sprintf(str, crl_confirm_quit ? "ON" : "OFF" );
    MN_DrTextA(str, M_ItemRightAlign(str), 130,
               M_Item_Glow(11, crl_confirm_quit ? GLOW_DARKRED : GLOW_GREEN));

    // Cap framerate in the menu
    sprintf(str, crl_menu_cap_fps ? "ON" : "OFF" );
    MN_DrTextA(str, M_ItemRightAlign(str), 140,
               M_Item_Glow(12, crl_menu_cap_fps ? GLOW_GREEN : GLOW_DARKRED));

    // [PN] Added explanations for colorblind filters
    if (CurrentItPos == 4 && crl_colorblind)
    {
        const char *colorblind_hint[] = {
            "","RED-BLIND","RED-WEAK","GREEN-BLIND","GREEN-WEAK",
            "BLUE-BLIND","BLUE-WEAK","MONOCHROMACY","BLUE CONE MONOCHROMACY"
        };

        MN_DrTextACentered(colorblind_hint[crl_colorblind], 151, cr[CR_WHITE]);
    }
    // [PN] Added explanations for autoload variables
    if (CurrentItPos == 6 || CurrentItPos == 7)
    {
        const char *off = "AUTOLOAD IS DISABLED";
        const char *first_line = "AUTOLOAD AND FOLDER CREATION";
        const char *second_line1 = "ONLY ALLOWED FOR IWAD FILES";
        const char *second_line2 = "ALLOWED FOR BOTH IWAD AND PWAD FILES";
        const int   autoload_option = (CurrentItPos == 6) ? crl_autoload_wad : crl_autoload_deh;

        switch (autoload_option)
        {
            case 1:
                MN_DrTextACentered(first_line, 160, cr[CR_GRAY]);
                MN_DrTextACentered(second_line1, 170, cr[CR_GRAY]);
                break;

            case 2:
                MN_DrTextACentered(first_line, 160, cr[CR_GRAY]);
                MN_DrTextACentered(second_line2, 170, cr[CR_GRAY]);
                break;

            default:
                MN_DrTextACentered(off, 160, cr[CR_GRAY]);
                break;            
        }
    }
    else
    {
        // < Scroll pages >
        M_DrawScrollPages(CRL_MENU_LEFTOFFSET_BIG, 160, 14, "1/2");
    }
}

static void CRL_Invul (int option)
{
    crl_a11y_invul ^= 1;
}

static void CRL_PalFlash (int option)
{
    crl_a11y_pal_flash = M_INT_Slider(crl_a11y_pal_flash, 0, 3, option, false);
    CRL_ReloadPalette();
}

static void CRL_MoveBob (int option)
{
    crl_a11y_move_bob = M_INT_Slider(crl_a11y_move_bob, 0, 20, option, true);
}

static void CRL_WeaponBob (int option)
{
    crl_a11y_weapon_bob = M_INT_Slider(crl_a11y_weapon_bob, 0, 20, option, true);
}

static void CRL_Colorblind (int option)
{
    crl_colorblind = M_INT_Slider(crl_colorblind, 0, 8, option, false);
    CRL_ReloadPalette();
}

static void CRL_AutoloadWAD (int option)
{
    crl_autoload_wad = M_INT_Slider(crl_autoload_wad, 0, 2, option, false);
}

static void CRL_AutoloadDEH (int option)
{
    crl_autoload_deh = M_INT_Slider(crl_autoload_deh, 0, 2, option, false);
}

static void CRL_Hightlight (int option)
{
    crl_menu_highlight = M_INT_Slider(crl_menu_highlight, 0, 2, option, false);
}

static void CRL_MenuEscKey (int option)
{
    crl_menu_esc_key ^= 1;
}

static void CRL_ConfirmQuit (int option)
{
    crl_confirm_quit ^= 1;
}

static void CRL_MenuCapFps (int choice)
{
    crl_menu_cap_fps ^= 1;
}

// -----------------------------------------------------------------------------
// Misc features 2
// -----------------------------------------------------------------------------

static MenuItem_t CRLMiscItems_2[] = {
    { ITT_LRFUNC1, "ENABLE REWIND",             CRL_Misc_RewindEnable,   0, MENU_NONE },
    { ITT_LRFUNC2, "REWIND INTERWAL (S)",       CRL_Misc_RewindInterwal, 0, MENU_NONE },
    { ITT_LRFUNC1, "REWIND DEPTH (KEY FRAMES)", CRL_Misc_RewindDepth,    0, MENU_NONE },
    { ITT_LRFUNC1, "REWIND TIMEOUT (MS)",       CRL_Misc_RewindTimeout,  0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_LRFUNC1, "SCREENSHOT FORMAT",         CRL_Misc_ShotFormat,     0, MENU_NONE },
    { ITT_LRFUNC1, "", /* Dynamic string */     CRL_Misc_ShotSetup,      0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                        NULL,                    0, MENU_NONE },
    { ITT_LRFUNC2, "", /* < SCROLL PAGES >*/    M_ScrollMisc,            0, MENU_NONE },
};

static Menu_t CRLMisc_2 = {
    CRL_MENU_LEFTOFFSET_BIG, CRL_MENU_TOPOFFSET,
    DrawCRLMisc_2,
    ITEMCOUNT(CRLMiscItems_2), CRLMiscItems_2,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLMisc_2 (void)
{
    char str[32];

    Misc_Cur = (MenuType_t)MENU_MISC_2;

    MN_DrTextACentered("REWIND", 10, cr[CR_YELLOW]);

    // Enable rewind
    sprintf(str, crl_rewind_enable ? "ON" : "OFF" );
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_rewind_enable ? GLOW_GREEN : GLOW_DARKRED));

    // Rewind interwal (s)
    sprintf(str, "%d", crl_rewind_interval);
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, !crl_rewind_enable ? GLOW_DARKRED :
                               crl_rewind_interval == 600 ? GLOW_YELLOW : GLOW_GREEN));

    // Rewind depth (key frames)
    sprintf(str, "%d", crl_rewind_depth);
    MN_DrTextA(str, M_ItemRightAlign(str), 40,
               M_Item_Glow(2, !crl_rewind_enable ? GLOW_DARKRED :
                               crl_rewind_depth == 600 ? GLOW_YELLOW : GLOW_GREEN));

    // Rewind timeout (ms)
    sprintf(str, crl_rewind_timeout == 0 ? "NO LIMIT" : "%d", crl_rewind_timeout);
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, !crl_rewind_enable ? GLOW_DARKRED :
                               crl_rewind_timeout == 25 ? GLOW_YELLOW : GLOW_GREEN));

    MN_DrTextACentered("SCREENSHOTS", 60, cr[CR_YELLOW]);

    // Screenshot format
    sprintf(str, !strcmp(screenshots_format, "png") ? "PNG" : "JPEG");
    MN_DrTextA(str, M_ItemRightAlign(str), 70, M_Item_Glow(5, GLOW_GREEN));

    // Dynamic string: compression level for PNG, quality for JPG
    const char *const label = !strcmp(screenshots_format, "png") ? "COMPRESSION LEVEL" : "QUALITY LEVEL";
    int value = !strcmp(screenshots_format, "png") ? screenshots_png_compression : screenshots_jpg_quality;

    MN_DrTextA(label, CRL_MENU_LEFTOFFSET_BIG, 80, M_Item_Glow(6, GLOW_UNCOLORED));

    M_snprintf(str, 4, "%d", value);
    MN_DrTextA(str, M_ItemRightAlign(str), 80, M_Item_Glow(6, GLOW_GREEN));

    // Dynamic hints for screenshot settings.
    if (CurrentItPos == 5)
    {
        MN_DrTextACentered("\"PNG\" PROVIDES LOSSLESS QUALITY,", 100, cr[CR_GRAY]);
        MN_DrTextACentered("\"JPEG\" OFFERS FASTER SAVING",      110, cr[CR_GRAY]);
    }
    if (CurrentItPos == 6)
    {
        if (!strcmp(screenshots_format, "png"))
        {
            MN_DrTextACentered("HIGHER = SLOWER SAVE, SMALLER FILE", 100, cr[CR_GRAY]);
            MN_DrTextACentered("LOWER = FASTER SAVE, LARGER FILE",   110, cr[CR_GRAY]);
            MN_DrTextACentered("DEFAULT LEVEL IS 6",                 120, cr[CR_GRAY]);
        }
        else
        {
            MN_DrTextACentered("HIGHER = BETTER QUALITY, LARGER FILE", 100, cr[CR_GRAY]);
            MN_DrTextACentered("LOWER = WORSE QUALITY, SMALLER FILE",  110, cr[CR_GRAY]);
            MN_DrTextACentered("DEFAULT LEVEL IS 90",                  120, cr[CR_GRAY]);
        }
    }

    // < Scroll pages >
    M_DrawScrollPages(CRL_MENU_LEFTOFFSET_BIG, 160, 14, "2/2");
}

static void CRL_Misc_RewindEnable (int option)
{
    crl_rewind_enable ^= 1;

    // Clear key frames after disabling.
    if (!crl_rewind_enable)
    {
        G_ResetRewind(true);
    }
}

static void CRL_Misc_RewindInterwal (int option)
{
    crl_rewind_interval = M_INT_Slider(crl_rewind_interval, 1, 600, option, false);
}

static void CRL_Misc_RewindDepth (int option)
{
    crl_rewind_depth = M_INT_Slider(crl_rewind_depth, 10, 600, option, false);
}

static void CRL_Misc_RewindTimeout (int option)
{
    crl_rewind_timeout = M_INT_Slider(crl_rewind_timeout, 0, 25, option, false);
}

static void CRL_Misc_ShotFormat (int option)
{
    screenshots_format = strcmp(screenshots_format, "png") ? "png" : "jpg";
}

static void CRL_Misc_ShotSetup (int option)
{
    if (!strcmp(screenshots_format, "png"))
    {
        screenshots_png_compression = M_INT_Slider(screenshots_png_compression, 0, 10, option, false);
    }
    else
    {
        screenshots_jpg_quality = M_INT_Slider(screenshots_jpg_quality, 1, 100, option, false);
    }
}

static void M_ScrollMisc (int option)
{
         if (CurrentMenu == &CRLMisc_1) { SetMenu(MENU_MISC_2); }
    else if (CurrentMenu == &CRLMisc_2) { SetMenu(MENU_MISC_1); }

    CurrentItPos = 14;
}

// -----------------------------------------------------------------------------
// Limits and warnings
// -----------------------------------------------------------------------------

static MenuItem_t CRLLimitsItems[] = {
    { ITT_LRFUNC1, "UNKNOWN LINE SPECIALS",   CRL_UnknownLineWarning, 0, MENU_NONE },
    { ITT_LRFUNC1, "SAVE GAME LIMIT WARNING", CRL_SaveSizeWarning,    0, MENU_NONE },
    { ITT_EMPTY,   NULL,                      NULL,                   0, MENU_NONE },
    { ITT_LRFUNC1, "RENDER LIMITS LEVEL",     CRL_Limits,             0, MENU_NONE }
};

static Menu_t CRLLimits = {
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    DrawCRLLimits,
    ITEMCOUNT(CRLLimitsItems), CRLLimitsItems,
    0,
    SmallFont, false, false,
    MENU_CRLMAIN
};

static void DrawCRLLimits (void)
{
    char str[32];

    MN_DrTextACentered("WARNINGS", 10, cr[CR_YELLOW]);

    // Unknown line specials
    sprintf(str, crl_unknown_linedefs ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 20,
               M_Item_Glow(0, crl_unknown_linedefs ? GLOW_GREEN : GLOW_DARKRED));

    // Save game limit warning
    sprintf(str, vanilla_savegame_limit ? "ON" : "OFF");
    MN_DrTextA(str, M_ItemRightAlign(str), 30,
               M_Item_Glow(1, vanilla_savegame_limit ? GLOW_GREEN : GLOW_DARKRED));

    MN_DrTextACentered("ENGINE LIMITS", 40, cr[CR_YELLOW]);

    // Level of the limits
    sprintf(str, crl_vanilla_limits ? "VANILLA" : "HERETIC-PLUS");
    MN_DrTextA(str, M_ItemRightAlign(str), 50,
               M_Item_Glow(3, crl_vanilla_limits ? GLOW_RED : GLOW_GREEN));

    MN_DrTextA("MAXVISPLANES",  CRL_MENU_LEFTOFFSET_SML + 16,  60, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXDRAWSEGS",   CRL_MENU_LEFTOFFSET_SML + 16,  70, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXVISSPRITES", CRL_MENU_LEFTOFFSET_SML + 16,  80, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXOPENINGS",   CRL_MENU_LEFTOFFSET_SML + 16,  90, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXPLATS",      CRL_MENU_LEFTOFFSET_SML + 16, 100, cr[CR_MENU_DARK2]);
    MN_DrTextA("MAXLINEANIMS",  CRL_MENU_LEFTOFFSET_SML + 16, 110, cr[CR_MENU_DARK2]);

    if (crl_vanilla_limits)
    {
        MN_DrTextA("128",   CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"),   60, cr[CR_RED]);
        MN_DrTextA("256",   CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("256"),   70, cr[CR_RED]);
        MN_DrTextA("128",   CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("128"),   80, cr[CR_RED]);
        MN_DrTextA("20480", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("20480"), 90, cr[CR_RED]);
        MN_DrTextA("30",    CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("30"),   100, cr[CR_RED]);
        MN_DrTextA("64",    CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("64"),   110, cr[CR_RED]);
    }
    else
    {
        MN_DrTextA("1024",  CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"),   60, cr[CR_GREEN]);
        MN_DrTextA("2048",  CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("2048"),   70, cr[CR_GREEN]);
        MN_DrTextA("1024",  CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("1024"),   80, cr[CR_GREEN]);
        MN_DrTextA("65536", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("65536"),  90, cr[CR_GREEN]);
        MN_DrTextA("7680",  CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("7680"),  100, cr[CR_GREEN]);
        MN_DrTextA("16384", CRL_MENU_RIGHTOFFSET_SML - 16 - MN_TextAWidth("16384"), 110, cr[CR_GREEN]);
    }
}

static void CRL_UnknownLineWarning (int choice)
{
    crl_unknown_linedefs ^= 1;
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
    &CRLWidgetsMenu,
    &CRLAutomap,
    &CRLGameplay,
    &CRLMisc_1,
    &CRLMisc_2,
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
    // [JN] Initialize to prevent crashing on pressing "back".
    CurrentMenu = &MainMenu;
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
    Misc_Cur = (MenuType_t)MENU_MISC_1;

    // [JN] Initialize cursor position with hidden, will be set on menu opening.
    CurrentItPos = -1;
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

static char MN_CheckValidChar (char ascii_index, int have_cursor)
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
    // [JN] Make KIS/time widgets translucent while in active Save/Load menu.
    savemenuactive = (MenuActive && !askforquit
                  && (CurrentMenu == &SaveMenu || CurrentMenu == &LoadMenu));

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

    // [JN] Call the menu control routine for mouse input.
    M_ID_MenuMouseControl();

    // [JN] Cursor glowing animation:
    cursor_tics += cursor_direction ? -1 : 1;
    if (cursor_tics == 8 || cursor_tics == -8)
    {
        cursor_direction = !cursor_direction;
    }

    // [JN] Menu item fading effect:
    // Keep menu item bright or decrease tics for fading effect.
    for (int i = 0 ; i < CurrentMenu->itemCount ; i++)
    {
        if (crl_menu_highlight == 1)
        {
            CurrentMenu->items[i].tics =
                (CurrentItPos == i) ? 5 : 0;
        }
        else
        if (crl_menu_highlight == 2)
        {
            CurrentMenu->items[i].tics = (CurrentItPos == i) ? 5 :
                (CurrentMenu->items[i].tics > 0 ? CurrentMenu->items[i].tics - 1 : 0);
        }
        else
        {
            CurrentMenu->items[i].tics = 0;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ID_MenuMouseControl
//  [PN/JN] Set menu cursor position under the mouse cursor.
// -----------------------------------------------------------------------------

inline static void M_ID_MenuMouseControl (void)
{
    // Skip if mouse control disabled or any binding is active
    if (!menu_mouse_allow || KbdIsBinding || MouseIsBinding)
        return;

    // Precompute scaled horizontal boundaries for the entire menu
    const int left = CurrentMenu->x;
    const int right = SCREENWIDTH - CurrentMenu->x;

    // Determine line height based on font type
    const int line_height = (CurrentMenu->FontType == SmallFont) ? ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;
    const int slider_height = (CurrentMenu->FontType == SmallFont) ? 3 : 2;
    const int scaled_line_height = line_height;
    const int base_y = CurrentMenu->y;

    // Quick reject: mouse outside horizontal bounds or above menu
    if (menu_mouse_x < left || menu_mouse_x > right || menu_mouse_y < base_y)
    {
        CurrentItPos = -1;
        return;
    }

    // Approximate bottom boundary - if mouse is far below, reject early
    const int max_bottom_estimate = base_y + CurrentMenu->itemCount * scaled_line_height;
    if (menu_mouse_y > max_bottom_estimate)
    {
        CurrentItPos = -1;
        return;
    }

    // Reset selection; will be set if cursor is over an item
    CurrentItPos = -1;

    // Scan menu items from top to bottom
    for (int i = 0; i < CurrentMenu->itemCount; ++i)
    {
        // Skip empty items (ITT_EMPTY)
        if (CurrentMenu->items[i].type == ITT_EMPTY)
            continue;

        // Sliders occupy three lines, normal items one line
        const int mn_lines = (CurrentMenu->items[i].type == ITT_SLDR) ? slider_height : 1;
        const int mn_top = base_y + i * scaled_line_height;
        const int mn_bottom = mn_top + mn_lines * scaled_line_height;

        // If mouse is above current item, further items are even lower - stop scan
        if (menu_mouse_y < mn_top)
            break;

        // Check vertical overlap
        if (menu_mouse_y <= mn_bottom)
        {
            CurrentItPos = i;
            break; // Found the topmost item under cursor
        }
    }
}

// -----------------------------------------------------------------------------
// M_ID_HandleSliderMouseControl
//  [PN/JN] Handle slider position setting under the mouse cursor.
// -----------------------------------------------------------------------------

inline static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max)
{
    // Ignore if mouse clicks are not allowed (prevents multiple adjustments)
    if (!menu_mouse_allow_click)
        return;

    // Adjust slider boundaries to account for screen resolution and widescreen offset
    const int adj_x = x;
    const int adj_y = y;
    const int adj_width = width;
    const int adj_height = ITEM_HEIGHT;

    // Verify mouse is within slider boundaries and current item is actually a slider
    const MenuItem_t *const item = &CurrentMenu->items[CurrentItPos];
    if (menu_mouse_x < adj_x || menu_mouse_x > adj_x + adj_width
    ||  menu_mouse_y < adj_y || menu_mouse_y > adj_y + adj_height
    ||  item->type != ITT_SLDR)
        return;

    // Calculate normalized cursor position (0.0 to 1.0) within slider
    // Adding +5 provides a small deadzone for better precision at edges
    const float normalized = (float)(menu_mouse_x - adj_x + 5) / (float)adj_width;
    const float range = max - min;
    boolean value_changed = false;

    // Update the actual value based on cursor position and data type
    if (is_float)
    {
        float *v = (float *)value;
        const float old_value = *v;
        *v = min + normalized * range;
        value_changed = (old_value != *v);
    }
    else
    {
        int *v = (int *)value;
        const int old_value = *v;
        *v = (int)(min + normalized * range);
        value_changed = (old_value != *v);
    }

    // Execute the item's routine to handle any side effects
    // and prevent multiple clicks from processing in the same frame
    item->func(-1);
    menu_mouse_allow_click = false;

    // Provide audio feedback only if the value actually changed
    if (value_changed)
        S_StartSound(NULL, sfx_keyup);
}

//---------------------------------------------------------------------------
//
// PROC MN_Drawer
//
//---------------------------------------------------------------------------

static const char *const QuitEndMsg[] = {
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
        // [JN] Disallow cursor if menu is not active.
        menu_mouse_allow = false;

        if (askforquit)
        {
            message = DEH_String(QuitEndMsg[typeofask - 1]);
            // [JN] Allow cursor while active type of asking.
            menu_mouse_allow = true;

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
                    MN_DrTextB(DEH_String(item->text), x, y, M_Big_Line_Glow(CurrentMenu->items[i].tics));
                }
                // [JN] Else, don't draw file slot names (1, 2, 3, ...) in Save/Load menus.
            }
            y += CurrentMenu->FontType == SmallFont ? ID_MENU_LINEHEIGHT_SMALL : ITEM_HEIGHT;
            item++;
        }
        
        if (CurrentItPos != -1)
        {
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

// [PN] Read basic map/skill/time metadata from Heretic save header.
static boolean MN_ReadSaveMeta(FILE *fp, byte *skill, byte *episode,
                               byte *map, int *save_leveltime)
{
    const int save_version_size = 16;

    if (fp == NULL || skill == NULL || episode == NULL
     || map == NULL || save_leveltime == NULL)
    {
        return false;
    }

    if (fseek(fp, SAVESTRINGSIZE + save_version_size, SEEK_SET) != 0)
    {
        return false;
    }

    const int s = fgetc(fp);
    const int e = fgetc(fp);
    const int m = fgetc(fp);
    const int idmus = fgetc(fp);

    if (s == EOF || e == EOF || m == EOF || idmus == EOF)
    {
        return false;
    }

    if (fseek(fp, MAXPLAYERS, SEEK_CUR) != 0)
    {
        return false;
    }

    const int a = fgetc(fp);
    const int b = fgetc(fp);
    const int c = fgetc(fp);

    if (a == EOF || b == EOF || c == EOF)
    {
        return false;
    }

    *skill = (byte)s;
    *episode = (byte)e;
    *map = (byte)m;
    *save_leveltime = (a << 16) | (b << 8) | c;

    return true;
}

// [PN] Format Heretic map identifier from save metadata.
static void MN_FormatSaveMap(char *buf, size_t buflen, byte episode, byte map)
{
    M_snprintf(buf, buflen, "E%dM%d", episode, map);
}

// [PN] Format level time from tics as MM:SS or H:MM:SS.
static void MN_FormatSaveTime(char *buf, size_t buflen, int tics)
{
    int total_seconds = (tics >= 0) ? (tics / TICRATE) : 0;
    const int hours = total_seconds / 3600;
    const int minutes = (total_seconds % 3600) / 60;
    const int seconds = total_seconds % 60;

    if (hours > 0)
    {
        M_snprintf(buf, buflen, "%d:%02d:%02d", hours, minutes, seconds);
    }
    else
    {
        M_snprintf(buf, buflen, "%d:%02d", minutes, seconds);
    }
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
    // [PN] Keep PAGE label aligned with Save/Load list shift (base x was 70).
    MN_DrTextA(pagestr, SCREENWIDTH / 2 + (menu->x - 65) - MN_TextAWidth(pagestr) / 2,
               y, cr[CR_MENU_DARK4]);

    // [JN] Print "modified" (or created initially) time of savegame file.
    if (CurrentItPos != -1 && SlotStatus[CurrentItPos] && !FileMenuKeySteal)
    {
        struct stat filestat;
        char filedate[32];
        char filetime[32];
        char *filename = SV_Filename(CurrentItPos);

        if (M_stat(filename, &filestat) == 0)
        {
        int date_x, time_x;
// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
        strftime(filedate, sizeof(filedate), "%x", localtime(&filestat.st_mtime));
        strftime(filetime, sizeof(filetime), "%X", localtime(&filestat.st_mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        date_x = SAVE_PREVIEW_X + (V_SAVEPREVIEW_WIDTH - MN_TextAWidth(filedate)) / 2;
        time_x = SAVE_PREVIEW_X + (V_SAVEPREVIEW_WIDTH - MN_TextAWidth(filetime)) / 2;
        MN_DrTextA(filedate, date_x, SAVE_PREVIEW_Y + V_SAVEPREVIEW_HEIGHT + 6, cr[CR_MENU_DARK4]);
        MN_DrTextA(filetime, time_x, SAVE_PREVIEW_Y + V_SAVEPREVIEW_HEIGHT + 16, cr[CR_MENU_DARK4]);

        if (SlotMeta[CurrentItPos].present)
        {
            char mapid[16];
            char mapline[32];
            char skillline[32];
            char timestr[16];
            char timeline[32];
            const byte skill = SlotMeta[CurrentItPos].skill;
            const char *skillname = "?";
            int map_x, skill_x, time2_x;

            MN_FormatSaveMap(mapid, sizeof(mapid),
                             SlotMeta[CurrentItPos].episode,
                             SlotMeta[CurrentItPos].map);
            M_snprintf(mapline, sizeof(mapline), "MAP: %s", mapid);

            if (skill < 5)
            {
                skillname = SaveSkillShortName[skill];
            }

            M_snprintf(skillline, sizeof(skillline), "SKILL: %s", skillname);
            MN_FormatSaveTime(timestr, sizeof(timestr), SlotMeta[CurrentItPos].leveltime);
            M_snprintf(timeline, sizeof(timeline), "TIME: %s", timestr);

            map_x = SAVE_PREVIEW_X + (V_SAVEPREVIEW_WIDTH - MN_TextAWidth(mapline)) / 2;
            skill_x = SAVE_PREVIEW_X + (V_SAVEPREVIEW_WIDTH - MN_TextAWidth(skillline)) / 2;
            time2_x = SAVE_PREVIEW_X + (V_SAVEPREVIEW_WIDTH - MN_TextAWidth(timeline)) / 2;

            MN_DrTextA(mapline, map_x, SAVE_PREVIEW_Y + V_SAVEPREVIEW_HEIGHT + 36, cr[CR_MENU_DARK4]);
            MN_DrTextA(skillline, skill_x, SAVE_PREVIEW_Y + V_SAVEPREVIEW_HEIGHT + 46, cr[CR_MENU_DARK4]);
            MN_DrTextA(timeline, time2_x, SAVE_PREVIEW_Y + V_SAVEPREVIEW_HEIGHT + 56, cr[CR_MENU_DARK4]);
        }
        }
        free(filename);
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
    DrawSavePreview(&LoadMenu);
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
    DrawSavePreview(&SaveMenu);
    MN_DrTextB(title, 160 - MN_TextBWidth(title) / 2, 1, NULL);
    DrawSaveLoadBottomLine(&SaveMenu);
}

// [PN] Draw decorative preview frame using Heretic beveled border patches.
static void DrawSavePreviewBorder(int x, int y, int w, int h)
{
    patch_t *const patch_top = W_CacheLumpName(DEH_String("bordt"), PU_CACHE);
    patch_t *const patch_bottom = W_CacheLumpName(DEH_String("bordb"), PU_CACHE);
    patch_t *const patch_left = W_CacheLumpName(DEH_String("bordl"), PU_CACHE);
    patch_t *const patch_right = W_CacheLumpName(DEH_String("bordr"), PU_CACHE);
    patch_t *const patch_tl = W_CacheLumpName(DEH_String("bordtl"), PU_CACHE);
    patch_t *const patch_tr = W_CacheLumpName(DEH_String("bordtr"), PU_CACHE);
    patch_t *const patch_bl = W_CacheLumpName(DEH_String("bordbl"), PU_CACHE);
    patch_t *const patch_br = W_CacheLumpName(DEH_String("bordbr"), PU_CACHE);

    // [PN] Tile top/bottom without overshooting when w is not divisible by 16.
    for (int i = 0; i + 16 < w; i += 16)
    {
        V_DrawPatch(x + i, y - 4, patch_top, "BORDT");
        V_DrawPatch(x + i, y + h, patch_bottom, "BORDB");
    }
    V_DrawPatch(x + ((w > 16) ? (w - 16) : 0), y - 4, patch_top, "BORDT");
    V_DrawPatch(x + ((w > 16) ? (w - 16) : 0), y + h, patch_bottom, "BORDB");

    // [PN] Tile left/right without overshooting when h is not divisible by 16.
    for (int i = 0; i + 16 < h; i += 16)
    {
        V_DrawPatch(x - 4, y + i, patch_left, "BORDL");
        V_DrawPatch(x + w, y + i, patch_right, "BORDR");
    }
    V_DrawPatch(x - 4, y + ((h > 16) ? (h - 16) : 0), patch_left, "BORDL");
    V_DrawPatch(x + w, y + ((h > 16) ? (h - 16) : 0), patch_right, "BORDR");

    V_DrawPatch(x - 4, y - 4, patch_tl, "BORDTL");
    V_DrawPatch(x + w, y - 4, patch_tr, "BORDTR");
    V_DrawPatch(x - 4, y + h, patch_bl, "BORDBL");
    V_DrawPatch(x + w, y + h, patch_br, "BORDBR");
}

// [PN] Draw selected slot thumbnail or black fallback, then frame it.
static void DrawSavePreview(const Menu_t *menu)
{
    const int slot = (CurrentItPos >= 0 && CurrentItPos < SAVES_PER_PAGE) ? CurrentItPos : 0;
    const boolean has_slot = (CurrentItPos >= 0 && CurrentItPos < SAVES_PER_PAGE);

    if (has_slot && SlotPreviewStatus[slot])
    {
        V_DrawBlock(SAVE_PREVIEW_X, SAVE_PREVIEW_Y,
                    V_SAVEPREVIEW_WIDTH, V_SAVEPREVIEW_HEIGHT,
                    SlotPreview[slot]);
    }
    else
    {
        V_DrawFilledBox(SAVE_PREVIEW_X, SAVE_PREVIEW_Y,
                        V_SAVEPREVIEW_WIDTH, V_SAVEPREVIEW_HEIGHT, 0);
    }

    DrawSavePreviewBorder(SAVE_PREVIEW_X, SAVE_PREVIEW_Y,
                            V_SAVEPREVIEW_WIDTH, V_SAVEPREVIEW_HEIGHT);
}

//===========================================================================
//
// MN_LoadSlotText
//
//              Loads in the text message for each slot
//===========================================================================

static void MN_LoadSlotText(void)
{
    FILE *fp;
    int i;
    char *filename;

    for (i = 0; i < SAVES_PER_PAGE; i++)
    {
        int retval;
        filename = SV_Filename(i);
        fp = M_fopen(filename, "rb");
	free(filename);

        if (!fp)
        {
            SlotText[i][0] = 0; // empty the string
            SlotStatus[i] = 0;
            SlotPreviewStatus[i] = false;
            SlotMeta[i].present = false;
            continue;
        }
        retval = fread(&SlotText[i], 1, SLOTTEXTLEN, fp);
        SlotStatus[i] = retval == SLOTTEXTLEN;
        if (SlotStatus[i])
        {
            byte skill, episode, map;
            int slot_leveltime;

            SlotMeta[i].present = MN_ReadSaveMeta(fp, &skill, &episode, &map,
                                                  &slot_leveltime);
            if (SlotMeta[i].present)
            {
                SlotMeta[i].skill = skill;
                SlotMeta[i].episode = episode;
                SlotMeta[i].map = map;
                SlotMeta[i].leveltime = slot_leveltime;
            }
        }
        else
        {
            SlotMeta[i].present = false;
        }
        SlotPreviewStatus[i] = SlotStatus[i] && V_SavePreview_ReadFromFile(fp, SlotPreview[i]);
        fclose(fp);
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
    for (i = 0; i < SAVES_PER_PAGE; i++)
    {
        // [JN] Highlight selected item (CurrentItPos == i) or apply fading effect.
        dp_translation = (crl_menu_highlight && CurrentItPos == i) ? cr[CR_MENU_BRIGHT2] : NULL;
        V_DrawShadowedPatchRavenOptional(x, y, W_CacheLumpName(DEH_String("M_FSLOT"), PU_CACHE), "M_FSLOT");
        dp_translation = NULL;

        if (SlotStatus[i])
        {
            MN_DrTextA(SlotText[i], x + 5, y + 5, crl_menu_highlight && CurrentItPos == i ?
                       cr[CR_MENU_BRIGHT2] : M_Small_Line_Glow(CurrentMenu->items[i].tics));
        }
        y += ITEM_HEIGHT;
    }

    // [JN] Forcefully hide the mouse cursor while typing.
    if (FileMenuKeySteal)
    {
        menu_mouse_allow = false;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawOptionsMenu
//
//---------------------------------------------------------------------------

static void DrawOptionsMenu(void)
{
    MN_DrTextB(DEH_String(showMessages ? "ON" : "OFF"), 196, 50,
                   M_Big_Line_Glow(CurrentMenu->items[1].tics));
    DrawSlider(&OptionsMenu, 3, 10, mouseSensitivity, true, 2);
    M_ID_HandleSliderMouseControl(110, 90, 84, &mouseSensitivity, false, 0, 10);
}

//---------------------------------------------------------------------------
//
// PROC DrawOptions2Menu
//
//---------------------------------------------------------------------------

static void DrawOptions2Menu(void)
{
    char str[32];

    // SFX Volume
    sprintf(str, "%d", snd_MaxVolume);
    DrawSlider(&Options2Menu, 1, 16, snd_MaxVolume, true, 0);
    M_ID_HandleSliderMouseControl(94, 40, 132, &snd_MaxVolume, false, 0, 15);
    MN_DrTextA(str, 252, 45, M_Item_Glow(0, snd_MaxVolume ? GLOW_LIGHTGRAY : GLOW_DARKGRAY));

    // Music Volume
    sprintf(str, "%d", snd_MusicVolume);
    DrawSlider(&Options2Menu, 3, 16, snd_MusicVolume, true, 2);
    M_ID_HandleSliderMouseControl(94, 80, 132, &snd_MusicVolume, false, 0, 15);
    MN_DrTextA(str, 252, 85, M_Item_Glow(2, snd_MusicVolume ? GLOW_LIGHTGRAY : GLOW_DARKGRAY));

    // Screen Size
    sprintf(str, "%d", crl_screen_size);
    DrawSlider(&Options2Menu, 5, 9, crl_screen_size - 3, true, 4);
    M_ID_HandleSliderMouseControl(94, 120, 76, &crl_screen_size, false, 3, 12);
    MN_DrTextA(str, 196, 125, M_Item_Glow(4, GLOW_LIGHTGRAY));
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
    // [JN] CRL - optionally don’t ask for quit confirmation.
    if (!crl_confirm_quit)
    {
        I_Quit();
    }

    MenuWasPaused = paused;
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
        MenuWasPaused = paused;
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
        CT_SetMessage(&players[consoleplayer],
                      DEH_String(showMessages ? "MESSAGES ON" : "MESSAGES OFF"), true, NULL);
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
static boolean StartsWithMapIdentifier (const char *str)
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
    snd_MaxVolume = M_INT_Slider(snd_MaxVolume, 0, 15, option, true);
    // [JN] Always do a full recalc of the sound curve imideatelly.
    // Needed for proper volume update of ambient sounds
    // while active menu and in demo playback mode.
    S_SetMaxVolume();
}

//---------------------------------------------------------------------------
//
// PROC SCMusicVolume
//
//---------------------------------------------------------------------------

static void SCMusicVolume(int option)
{
    snd_MusicVolume = M_INT_Slider(snd_MusicVolume, 0, 15, option, true);
    S_SetMusicVolume();
}

// -----------------------------------------------------------------------------
// SCScreenSize
// -----------------------------------------------------------------------------

static void SCScreenSize(int option)
{
    const int new_size = crl_screen_size + (option == RIGHT_DIR ? 1 : -1);
    
    if (new_size >= 3 && new_size <= 11)
    {
        crl_screen_size = new_size;
        S_StartSound(NULL, sfx_keyup);
        R_SetViewSize(crl_screen_size, detailLevel);
    }
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

static byte heretic_next[6][9] = {
    {12, 13, 14, 15, 16, 19, 18, 21, 17},
    {22, 23, 24, 29, 26, 27, 28, 31, 25},
    {32, 33, 34, 39, 36, 37, 38, 41, 35},
    {42, 43, 44, 49, 46, 47, 48, 51, 45},
    {52, 53, 59, 55, 56, 57, 58, 61, 54},
    {62, 63, 11, 11, 11, 11, 11, 11, 11}, // E6M4-E6M9 shouldn't be accessible
};

// -----------------------------------------------------------------------------
// G_GotoPrevLevel
//  [PN] Mirror of G_GotoNextLevel: warp to the level that would have led here.
//  Keeps the same episode/secret flow and the same shareware/registered guards.
//  IMPORTANT: E6M4-E6M9 are intentionally excluded (they map to E1M1 forward).
// -----------------------------------------------------------------------------
static int G_GotoPrevLevel (void)
{
    int changed = false;

    // Apply the same runtime tweaks as G_GotoNextLevel.
    if (gamemode == shareware)
        heretic_next[0][7] = 11;  // E1M8 secret > E1M1 in shareware

    if (gamemode == registered)
        heretic_next[2][7] = 11;  // E3M8 secret > E1M1 in registered

    if (gamestate == GS_LEVEL)
    {
        // Current level in E*10+M encoding (e.g., E2M3 > 23).
        const int cur  = gameepisode * 10 + gamemap;
        int       prev = cur; // Fallback: stay if no predecessor is found.

        // Find (e,m) such that heretic_next[e][m] == cur and use that (E=M prev).
        // Skip E6M4-E6M9 to avoid back-warping into disallowed maps.
        for (int e = 0; e < 6; ++e)
        {
            for (int m = 0; m < 9; ++m)
            {
                if (e == 5 && m >= 3)         // E6 indexes: m=3..8 are M4..M9
                    continue;

                if (heretic_next[e][m] == cur)
                {
                    prev = (e + 1) * 10 + (m + 1);
                    e = 6;                    // break both loops
                    break;
                }
            }
        }

        const int epsd = prev / 10;
        const int map  = prev % 10;

        // Defer the actual change, just like the original.
        G_DeferedInitNew(gameskill, epsd, map);
        changed = true;
    }

    return changed;
}


static int G_GotoNextLevel(void)
{
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

static boolean MN_ID_TypeOfAsk (void)
{
    switch (typeofask)
    {
        case 1:
            G_CheckDemoStatus();
            I_Quit();
            break;

        case 2:
            players[consoleplayer].messageTics = 0;
            //set the msg to be cleared
            players[consoleplayer].message = NULL;
            paused = false;
            I_SetPalette(W_CacheLumpName ("PLAYPAL", PU_CACHE));
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
/*
        case 8: // [JN] Setting reset.
            M_ID_ApplyReset();
            if (!netgame && !demoplayback)
            {
                paused = true;
            }
            MN_ReturnToMenu();
*/
        default:
            break;
    }

    askforquit = false;
    typeofask = 0;
    return true;
}

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

    // [JN] CRL - optionally don’t ask for quit confirmation.
    if (!crl_confirm_quit)
    {
        if (event->type == ev_quit || (event->type == ev_keydown && event->data1 == key_menu_quit))
        {
            I_Quit();
            return true;
        }
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
        // [JN] Shows the mouse cursor when moved.
        if (event->data2 || event->data3)
        {
        menu_mouse_allow = true;
        menu_mouse_allow_click = false;
        }

        // [JN] Allow menu control by mouse.
        if (event->type == ev_mouse && mousewait < I_GetTime())
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
            if (MouseIsBinding && event->data1 && !event->data2 && !event->data3)
            {
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 5;
                return true;
            }

            if (event->data1 & 1)
            {
                // [JN] ASAN: do not proceed with menu routine
                // for -1 position, just open up a game menu.
                if (CurrentItPos == -1)
                {
                    return false;
                }
                
                if (MenuActive && CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // [JN] Allow repetitive on sliders to move it while mouse movement.
                    menu_mouse_allow_click = true;      
                }
                else
                if (!event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
                {
                if (!MenuActive && !usergame && !demorecording)
                {
                    // [JN] Open the main menu if the game is not active.
                    MN_ActivateMenu();
                }
                else
                {
                key = key_menu_forward;
                mousewait = I_GetTime() + 1;
                }
                }

                if (typeofask
                && !event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
                {
                    MN_ID_TypeOfAsk();
                }
            }

            if (event->data1 & 2
            && !event->data2 && !event->data3) // [JN] Do not consider movement as pressing.
            {
                if (!MenuActive && !usergame && !demorecording)
                {
                    // [JN] Open the main menu if the game is not active.
                    MN_ActivateMenu();
                }
                else
                if (FileMenuKeySteal)
                {
                    key = KEY_ESCAPE;
                    FileMenuKeySteal = false;
                }
                else
                {
                    key = key_menu_back;
                }

                // [JN] Properly return to active menu.
                if (askforquit)
                {
                    askforquit = false;
                    typeofask = 0;
                    MenuActive = true;
                    if (!netgame && !demoplayback)
                    {
                        paused = true;
                    }
                }
                mousewait = I_GetTime() + 1;
            }

            // [JN] Scrolls through menu item values or navigates between pages.
            if (event->data1 & (1 << 4) && MenuActive)  // Wheel down
            {
                if (CurrentItPos == -1
                || (CurrentMenu->ScrollAR && !FileMenuKeySteal && !KbdIsBinding))
                {
                    M_ScrollPages(1);
                }
                else
                if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2
                ||  CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // Scroll menu item backward normally, or forward for ITT_LRFUNC2
                    CurrentMenu->items[CurrentItPos].func(CurrentMenu->items[CurrentItPos].type != ITT_LRFUNC2 ? LEFT_DIR : RIGHT_DIR);
                    if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                    ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2)
                    {
                        S_StartSound(NULL, sfx_switch);
                    }
                }
                mousewait = I_GetTime();
            }
            else
            if (event->data1 & (1 << 3) && MenuActive)  // Wheel up
            {
                if (CurrentItPos == -1
                || (CurrentMenu->ScrollAR && !FileMenuKeySteal && !KbdIsBinding))
                {
                    M_ScrollPages(0);
                }
                else
                if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2
                ||  CurrentMenu->items[CurrentItPos].type == ITT_SLDR)
                {
                    // Scroll menu item forward normally, or backward for ITT_LRFUNC2
                    CurrentMenu->items[CurrentItPos].func(CurrentMenu->items[CurrentItPos].type != ITT_LRFUNC2 ? RIGHT_DIR : LEFT_DIR);
                    if (CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC1
                    ||  CurrentMenu->items[CurrentItPos].type == ITT_LRFUNC2)
                    {
                        S_StartSound(NULL, sfx_switch);
                    }
                }
                mousewait = I_GetTime();
            }
        }
        else
        {
            if (event->type == ev_keydown)
            {
                key = event->data1;
                charTyped = event->data2;
                // [JN] Hide mouse cursor by pressing a key.
                menu_mouse_allow = false;
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
            paused = MenuWasPaused;
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
        (key != 0 && (key == key_menu_screenshot || key == key_menu_screenshot2)))
    {
        G_ScreenShot();
        // [JN] Audible feedback.
        S_StartSound(NULL, sfx_itemup);
        return (true);
    }

    // [PN] Clean screenshot.
    if (key != 0 && (key == key_menu_cleanshot || key == key_menu_cleanshot2))
    {
        R_SetViewSize(13, detailLevel);
        S_StartSound(NULL, sfx_itemup);
        cleanshot_pending = true;
        return (true);
    }

    if (askforquit)
    {
        if (key == key_menu_confirm
        // [JN] Allow to confirm quit (1) and end game (2) by pressing Enter.
        || (key == key_menu_forward && (typeofask == 1 || typeofask == 2))
        // [JN] Confirm by left mouse button.
        || (event->type == ev_mouse && event->data1 & 1)
        // [JN] Allow to exclusevely confirm quit game by pressing F10 again.
        || (key == key_menu_quit && typeofask == 1))
        {
            MN_ID_TypeOfAsk();
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
                return true;
            }
            else
            {
                players[consoleplayer].messageTics = 1;  //set the msg to be cleared
                askforquit = false;
                typeofask = 0;
                paused = MenuWasPaused;
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
            return (true);
        }
        else if (key == key_menu_incscreen)
        {
            if (automapactive)
            {               // Don't screen size in automap
                return (false);
            }
            SCScreenSize(RIGHT_DIR);
            return (true);
        }
        else if (key == key_menu_help || key == key_menu_help2)           // F1
        {
            MenuWasPaused = paused;
            SCInfo(0);      // start up info screens
            MenuActive = true;
            return (true);
        }
        else if (key == key_menu_save || key == key_menu_save2)           // F2 (save game)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                MenuWasPaused = paused;
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
        else if (key == key_menu_load || key == key_menu_load2)           // F3 (load game)
        {
            if (SCNetCheck(2))
            {
                MenuWasPaused = paused;
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
        else if (key == key_menu_volume || key == key_menu_volume2)         // F4 (volume)
        {
            MenuWasPaused = paused;
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
        else if (key == key_menu_qsave || key == key_menu_qsave2)           // F6 (quicksave)
        {
            if (gamestate == GS_LEVEL && !demoplayback)
            {
                if (!quicksave || quicksave == -1)
                {
                    MenuWasPaused = paused;
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
        else if (key == key_menu_endgame || key == key_menu_endgame2)         // F7 (end game)
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
        else if (key == key_menu_messages || key == key_menu_messages2)        // F8 (toggle messages)
        {
            SCMessages(0);
            S_StartSound(NULL, sfx_switch);
            return true;
        }
        else if (key == key_menu_qload || key == key_menu_qload2)           // F9 (quickload)
        {
            if (SCNetCheck(2))
            {
                if (!quickload || quickload == -1)
                {
                    MenuWasPaused = paused;
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
        else if (key == key_menu_quit || key == key_menu_quit2)            // F10 (quit)
        {
            // [JN] Allow to invoke quit in any game state.
            //if (gamestate == GS_LEVEL)
            {
                SCQuitGame(0);
                S_StartSound(NULL, sfx_chat);
            }
            return true;
        }
        // [PN] Go to previous level.
        else if ((singleplayer) && key != 0 && (key == key_crl_prevlevel || key == key_crl_prevlevel2))
        {
            if (G_GotoPrevLevel())
            return true;
        }
        // [crispy] those two can be considered as shortcuts for the IDCLEV cheat
        // and should be treated as such, i.e. add "if (!netgame)"
        else if (!netgame && key != 0
        && (key == key_crl_reloadlevel || key == key_crl_reloadlevel2))
        {
            if (G_ReloadLevel())
            return true;
        }
        else if (!netgame && key != 0
        && (key == key_crl_nextlevel || key == key_crl_nextlevel2))
        {
            if (G_GotoNextLevel())
            return true;
        }

    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma || key == key_menu_gamma2)           // F11 (gamma correction)
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
        if (key == key_menu_activate || key == key_crl_menu || key == key_crl_menu2)
        {
            MN_ActivateMenu();

            // [JN] Spawn CRL menu
            if (key == key_crl_menu || key == key_crl_menu2)
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
        if (key == key_crl_menu || key == key_crl_menu2)
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
            // [JN] Current menu item was hidden while mouse controls,
            // so move cursor to last one menu item by pressing "up" key.
            if (CurrentItPos == -1)
            {
                CurrentItPos = CurrentMenu->itemCount;
            }

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
        else if (key == key_menu_activate)     // Toggle menu
        {
            // [JN] If ESC key behaviour is set to "go back":
            if (crl_menu_esc_key)
            {
                if (CurrentMenu == &MainMenu || CurrentMenu == &Options2Menu
                ||  CurrentMenu == &LoadMenu || CurrentMenu == &SaveMenu)
                {
                    goto id_close_menu;  // [JN] Close menu imideatelly.
                }
                else
                {
                    goto id_prev_menu;   // [JN] Go to previous menu.
                }
            }
            else
            {
            id_close_menu:
            MN_DeactivateMenu();
            }
            return (true);
        }
        else if (key == key_menu_back)         // Go back to previous menu
        {
            id_prev_menu:
            if (CurrentMenu->prevMenu == MENU_NONE)
            {
                MN_DeactivateMenu();
            }
            else
            {
                S_StartSound(NULL, sfx_switch);
                SetMenu(CurrentMenu->prevMenu);
            }
            return (true);
        }
        else if (key == key_menu_left)       // Slider left
        {
            if ((item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2 || item->type == ITT_SLDR) && item->func != NULL)
            {
                item->func(LEFT_DIR);
                if (item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2)
                {
                    S_StartSound(NULL, sfx_switch);
                }
            }
            // [JN] Go to previous-left menu by pressing Left Arrow.
            if (CurrentMenu->ScrollAR || CurrentItPos == -1)
            {
                M_ScrollPages(false);
            }
            return (true);
        }
        else if (key == key_menu_right)      // Slider right
        {
            if ((item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2 || item->type == ITT_SLDR) && item->func != NULL)
            {
                item->func(RIGHT_DIR);
                if (item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2)
                {
                    S_StartSound(NULL, sfx_switch);
                }
            }
            // [JN] Go to next-right menu by pressing Right Arrow.
            if (CurrentMenu->ScrollAR || CurrentItPos == -1)
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
        else if (key == key_menu_forward && CurrentItPos != -1)    // Activate item (enter)
        {
            boolean line_action = false;

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
                if (item->type == ITT_LRFUNC1 || item->type == ITT_LRFUNC2)
                {
                    item->func(RIGHT_DIR);
                    line_action = true;
                }
                else if (item->type == ITT_EFUNC)
                {
                    item->func(item->option);
                }
            }
            S_StartSound(NULL, line_action ? sfx_switch : sfx_dorcls);
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
        // Jump to menu item based on first letter:
        // [JN] Allow multiple jumps over menu items with
        // same first letters. This behavior is same to Doom.
        // [PN] Combined loops using a cyclic index to traverse
        // the array twice, avoiding code duplication.
        else if (charTyped != 0)
        {
            for (i = CurrentItPos + 1; i < CurrentMenu->itemCount + CurrentItPos + 1; i++)
            {
                const int index = i % CurrentMenu->itemCount;

                if (CurrentMenu->items[index].text)
                {
                    if (toupper(charTyped) == toupper(DEH_String(CurrentMenu->items[index].text)[0]))
                    {
                        CurrentItPos = index;
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

    // [PN] Preserve pause state from before opening menu.
    // Do not overwrite while returning from confirmation prompts.
    // This behavior is same to Doom.
    if (!askforquit)
    {
        MenuWasPaused = paused;
    }
    MenuActive = true;
    FileMenuKeySteal = false;
    MenuTime = 0;
    CurrentMenu = &MainMenu;
    M_Reset_Line_Glow();
    CurrentItPos = CurrentMenu->oldItPos;
    // [JN] Update mouse cursor position.
    M_ID_MenuMouseControl();
    if (!netgame && !demoplayback)
    {
        paused = true;
    }
    S_StartSound(NULL, sfx_dorcls);
    slottextloaded = false;     //reload the slot text, when needed
    // [JN] Disallow menu items highlighting initially to prevent
    // cursor jumping. It will be allowed by mouse movement.
    menu_mouse_allow = false;
}

//---------------------------------------------------------------------------
//
// PROC MN_DeactivateMenu
//
//---------------------------------------------------------------------------

static void MN_DeactivateMenu(void)
{
    const boolean wasMenuActive = MenuActive;

    if (CurrentMenu != NULL)
    {
        M_Reset_Line_Glow();
        CurrentMenu->oldItPos = CurrentItPos;
    }
    MenuActive = false;
    if (FileMenuKeySteal)
    {
        I_StopTextInput();
    }
    if (wasMenuActive && !netgame && !demoplayback)
    {
        paused = MenuWasPaused;
    }
    // [JN] Do not play closing menu sound on quick save/loading actions.
    // Quick save playing it separatelly, quick load doesn't need it at all.
    if (!quicksave && !quickload)
    {
        S_StartSound(NULL, sfx_dorcls);
    }
    // [JN] Hide cursor on closing menu.
    menu_mouse_allow = false;
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
    M_Reset_Line_Glow();
    CurrentItPos = CurrentMenu->oldItPos;
    // [JN] Update mouse cursor position.
    M_ID_MenuMouseControl();
}

//---------------------------------------------------------------------------
//
// PROC DrawSlider
//
//---------------------------------------------------------------------------

static void DrawSlider(Menu_t * menu, int item, int width, int slot, boolean bigspacing, int itemPos)
{
    int x;
    int y;
    int x2;
    int count;

    x = menu->x + 24;
    y = menu->y + 2 + (item * (bigspacing ? ITEM_HEIGHT : ID_MENU_LINEHEIGHT_SMALL));

    // [JN] Highlight active slider and gem.
    if (itemPos == CurrentItPos)
    {
        dp_translation = cr[CR_MENU_BRIGHT2];
    }

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

    dp_translation = NULL;
}


// =============================================================================
//
//                 [JN/PN] Keyboard and mouse binding routines.
//                   Drawing, coloring, checking and binding.
//
// =============================================================================

enum {
    keyboard,
    mouse,
};

static struct {
    int key;
    char *name;
} key_names[] = KEY_NAMES_ARRAY_RAVEN;

static char *M_MakeBindName (int CurrentItPosOn, int key, int type)
{
    if (CurrentItPos == CurrentItPosOn && (KbdIsBinding || MouseIsBinding))
    {
        return "?";  // Means binding now
    }
    else
    {
        if (type == keyboard)
        {
            for (int i = 0; (size_t)i < arrlen(key_names); ++i)
            {
                if (key_names[i].key == key)
                    return key_names[i].name;
            }
            return "---";  // Means empty
        }
        else  // mouse
        {
            char  num[8];
            char *other_button;

            M_snprintf(num, 8, "%d", key + 1);
            other_button = M_StringJoin("BUTTON #", num, NULL);

            switch (key)
            {
                case -1:  return  "---";            break;  // Means empty
                case  0:  return  "LEFT";           break;
                case  1:  return  "RIGHT";          break;
                case  2:  return  "MIDDLE";         break;
                case  3:  return  "WHLUP";          break;
                case  4:  return  "WHLDN";          break;
                default:  return  other_button;     break;
            }
        }
    }
}

static char *M_NameBind (int CurrentItPosOn, int key1, int key2, int type)
{
    static char buf[32];
    const int empty_val = (type == keyboard ? 0 : -1);

    // Binding right now
    if (CurrentItPos == CurrentItPosOn && (KbdIsBinding || MouseIsBinding))
        return "?";

    // Both empty
    if (key1 == empty_val && key2 == empty_val)
        return "---";

    // Only one bind
    if (key2 == empty_val) return M_MakeBindName(CurrentItPosOn, key1, type);
    if (key1 == empty_val) return M_MakeBindName(CurrentItPosOn, key2, type);

    // Both binds
    const char *a = M_MakeBindName(CurrentItPosOn, key1, type);
    const char *b = M_MakeBindName(CurrentItPosOn, key2, type);
    M_snprintf(buf, sizeof(buf), "%s OR %s", a, b);
    return buf;
}

static void M_DoBindAction (int *slot1, int *slot2, int key, int type)
{
    const int empty_val = (type == keyboard ? 0 : -1);

    // [PN] 0) Ignore "empty" keys just in case
    if (key == empty_val)
    {
        return;
    }

    // [PN] 1) Toggle: re-binding the same key removes it
    if (*slot1 == key)
    {
        // clear primary slot and compact in-place: move alt -> primary
        *slot1 = empty_val;
        if (*slot2 != empty_val)
        {
            *slot1 = *slot2;
            *slot2 = empty_val;
        }
        return;
    }
    if (*slot2 == key)
    {
        // clear only the alt slot
        *slot2 = empty_val;
        return;
    }

    // [PN] 2) Global de-dup: remove this key from all other actions (both slots)
    if (type == keyboard)
    {
        M_CheckBind(key);
    }
    else
    {
        M_CheckMouseBind(key);
    }

    // [PN] 3) Assign: to first empty; if both occupied, overwrite alt
    if (*slot1 == empty_val)      *slot1 = key;
    else if (*slot2 == empty_val) *slot2 = key;
    else                          *slot2 = key;
}

// -----------------------------------------------------------------------------
// M_DrawBindFooter
//  Draw footer in key binding pages with numeration.
// -----------------------------------------------------------------------------

static void M_DrawBindFooter (char *pagenum, boolean drawPages)
{
    const char *string = "PRESS ENTER TO BIND, DEL TO CLEAR";

    if (drawPages)
    {
        MN_DrTextACentered(string, 170, cr[CR_GRAY]);
        MN_DrTextA("PGUP", CRL_MENU_LEFTOFFSET, 180, cr[CR_GRAY]);
        MN_DrTextACentered(M_StringJoin("PAGE ", pagenum, "/9", NULL), 180, cr[CR_GRAY]);
        MN_DrTextA("PGDN", M_ItemRightAlign("PGDN"), 180, cr[CR_GRAY]);
    }
    else
    {
        MN_DrTextACentered(string, 180, cr[CR_GRAY]);
    }
}
// =============================================================================
//
//                            Keyboard binding routines
//
// =============================================================================

typedef enum
{
    KBS_GLOBAL,
    KBS_AUTOMAP_ONLY,
    KBS_SENDTO_ONLY,
} keybind_scope_t;

typedef struct
{
    int bindnum;
    const Menu_t *menu;
    int item;
    int *slot1;
    int *slot2;
    int default1;
    int default2;
    keybind_scope_t scope;
} KeyBindEntry_t;

#define KEYBIND_ENTRY(bindnum, menu_ptr, item_idx, key1, key2, def1, def2, scope_mode) \
    { bindnum, menu_ptr, item_idx, &(key1), &(key2), def1, def2, scope_mode }

static const KeyBindEntry_t keybinds[] =
{
    // Page 1
    KEYBIND_ENTRY(100, &CRLKbdBinds1, 0,  key_up,          key_up2,          'w',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(101, &CRLKbdBinds1, 1,  key_down,        key_down2,        's',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(102, &CRLKbdBinds1, 2,  key_left,        key_left2,        KEY_LEFTARROW,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(103, &CRLKbdBinds1, 3,  key_right,       key_right2,       KEY_RIGHTARROW, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(104, &CRLKbdBinds1, 4,  key_strafeleft,  key_strafeleft2,  'a',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(105, &CRLKbdBinds1, 5,  key_straferight, key_straferight2, 'd',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(106, &CRLKbdBinds1, 6,  key_speed,       key_speed2,       KEY_RSHIFT,     0, KBS_GLOBAL),
    KEYBIND_ENTRY(107, &CRLKbdBinds1, 7,  key_strafe,      key_strafe2,      KEY_RALT,       0, KBS_GLOBAL),
    KEYBIND_ENTRY(108, &CRLKbdBinds1, 8,  key_180turn,     key_180turn2,     0,              0, KBS_GLOBAL),
    KEYBIND_ENTRY(109, &CRLKbdBinds1, 10, key_fire,        key_fire2,        KEY_RCTRL,      0, KBS_GLOBAL),
    KEYBIND_ENTRY(110, &CRLKbdBinds1, 11, key_use,         key_use2,         ' ',            0, KBS_GLOBAL),

    // Page 2 - View, Flying, Inventory
    KEYBIND_ENTRY(200, &CRLKbdBinds2, 0,  key_lookup,      key_lookup2,      KEY_PGDN,       0, KBS_GLOBAL),
    KEYBIND_ENTRY(201, &CRLKbdBinds2, 1,  key_lookdown,    key_lookdown2,    KEY_DEL,        0, KBS_GLOBAL),
    KEYBIND_ENTRY(202, &CRLKbdBinds2, 2,  key_lookcenter,  key_lookcenter2,  KEY_END,        0, KBS_GLOBAL),
    KEYBIND_ENTRY(203, &CRLKbdBinds2, 4,  key_flyup,       key_flyup2,       KEY_PGUP,       0, KBS_GLOBAL),
    KEYBIND_ENTRY(204, &CRLKbdBinds2, 5,  key_flydown,     key_flydown2,     KEY_INS,        0, KBS_GLOBAL),
    KEYBIND_ENTRY(205, &CRLKbdBinds2, 6,  key_flycenter,   key_flycenter2,   KEY_HOME,       0, KBS_GLOBAL),
    KEYBIND_ENTRY(206, &CRLKbdBinds2, 8,  key_invleft,     key_invleft2,     '[',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(207, &CRLKbdBinds2, 9,  key_invright,    key_invright2,    ']',            0, KBS_GLOBAL),
    KEYBIND_ENTRY(208, &CRLKbdBinds2, 10, key_useartifact, key_useartifact2, KEY_ENTER,      0, KBS_GLOBAL),

    // Page 3 - CRL Controls & Game Modes
    KEYBIND_ENTRY(300, &CRLKbdBinds3, 0,  key_crl_menu,        key_crl_menu2,        '`',  0, KBS_GLOBAL),
    KEYBIND_ENTRY(301, &CRLKbdBinds3, 1,  key_crl_prevlevel,   key_crl_prevlevel2, 0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(302, &CRLKbdBinds3, 2,  key_crl_reloadlevel, key_crl_reloadlevel2, 0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(303, &CRLKbdBinds3, 3,  key_crl_nextlevel,   key_crl_nextlevel2,   0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(304, &CRLKbdBinds3, 4,  key_crl_demospeed,   key_crl_demospeed2,   0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(305, &CRLKbdBinds3, 5,  key_crl_extendedhud, key_crl_extendedhud2, 0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(306, &CRLKbdBinds3, 7,  key_crl_spectator,   key_crl_spectator2,   0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(307, &CRLKbdBinds3, 8,  key_crl_cameraup,    key_crl_cameraup2,    0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(308, &CRLKbdBinds3, 9,  key_crl_cameradown,  key_crl_cameradown2,  0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(309, &CRLKbdBinds3, 10, key_crl_cameramoveto,key_crl_cameramoveto2,0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(310, &CRLKbdBinds3, 11, key_crl_freeze,      key_crl_freeze2,      0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(311, &CRLKbdBinds3, 12, key_crl_buddha,      key_crl_buddha2,      0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(312, &CRLKbdBinds3, 13, key_crl_notarget,    key_crl_notarget2,    0,    0, KBS_GLOBAL),
    KEYBIND_ENTRY(313, &CRLKbdBinds3, 14, key_crl_nomomentum,  key_crl_nomomentum2,  0,    0, KBS_GLOBAL),

    // Page 4 - Advanced Movement, Visplanes, Cheats
    KEYBIND_ENTRY(400, &CRLKbdBinds4, 0,  key_crl_autorun,  key_crl_autorun2,  KEY_CAPSLOCK, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(401, &CRLKbdBinds4, 1,  key_crl_mlook,    key_crl_mlook2,    0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(402, &CRLKbdBinds4, 2,  key_crl_novert,   key_crl_novert2,   0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(403, &CRLKbdBinds4, 3,  key_crl_vilebomb, key_crl_vilebomb2, 0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(404, &CRLKbdBinds4, 4,  key_crl_vilefly,  key_crl_vilefly2,  0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(405, &CRLKbdBinds4, 6,  key_crl_clearmax, key_crl_clearmax2, 0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(406, &CRLKbdBinds4, 7,  key_crl_movetomax,key_crl_movetomax2,0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(407, &CRLKbdBinds4, 9,  key_crl_iddqd,    key_crl_iddqd2,    0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(408, &CRLKbdBinds4, 10, key_crl_idfa,     key_crl_idfa2,     0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(409, &CRLKbdBinds4, 11, key_crl_idclip,   key_crl_idclip2,   0,            0, KBS_GLOBAL),
    KEYBIND_ENTRY(410, &CRLKbdBinds4, 12, key_crl_iddt,     key_crl_iddt2,     0,            0, KBS_GLOBAL),

    // Page 5 - Weapons
    KEYBIND_ENTRY(500, &CRLKbdBinds5, 0, key_weapon1,    key_weapon1_2,    '1', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(501, &CRLKbdBinds5, 1, key_weapon2,    key_weapon2_2,    '2', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(502, &CRLKbdBinds5, 2, key_weapon3,    key_weapon3_2,    '3', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(503, &CRLKbdBinds5, 3, key_weapon4,    key_weapon4_2,    '4', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(504, &CRLKbdBinds5, 4, key_weapon5,    key_weapon5_2,    '5', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(505, &CRLKbdBinds5, 5, key_weapon6,    key_weapon6_2,    '6', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(506, &CRLKbdBinds5, 6, key_weapon7,    key_weapon7_2,    '7', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(507, &CRLKbdBinds5, 7, key_weapon8,    key_weapon8_2,    '8', 0, KBS_GLOBAL),
    KEYBIND_ENTRY(508, &CRLKbdBinds5, 8, key_prevweapon, key_prevweapon2,  0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(509, &CRLKbdBinds5, 9, key_nextweapon, key_nextweapon2,  0,   0, KBS_GLOBAL),

    // Page 6 - Artifacts
    KEYBIND_ENTRY(600, &CRLKbdBinds6, 0, key_arti_quartz,       key_arti_quartz2,       0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(601, &CRLKbdBinds6, 1, key_arti_urn,          key_arti_urn2,          0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(602, &CRLKbdBinds6, 2, key_arti_bomb,         key_arti_bomb2,         0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(603, &CRLKbdBinds6, 3, key_arti_tome,         key_arti_tome2,         127, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(604, &CRLKbdBinds6, 4, key_arti_ring,         key_arti_ring2,         0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(605, &CRLKbdBinds6, 5, key_arti_chaosdevice,  key_arti_chaosdevice2,  0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(606, &CRLKbdBinds6, 6, key_arti_shadowsphere, key_arti_shadowsphere2, 0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(607, &CRLKbdBinds6, 7, key_arti_wings,        key_arti_wings2,        0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(608, &CRLKbdBinds6, 8, key_arti_torch,        key_arti_torch2,        0,   0, KBS_GLOBAL),
    KEYBIND_ENTRY(609, &CRLKbdBinds6, 9, key_arti_morph,        key_arti_morph2,        0,   0, KBS_GLOBAL),

    // Page 7 - Automap
    KEYBIND_ENTRY(700, &CRLKbdBinds7, 0, key_map_toggle,     key_map_toggle2,     KEY_TAB,      0, KBS_GLOBAL),
    KEYBIND_ENTRY(701, &CRLKbdBinds7, 1, key_map_zoomin,     key_map_zoomin2,     '=',  KEYP_PLUS, KBS_AUTOMAP_ONLY),
    KEYBIND_ENTRY(702, &CRLKbdBinds7, 2, key_map_zoomout,    key_map_zoomout2,    '-', KEYP_MINUS, KBS_AUTOMAP_ONLY),
    KEYBIND_ENTRY(703, &CRLKbdBinds7, 3, key_map_maxzoom,    key_map_maxzoom2,    '0',          0, KBS_AUTOMAP_ONLY),
    KEYBIND_ENTRY(704, &CRLKbdBinds7, 4, key_map_follow,     key_map_follow2,     'f',          0, KBS_AUTOMAP_ONLY),
    KEYBIND_ENTRY(705, &CRLKbdBinds7, 5, key_crl_map_sndprop,key_crl_map_sndprop2,'p',          0, KBS_AUTOMAP_ONLY),
    KEYBIND_ENTRY(706, &CRLKbdBinds7, 6, key_map_grid,       key_map_grid2,       'g',          0, KBS_AUTOMAP_ONLY),

    // Page 8 - Function Keys
    KEYBIND_ENTRY(800, &CRLKbdBinds8, 0,  key_menu_help,     key_menu_help2,     KEY_F1,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(801, &CRLKbdBinds8, 1,  key_menu_save,     key_menu_save2,     KEY_F2,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(802, &CRLKbdBinds8, 2,  key_menu_load,     key_menu_load2,     KEY_F3,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(803, &CRLKbdBinds8, 3,  key_menu_volume,   key_menu_volume2,   KEY_F4,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(804, &CRLKbdBinds8, 4,  key_menu_qsave,    key_menu_qsave2,    KEY_F6,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(805, &CRLKbdBinds8, 5,  key_menu_endgame,  key_menu_endgame2,  KEY_F7,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(806, &CRLKbdBinds8, 6,  key_menu_messages, key_menu_messages2, KEY_F8,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(807, &CRLKbdBinds8, 7,  key_menu_qload,    key_menu_qload2,    KEY_F9,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(808, &CRLKbdBinds8, 8,  key_menu_quit,     key_menu_quit2,     KEY_F10, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(809, &CRLKbdBinds8, 9,  key_menu_gamma,    key_menu_gamma2,    KEY_F11, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(810, &CRLKbdBinds8, 10, key_spy,           key_spy2,           KEY_F12, 0, KBS_GLOBAL),

    // Page 9 - Shortcuts & Multiplayer
    KEYBIND_ENTRY(900, &CRLKbdBinds9, 0, key_pause,              key_pause2,              KEY_PAUSE,  0, KBS_GLOBAL),
    KEYBIND_ENTRY(901, &CRLKbdBinds9, 1, key_menu_screenshot,    key_menu_screenshot2,    KEY_PRTSCR, 0, KBS_GLOBAL),
    KEYBIND_ENTRY(902, &CRLKbdBinds9, 2, key_demo_quit,          key_demo_quit2,          'q',        0, KBS_GLOBAL),
    KEYBIND_ENTRY(903, &CRLKbdBinds9, 4, key_multi_msg,          key_multi_msg2,          't',        0, KBS_GLOBAL),
    KEYBIND_ENTRY(904, &CRLKbdBinds9, 5, key_multi_msgplayer[0], key_multi_msgplayer2[0], 'g',        0, KBS_SENDTO_ONLY),
    KEYBIND_ENTRY(905, &CRLKbdBinds9, 6, key_multi_msgplayer[1], key_multi_msgplayer2[1], 'i',        0, KBS_SENDTO_ONLY),
    KEYBIND_ENTRY(906, &CRLKbdBinds9, 7, key_multi_msgplayer[2], key_multi_msgplayer2[2], 'b',        0, KBS_SENDTO_ONLY),
    KEYBIND_ENTRY(907, &CRLKbdBinds9, 8, key_multi_msgplayer[3], key_multi_msgplayer2[3], 'r',        0, KBS_SENDTO_ONLY),
};

#undef KEYBIND_ENTRY

static boolean M_KeybindScopeAllowsCheck (const KeyBindEntry_t *entry)
{
    if (entry->scope == KBS_GLOBAL)
    {
        return true;
    }
    else if (entry->scope == KBS_AUTOMAP_ONLY)
    {
        return CurrentMenu == &CRLKbdBinds7;
    }
    else
    {
        return CurrentMenu == &CRLKbdBinds9;
    }
}


// -----------------------------------------------------------------------------
// M_StartBind
//  Indicate that key binding is started (KbdIsBinding), and
//  pass internal number (keyToBind) for binding a new key.
// -----------------------------------------------------------------------------

static void M_StartBind (int keynum)
{
    KbdIsBinding = true;
    keyToBind = keynum;
}

// -----------------------------------------------------------------------------
// M_CheckBind
//  Check if pressed key is already binded, clear previous bind if found.
//  The unbind scope is controlled by keybind metadata table.
// -----------------------------------------------------------------------------

static void M_CheckBind (int key)
{
    for (size_t i = 0; i < sizeof(keybinds) / sizeof(keybinds[0]); i++)
    {
        if (!M_KeybindScopeAllowsCheck(&keybinds[i]))
        {
            continue;
        }

        if (*keybinds[i].slot1 == key)
        {
            *keybinds[i].slot1 = 0;
        }
        if (*keybinds[i].slot2 == key)
        {
            *keybinds[i].slot2 = 0;
        }
    }
}

// -----------------------------------------------------------------------------
// M_DoBind
//  By catching internal bind number (keynum), do actual binding
//  of pressed key (key) to real keybind.
// -----------------------------------------------------------------------------

static void M_DoBind (int keynum, int key)
{
    for (size_t i = 0; i < sizeof(keybinds) / sizeof(keybinds[0]); i++)
    {
        if (keybinds[i].bindnum == keynum)
        {
            M_DoBindAction(keybinds[i].slot1, keybinds[i].slot2, key, keyboard);
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ClearBind
//  Clear key bind on the line where cursor is placed (CurrentItPos).
// -----------------------------------------------------------------------------

static void M_ClearBind (int Current_ItPos)
{
    for (size_t i = 0; i < sizeof(keybinds) / sizeof(keybinds[0]); i++)
    {
        if (keybinds[i].menu == CurrentMenu && keybinds[i].item == Current_ItPos)
        {
            *keybinds[i].slot1 = 0;
            *keybinds[i].slot2 = 0;
            return;
        }
    }
}        

// -----------------------------------------------------------------------------
// M_ResetBinds
//  Reset all keyboard bindings to default.
// -----------------------------------------------------------------------------

static void M_ResetBinds (void)
{
    for (size_t i = 0; i < sizeof(keybinds) / sizeof(keybinds[0]); i++)
    {
        *keybinds[i].slot1 = keybinds[i].default1;
        *keybinds[i].slot2 = keybinds[i].default2;
    }
}

// -----------------------------------------------------------------------------
// M_ColorizeBind
//  Do key bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeBind (int CurrentItPosOn, int key1, int key2)
{
    if (CurrentItPos == CurrentItPosOn && KbdIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        const boolean empty = (key1 == 0 && key2 == 0);

        if (empty)
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
//  Do keyboard bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindKey (int itemNum, int yPos, int key1, int key2)
{
    char *text = M_NameBind(itemNum, key1, key2, keyboard);

    MN_DrTextA(text, M_ItemRightAlign(text), yPos, 
               M_ColorizeBind(itemNum, key1, key2));
}


// =============================================================================
//
//                            Mouse binding routines
//
// =============================================================================

typedef struct
{
    int bindnum;
    int item;
    int *slot1;
    int *slot2;
    int default1;
    int default2;
} MouseBindEntry_t;

#define MOUSEBIND_ENTRY(bindnum, item_idx, btn1, btn2, def1, def2) \
    { bindnum, item_idx, &(btn1), &(btn2), def1, def2 }

static const MouseBindEntry_t mousebinds[] =
{
    MOUSEBIND_ENTRY(1000, 0,  mousebfire,        mousebfire2,         0, -1),
    MOUSEBIND_ENTRY(1001, 1,  mousebforward,     mousebforward2,      2, -1),
    MOUSEBIND_ENTRY(1002, 2,  mousebspeed,       mousebspeed2,       -1, -1),
    MOUSEBIND_ENTRY(1003, 3,  mousebstrafe,      mousebstrafe2,       1, -1),
    MOUSEBIND_ENTRY(1004, 4,  mousebbackward,    mousebbackward2,    -1, -1),
    MOUSEBIND_ENTRY(1005, 5,  mousebuse,         mousebuse2,         -1, -1),
    MOUSEBIND_ENTRY(1006, 6,  mousebstrafeleft,  mousebstrafeleft2,  -1, -1),
    MOUSEBIND_ENTRY(1007, 7,  mousebstraferight, mousebstraferight2, -1, -1),
    MOUSEBIND_ENTRY(1008, 8,  mousebprevweapon,  mousebprevweapon2,   4, -1),
    MOUSEBIND_ENTRY(1009, 9,  mousebnextweapon,  mousebnextweapon2,   3, -1),
    MOUSEBIND_ENTRY(1010, 10, mousebinvleft,     mousebinvleft2,     -1, -1),
    MOUSEBIND_ENTRY(1011, 11, mousebinvright,    mousebinvright2,    -1, -1),
    MOUSEBIND_ENTRY(1012, 12, mousebuseartifact, mousebuseartifact2, -1, -1),
};

#undef MOUSEBIND_ENTRY


// -----------------------------------------------------------------------------
// M_StartMouseBind
//  Indicate that mouse button binding is started (MouseIsBinding), and
//  pass internal number (btnToBind) for binding a new button.
// -----------------------------------------------------------------------------

static void M_StartMouseBind (int btn)
{
    MouseIsBinding = true;
    btnToBind = btn;
}

// -----------------------------------------------------------------------------
// M_CheckMouseBind
//  Check if pressed button is already binded, clear previous bind if found.
// -----------------------------------------------------------------------------

static void M_CheckMouseBind (int btn)
{
    for (size_t i = 0; i < sizeof(mousebinds) / sizeof(mousebinds[0]); i++)
    {
        if (*mousebinds[i].slot1 == btn)
        {
            *mousebinds[i].slot1 = -1;
        }
        if (*mousebinds[i].slot2 == btn)
        {
            *mousebinds[i].slot2 = -1;
        }
    }
}

// -----------------------------------------------------------------------------
// M_DoMouseBind
//  By catching internal bind number (btnnum), do actual binding
//  of pressed button (btn) to real mouse bind.
// -----------------------------------------------------------------------------

static void M_DoMouseBind (int btnnum, int btn)
{
    for (size_t i = 0; i < sizeof(mousebinds) / sizeof(mousebinds[0]); i++)
    {
        if (mousebinds[i].bindnum == btnnum)
        {
            M_DoBindAction(mousebinds[i].slot1, mousebinds[i].slot2, btn, mouse);
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// M_ClearMouseBind
//  Clear mouse bind on the line where cursor is placed (CurrentItPos).
// -----------------------------------------------------------------------------

static void M_ClearMouseBind (int Current_ItPos)
{
    for (size_t i = 0; i < sizeof(mousebinds) / sizeof(mousebinds[0]); i++)
    {
        if (mousebinds[i].item == Current_ItPos)
        {
            *mousebinds[i].slot1 = -1;
            *mousebinds[i].slot2 = -1;
            return;
        }
    }
}

// -----------------------------------------------------------------------------
// M_DrawBindButton
//  Do mouse button bind drawing.
// -----------------------------------------------------------------------------

static void M_DrawBindButton (int itemNum, int yPos, int btn1, int btn2)
{
    char *text = M_NameBind(itemNum, btn1, btn2, mouse);

    MN_DrTextA(text, M_ItemRightAlign(text), yPos,
               M_ColorizeMouseBind(itemNum, btn1, btn2));
}

// -----------------------------------------------------------------------------
// M_ColorizeMouseBind
//  Do mouse bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeMouseBind (int CurrentItPosOn, int btn1, int btn2)
{
    if (CurrentItPos == CurrentItPosOn && MouseIsBinding)
    {
        return cr[CR_YELLOW];
    }
    else
    {
        if (btn1 == -1 && btn2 == -1)
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
// M_ResetMouseBinds
//  Reset all mouse bindings to default.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    for (size_t i = 0; i < sizeof(mousebinds) / sizeof(mousebinds[0]); i++)
    {
        *mousebinds[i].slot1 = mousebinds[i].default1;
        *mousebinds[i].slot2 = mousebinds[i].default2;
    }
}
