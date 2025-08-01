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
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "deh_main.h"
#include "gusconf.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_controls.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "r_local.h"
#include "g_game.h"
#include "m_argv.h"
#include "m_controls.h"
#include "s_sound.h"
#include "doomstat.h"
#include "sounds.h"
#include "m_menu.h"
#include "p_local.h"
#include "v_trans.h"
#include "st_bar.h"

#include "crlcore.h"
#include "crlvars.h"


#define SKULLXOFF		-32
#define LINEHEIGHT       16


boolean menuactive;

// -1 = no quicksave slot picked!
static int quickSaveSlot;

// 1 = message to be printed
static int messageToPrint;
// ...and here is the message string!
static const char *messageString;
// [JN] Certain messages needs background filling.
static boolean messageFillBG;

// message x & y
static int messageLastMenuActive;

// timed message = no input from user
static boolean messageNeedsInput;

static void (*messageRoutine)(int response);

static char *gammalvls[16][2] =
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

// we are going to be entering a savegame string
static int saveStringEnter;              
static int saveSlot;	   // which slot to save in
static int saveCharIndex;  // which char we're editing
static boolean joypadSave = false; // was the save action initiated by joypad?

// old save description before edit
static char saveOldString[SAVESTRINGSIZE];  

// [FG] support up to 16 pages of savegames
int savepage = 0;
static const int savepage_max = 15;

static char savegamestrings[10][SAVESTRINGSIZE];
static char endstring[160];

static boolean opldev;

//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;

    // [JN] Menu item timer for glowing effect.
    short   tics;
    
    char	name[32];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			
} menuitem_t;

typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)(void);	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
    boolean		smallFont;  // [JN] If true, use small font
    boolean		ScrollAR;	// [JN] Menu can be scrolled by arrow keys
    boolean		ScrollPG;	// [JN] Menu can be scrolled by PGUP/PGDN keys
} menu_t;

// [JN] For currentMenu->menuitems[itemOn].statuses:
#define STS_SKIP -1
#define STS_NONE  0
#define STS_SWTC  1
#define STS_MUL1  2
#define STS_MUL2  3
#define STS_SLDR  4

// [JN] Macro definitions for first two items of menuitem_t.
// Trailing zero initializes "tics" field.
#define M_SKIP    STS_SKIP,0  // Skippable, cursor can't get here.
#define M_SWTC    STS_SWTC,0  // On/off type or entering function.
#define M_MUL1    STS_MUL1,0  // Multichoice function: increase by wheel up, decrease by wheel down
#define M_MUL2    STS_MUL2,0  // Multichoice function: decrease by wheel up, increase by wheel down
#define M_SLDR    STS_SLDR,0  // Slider line.

// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

static short itemOn;            // menu item skull is on (-1 = no selection)
static short skullAnimCounter;  // skull animation counter
static short whichSkull;        // which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
static char *skullName[2] = {"M_SKULL1","M_SKULL2"};

// current menudef
static menu_t *currentMenu;

// =============================================================================
// PROTOTYPES
// =============================================================================

static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
static void M_LoadGame(int choice);
static void M_SaveGame(int choice);
static void M_Options(int choice);
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_ChooseCRL_Main (int choice);
static menu_t CRLDef_Main;

static void M_ChangeMessages(int choice);
static void M_ChangeSensitivity(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
static void M_ChangeDetail(int choice);
static void M_SizeDisplay(int choice);
static void M_Sound(int choice);

static void M_FinishReadThis(int choice);
static void M_LoadSelect(int choice);
static void M_SaveSelect(int choice);
static void M_ReadSaveStrings(void);
static void M_QuickSave(void);
static void M_QuickLoad(void);

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawOptions(void);
static void M_DrawSound(void);
static void M_DrawLoad(void);
static void M_DrawSave(void);

static void M_DrawSaveLoadBorder(int x,int y);
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x,int y,int thermWidth,int thermDot,int itemPos);
static int  M_StringHeight(const char *string);
static void M_StartMessage(const char *string,void (*routine)(int),boolean input);
static void M_ClearMenus (void);

static void M_ID_MenuMouseControl (void);
static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max);

// =============================================================================
// DOOM MENU
// =============================================================================

enum
{
    newgame = 0,
    options,
    loadgame,
    savegame,
    readthis,
    quitdoom,
    main_end
} main_e;

static menuitem_t MainMenu[]=
{
    { M_SWTC, "M_NGAME",  M_NewGame,        'n' },
    { M_SWTC, "M_OPTION", M_ChooseCRL_Main, 'o' },
    { M_SWTC, "M_LOADG",  M_LoadGame,       'l' },
    { M_SWTC, "M_SAVEG",  M_SaveGame,       's' },
    // Another hickup with Special edition.
    { M_SWTC, "M_RDTHIS", M_ReadThis,       'r' },
    { M_SWTC, "M_QUITG",  M_QuitDOOM,       'q' }
};

static menu_t MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,64,
    0,
    false, false, false,
};

//
// EPISODE SELECT
//

enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

static menuitem_t EpisodeMenu[]=
{
    {M_SWTC, "M_EPI1", M_Episode, 'k' },
    {M_SWTC, "M_EPI2", M_Episode, 't' },
    {M_SWTC, "M_EPI3", M_Episode, 'i' },
    {M_SWTC, "M_EPI4", M_Episode, 't' }
};

static menu_t EpiDef =
{
    ep_end,		// # of menu items
    &MainDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,63,              // x,y
    ep1,			// lastOn
    false, false, false,
};

//
// NEW GAME
//

enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

static menuitem_t NewGameMenu[]=
{
    { M_SWTC, "M_JKILL", M_ChooseSkill, 'i' },
    { M_SWTC, "M_ROUGH", M_ChooseSkill, 'h' },
    { M_SWTC, "M_HURT",  M_ChooseSkill, 'h' },
    { M_SWTC, "M_ULTRA", M_ChooseSkill, 'u' },
    { M_SWTC, "M_NMARE", M_ChooseSkill, 'n' }
};

static menu_t NewDef =
{
    newg_end,		// # of menu items
    &EpiDef,		// previous menu
    NewGameMenu,	// menuitem_t ->
    M_DrawNewGame,	// drawing routine ->
    48,63,              // x,y
    hurtme,		// lastOn
    false, false, false,
};

//
// OPTIONS MENU
//

enum
{
    endgame,
    messages,
    detail,
    scrnsize,
    option_empty1,
    mousesens,
    option_empty2,
    soundvol,
    opt_end
} options_e;

static menuitem_t OptionsMenu[]=
{
    { M_SWTC, "M_ENDGAM", M_EndGame,           'e' },
    { M_MUL1, "M_MESSG",  M_ChangeMessages,    'm' },
    { M_MUL1, "M_DETAIL", M_ChangeDetail,      'g' },
    { M_SLDR, "M_SCRNSZ", M_SizeDisplay,       's' },
    { M_SKIP, "",0,'\0'},
    { M_SLDR, "M_MSENS",  M_ChangeSensitivity, 'm' },
    { M_SKIP, "",0,'\0'},
    { M_SWTC, "M_SVOL",   M_Sound,             's' }
};

static menu_t OptionsDef =
{
    opt_end,
    &CRLDef_Main,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0,
    false, false, false,
};

//
// Read This! MENU 1 & 2
//

enum
{
    rdthsempty1,
    read1_end
} read_e;

static menuitem_t ReadMenu1[] =
{
    {M_SWTC, "", M_ReadThis2, 0}
};

static menu_t ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0,
    false, false, false,
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

static menuitem_t ReadMenu2[]=
{
    { M_SWTC, "", M_FinishReadThis, 0}
};

static menu_t ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0,
    false, false, false,
};

//
// SOUND VOLUME MENU
//

enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

static menuitem_t SoundMenu[]=
{
    { M_SLDR, "M_SFXVOL", M_SfxVol,   's' },
    { M_SKIP,"",0,'\0'},
    { M_SLDR, "M_MUSVOL", M_MusicVol, 'm' },
    { M_SKIP,"",0,'\0'}
};

static menu_t SoundDef =
{
    sound_end,
    &CRLDef_Main,
    SoundMenu,
    M_DrawSound,
    80,64,
    0,
    false, false, false,
};

//
// LOAD GAME MENU
//

enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load7,
    load8,
    load_end
} load_e;

static menuitem_t LoadMenu[]=
{
    { M_SWTC, "", M_LoadSelect, '1' },
    { M_SWTC, "", M_LoadSelect, '2' },
    { M_SWTC, "", M_LoadSelect, '3' },
    { M_SWTC, "", M_LoadSelect, '4' },
    { M_SWTC, "", M_LoadSelect, '5' },
    { M_SWTC, "", M_LoadSelect, '6' },
    { M_SWTC, "", M_LoadSelect, '7' },
    { M_SWTC, "", M_LoadSelect, '8' }
};

static menu_t LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    67,27,
    0,
    false, true, true,
};

//
// SAVE GAME MENU
//

static menuitem_t SaveMenu[]=
{
    { M_SWTC, "", M_SaveSelect, '1' },
    { M_SWTC, "", M_SaveSelect, '2' },
    { M_SWTC, "", M_SaveSelect, '3' },
    { M_SWTC, "", M_SaveSelect, '4' },
    { M_SWTC, "", M_SaveSelect, '5' },
    { M_SWTC, "", M_SaveSelect, '6' },
    { M_SWTC, "", M_SaveSelect, '7' },
    { M_SWTC, "", M_SaveSelect, '8' }
};

static menu_t SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    67,27,
    0,
    false, true, true,
};

// =============================================================================
// [JN] Custom CRL menu
// =============================================================================

#define CRL_MENU_TOPOFFSET         (16)
#define CRL_MENU_LEFTOFFSET        (48)
#define CRL_MENU_LEFTOFFSET_SML    (72)
#define CRL_MENU_LEFTOFFSET_BIG    (32)
#define CRL_MENU_RIGHTOFFSET_SML   (SCREENWIDTH - CRL_MENU_LEFTOFFSET_SML)

#define CRL_MENU_LINEHEIGHT_SMALL  (9)
#define CRL_MENU_CURSOR_OFFSET     (10)

// Utility function to align menu item names by the right side.
static int M_ItemRightAlign (const char *text)
{
    return SCREENWIDTH - currentMenu->x - M_StringWidth(text);
}

static player_t *player;

//static void M_ChooseCRL_Main (int choice);
static void M_DrawCRL_Main (void);
static void M_CRL_Spectating (int choice);
static void M_CRL_Freeze (int choice);
static void M_CRL_Buddha (int choice);
static void M_CRL_NoTarget (int choice);
static void M_CRL_NoMomentum (int choice);

static void M_ChooseCRL_Video (int choice);
static void M_DrawCRL_Video (void);
static void M_CRL_UncappedFPS (int choice);
static void M_CRL_LimitFPS (int choice);
static void M_CRL_VSync (int choice);
static void M_CRL_ShowFPS (int choice);
static void M_CRL_PixelScaling (int choice);
static void M_CRL_VisplanesDraw (int choice);
static void M_CRL_HOMDraw (int choice);
static void M_CRL_ScreenWipe (int choice);
static void M_CRL_ShowENDOOM (int choice);

static void M_ChooseCRL_Display (int choice);
static void M_DrawCRL_Display (void);
static void M_CRL_Gamma (int choice);
static void M_CRL_MenuBgShading (int choice);
static void M_CRL_LevelBrightness (int choice);
static void M_CRL_MsgCritical (int choice);
static void M_CRL_TextShadows (int choice);

static void M_ChooseCRL_Sound (int choice);
static void M_DrawCRL_Sound (void);
static void M_CRL_SFXSystem (int choice);
static void M_CRL_MusicSystem (int choice);
static void M_CRL_SFXMode (int choice);
static void M_CRL_SFXChannels (int choice);
static void M_CRL_PitchShift (int choice);

static void M_ChooseCRL_Controls (int choice);
static void M_DrawCRL_Controls (void);
static void M_CRL_Controls_Sensitivity (int choice);
static void M_CRL_Controls_Acceleration (int choice);
static void M_CRL_Controls_Threshold (int choice);
static void M_CRL_Controls_NoVert (int choice);
static void M_CRL_Controls_DblClck (int choice);

static void M_DrawCRL_Keybinds_1 (void);
static void M_Bind_MoveForward (int choice);
static void M_Bind_MoveBackward (int choice);
static void M_Bind_TurnLeft (int choice);
static void M_Bind_TurnRight (int choice);
static void M_Bind_StrafeLeft (int choice);
static void M_Bind_StrafeRight (int choice);
static void M_Bind_SpeedOn (int choice);
static void M_Bind_StrafeOn (int choice);
static void M_Bind_180Turn (int choice);
static void M_Bind_FireAttack (int choice);
static void M_Bind_Use (int choice);

static void M_DrawCRL_Keybinds_2 (void);
static void M_Bind_CRLmenu (int choice);
static void M_Bind_RestartLevel (int choice);
static void M_Bind_NextLevel (int choice);
static void M_Bind_FastForward (int choice);
static void M_Bind_ExtendedHUD (int choice);
static void M_Bind_SpectatorMode (int choice);
static void M_Bind_CameraUp (int choice);
static void M_Bind_CameraDown (int choice);
static void M_Bind_CameraMoveTo (int choice);
static void M_Bind_FreezeMode (int choice);
static void M_Bind_BuddhaMode (int choice);
static void M_Bind_NotargetMode (int choice);
static void M_Bind_NomomentumMode (int choice);

static void M_DrawCRL_Keybinds_3 (void);
static void M_Bind_AlwaysRun (int choice);
static void M_Bind_NoVert (int choice);
static void M_Bind_VileBomb (int choice);
static void M_Bind_VileFly (int choice);
static void M_Bind_ClearMAX (int choice);
static void M_Bind_MoveToMAX (int choice);
static void M_Bind_IDDQD (int choice);
static void M_Bind_IDKFA (int choice);
static void M_Bind_IDFA (int choice);
static void M_Bind_IDCLIP (int choice);
static void M_Bind_IDDT (int choice);
static void M_Bind_MDK (int choice);

static void M_DrawCRL_Keybinds_4 (void);
static void M_Bind_Weapon1 (int choice);
static void M_Bind_Weapon2 (int choice);
static void M_Bind_Weapon3 (int choice);
static void M_Bind_Weapon4 (int choice);
static void M_Bind_Weapon5 (int choice);
static void M_Bind_Weapon6 (int choice);
static void M_Bind_Weapon7 (int choice);
static void M_Bind_Weapon8 (int choice);
static void M_Bind_PrevWeapon (int choice);
static void M_Bind_NextWeapon (int choice);

static void M_DrawCRL_Keybinds_5 (void);
static void M_Bind_ToggleMap (int choice);
static void M_Bind_ZoomIn (int choice);
static void M_Bind_ZoomOut (int choice);
static void M_Bind_MaxZoom (int choice);
static void M_Bind_FollowMode (int choice);
static void M_Bind_RotateMode (int choice);
static void M_Bind_OverlayMode (int choice);
static void M_Bind_SndPropMode (int choice);
static void M_Bind_ToggleGrid (int choice);
static void M_Bind_AddMark (int choice);
static void M_Bind_ClearMarks (int choice);

static void M_DrawCRL_Keybinds_6 (void);
static void M_Bind_HelpScreen (int choice);
static void M_Bind_SaveGame (int choice);
static void M_Bind_LoadGame (int choice);
static void M_Bind_SoundVolume (int choice);
static void M_Bind_ToggleDetail (int choice);
static void M_Bind_QuickSave (int choice);
static void M_Bind_EndGame (int choice);
static void M_Bind_ToggleMessages (int choice);
static void M_Bind_QuickLoad (int choice);
static void M_Bind_QuitGame (int choice);
static void M_Bind_ToggleGamma (int choice);
static void M_Bind_MultiplayerSpy (int choice);

static void M_DrawCRL_Keybinds_7 (void);
static void M_Bind_Pause (int choice);
static void M_Bind_SaveScreenshot (int choice);
static void M_Bind_LastMessage (int choice);
static void M_Bind_FinishDemo (int choice);
static void M_Bind_SendMessage (int choice);
static void M_Bind_ToPlayer1 (int choice);
static void M_Bind_ToPlayer2 (int choice);
static void M_Bind_ToPlayer3 (int choice);
static void M_Bind_ToPlayer4 (int choice);
static void M_Bind_Reset (int choice);

static void M_ChooseCRL_MouseBinds (int choice);
static void M_DrawCRL_MouseBinds (void);
static void M_Bind_M_FireAttack (int choice);
static void M_Bind_M_MoveForward (int choice);
static void M_Bind_M_SpeedOn (int choice);
static void M_Bind_M_StrafeOn (int choice);
static void M_Bind_M_MoveBackward (int choice);
static void M_Bind_M_Use (int choice);
static void M_Bind_M_StrafeLeft (int choice);
static void M_Bind_M_StrafeRight (int choice);
static void M_Bind_M_PrevWeapon (int choice);
static void M_Bind_M_NextWeapon (int choice);
static void M_Bind_M_Reset (int choice);

static void M_ChooseCRL_Widgets (int choice);
static void M_DrawCRL_Widgets (void);
static void M_CRL_Widget_Render (int choice);
static void M_CRL_Widget_MAX (int choice);
static void M_CRL_Widget_Playstate (int choice);
static void M_CRL_Widget_KIS (int choice);
static void M_CRL_Widget_KIS_Format (int choice);
static void M_CRL_Widget_KIS_Items (int choice);
static void M_CRL_Widget_Coords (int choice);
static void M_CRL_Widget_Speed (int choice);
static void M_CRL_Widget_Time (int choice);
static void M_CRL_Widget_Powerups (int choice);
static void M_CRL_Widget_Health (int choice);

static void M_ChooseCRL_Automap (int choice);
static void M_DrawCRL_Automap (void);
static void M_CRL_Automap_Rotate (int choice);
static void M_CRL_Automap_Overlay (int choice);
static void M_CRL_Automap_Shading (int choice);
static void M_CRL_Automap_Drawing (int choice);
static void M_CRL_Automap_Secrets (int choice);
static void M_CRL_Automap_SndProp (int choice);

static void M_ChooseCRL_Gameplay (int choice);
static void M_DrawCRL_Gameplay (void);
static void M_CRL_DefaulSkill (int choice);
static void M_CRL_PistolStart (int choice);
static void M_CRL_ColoredSTBar (int choice);
static void M_CRL_RevealedSecrets (int choice);
static void M_CRL_RestoreTargets (int choice);
static void M_CRL_OnDeathAction (int choice);
static void M_CRL_DemoTimer (int choice);
static void M_CRL_TimerDirection (int choice);
static void M_CRL_ProgressBar (int choice);
static void M_CRL_InternalDemos (int choice);

static void M_ChooseCRL_Misc (int choice);
static void M_DrawCRL_Misc (void);
static void M_CRL_Invul (int choice);
static void M_CRL_PalFlash (int choice);
static void M_CRL_MoveBob (int choice);
static void M_CRL_WeaponBob (int choice);
static void M_CRL_Colorblind (int choice);
static void M_CRL_AutoloadWAD (int choice);
static void M_CRL_AutoloadDEH (int choice);
static void M_CRL_Hightlight (int choice);
static void M_CRL_MenuEscKey (int choice);
static void M_CRL_ConfirmQuit (int choice);

static void M_ChooseCRL_Limits (int choice);
static void M_DrawCRL_Limits (void);
static void M_CRL_Limits (int choice);
static void M_CRL_SaveSizeWarning (int choice);

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
static void    M_DrawBindKey (int itemNum, int yPos, int key);
static void    M_DrawBindFooter (char *pagenum, boolean drawPages);

// Mouse binding prototypes
static boolean MouseIsBinding;
static int     btnToBind;

static char   *M_NameMouseBind (int itemSetOn, int btn);
static void    M_StartMouseBind (int btn);
static void    M_CheckMouseBind (int btn);
static void    M_DoMouseBind (int btnnum, int btn);
static void    M_ClearMouseBind (int itemOn);
static byte   *M_ColorizeMouseBind (int itemSetOn, int btn);
static void    M_DrawBindButton (int itemNum, int yPos, int btn);
static void    M_ResetMouseBinds (void);

// Forward declarations for scrolling and remembering last pages.
static menu_t CRLDef_Keybinds_1;
static menu_t CRLDef_Keybinds_2;
static menu_t CRLDef_Keybinds_3;
static menu_t CRLDef_Keybinds_4;
static menu_t CRLDef_Keybinds_5;
static menu_t CRLDef_Keybinds_6;
static menu_t CRLDef_Keybinds_7;

// Remember last keybindings page.
static int Keybinds_Cur;

static menu_t *KeybindsMenus[] =
{
    &CRLDef_Keybinds_1,
    &CRLDef_Keybinds_2,
    &CRLDef_Keybinds_3,
    &CRLDef_Keybinds_4,
    &CRLDef_Keybinds_5,
    &CRLDef_Keybinds_6,
    &CRLDef_Keybinds_7,
};

static void M_Choose_CRL_Keybinds (int choice)
{
    M_SetupNextMenu(KeybindsMenus[Keybinds_Cur]);
}

// [JN/PN] Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
    // "sfx_pstop" sound will be played only if menu will be changed.
    menu_t *nextMenu = NULL;

    // Remember cursor position.
    currentMenu->lastOn = itemOn;

    // Save/Load menu:
    if (currentMenu == &LoadDef || currentMenu == &SaveDef)
    {
        if (direction)
        {
            if (savepage < savepage_max)
            {
                savepage++;
                S_StartSound(NULL, sfx_pstop);
            }
        }
        else
        {
            if (savepage > 0)
            {
                savepage--;
                S_StartSound(NULL, sfx_pstop);
            }
        }
        quickSaveSlot = -1;
        M_ReadSaveStrings();
        return;
    }

    // Keyboard bindings:
    else if (currentMenu == &CRLDef_Keybinds_1) nextMenu = (direction ? &CRLDef_Keybinds_2 : &CRLDef_Keybinds_7);
    else if (currentMenu == &CRLDef_Keybinds_2) nextMenu = (direction ? &CRLDef_Keybinds_3 : &CRLDef_Keybinds_1);
    else if (currentMenu == &CRLDef_Keybinds_3) nextMenu = (direction ? &CRLDef_Keybinds_4 : &CRLDef_Keybinds_2);
    else if (currentMenu == &CRLDef_Keybinds_4) nextMenu = (direction ? &CRLDef_Keybinds_5 : &CRLDef_Keybinds_3);
    else if (currentMenu == &CRLDef_Keybinds_5) nextMenu = (direction ? &CRLDef_Keybinds_6 : &CRLDef_Keybinds_4);
    else if (currentMenu == &CRLDef_Keybinds_6) nextMenu = (direction ? &CRLDef_Keybinds_7 : &CRLDef_Keybinds_5);
    else if (currentMenu == &CRLDef_Keybinds_7) nextMenu = (direction ? &CRLDef_Keybinds_1 : &CRLDef_Keybinds_6);

    // If a new menu was set up, play the navigation sound.
    if (nextMenu)
    {
        M_SetupNextMenu(nextMenu);
        S_StartSound(NULL, sfx_pstop);
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
        for (int y = 0; y < SCREENAREA; y++)
        {
            I_VideoBuffer[y] = colormaps[crl_menu_shading * 256 + I_VideoBuffer[y]];
        }
    }
}

// [JN] Fill background with FLOOR4_8 flat.
static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("FLOOR4_8", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
}

static byte *M_Small_Line_Glow (const int tics)
{
    switch (crl_menu_highlight)
    {
        case 1:
            return
            tics == 5 ? cr[CR_MENU_BRIGHT5] : NULL;

        case 2:
            return
            tics == 5 ? cr[CR_MENU_BRIGHT5] :
            tics == 4 ? cr[CR_MENU_BRIGHT4] :
            tics == 3 ? cr[CR_MENU_BRIGHT3] :
            tics == 2 ? cr[CR_MENU_BRIGHT2] :
            tics == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
    }

    return NULL;
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
    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        currentMenu->menuitems[i].tics = 0;
    }

    // [JN] If menu is controlled by mouse, reset "last on" position
    // so this item won't blink upon reentering to the current menu.
    if (menu_mouse_allow)
    {
        currentMenu->lastOn = -1;
    }
}

#define GLOW_UNCOLORED  0
#define GLOW_RED        1
#define GLOW_DARKRED    2
#define GLOW_GREEN      3
#define GLOW_YELLOW     4
#define GLOW_ORANGE     5
#define GLOW_LIGHTGRAY  6
#define GLOW_BLUE       7
#define GLOW_OLIVE      8
#define GLOW_DARKGREEN  9

#define ITEMONTICS      currentMenu->menuitems[itemOn].tics
#define ITEMSETONTICS   currentMenu->menuitems[itemSetOn].tics

static byte *M_Item_Glow (const int itemSetOn, const int color)
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

    if (itemOn == itemSetOn)
    {
        return
            color == GLOW_RED ||
            color == GLOW_DARKRED   ? cr[CR_RED_BRIGHT5]       :
            color == GLOW_GREEN     ? cr[CR_GREEN_BRIGHT5]     :
            color == GLOW_YELLOW    ? cr[CR_YELLOW_BRIGHT5]    :
            color == GLOW_ORANGE    ? cr[CR_ORANGE_BRIGHT5]    :
            color == GLOW_LIGHTGRAY ? cr[CR_LIGHTGRAY_BRIGHT5] :
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
                ITEMSETONTICS == 5 ? cr[CR_ORANGE_BRIGHT5] :
                ITEMSETONTICS == 4 ? cr[CR_ORANGE_BRIGHT4] :
                ITEMSETONTICS == 3 ? cr[CR_ORANGE_BRIGHT3] :
                ITEMSETONTICS == 2 ? cr[CR_ORANGE_BRIGHT2] :
                ITEMSETONTICS == 1 ? cr[CR_ORANGE_BRIGHT1] : cr[CR_ORANGE];
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
    return whichSkull ? NULL : cr[CR_MENU_DARK4];

    return
        tics ==  8 || tics ==   7 ? cr[CR_MENU_BRIGHT4] :
        tics ==  6 || tics ==   5 ? cr[CR_MENU_BRIGHT3] :
        tics ==  4 || tics ==   3 ? cr[CR_MENU_BRIGHT2] :
        tics ==  2 || tics ==   1 ? cr[CR_MENU_BRIGHT1] :
        tics == -1 || tics ==  -2 ? cr[CR_MENU_DARK1]   :
        tics == -3 || tics ==  -4 ? cr[CR_MENU_DARK2]   :
        tics == -5 || tics ==  -6 ? cr[CR_MENU_DARK3]   :
        tics == -7 || tics ==  -8 ? cr[CR_MENU_DARK4]   : NULL;
}

enum
{
    saveload_border,
    saveload_slider,
    saveload_text,
    saveload_cursor,
};

static byte *M_SaveLoad_Glow (int itemSetOn, int tics, int type)
{
    if (!crl_menu_highlight)
    return NULL;

    switch (type)
    {
        case saveload_border:
        case saveload_slider:
            return itemSetOn ? cr[CR_MENU_BRIGHT2] : NULL;

        case saveload_text:
            return itemSetOn ? cr[CR_MENU_BRIGHT5] : M_Small_Line_Glow(tics);

        case saveload_cursor:
            return cr[CR_MENU_BRIGHT5];
    }

    return NULL;
}

static int M_INT_Slider (int val, int min, int max, int direction, boolean capped)
{
    // [PN] Adjust the slider value based on direction and handle min/max limits
    val += (direction == -1) ?  0 :     // [JN] Routine "-1" just reintializes value.
           (direction ==  0) ? -1 : 1;  // Otherwise, move either left "0" or right "1".

    if (val < min)
        val = capped ? min : max;
    else
    if (val > max)
        val = capped ? max : min;

    return val;
}

static float M_FLOAT_Slider (float val, float min, float max, float step,
                             int direction, boolean capped)
{
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
                     GLOW_RED;
}

static char *const DefSkillName[5] = 
{
    "IMTYTD" ,
    "HNTR"   ,
    "HMP"    ,
    "UV"     ,
    "NM"     
};


// -----------------------------------------------------------------------------
// Main CRL Menu
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Main[]=
{
    { M_MUL1, "SPECTATOR MODE",       M_CRL_Spectating,     's'},
    { M_MUL1, "FREEZE MODE",          M_CRL_Freeze,         'f'},
    { M_MUL1, "BUDDHA MODE",          M_CRL_Buddha,         'f'},
    { M_MUL1, "NO TARGET MODE",       M_CRL_NoTarget,       'n'},
    { M_MUL1, "NO MOMENTUM MODE",     M_CRL_NoMomentum,     'n'},
    { M_SKIP, "", 0, '\0'},
    { M_SWTC, "VIDEO OPTIONS",        M_ChooseCRL_Video,    'v'},
    { M_SWTC, "DISPLAY OPTIONS",      M_ChooseCRL_Display,  'd'},
    { M_SWTC, "SOUND OPTIONS",        M_ChooseCRL_Sound,    's'},
    { M_SWTC, "CONTROL SETTINGS",     M_ChooseCRL_Controls, 'c'},
    { M_SWTC, "WIDGETS SETTINGS",     M_ChooseCRL_Widgets,  'w'},
    { M_SWTC, "AUTOMAP SETTINGS",     M_ChooseCRL_Automap,  'a'},
    { M_SWTC, "GAMEPLAY FEATURES",    M_ChooseCRL_Gameplay, 'g'},
    { M_SWTC, "MISC FEATURES",        M_ChooseCRL_Misc,     'm'},
    { M_SWTC, "STATIC ENGINE LIMITS", M_ChooseCRL_Limits,   's'},
    { M_SWTC, "VANILLA OPTIONS MENU", M_Options,            'v'},
};

static menu_t CRLDef_Main =
{
    16,
    &MainDef,
    CRLMenu_Main,
    M_DrawCRL_Main,
    CRL_MENU_LEFTOFFSET_SML, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Main (int choice)
{
    M_SetupNextMenu (&CRLDef_Main);
}

static void M_DrawCRL_Main (void)
{
    char str[32];

    M_WriteTextCentered(7, "MAIN CRL MENU", cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 16, str,
                 M_Item_Glow(0, crl_spectating ? GLOW_GREEN : GLOW_DARKRED));

    // Freeze
    sprintf(str, !singleplayer ? "N/A" :
            crl_freeze ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, !singleplayer ? GLOW_DARKRED :
                             crl_freeze ? GLOW_GREEN : GLOW_DARKRED));

    // Buddha
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_BUDDHA ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(2, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_BUDDHA ? GLOW_GREEN : GLOW_DARKRED));

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(3, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOTARGET ? GLOW_GREEN : GLOW_DARKRED));

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(4, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOMOMENTUM ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(61, "SETTINGS", cr[CR_YELLOW]);
}

static void M_CRL_Spectating (int choice)
{
    crl_spectating ^= 1;
}

static void M_CRL_Freeze (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    crl_freeze ^= 1;
}

static void M_CRL_Buddha (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_BUDDHA;
}

static void M_CRL_NoTarget (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_NOTARGET;
    P_ForgetPlayer(player);
}

static void M_CRL_NoMomentum (int choice)
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

static menuitem_t CRLMenu_Video[]=
{
    { M_MUL1, "UNCAPPED FRAMERATE", M_CRL_UncappedFPS,   'u' },
    { M_MUL1, "FRAMERATE LIMIT",    M_CRL_LimitFPS,      'f' },
    { M_MUL1, "ENABLE VSYNC",       M_CRL_VSync,         'e' },
    { M_MUL1, "SHOW FPS COUNTER",   M_CRL_ShowFPS,       's' },
    { M_MUL1, "PIXEL SCALING",      M_CRL_PixelScaling,  'p' },
    { M_MUL2, "VISPLANES DRAWING",  M_CRL_VisplanesDraw, 'v' },
    { M_MUL1, "HOM EFFECT",         M_CRL_HOMDraw,       'h' },
    { M_SKIP, "", 0, '\0'},                              
    { M_MUL2, "SCREEN WIPE EFFECT", M_CRL_ScreenWipe,    's' },
    { M_MUL2, "SHOW ENDOOM SCREEN", M_CRL_ShowENDOOM,    's' },
};

static menu_t CRLDef_Video =
{
    10,
    &CRLDef_Main,
    CRLMenu_Video,
    M_DrawCRL_Video,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Video (int choice)
{
    M_SetupNextMenu (&CRLDef_Video);
}

static void M_DrawCRL_Video (void)
{
    char str[32];

    M_WriteTextCentered(7, "VIDEO OPTIONS", cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 16, str, 
                 M_Item_Glow(0, crl_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !crl_uncapped_fps ? "35" :
                 crl_fpslimit ? "%d" : "NONE", crl_fpslimit);
    M_WriteText (M_ItemRightAlign(str), 25, str, 
                 !crl_uncapped_fps ? cr[CR_DARKRED] :
                 M_Item_Glow(1, crl_fpslimit ? GLOW_GREEN : GLOW_DARKRED));

    // Enable vsync
    sprintf(str, crl_vsync ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str, 
                 M_Item_Glow(2, crl_vsync ? GLOW_GREEN : GLOW_DARKRED));

    // Show FPS counter
    sprintf(str, crl_showfps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str, 
                 M_Item_Glow(3, crl_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (itemOn == 3 && !crl_extended_hud)
    {
        M_WriteTextCentered(160, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }

    // Pixel scaling
    sprintf(str, smooth_scaling ? "SMOOTH" : "SHARP");
    M_WriteText (M_ItemRightAlign(str), 52, str, 
                 M_Item_Glow(4, smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    // Visplanes drawing mode
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(5, crl_visplanes_drawing ? GLOW_GREEN : GLOW_DARKRED));

    // HOM effect
    sprintf(str, crl_hom_effect == 1 ? "MULTICOLOR" :
                 crl_hom_effect == 2 ? "BLACK" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(6, crl_hom_effect ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(79, "MISCELLANEOUS", cr[CR_YELLOW]);

    // Screen wipe effect
    sprintf(str, crl_screenwipe == 1 ? "ON" :
                 crl_screenwipe == 2 ? "FAST" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(8, crl_screenwipe == 1 ? GLOW_DARKRED : GLOW_GREEN));

    // Screen ENDOOM screen
    sprintf(str, show_endoom == 1 ? "ALWAYS" :
                 show_endoom == 2 ? "PWAD ONLY" : "NEVER");
    M_WriteText (M_ItemRightAlign(str), 97, str, 
                 M_Item_Glow(9, show_endoom == 1 ? GLOW_DARKRED : GLOW_GREEN));
}

static void M_CRL_UncappedFPS (int choice)
{
    crl_uncapped_fps ^= 1;
}

static void M_CRL_LimitFPS (int choice)
{
    if (!crl_uncapped_fps)
    {
        return;  // Do not allow change value in capped framerate.
    }
    
    switch (choice)
    {
        case 0:
            if (crl_fpslimit)
                crl_fpslimit--;

            if (crl_fpslimit < TICRATE)
                crl_fpslimit = 0;

            break;
        case 1:
            if (crl_fpslimit < 500)
                crl_fpslimit++;

            if (crl_fpslimit < TICRATE)
                crl_fpslimit = TICRATE;

        default:
            break;
    }
}

static void M_CRL_VSync (int choice)
{
    crl_vsync ^= 1;

    I_ToggleVsync();
}

static void M_CRL_ShowFPS (int choice)
{
    crl_showfps ^= 1;
}

static void M_CRL_PixelScaling (int choice)
{
    smooth_scaling ^= 1;
    I_TogglePixelScaling();
}

static void M_CRL_VisplanesDraw (int choice)
{
    crl_visplanes_drawing = M_INT_Slider(crl_visplanes_drawing, 0, 4, choice, false);
}

static void M_CRL_HOMDraw (int choice)
{
    crl_hom_effect = M_INT_Slider(crl_hom_effect, 0, 2, choice, false);
}

static void M_CRL_ScreenWipe (int choice)
{
    crl_screenwipe = M_INT_Slider(crl_screenwipe, 0, 2, choice, false);
}

static void M_CRL_ShowENDOOM (int choice)
{
    show_endoom = M_INT_Slider(show_endoom, 0, 2, choice, false);
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Display[]=
{
    { M_SLDR, "GAMMA-CORRECTION",        M_CRL_Gamma,           'g'},
    { M_SKIP, "", 0, '\0'},                                     
    { M_SKIP, "", 0, '\0'},                                     
    { M_MUL1, "MENU BACKGROUND SHADING", M_CRL_MenuBgShading,   'm'},
    { M_MUL1, "EXTRA LEVEL BRIGHTNESS",  M_CRL_LevelBrightness, 'e'},
    { M_SKIP, "", 0, '\0'},                                     
    { M_MUL1, "MESSAGES ENABLED",        M_ChangeMessages,      'm'},
    { M_MUL2, "CRITICAL MESSAGE",        M_CRL_MsgCritical,     'c'},
    { M_MUL1, "TEXT CAST SHADOWS",       M_CRL_TextShadows,     't'},
};

static menu_t CRLDef_Display =
{
    9,
    &CRLDef_Main,
    CRLMenu_Display,
    M_DrawCRL_Display,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET + 9,  // [JN] This menu is one line down.
    0,
    true, false, false,
};

static void M_ChooseCRL_Display (int choice)
{
    M_SetupNextMenu (&CRLDef_Display);
}

static void M_DrawCRL_Display (void)
{
    char str[32];

    if (itemOn != 7) // Do not show title while preview-reminder is active.
    M_WriteTextCentered(16, "DISPLAY OPTIONS", cr[CR_YELLOW]);

    // Gamma-correction slider and num
    M_DrawThermo(46, 35, 15, crl_gamma, 0);
    M_ID_HandleSliderMouseControl(52, 36, 124, &crl_gamma, false, 0, 15);
    M_WriteText (184, 38, gammalvls[crl_gamma][1],
                           M_Item_Glow(0, GLOW_UNCOLORED));

    // Menu background shading
    sprintf(str, crl_menu_shading ? "%d" : "OFF", crl_menu_shading);
    M_WriteText (M_ItemRightAlign(str), 52, str, 
                 M_Item_Glow(3, crl_menu_shading ? GLOW_GREEN : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, crl_level_brightness ? "%d" : "OFF", crl_level_brightness);
    M_WriteText (M_ItemRightAlign(str), 61, str, 
                 M_Item_Glow(4, crl_level_brightness ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(70, "MESSAGES SETTINGS", cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, showMessages ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 79, str, 
                 M_Item_Glow(6, showMessages ? GLOW_GREEN : GLOW_DARKRED));

    // Critical message style
    sprintf(str, crl_msg_critical ? "BLINKING" : "STATIC");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(7, crl_msg_critical ? GLOW_GREEN : GLOW_DARKRED));
    // Show nice preview-reminder :)
    if (itemOn == 7)
    {
        CRL_SetMessageCritical("CRL_REMINDER:", "CRITICAL MESSAGES ARE ALWAYS ENABLED!", 2);
    }

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 97, str, 
                 M_Item_Glow(8, crl_text_shadows ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_Gamma (int choice)
{
    shade_wait = I_GetTime() + TICRATE;
    crl_gamma = M_INT_Slider(crl_gamma, 0, 14, choice, true);

    CRL_ReloadPalette();
}

static void M_CRL_MenuBgShading (int choice)
{
    crl_menu_shading = M_INT_Slider(crl_menu_shading, 0, 24, choice, true);
}

static void M_CRL_LevelBrightness (int choice)
{
    crl_level_brightness = M_INT_Slider(crl_level_brightness, 0, 8, choice, true);
}

static void M_CRL_MsgCritical (int choice)
{
    crl_msg_critical ^= 1;
}

static void M_CRL_TextShadows (int choice)
{
    crl_text_shadows ^= 1;
}

// -----------------------------------------------------------------------------
// Sound options
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Sound[]=
{
    { M_SLDR, "SFX VOLUME",            M_SfxVol,           's'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_SLDR, "MUSIC VOLUME",          M_MusicVol,         'm'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_MUL2, "SFX PLAYBACK",          M_CRL_SFXSystem,    's'},
    { M_MUL2, "MUSIC PLAYBACK",        M_CRL_MusicSystem,  'm'},
    { M_MUL1, "SOUND EFFECTS MODE",    M_CRL_SFXMode,      's'},
    { M_MUL2, "PITCH-SHIFTED SOUNDS",  M_CRL_PitchShift,   'p'},
    { M_MUL1, "NUMBER OF SFX TO MIX",  M_CRL_SFXChannels,  'n'},
};

static menu_t CRLDef_Sound =
{
    12,
    &CRLDef_Main,
    CRLMenu_Sound,
    M_DrawCRL_Sound,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Sound (int choice)
{
    M_SetupNextMenu (&CRLDef_Sound);
}

static void M_DrawCRL_Sound (void)
{
    char str[16];

    M_WriteTextCentered(7, "VOLUME", cr[CR_YELLOW]);

    M_DrawThermo(46, 26, 16, sfxVolume, 0);
    M_ID_HandleSliderMouseControl(52, 26, 132, &sfxVolume, false, 0, 15);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (192, 29, str, M_Item_Glow(0, GLOW_UNCOLORED));

    M_DrawThermo(46, 53, 16, musicVolume, 3);
    M_ID_HandleSliderMouseControl(52, 53, 132, &musicVolume, false, 0, 15);
    sprintf(str,"%d", musicVolume);
    M_WriteText (192, 56, str, M_Item_Glow(3, GLOW_UNCOLORED));

    M_WriteTextCentered(70, "SOUND SYSTEM", cr[CR_YELLOW]);

    // SFX playback
    sprintf(str, snd_sfxdevice == 0 ? "DISABLED"    :
                 snd_sfxdevice == 1 ? "PC SPEAKER"  :
                 snd_sfxdevice == 3 ? "DIGITAL SFX" :
                                      "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(7, snd_sfxdevice ? GLOW_GREEN : GLOW_DARKRED));

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ? "GUS (EMULATED)" :
                 snd_musicdevice == 8 ? "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                        "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(8, snd_musicdevice ? GLOW_GREEN : GLOW_DARKRED));

    // Sound effects mode
    sprintf(str, crl_monosfx ? "MONO" : "STEREO");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(9, crl_monosfx ? GLOW_DARKRED : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(10, snd_pitchshift ? GLOW_GREEN : GLOW_DARKRED));

    // Number of SFX to mix
    sprintf(str, "%i", snd_channels);
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(11, snd_channels == 8 ? GLOW_GREEN :
                                 snd_channels == 1 ? GLOW_DARKRED : GLOW_DARKGREEN));

    // Inform if FSYNTH/GUS paths anen't set.
    if (itemOn == 8)
    {
        if (snd_musicdevice == 5 && strcmp(gus_patch_path, "") == 0)
        {
            M_WriteTextCentered(151, "\"GUS_PATCH_PATH\" VARIABLE IS NOT SET", cr[CR_GRAY]);
        }
#ifdef HAVE_FLUIDSYNTH
        if (snd_musicdevice == 11 && strcmp(fsynth_sf_path, "") == 0)
        {
            M_WriteTextCentered(151, "\"FSYNTH_SF_PATH\" VARIABLE IS NOT SET", cr[CR_GRAY]);
        }
#endif // HAVE_FLUIDSYNTH
    }
}

static void M_CRL_SFXSystem (int choice)
{
    switch (choice)
    {
        case 0:
            snd_sfxdevice =
                snd_sfxdevice == 0 ? 1 :
                snd_sfxdevice == 1 ? 3 :
                                     0 ;
            break;
        case 1:
            snd_sfxdevice =
                snd_sfxdevice == 0 ? 3 :
                snd_sfxdevice == 3 ? 1 :
                                     0 ;
        default:
            break;
    }

    // Shut down current music
    S_StopMusic();

    // Free all sound channels/usefulness
    S_ChangeSFXSystem();

    // Shut down sound/music system
    I_ShutdownSound();

    // Start sound/music system
    I_InitSound(doom);

    // Re-generate SFX cache
    I_PrecacheSounds(S_sfx, NUMSFX);

    // Reinitialize sound volume
    S_SetSfxVolume(sfxVolume * 8);

    // Reinitialize music volume
    S_SetMusicVolume(musicVolume * 8);

    // Restart current music
    S_ChangeMusic(crl_musicnum, usergame);
}

static void M_CRL_MusicSystem (int choice)
{
    switch (choice)
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

    // Shut down current music
    S_StopMusic();

    // Shut down music system
    S_Shutdown();
    
    // Start music system
    I_InitSound(doom);

    // Reinitialize music volume
    S_SetMusicVolume(musicVolume * 8);

    // Restart current music
    S_ChangeMusic(crl_musicnum, usergame);
}

static void M_CRL_SFXMode (int choice)
{
    crl_monosfx ^= 1;

    // Update stereo separation
    S_UpdateStereoSeparation();
}

static void M_CRL_PitchShift (int choice)
{
    snd_pitchshift ^= 1;
}

static void M_CRL_SFXChannels (int choice)
{
    snd_channels = M_INT_Slider(snd_channels, 1, 8, choice, true);
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Controls[]=
{
    { M_SWTC, "KEYBOARD BINDINGS",            M_Choose_CRL_Keybinds,       'k'},
    { M_SWTC, "MOUSE BINDINGS",               M_ChooseCRL_MouseBinds,      'm'},
    { M_SKIP, "", 0, '\0'},
    { M_SLDR, "SENSITIVITY",                  M_CRL_Controls_Sensitivity,  's'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_SLDR, "ACCELERATION",                 M_CRL_Controls_Acceleration, 'a'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_SLDR, "ACCELERATION THRESHOLD",       M_CRL_Controls_Threshold,    'a'},
    { M_SKIP, "", 0, '\0'},
    { M_SKIP, "", 0, '\0'},
    { M_MUL1, "VERTICAL MOUSE MOVEMENT",      M_CRL_Controls_NoVert,       'v'},
    { M_MUL1, "DOUBLE CLICK ACTS AS \"USE\"", M_CRL_Controls_DblClck,      'd'},
};

static menu_t CRLDef_Controls =
{
    14,
    &CRLDef_Main,
    CRLMenu_Controls,
    M_DrawCRL_Controls,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Controls (int choice)
{
    M_SetupNextMenu (&CRLDef_Controls);
}

static void M_DrawCRL_Controls (void)
{
    char str[32];

    M_WriteTextCentered(7, "BINDINGS", cr[CR_YELLOW]);
    
    M_WriteTextCentered(34, "MOUSE CONFIGURATION", cr[CR_YELLOW]);

    M_DrawThermo(46, 54, 15, mouseSensitivity, 3);
    M_ID_HandleSliderMouseControl(52, 54, 124, &mouseSensitivity, false, 0, 14);
    sprintf(str,"%d", mouseSensitivity);
    M_WriteText (184, 57, str, M_Item_Glow(3, mouseSensitivity == 255 ? GLOW_YELLOW :
                                              mouseSensitivity > 14 ? GLOW_GREEN : GLOW_UNCOLORED));

    M_DrawThermo(46, 81, 8, (mouse_acceleration * 1.8f) - 2, 6);
    M_ID_HandleSliderMouseControl(52, 81, 100, &mouse_acceleration, true, 0, 9);
    sprintf(str,"%.1f", mouse_acceleration);
    M_WriteText (128, 83, str, M_Item_Glow(6, GLOW_UNCOLORED));

    M_DrawThermo(46, 108, 15, mouse_threshold / 2.2f, 9);
    M_ID_HandleSliderMouseControl(52, 108, 124, &mouse_threshold, false, 0, 32);
    sprintf(str,"%d", mouse_threshold);
    M_WriteText (184, 110, str, M_Item_Glow(9, mouse_threshold == 0 ? GLOW_DARKRED : GLOW_UNCOLORED));

    // Vertical mouse movement
    sprintf(str, novert ? "OFF" : "ON");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(12, novert ? GLOW_DARKRED : GLOW_GREEN));

    // Double click acts as "use"
    sprintf(str, dclick_use ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 133, str,
                 M_Item_Glow(13, dclick_use ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_Controls_Sensitivity (int choice)
{
    // [crispy] extended range
    mouseSensitivity = M_INT_Slider(mouseSensitivity, 0, 255, choice, true);
}

static void M_CRL_Controls_Acceleration (int choice)
{
    mouse_acceleration = M_FLOAT_Slider(mouse_acceleration, 1.000000f, 5.000000f, 0.100000f, choice, true);
}

static void M_CRL_Controls_Threshold (int choice)
{
    mouse_threshold = M_INT_Slider(mouse_threshold, 0, 32, choice, true);
}

static void M_CRL_Controls_NoVert (int choice)
{
    novert ^= 1;
}

static void M_CRL_Controls_DblClck (int choice)
{
    dclick_use ^= 1;
}

// -----------------------------------------------------------------------------
// Keybinds 1
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_1[]=
{
    { M_SWTC, "MOVE FORWARD",   M_Bind_MoveForward,   'm' },
    { M_SWTC, "MOVE BACKWARD",  M_Bind_MoveBackward,  'm' },
    { M_SWTC, "TURN LEFT",      M_Bind_TurnLeft,      't' },
    { M_SWTC, "TURN RIGHT",     M_Bind_TurnRight,     't' },
    { M_SWTC, "STRAFE LEFT",    M_Bind_StrafeLeft,    's' },
    { M_SWTC, "STRAFE RIGHT",   M_Bind_StrafeRight,   's' },
    { M_SWTC, "SPEED ON",       M_Bind_SpeedOn,       's' },
    { M_SWTC, "STRAFE ON",      M_Bind_StrafeOn,      's' },
    { M_SWTC, "180 DEGREE TURN",M_Bind_180Turn,       '1' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "FIRE/ATTACK",    M_Bind_FireAttack,    'f' },
    { M_SWTC, "USE",            M_Bind_Use,           'u' },
};

static menu_t CRLDef_Keybinds_1 =
{
    12,
    &CRLDef_Controls,
    CRLMenu_Keybinds_1,
    M_DrawCRL_Keybinds_1,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_1 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 0;

    M_FillBackground();

    M_WriteTextCentered(7, "MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_up);
    M_DrawBindKey(1, 25, key_down);
    M_DrawBindKey(2, 34, key_left);
    M_DrawBindKey(3, 43, key_right);
    M_DrawBindKey(4, 52, key_strafeleft);
    M_DrawBindKey(5, 61, key_straferight);
    M_DrawBindKey(6, 70, key_speed);
    M_DrawBindKey(7, 79, key_strafe);
    M_DrawBindKey(8, 88, key_180turn);

    M_WriteTextCentered(97, "ACTION", cr[CR_YELLOW]);

    M_DrawBindKey(10, 106, key_fire);
    M_DrawBindKey(11, 115, key_use);

    M_DrawBindFooter("1", true);
}

static void M_Bind_MoveForward (int choice)
{
    M_StartBind(100);  // key_up
}

static void M_Bind_MoveBackward (int choice)
{
    M_StartBind(101);  // key_down
}

static void M_Bind_TurnLeft (int choice)
{
    M_StartBind(102);  // key_left
}

static void M_Bind_TurnRight (int choice)
{
    M_StartBind(103);  // key_right
}

static void M_Bind_StrafeLeft (int choice)
{
    M_StartBind(104);  // key_strafeleft
}

static void M_Bind_StrafeRight (int choice)
{
    M_StartBind(105);  // key_straferight
}

static void M_Bind_SpeedOn (int choice)
{
    M_StartBind(106);  // key_speed
}

static void M_Bind_StrafeOn (int choice)
{
    M_StartBind(107);  // key_strafe
}

static void M_Bind_180Turn (int choice)
{
    M_StartBind(108);  // key_180turn
}

static void M_Bind_FireAttack (int choice)
{
    M_StartBind(109);  // key_fire
}

static void M_Bind_Use (int choice)
{
    M_StartBind(110);  // key_use
}

// -----------------------------------------------------------------------------
// Keybinds 2
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_2[]=
{
    { M_SWTC, "MAIN CRL MENU",             M_Bind_CRLmenu,        'm' },
    { M_SWTC, "RESTART LEVEL/DEMO",        M_Bind_RestartLevel,   'r' },
    { M_SWTC, "GO TO NEXT LEVEL",          M_Bind_NextLevel,      'g' },
    { M_SWTC, "DEMO FAST-FORWARD",         M_Bind_FastForward,    'd' },
    { M_SWTC, "TOGGLE EXTENDED HUD",       M_Bind_ExtendedHUD,    't' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "SPECTATOR MODE",            M_Bind_SpectatorMode,  's' },
    { M_SWTC, "- MOVE CAMERA UP",          M_Bind_CameraUp,       'm' },
    { M_SWTC, "- MOVE CAMERA DOWN",        M_Bind_CameraDown,     'm' },
    { M_SWTC, "- MOVE TO CAMERA POSITION", M_Bind_CameraMoveTo,   'm' },
    { M_SWTC, "FREEZE MODE",               M_Bind_FreezeMode,     'f' },
    { M_SWTC, "BUDDHA MODE",               M_Bind_BuddhaMode,     'b' },
    { M_SWTC, "NO TARGET MODE",            M_Bind_NotargetMode,   'n' },
    { M_SWTC, "NO MOMENTUM MODE",          M_Bind_NomomentumMode, 'n' },
};

static menu_t CRLDef_Keybinds_2 =
{
    14,
    &CRLDef_Controls,
    CRLMenu_Keybinds_2,
    M_DrawCRL_Keybinds_2,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_2 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 1;

    M_FillBackground();

    M_WriteTextCentered(7, "CRL CONTROLS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_crl_menu);
    M_DrawBindKey(1, 25, key_crl_reloadlevel);
    M_DrawBindKey(2, 34, key_crl_nextlevel);
    M_DrawBindKey(3, 43, key_crl_demospeed);
    M_DrawBindKey(4, 52, key_crl_extendedhud);

    M_WriteTextCentered(61, "GAME MODES", cr[CR_YELLOW]);

    M_DrawBindKey(6, 70, key_crl_spectator);
    M_DrawBindKey(7, 79, key_crl_cameraup);
    M_DrawBindKey(8, 88, key_crl_cameradown);
    M_DrawBindKey(9, 97, key_crl_cameramoveto);
    M_DrawBindKey(10, 106, key_crl_freeze);
    M_DrawBindKey(11, 115, key_crl_buddha);
    M_DrawBindKey(12, 124, key_crl_notarget);
    M_DrawBindKey(13, 133, key_crl_nomomentum);

    M_DrawBindFooter("2", true);
}

static void M_Bind_CRLmenu (int choice)
{
    M_StartBind(200);  // key_crl_menu
}

static void M_Bind_RestartLevel (int choice)
{
    M_StartBind(201);  // key_crl_reloadlevel
}

static void M_Bind_NextLevel (int choice)
{
    M_StartBind(202);  // key_crl_nextlevel
}

static void M_Bind_FastForward (int choice)
{
    M_StartBind(203);  // key_crl_demospeed
}

static void M_Bind_ExtendedHUD (int choice)
{
    M_StartBind(204);  // key_crl_extendedhud
}

static void M_Bind_SpectatorMode (int choice)
{
    M_StartBind(205);  // key_crl_spectator
}

static void M_Bind_CameraUp (int choice)
{
    M_StartBind(206);  // key_crl_cameraup
}

static void M_Bind_CameraDown (int choice)
{
    M_StartBind(207);  // key_crl_cameradown
}

static void M_Bind_CameraMoveTo (int choice)
{
    M_StartBind(208);  // key_crl_cameramoveto
}

static void M_Bind_FreezeMode (int choice)
{
    M_StartBind(209);  // key_crl_freeze
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(210);  // key_crl_buddha
}

static void M_Bind_NotargetMode (int choice)
{
    M_StartBind(211);  // key_crl_notarget
}

static void M_Bind_NomomentumMode (int choice)
{
    M_StartBind(212);  // key_crl_nomomentum
}

// -----------------------------------------------------------------------------
// Keybinds 3
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_3[]=
{
    { M_SWTC, "ALWAYS RUN",              M_Bind_AlwaysRun,  'a'  },
    { M_SWTC, "VERTICAL MOUSE MOVEMENT", M_Bind_NoVert,     'v'  },
    { M_SWTC, "ARCH-VILE JUMP (PRESS)",  M_Bind_VileBomb,   'a'  },
    { M_SWTC, "ARCH-VILE FLY (HOLD)",    M_Bind_VileFly,    'a'  },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "CLEAR MAX",               M_Bind_ClearMAX,   'c'  },
    { M_SWTC, "MOVE TO MAX ",            M_Bind_MoveToMAX,  'm'  },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "IDDQD",                   M_Bind_IDDQD,      'i'  },
    { M_SWTC, "IDKFA",                   M_Bind_IDKFA,      'i'  },
    { M_SWTC, "IDFA",                    M_Bind_IDFA,       'i'  },
    { M_SWTC, "IDCLIP",                  M_Bind_IDCLIP,     'i'  },
    { M_SWTC, "IDDT",                    M_Bind_IDDT,       'i'  },
    { M_SWTC, "MDK",                     M_Bind_MDK,        'm'  },
};

static menu_t CRLDef_Keybinds_3 =
{
    14,
    &CRLDef_Controls,
    CRLMenu_Keybinds_3,
    M_DrawCRL_Keybinds_3,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_3 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 2;

    M_FillBackground();

    M_WriteTextCentered(7, "ADVANCED MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_crl_autorun);
    M_DrawBindKey(1, 25, key_crl_novert);
    M_DrawBindKey(2, 34, key_crl_vilebomb);
    M_DrawBindKey(3, 43, key_crl_vilefly);

    M_WriteTextCentered(52, "VISPLANES MAX VALUE", cr[CR_YELLOW]);

    M_DrawBindKey(5, 61, key_crl_clearmax);
    M_DrawBindKey(6, 70, key_crl_movetomax);

    M_WriteTextCentered(79, "CHEAT SHORTCUTS", cr[CR_YELLOW]);

    M_DrawBindKey(8, 88, key_crl_iddqd);
    M_DrawBindKey(9, 97, key_crl_idkfa);
    M_DrawBindKey(10, 106, key_crl_idfa);
    M_DrawBindKey(11, 115, key_crl_idclip);
    M_DrawBindKey(12, 124, key_crl_iddt);
    M_DrawBindKey(13, 133, key_crl_mdk);

    M_DrawBindFooter("3", true);
}

static void M_Bind_AlwaysRun (int choice)
{
    M_StartBind(300);  // key_crl_autorun
}

static void M_Bind_NoVert (int choice)
{
    M_StartBind(301);  // key_crl_novert
}

static void M_Bind_VileBomb (int choice)
{
    M_StartBind(302);  // key_crl_vilebomb
}

static void M_Bind_VileFly (int choice)
{
    M_StartBind(303);  // key_crl_vilefly
}

static void M_Bind_ClearMAX (int choice)
{
    M_StartBind(304);  // key_crl_clearmax
}

static void M_Bind_MoveToMAX (int choice)
{
    M_StartBind(305);  // key_crl_movetomax
}

static void M_Bind_IDDQD (int choice)
{
    M_StartBind(306);  // key_crl_iddqd
}

static void M_Bind_IDKFA (int choice)
{
    M_StartBind(307);  // key_crl_idkfa
}

static void M_Bind_IDFA (int choice)
{
    M_StartBind(308);  // key_crl_idfa
}

static void M_Bind_IDCLIP (int choice)
{
    M_StartBind(309);  // key_crl_idclip
}

static void M_Bind_IDDT (int choice)
{
    M_StartBind(310);  // key_crl_iddt
}

static void M_Bind_MDK (int choice)
{
    M_StartBind(311);  // key_crl_mdk
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_4[]=
{
    { M_SWTC, "WEAPON 1",        M_Bind_Weapon1,    'w' },
    { M_SWTC, "WEAPON 2",        M_Bind_Weapon2,    'w' },
    { M_SWTC, "WEAPON 3",        M_Bind_Weapon3,    'w' },
    { M_SWTC, "WEAPON 4",        M_Bind_Weapon4,    'w' },
    { M_SWTC, "WEAPON 5",        M_Bind_Weapon5,    'w' },
    { M_SWTC, "WEAPON 6",        M_Bind_Weapon6,    'w' },
    { M_SWTC, "WEAPON 7",        M_Bind_Weapon7,    'w' },
    { M_SWTC, "WEAPON 8",        M_Bind_Weapon8,    'w' },
    { M_SWTC, "PREVIOUS WEAPON", M_Bind_PrevWeapon, 'p' },
    { M_SWTC, "NEXT WEAPON",     M_Bind_NextWeapon, 'n' },
};

static menu_t CRLDef_Keybinds_4 =
{
    10,
    &CRLDef_Controls,
    CRLMenu_Keybinds_4,
    M_DrawCRL_Keybinds_4,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_4 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 3;

    M_FillBackground();

    M_WriteTextCentered(7, "WEAPONS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_weapon1);
    M_DrawBindKey(1, 25, key_weapon2);
    M_DrawBindKey(2, 34, key_weapon3);
    M_DrawBindKey(3, 43, key_weapon4);
    M_DrawBindKey(4, 52, key_weapon5);
    M_DrawBindKey(5, 61, key_weapon6);
    M_DrawBindKey(6, 70, key_weapon7);
    M_DrawBindKey(7, 79, key_weapon8);
    M_DrawBindKey(8, 88, key_prevweapon);
    M_DrawBindKey(9, 97, key_nextweapon);

    M_DrawBindFooter("4", true);
}

static void M_Bind_Weapon1 (int choice)
{
    M_StartBind(400);  // key_weapon1
}

static void M_Bind_Weapon2 (int choice)
{
    M_StartBind(401);  // key_weapon2
}

static void M_Bind_Weapon3 (int choice)
{
    M_StartBind(402);  // key_weapon3
}

static void M_Bind_Weapon4 (int choice)
{
    M_StartBind(403);  // key_weapon4
}

static void M_Bind_Weapon5 (int choice)
{
    M_StartBind(404);  // key_weapon5
}

static void M_Bind_Weapon6 (int choice)
{
    M_StartBind(405);  // key_weapon6
}

static void M_Bind_Weapon7 (int choice)
{
    M_StartBind(406);  // key_weapon7
}

static void M_Bind_Weapon8 (int choice)
{
    M_StartBind(407);  // key_weapon8
}

static void M_Bind_PrevWeapon (int choice)
{
    M_StartBind(408);  // key_prevweapon
}

static void M_Bind_NextWeapon (int choice)
{
    M_StartBind(408);  // key_nextweapon
}

// -----------------------------------------------------------------------------
// Keybinds 5
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_5[]=
{
    { M_SWTC, "TOGGLE MAP",             M_Bind_ToggleMap,    't' },
    { M_SWTC, "ZOOM IN",                M_Bind_ZoomIn,       'z' },
    { M_SWTC, "ZOOM OUT",               M_Bind_ZoomOut,      'z' },
    { M_SWTC, "MAXIMUM ZOOM OUT",       M_Bind_MaxZoom,      'm' },
    { M_SWTC, "FOLLOW MODE",            M_Bind_FollowMode,   'f' },
    { M_SWTC, "ROTATE MODE",            M_Bind_RotateMode,   'r' },
    { M_SWTC, "OVERLAY MODE",           M_Bind_OverlayMode,  'o' },
    { M_SWTC, "SOUND PROPAGATION MODE", M_Bind_SndPropMode,  's' },
    { M_SWTC, "TOGGLE GRID",            M_Bind_ToggleGrid,   't' },
    { M_SWTC, "MARK LOCATION",          M_Bind_AddMark,      'm' },
    { M_SWTC, "CLEAR ALL MARKS",        M_Bind_ClearMarks,   'c' },
};

static menu_t CRLDef_Keybinds_5 =
{
    11,
    &CRLDef_Controls,
    CRLMenu_Keybinds_5,
    M_DrawCRL_Keybinds_5,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_5 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 4;

    M_FillBackground();

    M_WriteTextCentered(7, "AUTOMAP", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_map_toggle);
    M_DrawBindKey(1, 25, key_map_zoomin);
    M_DrawBindKey(2, 34, key_map_zoomout);
    M_DrawBindKey(3, 43, key_map_maxzoom);
    M_DrawBindKey(4, 52, key_map_follow);
    M_DrawBindKey(5, 61, key_crl_map_rotate);
    M_DrawBindKey(6, 70, key_crl_map_overlay);
    M_DrawBindKey(7, 79, key_crl_map_sndprop);
    M_DrawBindKey(8, 88, key_map_grid);
    M_DrawBindKey(9, 97, key_map_mark);
    M_DrawBindKey(10, 106, key_map_clearmark);

    M_DrawBindFooter("5", true);
}

static void M_Bind_ToggleMap (int choice)
{
    M_StartBind(500);  // key_map_toggle
}

static void M_Bind_ZoomIn (int choice)
{
    M_StartBind(501);  // key_map_zoomin
}

static void M_Bind_ZoomOut (int choice)
{
    M_StartBind(502);  // key_map_zoomout
}

static void M_Bind_MaxZoom (int choice)
{
    M_StartBind(503);  // key_map_maxzoom
}

static void M_Bind_FollowMode (int choice)
{
    M_StartBind(504);  // key_map_follow
}

static void M_Bind_RotateMode (int choice)
{
    M_StartBind(505);  // key_crl_map_rotate
}

static void M_Bind_OverlayMode (int choice)
{
    M_StartBind(506);  // key_crl_map_overlay
}

static void M_Bind_SndPropMode (int choice)
{
    M_StartBind(507);  // key_crl_map_sndprop
}

static void M_Bind_ToggleGrid (int choice)
{
    M_StartBind(508);  // key_map_grid
}

static void M_Bind_AddMark (int choice)
{
    M_StartBind(509);  // key_map_mark
}

static void M_Bind_ClearMarks (int choice)
{
    M_StartBind(510);  // key_map_clearmark
}

// -----------------------------------------------------------------------------
// Keybinds 6
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_6[]=
{
    { M_SWTC, "HELP SCREEN",      M_Bind_HelpScreen,      'h' },
    { M_SWTC, "SAVE GAME",        M_Bind_SaveGame,        's' },
    { M_SWTC, "LOAD GAME",        M_Bind_LoadGame,        'l' },
    { M_SWTC, "SOUND VOLUME",     M_Bind_SoundVolume,     's' },
    { M_SWTC, "TOGGLE DETAIL",    M_Bind_ToggleDetail,    't' },
    { M_SWTC, "QUICK SAVE",       M_Bind_QuickSave,       'q' },
    { M_SWTC, "END GAME",         M_Bind_EndGame,         'e' },
    { M_SWTC, "TOGGLE MESSAGES",  M_Bind_ToggleMessages,  't' },
    { M_SWTC, "QUICK LOAD",       M_Bind_QuickLoad,       'q' },
    { M_SWTC, "QUIT GAME",        M_Bind_QuitGame,        'q' },
    { M_SWTC, "TOGGLE GAMMA",     M_Bind_ToggleGamma,     't' },
    { M_SWTC, "MULTIPLAYER SPY",  M_Bind_MultiplayerSpy,  'm' },
};

static menu_t CRLDef_Keybinds_6 =
{
    12,
    &CRLDef_Controls,
    CRLMenu_Keybinds_6,
    M_DrawCRL_Keybinds_6,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_6 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 5;

    M_FillBackground();

    M_WriteTextCentered(7, "FUNCTION KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_menu_help);
    M_DrawBindKey(1, 25, key_menu_save);
    M_DrawBindKey(2, 34, key_menu_load);
    M_DrawBindKey(3, 43, key_menu_volume);
    M_DrawBindKey(4, 52, key_menu_detail);
    M_DrawBindKey(5, 61, key_menu_qsave);
    M_DrawBindKey(6, 70, key_menu_endgame);
    M_DrawBindKey(7, 79, key_menu_messages);
    M_DrawBindKey(8, 88, key_menu_qload);
    M_DrawBindKey(9, 97, key_menu_quit);
    M_DrawBindKey(10, 106, key_menu_gamma);
    M_DrawBindKey(11, 115, key_spy);

    M_DrawBindFooter("6", true);
}

static void M_Bind_HelpScreen (int choice)
{
    M_StartBind(600);  // key_menu_help
}

static void M_Bind_SaveGame (int choice)
{
    M_StartBind(601);  // key_menu_save
}

static void M_Bind_LoadGame (int choice)
{
    M_StartBind(602);  // key_menu_load
}

static void M_Bind_SoundVolume (int choice)
{
    M_StartBind(603);  // key_menu_volume
}

static void M_Bind_ToggleDetail (int choice)
{
    M_StartBind(604);  // key_menu_detail
}

static void M_Bind_QuickSave (int choice)
{
    M_StartBind(605);  // key_menu_qsave
}

static void M_Bind_EndGame (int choice)
{
    M_StartBind(606);  // key_menu_endgame
}

static void M_Bind_ToggleMessages (int choice)
{
    M_StartBind(607);  // key_menu_messages
}

static void M_Bind_QuickLoad (int choice)
{
    M_StartBind(608);  // key_menu_qload
}

static void M_Bind_QuitGame (int choice)
{
    M_StartBind(609);  // key_menu_quit
}

static void M_Bind_ToggleGamma (int choice)
{
    M_StartBind(610);  // key_menu_gamma
}

static void M_Bind_MultiplayerSpy (int choice)
{
    M_StartBind(611);  // key_spy
}

// -----------------------------------------------------------------------------
// Keybinds 7
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_7[]=
{
    { M_SWTC, "PAUSE GAME",             M_Bind_Pause,           'p'  },
    { M_SWTC, "SAVE A SCREENSHOT",      M_Bind_SaveScreenshot,  's'  },
    { M_SWTC, "DISPLAY LAST MESSAGE",   M_Bind_LastMessage,     'd'  },
    { M_SWTC, "FINISH DEMO RECORDING",  M_Bind_FinishDemo,      'f'  },
    { M_SKIP, "",                       0,                      '\0' },  // MULTIPLAYER
    { M_SWTC, "SEND MESSAGE",           M_Bind_SendMessage,     's'  },
    { M_SWTC, "- TO PLAYER 1",          M_Bind_ToPlayer1,       '1'  },
    { M_SWTC, "- TO PLAYER 2",          M_Bind_ToPlayer2,       '2'  },
    { M_SWTC, "- TO PLAYER 3",          M_Bind_ToPlayer3,       '3'  },
    { M_SWTC, "- TO PLAYER 4",          M_Bind_ToPlayer4,       '4'  },
    { M_SKIP, "",                       0,                      '\0' },
    { M_SWTC, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,        'r'  },
};

static menu_t CRLDef_Keybinds_7 =
{
    12,
    &CRLDef_Controls,
    CRLMenu_Keybinds_7,
    M_DrawCRL_Keybinds_7,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

static void M_DrawCRL_Keybinds_7 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 6;

    M_FillBackground();

    M_WriteTextCentered(7, "SHORTCUT KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 16, key_pause);
    M_DrawBindKey(1, 25, key_menu_screenshot);
    M_DrawBindKey(2, 34, key_message_refresh);
    M_DrawBindKey(3, 43, key_demo_quit);

    M_WriteTextCentered(52, "MULTIPLAYER", cr[CR_YELLOW]);

    M_DrawBindKey(5, 61, key_multi_msg);
    M_DrawBindKey(6, 70, key_multi_msgplayer[0]);
    M_DrawBindKey(7, 79, key_multi_msgplayer[1]);
    M_DrawBindKey(8, 88, key_multi_msgplayer[2]);
    M_DrawBindKey(9, 97, key_multi_msgplayer[3]);

    M_WriteTextCentered(106, "RESET", cr[CR_YELLOW]);

    M_DrawBindFooter("7", true);
}

static void M_Bind_Pause (int choice)
{
    M_StartBind(700);  // key_pause
}

static void M_Bind_SaveScreenshot (int choice)
{
    M_StartBind(701);  // key_menu_screenshot
}

static void M_Bind_LastMessage (int choice)
{
    M_StartBind(702);  // key_message_refresh
}

static void M_Bind_FinishDemo (int choice)
{
    M_StartBind(703);  // key_demo_quit
}

static void M_Bind_SendMessage (int choice)
{
    M_StartBind(704);  // key_multi_msg
}

static void M_Bind_ToPlayer1 (int choice)
{
    M_StartBind(705);  // key_multi_msgplayer1
}

static void M_Bind_ToPlayer2 (int choice)
{
    M_StartBind(706);  // key_multi_msgplayer2
}

static void M_Bind_ToPlayer3 (int choice)
{
    M_StartBind(707);  // key_multi_msgplayer3
}

static void M_Bind_ToPlayer4 (int choice)
{
    M_StartBind(708);  // key_multi_msgplayer4
}

static void M_Bind_ResetResponse (int key)
{
    if (key != key_menu_confirm)
    {
        return;
    }

    M_ResetBinds();
}

static void M_Bind_Reset (int choice)
{
    const char *resetwarning =
	    M_StringJoin("RESET KEYBOARD BINDINGS TO DEFAULT VALUES?",
                     "\n\n", PRESSYN, NULL);

    messageFillBG = true;
    M_StartMessage(resetwarning, M_Bind_ResetResponse, true);
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_MouseBinds[]=
{
    { M_SWTC, "FIRE/ATTACK",    M_Bind_M_FireAttack,       'f' },
    { M_SWTC, "MOVE FORWARD",   M_Bind_M_MoveForward,      'm' },
    { M_SWTC, "SPEED ON",       M_Bind_M_SpeedOn,          's' },
    { M_SWTC, "STRAFE ON",      M_Bind_M_StrafeOn,         's' },
    { M_SWTC, "MOVE BACKWARD",  M_Bind_M_MoveBackward,     'm' },
    { M_SWTC, "USE",            M_Bind_M_Use,              'u' },
    { M_SWTC, "STRAFE LEFT",    M_Bind_M_StrafeLeft,       's' },
    { M_SWTC, "STRAFE RIGHT",   M_Bind_M_StrafeRight,      's' },
    { M_SWTC, "PREV WEAPON",    M_Bind_M_PrevWeapon,       'p' },
    { M_SWTC, "NEXT WEAPON",    M_Bind_M_NextWeapon,       'n' },
    { M_SKIP, "", 0, '\0' },
    { M_SWTC, "RESET BINDINGS TO DEFAULT", M_Bind_M_Reset, 'r' },
};

static menu_t CRLDef_MouseBinds =
{
    12,
    &CRLDef_Controls,
    CRLMenu_MouseBinds,
    M_DrawCRL_MouseBinds,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_DrawCRL_MouseBinds (void)
{
    st_fullupdate = true;

    M_FillBackground();

    M_WriteTextCentered(7, "MOUSE BINDINGS", cr[CR_YELLOW]);

    M_DrawBindButton(0, 16, mousebfire);
    M_DrawBindButton(1, 25, mousebforward);
    M_DrawBindButton(2, 34, mousebspeed);
    M_DrawBindButton(3, 43, mousebstrafe);
    M_DrawBindButton(4, 52, mousebbackward);
    M_DrawBindButton(5, 61, mousebuse);
    M_DrawBindButton(6, 70, mousebstrafeleft);
    M_DrawBindButton(7, 79, mousebstraferight);
    M_DrawBindButton(8, 88, mousebprevweapon);
    M_DrawBindButton(9, 97, mousebnextweapon);

    M_WriteTextCentered(106, "RESET", cr[CR_YELLOW]);

    M_DrawBindFooter(NULL, false);
}

static void M_ChooseCRL_MouseBinds (int choice)
{
    M_SetupNextMenu (&CRLDef_MouseBinds);
}

static void M_Bind_M_FireAttack (int choice)
{
    M_StartMouseBind(1000);  // mousebfire
}

static void M_Bind_M_MoveForward (int choice)
{
    M_StartMouseBind(1001);  // mousebforward
}

static void M_Bind_M_SpeedOn (int choice)
{
    M_StartMouseBind(1002);  // mousebspeed
}

static void M_Bind_M_StrafeOn (int choice)
{
    M_StartMouseBind(1003);  // mousebstrafe
}

static void M_Bind_M_MoveBackward (int choice)
{
    M_StartMouseBind(1004);  // mousebbackward
}

static void M_Bind_M_Use (int choice)
{
    M_StartMouseBind(1005);  // mousebuse
}

static void M_Bind_M_StrafeLeft (int choice)
{
    M_StartMouseBind(1006);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int choice)
{
    M_StartMouseBind(1007);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int choice)
{
    M_StartMouseBind(1008);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int choice)
{
    M_StartMouseBind(1009);  // mousebnextweapon
}

static void M_Bind_M_ResetResponse (int key)
{
    if (key != key_menu_confirm)
    {
        return;
    }

    M_ResetMouseBinds();
}

static void M_Bind_M_Reset (int choice)
{
    const char *resetwarning =
	    M_StringJoin("RESET MOUSE BINDINGS TO DEFAULT VALUES?",
                     "\n\n", PRESSYN, NULL);

    messageFillBG = true;
    M_StartMessage(resetwarning, M_Bind_M_ResetResponse, true);
}

// -----------------------------------------------------------------------------
// Widgets settings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Widgets[]=
{
    { M_MUL2, "RENDER COUNTERS",    M_CRL_Widget_Render,     'r' },
    { M_MUL2, "MAX OVERFLOW STYLE", M_CRL_Widget_MAX,        'm' },
    { M_MUL2, "PLAYSTATE COUNTERS", M_CRL_Widget_Playstate,  'p' },
    { M_MUL2, "KIS STATS/FRAGS",    M_CRL_Widget_KIS,        'k' },
    { M_MUL2, "- STATS FORMAT",     M_CRL_Widget_KIS_Format, 's' },
    { M_MUL2, "- SHOW ITEMS",       M_CRL_Widget_KIS_Items,  'i' },
    { M_MUL2, "LEVEL/DM TIMER",     M_CRL_Widget_Time,       'l' },
    { M_MUL2, "PLAYER COORDS",      M_CRL_Widget_Coords,     'p' },
    { M_MUL2, "PLAYER SPEED",       M_CRL_Widget_Speed,      'p' },
    { M_MUL2, "POWERUP TIMERS",     M_CRL_Widget_Powerups,   'p' },
    { M_MUL2, "TARGET'S HEALTH",    M_CRL_Widget_Health,     't' },
};

static menu_t CRLDef_Widgets =
{
    11,
    &CRLDef_Main,
    CRLMenu_Widgets,
    M_DrawCRL_Widgets,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Widgets (int choice)
{
    M_SetupNextMenu (&CRLDef_Widgets);
}

static void M_DrawCRL_Widgets (void)
{
    char str[32];

    M_WriteTextCentered(7, "WIDGETS", cr[CR_YELLOW]);

    // Rendering counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 16, str,
                 M_Item_Glow(0, crl_widget_render == 1 ? GLOW_GREEN :
                                crl_widget_render == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // MAX overflow style
    sprintf(str, crl_widget_maxvp == 1 ? "BLINKING 1" :
                 crl_widget_maxvp == 2 ? "BLINKING 2" : "STATIC");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, crl_widget_maxvp == 1 ? (gametic &  8 ? GLOW_YELLOW : GLOW_GREEN) :
                                crl_widget_maxvp == 2 ? (gametic & 16 ? GLOW_YELLOW : GLOW_GREEN) : GLOW_YELLOW));

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(2, crl_widget_playstate == 1 ? GLOW_GREEN :
                                crl_widget_playstate == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // K/I/S stats
    sprintf(str, crl_widget_kis == 1 ? "ON" :
                 crl_widget_kis == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(3, crl_widget_kis ? GLOW_GREEN : GLOW_DARKRED));

    // Stats format
    sprintf(str, crl_widget_kis_format == 1 ? "REMAINING" :
                 crl_widget_kis_format == 2 ? "PERCENT" : "RATIO");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(4, crl_widget_kis_format ? GLOW_GREEN : GLOW_DARKRED));

    // Show items
    sprintf(str, crl_widget_kis_items ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(5, crl_widget_kis_items ? GLOW_GREEN : GLOW_DARKRED));

    // Level time
    sprintf(str, crl_widget_time == 1 ? "ON" : 
                 crl_widget_time == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(6, crl_widget_time ? GLOW_GREEN : GLOW_DARKRED));

    // Player coords
    sprintf(str, crl_widget_coords == 1 ? "ON" :
                 crl_widget_coords == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(7, crl_widget_coords ? GLOW_GREEN : GLOW_DARKRED));

    // Player speed
    sprintf(str, crl_widget_speed ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(8, crl_widget_speed ? GLOW_GREEN : GLOW_DARKRED));

    // Powerup timers
    sprintf(str, crl_widget_powerups ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(9, crl_widget_powerups ? GLOW_GREEN : GLOW_DARKRED));

    // Target's health
    sprintf(str, crl_widget_health == 1 ? "TOP" :
                 crl_widget_health == 2 ? "TOP+NAME" :
                 crl_widget_health == 3 ? "BOTTOM" :
                 crl_widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(10, crl_widget_health ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (!crl_extended_hud)
    {
        M_WriteTextCentered(160, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }
}

static void M_CRL_Widget_Render (int choice)
{
    crl_widget_render = M_INT_Slider(crl_widget_render, 0, 2, choice, false);
}

static void M_CRL_Widget_MAX (int choice)
{
    crl_widget_maxvp = M_INT_Slider(crl_widget_maxvp, 0, 2, choice, false);
}

static void M_CRL_Widget_Playstate (int choice)
{
    crl_widget_playstate = M_INT_Slider(crl_widget_playstate, 0, 2, choice, false);
}

static void M_CRL_Widget_KIS (int choice)
{
    crl_widget_kis = M_INT_Slider(crl_widget_kis, 0, 2, choice, false);
}

static void M_CRL_Widget_KIS_Format (int choice)
{
    crl_widget_kis_format = M_INT_Slider(crl_widget_kis_format, 0, 2, choice, false);
}

static void M_CRL_Widget_KIS_Items (int choice)
{
    crl_widget_kis_items ^= 1;
}

static void M_CRL_Widget_Coords (int choice)
{
    crl_widget_coords = M_INT_Slider(crl_widget_coords, 0, 2, choice, false);
}

static void M_CRL_Widget_Speed (int choice)
{
    crl_widget_speed ^= 1;
}

static void M_CRL_Widget_Time (int choice)
{
    crl_widget_time = M_INT_Slider(crl_widget_time, 0, 2, choice, false);
}

static void M_CRL_Widget_Powerups (int choice)
{
    crl_widget_powerups ^= 1;
}

static void M_CRL_Widget_Health (int choice)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 4, choice, false);
}

static void M_CRL_Automap_Rotate (int choice)
{
    crl_automap_rotate ^= 1;
}

static void M_CRL_Automap_Overlay (int choice)
{
    crl_automap_overlay ^= 1;
}

static void M_CRL_Automap_Shading (int choice)
{
    crl_automap_shading = M_INT_Slider(crl_automap_shading, 0, 12, choice, true);
}

static void M_CRL_Automap_Drawing (int choice)
{
    crl_automap_mode = M_INT_Slider(crl_automap_mode, 0, 2, choice, false);
}

static void M_CRL_Automap_Secrets (int choice)
{
   crl_automap_secrets = M_INT_Slider(crl_automap_secrets, 0, 2, choice, false);
}

static void M_CRL_Automap_SndProp (int choice)
{
    crl_automap_sndprop ^= 1;
}

// -----------------------------------------------------------------------------
// Automap settings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Automap[]=
{
    { M_MUL2, "ROTATE MODE",            M_CRL_Automap_Rotate,    'r' },
    { M_MUL2, "OVERLAY MODE",           M_CRL_Automap_Overlay,   'o' },
    { M_MUL1, "OVERLAY SHADING LEVEL",  M_CRL_Automap_Shading,   'o' },
    { M_MUL2, "DRAWING MODE",           M_CRL_Automap_Drawing,   'd' },
    { M_MUL2, "MARK SECRET SECTORS",    M_CRL_Automap_Secrets,   'm' },
    { M_MUL2, "SOUND PROPAGATION MODE", M_CRL_Automap_SndProp,   's' },
};

static menu_t CRLDef_Automap =
{
    6,
    &CRLDef_Main,
    CRLMenu_Automap,
    M_DrawCRL_Automap,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Automap (int choice)
{
    M_SetupNextMenu (&CRLDef_Automap);
}

static void M_DrawCRL_Automap (void)
{
    char str[32];

    M_WriteTextCentered(7, "AUTOMAP", cr[CR_YELLOW]);

    // Rotate mode
    sprintf(str, crl_automap_rotate ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 16, str,
                 M_Item_Glow(0, crl_automap_rotate ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay mode
    sprintf(str, crl_automap_overlay ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, crl_automap_overlay ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay shading level
    sprintf(str,"%d", crl_automap_shading);
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(2, !crl_automap_overlay ? GLOW_DARKRED :
                                 crl_automap_shading ==  0 ? GLOW_RED :
                                 crl_automap_shading == 12 ? GLOW_YELLOW : GLOW_GREEN));

    // Drawing mode
    sprintf(str, crl_automap_mode == 1 ? "FLOOR VISPLANES" :
                 crl_automap_mode == 2 ? "CEILING VISPLANES" : "NORMAL");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(3, crl_automap_mode ? GLOW_GREEN : GLOW_DARKRED));

    // Mark secret sectors
    sprintf(str, crl_automap_secrets == 1 ? "REVEALED" :
                 crl_automap_secrets == 2 ? "ALWAYS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(4, crl_automap_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Sound propagation mode
    sprintf(str, crl_automap_sndprop ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(5, crl_automap_sndprop ? GLOW_GREEN : GLOW_DARKRED));
}

// -----------------------------------------------------------------------------
// Gameplay features
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Gameplay[]=
{
    { M_MUL2, "DEFAULT SKILL LEVEL",      M_CRL_DefaulSkill,      'd' },
    { M_MUL2, "PISTOL START GAME MODE",   M_CRL_PistolStart,      'p' },
    { M_MUL2, "COLORED STATUS BAR",       M_CRL_ColoredSTBar,     'c' },
    { M_MUL2, "REPORT REVEALED SECRETS",  M_CRL_RevealedSecrets,  'r' },
    { M_MUL2, "RESTORE MONSTER TARGETS",  M_CRL_RestoreTargets,   'r' },
    { M_MUL2, "ON DEATH ACTION",          M_CRL_OnDeathAction,    'o' },
    { M_SKIP, "", 0, '\0'},
    { M_MUL2, "SHOW DEMO TIMER",          M_CRL_DemoTimer,        's' },
    { M_MUL1, "TIMER DIRECTION",          M_CRL_TimerDirection,   't' },
    { M_MUL2, "SHOW PROGRESS BAR",        M_CRL_ProgressBar,      's' },
    { M_MUL2, "PLAY INTERNAL DEMOS",      M_CRL_InternalDemos,    'p' },
};

static menu_t CRLDef_Gameplay =
{
    11,
    &CRLDef_Main,
    CRLMenu_Gameplay,
    M_DrawCRL_Gameplay,
    CRL_MENU_LEFTOFFSET_BIG, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Gameplay (int choice)
{
    M_SetupNextMenu (&CRLDef_Gameplay);
}

static void M_DrawCRL_Gameplay (void)
{
    char str[32];

    M_WriteTextCentered(7, "GAMEPLAY FEATURES", cr[CR_YELLOW]);

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[crl_default_skill]);
    M_WriteText (M_ItemRightAlign(str), 16, str, 
                 M_Item_Glow(0, DefSkillColor(crl_default_skill)));

    // Pistol start game mode
    sprintf(str, crl_pistol_start ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, crl_pistol_start ? GLOW_GREEN : GLOW_DARKRED));

    // Colored status bar
    sprintf(str, crl_colored_stbar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(2, crl_colored_stbar ? GLOW_GREEN : GLOW_DARKRED));

    // Report revealed secrets
    sprintf(str, crl_revealed_secrets == 1 ? "TOP" :
                 crl_revealed_secrets == 2 ? "CENTERED" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(3, crl_revealed_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Restore monster target from savegames
    sprintf(str, crl_restore_targets ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(4, crl_restore_targets ? GLOW_GREEN : GLOW_DARKRED));

    // On death action
    sprintf(str, crl_death_use_action == 1 ? "LAST SAVE" :
                 crl_death_use_action == 2 ? "NOTHING" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(5, crl_death_use_action ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(70, "DEMOS", cr[CR_YELLOW]);

    // Demo timer
    sprintf(str, crl_demo_timer == 1 ? "PLAYBACK" : 
                 crl_demo_timer == 2 ? "RECORDING" : 
                 crl_demo_timer == 3 ? "ALWAYS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(7, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Timer direction
    sprintf(str, crl_demo_timerdir ? "BACKWARD" : "FORWARD");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(8, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Progress bar
    sprintf(str, crl_demo_bar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(9, crl_demo_bar ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (itemOn > 6 && itemOn < 10 && !crl_extended_hud)
    {
        M_WriteTextCentered(160, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }

    // Play internal demos
    sprintf(str, crl_internal_demos ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(10, crl_internal_demos ? GLOW_DARKRED : GLOW_GREEN));
}

static void M_CRL_DefaulSkill (int choice)
{
    crl_default_skill = M_INT_Slider(crl_default_skill, 0, 4, choice, false);
    // [JN] Set new skill in skill level menu.
    NewDef.lastOn = crl_default_skill;
}

static void M_CRL_PistolStart (int choice)
{
    crl_pistol_start ^= 1;
}

static void M_CRL_ColoredSTBar (int choice)
{
    crl_colored_stbar ^= 1;
}

static void M_CRL_RevealedSecrets (int choice)
{
    crl_revealed_secrets = M_INT_Slider(crl_revealed_secrets, 0, 2, choice, false);
}

static void M_CRL_RestoreTargets (int choice)
{
    crl_restore_targets ^= 1;
}

static void M_CRL_OnDeathAction (int choice)
{
    crl_death_use_action = M_INT_Slider(crl_death_use_action, 0, 2, choice, false);
}

static void M_CRL_DemoTimer (int choice)
{
    crl_demo_timer = M_INT_Slider(crl_demo_timer, 0, 3, choice, false);
}

static void M_CRL_TimerDirection (int choice)
{
    crl_demo_timerdir ^= 1;
}

static void M_CRL_ProgressBar (int choice)
{
    crl_demo_bar ^= 1;

    // [JN] Redraw status bar to possibly 
    // clean up remainings of progress bar.
    st_fullupdate = true;
}

static void M_CRL_InternalDemos (int choice)
{
    crl_internal_demos ^= 1;
}

// -----------------------------------------------------------------------------
// Miscellaneous features
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Misc[]=
{
    { M_MUL1, "INVULNERABILITY EFFECT", M_CRL_Invul,       'i' },
    { M_MUL2, "PALETTE FLASH EFFECTS",  M_CRL_PalFlash,    'p' },
    { M_MUL1, "MOVEMENT BOBBING",       M_CRL_MoveBob,     'm' },
    { M_MUL1, "WEAPON BOBBING",         M_CRL_WeaponBob,   'w' },
    { M_MUL2, "COLORBLIND FILTER",      M_CRL_Colorblind,  'c' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "AUTOLOAD WAD FILES",     M_CRL_AutoloadWAD, 'a' },
    { M_MUL2, "AUTOLOAD DEH FILES",     M_CRL_AutoloadDEH, 'a' },
    { M_SKIP, "", 0, '\0' },
    { M_MUL2, "HIGHLIGHTING EFFECT",    M_CRL_Hightlight,  'h' },
    { M_MUL1, "ESC KEY BEHAVIOUR",      M_CRL_MenuEscKey,  'e' },
    { M_MUL1, "QUIT CONFIRMATION",      M_CRL_ConfirmQuit, 'q' },
};

static menu_t CRLDef_Misc =
{
    12,
    &CRLDef_Main,
    CRLMenu_Misc,
    M_DrawCRL_Misc,
    CRL_MENU_LEFTOFFSET_BIG, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Misc (int choice)
{
    M_SetupNextMenu (&CRLDef_Misc);
}

static void M_DrawCRL_Misc (void)
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

    M_WriteTextCentered(7, "ACCESSIBILITY", cr[CR_YELLOW]);

    // Invulnerability effect
    sprintf(str, crl_a11y_invul ? "GRAYSCALE" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 16, str,
                 M_Item_Glow(0, crl_a11y_invul ? GLOW_GREEN : GLOW_DARKRED));

    // Palette flash effects
    sprintf(str, crl_a11y_pal_flash == 1 ? "HALVED" :
                 crl_a11y_pal_flash == 2 ? "QUARTERED" :
                 crl_a11y_pal_flash == 3 ? "OFF" : "DEFAULT");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, crl_a11y_pal_flash == 1 ? GLOW_YELLOW :
                                crl_a11y_pal_flash == 2 ? GLOW_ORANGE :
                                crl_a11y_pal_flash == 3 ? GLOW_RED : GLOW_DARKRED));
                 
    // Movement bobbing
    sprintf(str, "%s", bobpercent[crl_a11y_move_bob]);
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(2, crl_a11y_move_bob == 20 ? GLOW_DARKRED :
                                crl_a11y_move_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Weapon bobbing
    sprintf(str, "%s", bobpercent[crl_a11y_weapon_bob]);
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(3, crl_a11y_weapon_bob == 20 ? GLOW_DARKRED :
                                crl_a11y_weapon_bob ==  0 ? GLOW_RED : GLOW_YELLOW));

    // Colorblind
    sprintf(str, "%s", colorblind_name[crl_colorblind]);
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(4, crl_colorblind ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(61, "AUTOLOAD", cr[CR_YELLOW]);

    // Autoload WAD files
    sprintf(str, crl_autoload_wad == 1 ? "IWAD ONLY" :
                 crl_autoload_wad == 2 ? "IWAD AND PWAD" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(6, crl_autoload_wad == 1 ? GLOW_YELLOW :
                                crl_autoload_wad == 2 ? GLOW_GREEN : GLOW_DARKRED));

    // Autoload WAD files
    sprintf(str, crl_autoload_deh == 1 ? "IWAD ONLY" :
                 crl_autoload_deh == 2 ? "IWAD AND PWAD" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(7, crl_autoload_deh == 1 ? GLOW_YELLOW :
                                crl_autoload_deh == 2 ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(88, "MENU SETTINGS", cr[CR_YELLOW]);

    // Highlighting effect
    sprintf(str, crl_menu_highlight == 1 ? "STATIC" :
                 crl_menu_highlight == 2 ? "ANIMATED" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(9, crl_menu_highlight == 1 ? GLOW_YELLOW :
                                crl_menu_highlight == 2 ? GLOW_GREEN : GLOW_DARKRED));

    // ESC key behaviour
    sprintf(str, crl_menu_esc_key ? "GO BACK" : "CLOSE MENU" );
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(10, crl_menu_esc_key ? GLOW_GREEN : GLOW_DARKRED));

    // Quit confirmation
    sprintf(str, crl_confirm_quit ? "ON" : "OFF" );
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(11, crl_confirm_quit ? GLOW_DARKRED : GLOW_GREEN));

    // [PN] Added explanations for colorblind filters
    if (itemOn == 4 && crl_colorblind)
    {
        const char *colorblind_hint[] = {
            "","RED-BLIND","RED-WEAK","GREEN-BLIND","GREEN-WEAK",
            "BLUE-BLIND","BLUE-WEAK","MONOCHROMACY","BLUE CONE MONOCHROMACY"
        };

        M_WriteTextCentered(151, colorblind_hint[crl_colorblind], cr[CR_WHITE]);
    }
    // [PN] Added explanations for autoload variables
    if (itemOn == 6 || itemOn == 7)
    {
        const char *off = "AUTOLOAD IS DISABLED";
        const char *first_line = "AUTOLOAD AND FOLDER CREATION";
        const char *second_line1 = "ONLY ALLOWED FOR IWAD FILES";
        const char *second_line2 = "ALLOWED FOR BOTH IWAD AND PWAD FILES";
        const int   autoload_option = (itemOn == 6) ? crl_autoload_wad : crl_autoload_deh;

        switch (autoload_option)
        {
            case 1:
                M_WriteTextCentered(142, first_line, cr[CR_GRAY]);
                M_WriteTextCentered(151, second_line1, cr[CR_GRAY]);
                break;

            case 2:
                M_WriteTextCentered(142, first_line, cr[CR_GRAY]);
                M_WriteTextCentered(151, second_line2, cr[CR_GRAY]);
                break;

            default:
                M_WriteTextCentered(151, off, cr[CR_GRAY]);
                break;            
        }
    }
}

static void M_CRL_Invul (int choice)
{
    crl_a11y_invul ^= 1;
}

static void M_CRL_PalFlash (int choice)
{
    crl_a11y_pal_flash = M_INT_Slider(crl_a11y_pal_flash, 0, 3, choice, false);
    CRL_ReloadPalette();
}

static void M_CRL_MoveBob (int choice)
{
    crl_a11y_move_bob = M_INT_Slider(crl_a11y_move_bob, 0, 20, choice, true);
}

static void M_CRL_WeaponBob (int choice)
{
    crl_a11y_weapon_bob = M_INT_Slider(crl_a11y_weapon_bob, 0, 20, choice, true);
}

static void M_CRL_Colorblind (int choice)
{
    crl_colorblind = M_INT_Slider(crl_colorblind, 0, 8, choice, false);
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768);
}

static void M_CRL_AutoloadWAD (int choice)
{
    crl_autoload_wad = M_INT_Slider(crl_autoload_wad, 0, 2, choice, false);
}

static void M_CRL_AutoloadDEH (int choice)
{
    crl_autoload_deh = M_INT_Slider(crl_autoload_deh, 0, 2, choice, false);
}

static void M_CRL_Hightlight (int choice)
{
    crl_menu_highlight = M_INT_Slider(crl_menu_highlight, 0, 2, choice, false);
}

static void M_CRL_MenuEscKey (int choice)
{
    crl_menu_esc_key ^= 1;
}

static void M_CRL_ConfirmQuit (int choice)
{
    crl_confirm_quit ^= 1;
}

// -----------------------------------------------------------------------------
// Static engine limits
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Limits[]=
{
    { M_MUL1, "SAVE GAME LIMIT WARNING",    M_CRL_SaveSizeWarning, 's' },
    { M_MUL1, "RENDER LIMITS LEVEL",        M_CRL_Limits,          'r' },
};

static menu_t CRLDef_Limits =
{
    2,
    &CRLDef_Main,
    CRLMenu_Limits,
    M_DrawCRL_Limits,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, false, false,
};

static void M_ChooseCRL_Limits (int choice)
{
    M_SetupNextMenu (&CRLDef_Limits);
}

static void M_DrawCRL_Limits (void)
{
    char str[32];

    M_WriteTextCentered(7, "STATIC ENGINE LIMITS", cr[CR_YELLOW]);

    // Savegame limit warning
    sprintf(str, vanilla_savegame_limit ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 16, str,
                 M_Item_Glow(0, vanilla_savegame_limit ? GLOW_GREEN : GLOW_DARKRED));

    // Level of the limits
    sprintf(str, crl_vanilla_limits ? "VANILLA" : "DOOM-PLUS");
    M_WriteText (M_ItemRightAlign(str), 25, str,
                 M_Item_Glow(1, crl_vanilla_limits ? GLOW_RED : GLOW_GREEN));

    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 43, "MAXVISPLANES",  cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 52, "MAXDRAWSEGS",   cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 61, "MAXVISSPRITES", cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 70, "MAXOPENINGS",   cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 79, "MAXPLATS",      cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 88, "MAXLINEANIMS",  cr[CR_GRAY]);

    if (crl_vanilla_limits)
    {
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("128"),   43,   "128", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("256"),   52,   "256", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("128"),   61,   "128", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("20480"), 70, "20480", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("30"),    79,    "30", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("64"),    88,    "64", cr[CR_RED]);
    }
    else
    {
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("1024"),  43,  "1024", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("2048"),  52,  "2048", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("1024"),  61,  "1024", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("65536"), 70, "65536", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("7680"),  79,  "7680", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("16384"), 88, "16384", cr[CR_GREEN]);
    }
}

static void M_CRL_SaveSizeWarning (int choice)
{
    vanilla_savegame_limit ^= 1;
}

static void M_CRL_Limits (int choice)
{
    crl_vanilla_limits ^= 1;

    // [JN] CRL - re-define static engine limits.
    CRL_SetStaticLimits("DOOM+");
}

// =============================================================================


//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
static void M_ReadSaveStrings(void)
{
    FILE   *handle;
    int     i;
    char    name[256];

    for (i = 0;i < load_end;i++)
    {
        int retval;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = M_fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }
}


// [FG] support up to 8 pages of savegames
static void M_DrawSaveLoadBottomLine (void)
{
    char pagestr[16];

    if (savepage > 0)
    {
        M_WriteText(LoadDef.x, 151, "< PGUP", cr[CR_MENU_DARK2]);
    }
    if (savepage < savepage_max)
    {
        M_WriteText(LoadDef.x+(SAVESTRINGSIZE-6)*8, 151, "PGDN >", cr[CR_MENU_DARK2]);
    }

    M_snprintf(pagestr, sizeof(pagestr), "PAGE %d/%d", savepage + 1, savepage_max + 1);
    
    M_WriteTextCentered(151, pagestr, cr[CR_MENU_DARK1]);

    // [JN] Print "modified" (or created initially) time of savegame file.
    if (LoadMenu[itemOn].status)
    {
        struct stat filestat;
        char filedate[32];

        if (M_stat(P_SaveGameFile(itemOn), &filestat) == 0)
        {
// [FG] suppress the most useless compiler warning ever
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-y2k"
#endif
        strftime(filedate, sizeof(filedate), "%x %X", localtime(&filestat.st_mtime));
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
        M_WriteTextCentered(160, filedate, cr[CR_MENU_DARK1]);
        }
    }
}


//
// M_LoadGame & Cie.
//
static void M_DrawLoad(void)
{
    int             i;
    const char *m_loadg = DEH_String("M_LOADG");
	
    V_DrawShadowedPatch(72, 7, W_CacheLumpName(m_loadg, PU_CACHE), m_loadg);

    for (i = 0;i < load_end; i++)
    {
	// [JN] Highlight selected item (itemOn == i) or apply fading effect.
	dp_translation = M_SaveLoad_Glow(itemOn == i, 0, saveload_border);
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i],
	            M_SaveLoad_Glow(itemOn == i, currentMenu->menuitems[i].tics, saveload_text));
    }

    M_DrawSaveLoadBottomLine();
}



//
// Draw border for the savegame description
//
static void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
    const char *m_lsleft = DEH_String("M_LSLEFT");
    const char *m_lscntr = DEH_String("M_LSCNTR");
    const char *m_lsrght = DEH_String("M_LSRGHT");
	
    V_DrawShadowedPatch(x - 8, y, W_CacheLumpName(m_lsleft, PU_CACHE), m_lsleft);
	
    for (i = 0;i < 24;i++)
    {
	V_DrawShadowedPatch(x, y, W_CacheLumpName(m_lscntr, PU_CACHE), m_lscntr);
	x += 8;
    }

    V_DrawShadowedPatch(x, y, W_CacheLumpName(m_lsrght, PU_CACHE),m_lsrght);
}



//
// User wants to load this game
//
static void M_LoadSelect(int choice)
{
    char    name[256];
	
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));

    G_LoadGame (name);
    M_ClearMenus ();

    // [crispy] allow quickload before quicksave
    if (quickSaveSlot == -2)
	quickSaveSlot = choice;
}

//
// Selected from DOOM menu
//
static void M_LoadGame (int choice)
{
    // [crispy] allow loading game while multiplayer demo playback
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(LOADNET),NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}


//
//  M_SaveGame & Cie.
//
static void M_DrawSave(void)
{
    int             i;
    const char *m_saveg = DEH_String("M_SAVEG");
	
    V_DrawShadowedPatch(72, 7, W_CacheLumpName(m_saveg, PU_CACHE), m_saveg);
    for (i = 0;i < load_end; i++)
    {
	// [JN] Highlight selected item (itemOn == i) or apply fading effect.
	dp_translation = M_SaveLoad_Glow(itemOn == i, 0, saveload_border);
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i],
	            M_SaveLoad_Glow(itemOn == i, currentMenu->menuitems[i].tics, saveload_text));
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	// [JN] Highlight "_" cursor, line is always active while typing.
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_", M_SaveLoad_Glow(0, 0, saveload_cursor));
	// [JN] Forcefully hide the mouse cursor while typing.
	menu_mouse_allow = false;
    }

    M_DrawSaveLoadBottomLine();
}

//
// M_Responder calls this when user is finished
//
static void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
    // [JN] Generate save name. ExMx for Doom 1, MAPxx for Doom 2.
    if (gamemode == commercial)
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE - 1,
                   "MAP%02d", gamemap);
    }
    else
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE - 1,
                   "E%dM%d", gameepisode, gamemap);
    }
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

// [crispy] override savegame name if it already starts with a map identifier
static boolean StartsWithMapIdentifier (char *str)
{
    M_ForceUppercase(str);

    if (strlen(str) >= 4 &&
        str[0] == 'E' && isdigit(str[1]) &&
        str[2] == 'M' && isdigit(str[3]))
    {
        return true;
    }

    if (strlen(str) >= 5 &&
        str[0] == 'M' && str[1] == 'A' && str[2] == 'P' &&
        isdigit(str[3]) && isdigit(str[4]))
    {
        return true;
    }

    return false;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // [crispy] load the last game you saved
    LoadDef.lastOn = choice;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING) ||
        // [crispy] override savegame name if it already starts with a map identifier
        StartsWithMapIdentifier(savegamestrings[choice]))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave || true) // [crispy] always prefill empty savegame slot names
        {
            SetDefaultSaveName(choice);
        }
    }
    saveCharIndex = strlen(savegamestrings[choice]);
}

//
// Selected from DOOM menu
//
static void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//
static void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }

	// [JN] CRL - do not show confirmation dialog,
	// do a quick save immediately.
	M_DoSave(quickSaveSlot);
}



//
// M_QuickLoad
//
static void M_QuickLoad(void)
{
    // [crispy] allow quickloading game while multiplayer demo playback
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(QLOADNET),NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	// [crispy] allow quickload before quicksave
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&LoadDef);
	quickSaveSlot = -2;
	return;
    }

	// [JN] CRL - do not show confirmation dialog,
	// do a quick load immediately.
	M_LoadSelect(quickSaveSlot);
}




//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
static void M_DrawReadThis1(void)
{
    const char *help2 = DEH_String("HELP2");

    st_fullupdate = true;

    V_DrawPatch(0, 0, W_CacheLumpName(help2, PU_CACHE), help2);
}



//
// Read This Menus - optional second page.
//
static void M_DrawReadThis2(void)
{
    const char *help1 = DEH_String("HELP1");

    st_fullupdate = true;

    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

    V_DrawPatch(0, 0, W_CacheLumpName(help1, PU_CACHE), help1);
}

static void M_DrawReadThisCommercial(void)
{
    const char *help = DEH_String("HELP");

    st_fullupdate = true;

    V_DrawPatch(0, 0, W_CacheLumpName(help, PU_CACHE), help);
}


//
// Change Sfx & Music volumes
//
static void M_DrawSound(void)
{
    const char *m_svol = DEH_String("M_SVOL");
    char str[8];

    V_DrawShadowedPatch(60, 38, W_CacheLumpName(m_svol, PU_CACHE), m_svol);

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (sfx_vol + 1), 16, sfxVolume, 0);
    M_ID_HandleSliderMouseControl(86, 80, 132, &sfxVolume, false, 0, 15);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (226, 83, str, M_Item_Glow(0, sfxVolume ? GLOW_UNCOLORED : GLOW_DARKRED));

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (music_vol + 1), 16, musicVolume, 2);
    M_ID_HandleSliderMouseControl(86, 112, 132, &musicVolume, false, 0, 15);
    sprintf(str,"%d", musicVolume);
    M_WriteText (226, 115, str, M_Item_Glow(2, musicVolume ? GLOW_UNCOLORED : GLOW_DARKRED));
}

static void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

static void M_SfxVol(int choice)
{
    sfxVolume = M_INT_Slider(sfxVolume, 0, 15, choice, true);

    S_SetSfxVolume(sfxVolume * 8);
}

static void M_MusicVol(int choice)
{
    musicVolume = M_INT_Slider(musicVolume, 0, 15, choice, true);

    S_SetMusicVolume(musicVolume * 8);
}




//
// M_DrawMainMenu
//
static void M_DrawMainMenu(void)
{
    const char *m_doom = DEH_String("M_DOOM");

    V_DrawPatch(94, 2, W_CacheLumpName(m_doom, PU_CACHE), m_doom);
}




//
// M_NewGame
//
static void M_DrawNewGame(void)
{
    const char *m_newg = DEH_String("M_NEWG");
    const char *m_skill = DEH_String("M_SKILL");

    V_DrawShadowedPatch(96, 14, W_CacheLumpName(m_newg, PU_CACHE), m_newg);
    V_DrawShadowedPatch(54, 38, W_CacheLumpName(m_skill, PU_CACHE), m_skill);
}

static void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(NEWGAME),NULL,false);
	return;
    }
	
    // Chex Quest disabled the episode select screen, as did Doom II.

    if (gamemode == commercial || gameversion == exe_chex)
	M_SetupNextMenu(&NewDef);
    else
	M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
static int epi;

static void M_DrawEpisode(void)
{
    const char *m_episod = DEH_String("M_EPISOD");

    V_DrawShadowedPatch(54, 38, W_CacheLumpName(m_episod, PU_CACHE), m_episod);
}

static void M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
	return;
		
    G_DeferedInitNew(nightmare,epi+1,1);
    M_ClearMenus ();
}

static void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
	return;
    }
	
    G_DeferedInitNew(choice,epi+1,1);
    M_ClearMenus ();
}

static void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(DEH_String(SWSTRING),NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
static char *detailNames[2] = {"M_GDHIGH","M_GDLOW"};
static char *msgNames[2] = {"M_MSGOFF","M_MSGON"};

static void M_DrawOptions(void)
{
    const char *m_optttl = DEH_String("M_OPTTTL");

    V_DrawShadowedPatch(108, 15, W_CacheLumpName(m_optttl, PU_CACHE), m_optttl);
	
    dp_translation = M_Big_Line_Glow(currentMenu->menuitems[2].tics);
    V_DrawShadowedPatch(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		        W_CacheLumpName(DEH_String(detailNames[detailLevel]), PU_CACHE),
                                DEH_String(detailNames[detailLevel]));
    dp_translation = NULL;

    dp_translation = M_Big_Line_Glow(currentMenu->menuitems[1].tics);
    V_DrawShadowedPatch(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                W_CacheLumpName(DEH_String(msgNames[showMessages]), PU_CACHE),
                                DEH_String(msgNames[showMessages]));
    dp_translation = NULL;

    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity, 5);
    M_ID_HandleSliderMouseControl(66, 133, 84, &mouseSensitivity, false, 0, 9);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,(crl_screen_size-3), 3);
    M_ID_HandleSliderMouseControl(66, 101, 76, &crl_screen_size, false, 0, 13);
}

static void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}



//
//      Toggle messages on/off
//
static void M_ChangeMessages(int choice)
{
    showMessages = 1 - showMessages;
	
	CRL_SetMessage(&players[consoleplayer],
                   DEH_String(showMessages ? MSGON : MSGOFF), true, NULL);
}


//
// M_EndGame
//
static void M_EndGameResponse(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].messageCenteredTics = 1;
    players[consoleplayer].messageCentered = NULL;
    messageCriticalTics = 1;
    messageCritical1 = NULL;
    messageCritical2 = NULL;
    st_palette = 0;
    D_StartTitle ();
}

static void M_EndGame(int choice)
{
    if (!usergame)
    {
	S_StartSound(NULL,sfx_oof);
	return;
    }
	
    if (netgame)
    {
	M_StartMessage(DEH_String(NETEND),NULL,false);
	return;
    }
	
    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
static void M_ReadThis(int choice)
{
    M_SetupNextMenu(&ReadDef1);
}

static void M_ReadThis2(int choice)
{
    M_SetupNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int choice)
{
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
static const int quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

static const int quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



static void M_QuitResponse(int key)
{
    if (key != key_menu_confirm && key != key_menu_forward)
	return;
    if (!netgame && false) // [JN] CRL - quit imideatelly, don't play any sounds.
    {
	if (gamemode == commercial)
	    S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
	else
	    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
	I_WaitVBL(105);
    }
    I_Quit ();
}


static const char *M_SelectEndMessage(void)
{
    const char **endmsg;

    if (logical_gamemission == doom)
    {
        // Doom 1

        endmsg = doom1_endmsg;
    }
    else
    {
        // Doom 2
        
        endmsg = doom2_endmsg;
    }

    return endmsg[gametic % NUM_QUITMESSAGES];
}


static void M_QuitDOOM(int choice)
{
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY,
                 DEH_String(M_SelectEndMessage()));

    M_StartMessage(endstring,M_QuitResponse,true);
}




static void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}




static void M_ChangeDetail(int choice)
{
    detailLevel = 1 - detailLevel;

    R_SetViewSize (crl_screen_size, detailLevel);

    if (!detailLevel)
	CRL_SetMessage(&players[consoleplayer], DEH_String(DETAILHI), false, NULL);
    else
	CRL_SetMessage(&players[consoleplayer], DEH_String(DETAILLO), false, NULL);
}




static void M_SizeDisplay(int choice)
{
    crl_screen_size = M_INT_Slider(crl_screen_size, 3, 13, choice, true);

    R_SetViewSize (crl_screen_size, detailLevel);
}




//
//      Menu Functions
//
static void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot,
  int	itemPos )
{
    int		xx;
    int		i;
    const char	*m_therml = DEH_String("M_THERML");
    const char	*m_thermm = DEH_String("M_THERMM");
    const char	*m_thermr = DEH_String("M_THERMR");
    const char	*m_thermo = DEH_String("M_THERMO");

    // [JN] Highlight active slider and gem.
    dp_translation = M_SaveLoad_Glow(itemPos == itemOn, 0, saveload_slider);

    xx = x;
    V_DrawShadowedPatch(xx, y, W_CacheLumpName(m_therml, PU_CACHE), m_therml);
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawShadowedPatch(xx, y, W_CacheLumpName(m_thermm, PU_CACHE), m_thermm);
	xx += 8;
    }
    V_DrawShadowedPatch(xx, y, W_CacheLumpName(m_thermr, PU_CACHE), m_thermr);

    // [crispy] do not crash anymore if value exceeds thermometer range
    if (thermDot >= thermWidth)
    {
        thermDot = thermWidth - 1;
    }

    V_DrawPatch((x + 8) + thermDot * 8, y, W_CacheLumpName(m_thermo, PU_CACHE), m_thermo);
    dp_translation = NULL;
}

static void
M_StartMessage
( const char*	string,
  void		(*routine)(int),
  boolean	input )
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
    return;
}

//
// Find string width from hu_font chars
//
int M_StringWidth(const char* string)
{
    size_t             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
	c = toupper(string[i]) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
	    w += 4;
	else
	    w += SHORT (hu_font[c]->width);
    }
		
    return w;
}



//
//      Find string height from hu_font chars
//
static int M_StringHeight(const char* string)
{
    size_t             i;
    int             h;
    int             height = SHORT(hu_font[0]->height);
	
    h = height;
    for (i = 0;i < strlen(string);i++)
	if (string[i] == '\n')
	    h += height;
		
    return h;
}


// -----------------------------------------------------------------------------
// M_WriteText
// Write a string using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteText (int x, int y, const char *string, byte *table)
{
    const char*	ch;
    int w, c, cx, cy;
    char name[9];

    ch = string;
    cx = x;
    cy = y;

    dp_translation = table;

    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        if (c == '\n')
        {
            cx = x;
            cy += 12;
            continue;
        }

        // [JN] CRL - by providing \r symbol ("Carriage Return"), lesser vertical
        // spacing will be applied. Needed for nicer spacing in critical messages.
        // https://en.wikipedia.org/wiki/Escape_sequences_in_C
        if (c == '\r')
        {
            cx = x;
            cy += 9;
            continue;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        // [JN] Construct proper patch name for possible error handling:
        sprintf(name, "STCFN%03d", c + HU_FONTSTART);
        V_DrawShadowedPatch(cx, cy, hu_font[c], name);
        cx+=w;
    }

    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// M_WriteTextCentered
// [JN] Write a centered string using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteTextCentered (const int y, const char *string, byte *table)
{
    const char *ch;
    const int width = M_StringWidth(string);
    int w, c, cx, cy;
    char name[9];

    ch = string;
    cx = SCREENWIDTH/2-width/2;
    cy = y;

    dp_translation = table;

    // find width
    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c>= HU_FONTSIZE)
        {
            continue;
        }

        w = SHORT (hu_font[c]->width);
    }

    // draw it
    cx = SCREENWIDTH/2-width/2;
    ch = string;

    while (ch)
    {
        c = *ch++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c>= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx+w > SCREENWIDTH)
        {
            break;
        }
        
        // [JN] Construct proper patch name for possible error handling:
        sprintf(name, "STCFN%03d", c + HU_FONTSTART);
        V_DrawShadowedPatch(cx, cy, hu_font[c], name);
        cx += w;
    }
    
    dp_translation = NULL;
}

// -----------------------------------------------------------------------------
// M_WriteTextCritical
// [JN] Write a two line strings using the hu_font.
// -----------------------------------------------------------------------------

void M_WriteTextCritical (const int y, const char *string1, const char *string2, byte *table)
{
    const char*	ch1;
    const char*	ch2;
    int w, c, cx, cy;
    char name[9];

    ch1 = string1;
    ch2 = string2;
    cx = 0;
    cy = y;

    dp_translation = table;

    while (ch1)
    {
        c = *ch1++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        // [JN] Construct proper patch name for possible error handling:
        sprintf(name, "STCFN%03d", c + HU_FONTSTART);
        V_DrawShadowedPatch(cx, cy, hu_font[c], name);
        cx+=w;
    }
cx = 0;
    while (ch2)
    {
        c = *ch2++;

        if (!c)
        {
            break;
        }

        c = toupper(c) - HU_FONTSTART;

        if (c < 0 || c >= HU_FONTSIZE)
        {
            cx += 4;
            continue;
        }

        w = SHORT (hu_font[c]->width);

        if (cx + w > SCREENWIDTH)
        {
            break;
        }

        // [JN] Construct proper patch name for possible error handling:
        sprintf(name, "STCFN%03d", c + HU_FONTSTART);
        V_DrawShadowedPatch(cx, cy+8, hu_font[c], name);
        cx+=w;
    }

    dp_translation = NULL;
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.

static boolean IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

// -----------------------------------------------------------------------------
// G_ReloadLevel
// [crispy] reload current level / go to next level
// adapted from prboom-plus/src/e6y.c:369-449
// -----------------------------------------------------------------------------

static int G_ReloadLevel (void)
{
    int result = false;

    if (gamestate != GS_DEMOSCREEN)
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

static int G_GotoNextLevel (void)
{
    byte doom_next[4][9] = {
    {12, 13, 19, 15, 16, 17, 18, 21, 14},
    {22, 23, 24, 25, 29, 27, 28, 31, 26},
    {32, 33, 34, 35, 36, 39, 38, 41, 37},
    {42, 49, 44, 45, 46, 47, 48, 51, 43}
    //{52, 53, 54, 55, 56, 59, 58, 11, 57},
    };

    byte doom2_next[33] = {
     2,  3,  4,  5,  6,  7,  8,  9, 10, 11,
    12, 13, 14, 15, 31, 17, 18, 19, 20, 21,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 1,
    32, 16, 3
    };

    int changed = false;

    if (gamemode == commercial)
    {
        if (W_CheckNumForName("map31") < 0)
        {
            doom2_next[14] = 16;
        }

        if (gamemission == pack_hacx)
        {
            doom2_next[30] = 16;
            doom2_next[20] = 1;
        }

    }
    else
    {
        if (gamemode == shareware)
        {
            doom_next[0][7] = 11;
        }

        if (gamemode == registered)
        {
            doom_next[2][7] = 11;
        }
        
        // if (!crispy->haved1e5)
        {
            doom_next[3][7] = 11;
        }

        if (gameversion == exe_chex)
        {
            doom_next[0][2] = 14;
            doom_next[0][4] = 11;
        }
    }

    if (gamestate != GS_DEMOSCREEN)
    {
        int epsd, map;

        if (gamemode == commercial)
        {
            epsd = gameepisode;
            map = doom2_next[gamemap-1];
        }
        else
        {
            epsd = doom_next[gameepisode-1][gamemap-1] / 10;
            map = doom_next[gameepisode-1][gamemap-1] % 10;
        }

        G_DeferedInitNew(gameskill, epsd, map);
        changed = true;
    }

    return changed;
}


//
// CONTROL PANEL
//


//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             key;
    int             i;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    // [JN] Disable menu left/right controls by mouse movement.
    /*
    static  int     mousex = 0;
    static  int     lastx = 0;
    */

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (ev->type == ev_quit
        || (ev->type == ev_keydown
        && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // [JN] CRL - optionally don’t ask for quit confirmation.
    if (!crl_confirm_quit)
    {
        if (ev->type == ev_quit || (ev->type == ev_keydown && ev->data1 == key_menu_quit))
        {
            I_Quit();
            return true;
        }
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit

        if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
        {
            M_QuitResponse(key_menu_confirm);
        }
        else
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitDOOM(0);
        }

        return true;
    }

    // key is the key pressed, ch is the actual character typed

    ch = 0;
    key = -1;

    if (ev->type == ev_joystick)
    {
        // Simulate key presses from joystick events to interact with the menu.

        if (ev->data3 < 0)
        {
            key = key_menu_up;
            joywait = I_GetTime() + 5;
        }
        else if (ev->data3 > 0)
        {
            key = key_menu_down;
            joywait = I_GetTime() + 5;
        }

        if (ev->data2 < 0)
        {
            key = key_menu_left;
            joywait = I_GetTime() + 2;
        }
        else if (ev->data2 > 0)
        {
            key = key_menu_right;
            joywait = I_GetTime() + 2;
        }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

        if (JOY_BUTTON_PRESSED(joybfire))
        {
            // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_confirm;
            }
            // Simulate pressing "Enter" when we are supplying a save slot name
            else if (saveStringEnter)
            {
                key = KEY_ENTER;
            }
            else
            {
                // if selecting a save slot via joypad, set a flag
                if (currentMenu == &SaveDef)
                {
                    joypadSave = true;
                }
                key = key_menu_forward;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybuse))
        {
            // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
            if (messageToPrint && messageNeedsInput)
            {
                key = key_menu_abort;
            }
            // If user was entering a save name, back out
            else if (saveStringEnter)
            {
                key = KEY_ESCAPE;
            }
            else
            {
                key = key_menu_back;
            }
            joywait = I_GetTime() + 5;
        }
        if (JOY_BUTTON_PRESSED(joybmenu))
        {
            key = key_menu_activate;
            joywait = I_GetTime() + 5;
        }
    }
    else
    {
        const int item_status = currentMenu->menuitems[itemOn].status;

        // [JN] Shows the mouse cursor when moved.
        if (ev->data2 || ev->data3)
        {
            menu_mouse_allow = true;
            menu_mouse_allow_click = false;
        }

        if (ev->type == ev_mouse && mousewait < I_GetTime())
        {
            // [crispy] novert disables controlling the menus with the mouse
            // [JN] Not needed, as menu is fully controllable by mouse wheel and buttons.
            /*
            if (!novert)
            {
                mousey += ev->data3;
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

            // [JN] Disable menu left/right controls by mouse movement.
            /*
            mousex += ev->data2;
            if (mousex < lastx-30)
            {
                key = key_menu_left;
                mousewait = I_GetTime() + 5;
                mousex = lastx -= 30;
            }
            else if (mousex > lastx+30)
            {
                key = key_menu_right;
                mousewait = I_GetTime() + 5;
                mousex = lastx += 30;
            }
            */

            // [JN] Handle mouse bindings before going any farther.
            // Catch only button pressing events, i.e. ev->data1.
            if (MouseIsBinding && ev->data1 && !ev->data2 && !ev->data3)
            {
                M_CheckMouseBind(SDL_mouseButton);
                M_DoMouseBind(btnToBind, SDL_mouseButton);
                btnToBind = 0;
                MouseIsBinding = false;
                mousewait = I_GetTime() + 5;
                return true;
            }

            if (ev->data1 & 1)
            {
                if (menuactive && item_status == STS_SLDR)
                {
                    // [JN] Allow repetitive on sliders to move it while mouse movement.
                    menu_mouse_allow_click = true;
                }
                else
                if (!ev->data2 && !ev->data3) // [JN] Do not consider movement as pressing.
                {
                    if (!menuactive && !usergame && !demorecording)
                    {
                        M_StartControlPanel();  // [JN] Open the main menu if the game is not active.
                    }
                    else
                    if (messageToPrint && messageNeedsInput)
                    {
                        key = key_menu_confirm;  // [JN] Confirm by left mouse button.
                    }
                    else
                    {
                        key = key_menu_forward;
                    }
                    mousewait = I_GetTime() + 5;
                }
            }

            if (ev->data1 & 2
            && !ev->data2 && !ev->data3)  // [JN] Do not consider movement as pressing.
            {
                if (!menuactive && !usergame && !demorecording)
                {
                    M_StartControlPanel();  // [JN] Open the main menu if the game is not active.
                }
                else
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_abort;  // [JN] Cancel by right mouse button.
                }
                else
                if (saveStringEnter)
                {
                    key = key_menu_abort;
                    saveStringEnter = 0;
                    M_ReadSaveStrings();  // [JN] Reread save strings after cancelation.
                }
                else
                {
                    key = key_menu_back;
                }
                mousewait = I_GetTime() + 1;
            }

            // [JN] Scrolls through menu item values or navigates between pages.
            if (ev->data1 & (1 << 4) && menuactive)  // Wheel down
            {
                if (itemOn == -1
                || (currentMenu->ScrollAR && !saveStringEnter && !KbdIsBinding))
                {
                    M_ScrollPages(1);
                }
                else
                if (item_status > 1)
                {
                    // Scroll menu item backward normally, or forward for STS_MUL2
                    currentMenu->menuitems[itemOn].routine(item_status != STS_MUL2 ? 0 : 1);
                    S_StartSound(NULL, sfx_stnmov);
                }
                mousewait = I_GetTime();
            }
            else
            if (ev->data1 & (1 << 3) && menuactive)  // Wheel up
            {
                if (itemOn == -1
                || (currentMenu->ScrollAR && !saveStringEnter && !KbdIsBinding))
                {
                    M_ScrollPages(0);
                }
                else
                if (item_status > 1)
                {
                    // Scroll menu item forward normally, or backward for STS_MUL2
                    currentMenu->menuitems[itemOn].routine(item_status != STS_MUL2 ? 1 : 0);
                    S_StartSound(NULL,sfx_stnmov);
                }
                mousewait = I_GetTime();
            }
        }
        else
        {
            if (ev->type == ev_keydown)
            {
                key = ev->data1;
                ch = ev->data2;
                // [JN] Hide mouse cursor by pressing a key.
                menu_mouse_allow = false;
            }
        }
    }

    if (key == -1)
        return false;

    // Save Game string input
    if (saveStringEnter)
    {
        switch(key)
        {
            case KEY_BACKSPACE:
                if (saveCharIndex > 0)
                {
                    saveCharIndex--;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;

            case KEY_ESCAPE:
                saveStringEnter = 0;
                I_StopTextInput();
                M_StringCopy(savegamestrings[saveSlot], saveOldString, SAVESTRINGSIZE);
                break;

            case KEY_ENTER:
                saveStringEnter = 0;
                I_StopTextInput();
                if (savegamestrings[saveSlot][0])
                    M_DoSave(saveSlot);
                break;

            default:
                // Savegame name entry. This is complicated.
                // Vanilla has a bug where the shift key is ignored when entering
                // a savegame name. If vanilla_keyboard_mapping is on, we want
                // to emulate this bug by using ev->data1. But if it's turned off,
                // it implies the user doesn't care about Vanilla emulation:
                // instead, use ev->data3 which gives the fully-translated and
                // modified key input.

                if (ev->type != ev_keydown)
                {
                    break;
                }
                if (vanilla_keyboard_mapping)
                {
                    ch = ev->data1;
                }
                else
                {
                    ch = ev->data3;
                }

                ch = toupper(ch);

                if (ch != ' '
                && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE))
                {
                    break;
                }

                if (ch >= 32 && ch <= 127
                && saveCharIndex < SAVESTRINGSIZE - 1
                && M_StringWidth(savegamestrings[saveSlot]) < (SAVESTRINGSIZE - 2) * 8)
                {
                    savegamestrings[saveSlot][saveCharIndex++] = ch;
                    savegamestrings[saveSlot][saveCharIndex] = 0;
                }
                break;
        }
        return true;
    }

    // Take care of any messages that need input
    if (messageToPrint)
    {
        if (messageNeedsInput)
        {
             // [JN] Allow to exclusevely confirm quit game by pressing F10 again.
            if (key == key_menu_quit && messageRoutine == M_QuitResponse)
            {
                I_Quit ();
                return true;
            }

            if (key != ' ' && key != KEY_ESCAPE
            &&  key != key_menu_confirm && key != key_menu_abort
            // [JN] Allow to confirm nightmare, end game and quit by pressing Enter.
            && key != key_menu_forward)
            {
                return false;
            }
        }

        menuactive = messageLastMenuActive;
        messageFillBG = false;
        messageToPrint = 0;
        if (messageRoutine)
            messageRoutine(key);

        // [JN] Do not close Save/Load menu after deleting a savegame.
        if (currentMenu != &SaveDef && currentMenu != &LoadDef
        // [JN] Do not close Options menu after pressing "N" in End Game.
        &&  currentMenu != &CRLDef_Main
        // [JN] Do not close bindings menu keyboard/mouse binds reset.
        &&  currentMenu != &CRLDef_Keybinds_7 && currentMenu != &CRLDef_MouseBinds)
        {
            menuactive = false;
        }
        S_StartSound(NULL, sfx_swtchx);
        return true;
    }

    // [JN] Handle keyboard bindings:
    if (KbdIsBinding)
    {
        if (ev->type == ev_mouse)
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
        if (ev->type != ev_mouse)
        {
            btnToBind = 0;
            MouseIsBinding = false;
            return false;
        }
    }

    if ((devparm && key == key_menu_help)
    || (key != 0 && key == key_menu_screenshot))
    {
        S_StartSound(NULL, sfx_tink);    // [JN] Add audible feedback
        G_ScreenShot();
        return true;
    }

    // F-Keys
    if (!menuactive && !chatmodeon)
    {
        if (key == key_menu_decscreen)      // Screen size down
        {
            if (automapactive)
                return false;
            M_SizeDisplay(0);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_incscreen) // Screen size up
        {
            if (automapactive)
                return false;
            M_SizeDisplay(1);
            S_StartSound(NULL, sfx_stnmov);
            return true;
        }
        else if (key == key_menu_help)     // Help key
        {
            M_StartControlPanel ();

            if (gameversion >= exe_ultimate)
              currentMenu = &ReadDef2;
            else
              currentMenu = &ReadDef1;

            itemOn = 0;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_save)     // Save
        {
            M_StartControlPanel();
            S_StartSound(NULL, sfx_swtchn);
            M_SaveGame(0);
            return true;
        }
        else if (key == key_menu_load)     // Load
        {
            M_StartControlPanel();
            S_StartSound(NULL, sfx_swtchn);
            M_LoadGame(0);
            return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
            M_StartControlPanel ();
            currentMenu = &SoundDef;
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_detail)   // Detail toggle
        {
            M_ChangeDetail(0);
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qsave)    // Quicksave
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuickSave();
            return true;
        }
        else if (key == key_menu_endgame)  // End game
        {
            S_StartSound(NULL, sfx_swtchn);
            M_EndGame(0);
            return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
            M_ChangeMessages(0);
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        else if (key == key_menu_qload)    // Quickload
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuickLoad();
            return true;
        }
        else if (key == key_menu_quit)     // Quit DOOM
        {
            S_StartSound(NULL, sfx_swtchn);
            M_QuitDOOM(0);
            return true;
        }
        // [crispy] those two can be considered as shortcuts for the IDCLEV cheat
        // and should be treated as such, i.e. add "if (!netgame)"
        // [JN] Hovewer, allow while multiplayer demos.
        else if ((!netgame || netdemo) && key != 0 && key == key_crl_reloadlevel)
        {
            if (demoplayback)
            {
                if (demowarp)
                {
                    // [JN] Enable screen render back before replaying.
                    nodrawers = false;
                    singletics = false;
                }
                // [JN] Replay demo lump or file.
                G_DoPlayDemo();
                return true;
            }
            else
            if (G_ReloadLevel())
            return true;
        }
        else if ((!netgame || netdemo) && key != 0 && key == key_crl_nextlevel)
        {
            if (demoplayback)
            {
                // [JN] Go to next level.
                demo_gotonextlvl = true;
                G_DemoGoToNextLevel(true);
                return true;
            }
            else
            if (G_GotoNextLevel())
            return true;
        }
    }

    // [JN] Allow to change gamma while active menu.
    if (key == key_menu_gamma)    // gamma toggle
    {
        crl_gamma = M_INT_Slider(crl_gamma, 0, 14, 1 /*right*/, false);
        CRL_SetMessage(&players[consoleplayer], DEH_String(gammalvls[crl_gamma][0]), false, NULL);
        CRL_ReloadPalette();
        return true;
    }

    // Pop-up menu?
    if (!menuactive)
    {
        if (key == key_menu_activate || key == key_crl_menu)
        {
            M_StartControlPanel ();
            
            // RestlessRodent - Spawn CRL menu
            if (key == key_crl_menu)
            {
                M_SetupNextMenu(&CRLDef_Main);
            }
            
            S_StartSound(NULL, sfx_swtchn);
            return true;
        }
        return false;
    }
    else
    {
        // [JN] Deactivate CRL menu by pressing ~ key again.
        if (key == key_crl_menu)
        {
            currentMenu->lastOn = itemOn;
            M_ClearMenus();
            S_StartSound(NULL, sfx_swtchx);
            return true;
        }
    }

    // Keys usable within menu

    if (key == key_menu_down)
    {
        // [PN] Move down to the next available item
        do
        {
            itemOn = (itemOn + 1) % currentMenu->numitems;
        } while (currentMenu->menuitems[itemOn].status == STS_SKIP);

        S_StartSound(NULL, sfx_pstop);
        return true;
    }
    else if (key == key_menu_up)
    {
        // [JN] Current menu item was hidden while mouse controls,
        // so move cursor to last one menu item by pressing "up" key.
        if (itemOn == -1)
        {
            itemOn = currentMenu->numitems;
        }

        // [PN] Move back up to the previous available item
        do
        {
            itemOn = (itemOn == 0) ? currentMenu->numitems - 1 : itemOn - 1;
        } while (currentMenu->menuitems[itemOn].status == STS_SKIP);

        S_StartSound(NULL, sfx_pstop);
        return true;
    }
    else if (key == key_menu_activate)
    {
        // [JN] If ESC key behaviour is set to "go back":
        if (crl_menu_esc_key)
        {
            if (currentMenu == &MainDef || currentMenu == &SoundDef
            ||  currentMenu == &LoadDef || currentMenu == &SaveDef)
            {
                goto crl_close_menu;  // [JN] Close menu imideatelly.
            }
            else
            {
                goto crl_prev_menu;   // [JN] Go to previous menu.
            }
        }
        else
        {
            crl_close_menu:
            // Deactivate menu
            currentMenu->lastOn = itemOn;
            M_ClearMenus();
            S_StartSound(NULL, sfx_swtchx);
        }
        return true;
    }
    else if (key == key_menu_back)
    {
        crl_prev_menu:
        // Go back to previous menu

        currentMenu->lastOn = itemOn;
        if (currentMenu->prevMenu)
        {
            currentMenu = currentMenu->prevMenu;
            M_Reset_Line_Glow();
            itemOn = currentMenu->lastOn;
            S_StartSound(NULL, sfx_swtchn);
            // [JN] Update mouse cursor position.
            M_ID_MenuMouseControl();
        }
        // [JN] Close menu if pressed "back" in Doom main or CRL main menu.
        else
        if (currentMenu == &MainDef || currentMenu == &CRLDef_Main)
        {
            S_StartSound(NULL, sfx_swtchx);
            M_ClearMenus();
        }
        return true;
    }
    else if (key == key_menu_left)
    {
        // [JN] Go to previous-left menu by pressing Left Arrow.
        if (currentMenu->ScrollAR || itemOn == -1)
        {
            M_ScrollPages(false);
            return true;
        }
        // Slide slider left

        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status > STS_SWTC)
        {
            S_StartSound(NULL,sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(0);
        }
        return true;
    }
    else if (key == key_menu_right)
    {
        // [JN] Go to next-right menu by pressing Right Arrow.
        if (currentMenu->ScrollAR || itemOn == -1)
        {
            M_ScrollPages(true);
            return true;
        }
        // Slide slider right

        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status > STS_SWTC)
        {
            S_StartSound(NULL,sfx_stnmov);
            currentMenu->menuitems[itemOn].routine(1);
        }
        return true;
    }
    else if (key == key_menu_forward && itemOn != -1)
    {
        // Activate menu item

        if (currentMenu->menuitems[itemOn].routine
        &&  currentMenu->menuitems[itemOn].status)
        {
            currentMenu->lastOn = itemOn;
            if (currentMenu->menuitems[itemOn].status == STS_MUL1
            ||  currentMenu->menuitems[itemOn].status == STS_MUL2)
            {
                currentMenu->menuitems[itemOn].routine(1);      // right arrow
                S_StartSound(NULL, sfx_stnmov);
            }
            else
            {
                currentMenu->menuitems[itemOn].routine(itemOn);
                S_StartSound(NULL, sfx_pistol);
            }
        }
        return true;
    }
    // [crispy] delete a savegame
    else if (key == key_menu_del)
    {
        if (currentMenu == &LoadDef || currentMenu == &SaveDef)
        {
            if (LoadMenu[itemOn].status)
            {
                currentMenu->lastOn = itemOn;
                M_ConfirmDeleteGame();
                return true;
            }
            else
            {
                return true;
            }
        }
        // [JN] ...or clear key bind.
        else
        if (currentMenu == &CRLDef_Keybinds_1 || currentMenu == &CRLDef_Keybinds_2
        ||  currentMenu == &CRLDef_Keybinds_3 || currentMenu == &CRLDef_Keybinds_4
        ||  currentMenu == &CRLDef_Keybinds_5 || currentMenu == &CRLDef_Keybinds_6
        ||  currentMenu == &CRLDef_Keybinds_7)
        {
            M_ClearBind(itemOn);
            return true;
        }
        // [JN] ...or clear mouse bind.
        else
        if (currentMenu == &CRLDef_MouseBinds)
        {
            M_ClearMouseBind(itemOn);
            return true;
        }
    }
    // [JN] Go to previous-left menu by pressing Page Up key.
    else if (key == KEY_PGUP)
    {
        if (currentMenu->ScrollPG)
        {
            M_ScrollPages(false);
        }
    }
    // [JN] Go to next-right menu by pressing Page Down key.
    else if (key == KEY_PGDN)
    {
        if (currentMenu->ScrollPG)
        {
            M_ScrollPages(true);
        }
    }

    // Keyboard shortcut?
    // Vanilla Doom has a weird behavior where it jumps to the scroll bars
    // when the certain keys are pressed, so emulate this.

    else if (ch != 0 || IsNullKey(key))
    {
        for (i = itemOn+1;i < currentMenu->numitems;i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }

        for (i = 0;i <= itemOn;i++)
        {
            if (currentMenu->menuitems[i].alphaKey == ch)
            {
                itemOn = i;
                S_StartSound(NULL, sfx_pstop);
                return true;
            }
        }
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;   // JDC
    // [JN] Disallow menu items highlighting initially to prevent
    // cursor jumping. It will be allowed by mouse movement.
    menu_mouse_allow = false;
}

static void M_ID_MenuMouseControl (void)
{
    if (!menu_mouse_allow || KbdIsBinding || MouseIsBinding)
    {
        // [JN] Skip hovering if the cursor is disabled/hidden or a binding is active.
        return;
    }
    else
    {
        // [JN] Which line height should be used?
        const int line_height = currentMenu->smallFont ? CRL_MENU_LINEHEIGHT_SMALL : LINEHEIGHT;

        // [JN] Reset current menu item, it will be set in a cycle below.
        itemOn = -1;

        // [PN] Check if the cursor is hovering over a menu item
        for (int i = 0; i < currentMenu->numitems; i++)
        {
            // [JN] Slider takes three lines.
            const int line_item = currentMenu->menuitems[i].status == STS_SLDR ? 3 : 1;

            if (menu_mouse_x >= (currentMenu->x)
            &&  menu_mouse_x <= (SCREENWIDTH - currentMenu->x)
            &&  menu_mouse_y >= (currentMenu->y + i * line_height)
            &&  menu_mouse_y <= (currentMenu->y + (i + line_item) * line_height)
            &&  currentMenu->menuitems[i].status != -1)
            {
                // [PN] Highlight the current menu item
                itemOn = i;
            }
        }
    }
}

static void M_ID_HandleSliderMouseControl (int x, int y, int width, void *value, boolean is_float, float min, float max)
{
    if (!menu_mouse_allow_click)
        return;
    
    // [PN] Check cursor position and item status
    if (menu_mouse_x < x || menu_mouse_x > x + width
    ||  menu_mouse_y < y || menu_mouse_y > y + LINEHEIGHT
    ||  currentMenu->menuitems[itemOn].status != STS_SLDR)
        return;

    {
    // [PN] Calculate and update slider value
    const float normalized = (float)(menu_mouse_x - x + 5) / width;
    const float newValue = min + normalized * (max - min);
    if (is_float)
        *((float *)value) = newValue;
    else
        *((int *)value) = (int)newValue;
    // [JN/PN] Call related routine and reset mouse click allowance
    currentMenu->menuitems[itemOn].routine(-1);
    menu_mouse_allow_click = false;
    // Play sound
    S_StartSound(NULL, sfx_stnmov);
    }
}

// Display OPL debug messages - hack for GENMIDI development.

static void M_DrawOPLDev(void)
{
    char debug[1024];
    char *curr, *p;
    int line;

    I_OPL_DevMessages(debug, sizeof(debug));
    curr = debug;
    line = 0;

    for (;;)
    {
        p = strchr(curr, '\n');

        if (p != NULL)
        {
            *p = '\0';
        }

        M_WriteText(0, line * 8, curr, NULL);
        ++line;

        if (p == NULL)
        {
            break;
        }

        curr = p + 1;
    }
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
    static short	x;
    static short	y;
    unsigned int	i;
    unsigned int	max;
    char		string[80];
    const char *name;
    int			start;

    // [JN] Shade background while active menu.
    if (menuactive)
    {
        // Temporary unshade while changing certain settings.
        if (shade_wait < I_GetTime())
        {
            M_ShadeBackground();
        }
        // Always redraw status bar background.
        st_fullupdate = true;
    }

    // [JN] Certain messages (binds reset) needs background filling.
    if (messageFillBG)
    {
        M_FillBackground();
    }

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = SCREENHEIGHT/2 - M_StringHeight(messageString) / 2;
	while (messageString[start] != '\0')
	{
	    int foundnewline = 0;

            for (i = 0; i < strlen(messageString + start); i++)
            {
                if (messageString[start + i] == '\n')
                {
                    M_StringCopy(string, messageString + start,
                                 sizeof(string));
                    if (i < sizeof(string))
                    {
                        string[i] = '\0';
                    }

                    foundnewline = 1;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, messageString + start, sizeof(string));
                start += strlen(string);
            }

	    x = SCREENWIDTH/2 - M_StringWidth(string) / 2;
	    M_WriteText(x, y, string, NULL);
	    y += SHORT(hu_font[0]->height);
	}

	return;
    }

    if (opldev)
    {
        M_DrawOPLDev();
    }

    if (!menuactive)
    {
	menu_mouse_allow = false;  // [JN] Disallow cursor if menu is not active.
	return;
    }

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    if (currentMenu->smallFont)
    {
        // [JN] Draw glowing * symbol.
        if (itemOn != -1)
        M_WriteText(x - CRL_MENU_CURSOR_OFFSET, y + itemOn * CRL_MENU_LINEHEIGHT_SMALL, "*",
                    M_Cursor_Glow(cursor_tics));

        for (i = 0 ; i < max ; i++)
        {
            M_WriteText (x, y, currentMenu->menuitems[i].name,
                         M_Small_Line_Glow(currentMenu->menuitems[i].tics));
            y += CRL_MENU_LINEHEIGHT_SMALL;
        }
    }
    else
    {
        // DRAW SKULL
        if (itemOn != -1)
        V_DrawShadowedPatch(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
                            W_CacheLumpName(DEH_String(skullName[whichSkull]), PU_CACHE),
                            DEH_String(skullName[whichSkull]));

        for (i = 0 ; i < max ; i++)
        {
            name = DEH_String(currentMenu->menuitems[i].name);

            if (name[0])
            {
                dp_translation = M_Big_Line_Glow(currentMenu->menuitems[i].tics);
                V_DrawShadowedPatch(x, y, W_CacheLumpName(name, PU_CACHE), name);
                dp_translation = NULL;
            }
            y += LINEHEIGHT;
        }
    }

    // [JN] Call the menu control routine for mouse input.
    M_ID_MenuMouseControl();
}


//
// M_ClearMenus
//
static void M_ClearMenus (void)
{
    menuactive = 0;
    menu_mouse_allow = false;  // [JN] Hide cursor on closing menu.
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    M_Reset_Line_Glow();
    itemOn = currentMenu->lastOn;
    // [JN] Update mouse cursor position.
    M_ID_MenuMouseControl();
}


//
// M_Ticker
//
void M_Ticker (void)
{
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
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
    // Keep menu item bright or decrease tics for fading effect.
    for (int i = 0 ; i < currentMenu->numitems ; i++)
    {
        if (crl_menu_highlight == 1)
        {
            currentMenu->menuitems[i].tics =
                (itemOn == i) ? 5 : 0;
        }
        else
        if (crl_menu_highlight == 2)
        {
            currentMenu->menuitems[i].tics = (itemOn == i) ? 5 :
                (currentMenu->menuitems[i].tics > 0 ? currentMenu->menuitems[i].tics - 1 : 0);
        }
        else
        {
            currentMenu->menuitems[i].tics = 0;
        }
    }

    // [JN] Make KIS/time widgets translucent while in active Save/Load menu.
    savemenuactive = (menuactive && !messageToPrint
                  && (currentMenu == &SaveDef || currentMenu == &LoadDef));
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    menuactive = 0;
    itemOn = currentMenu->lastOn;
    whichSkull = 0;
    skullAnimCounter = 10;
    messageToPrint = 0;
    messageString = NULL;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;

    // [JN] CRL - player is always local, "console" player.
    player = &players[consoleplayer];

    // [JN] CRL - set cursor position in skill menu to default skill level.
    NewDef.lastOn = crl_default_skill;

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    // The same hacks were used in the original Doom EXEs.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
    }

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate)
    {
        EpiDef.numitems--;
    }
    // chex.exe shows only one episode.
    else if (gameversion == exe_chex)
    {
        EpiDef.numitems = 1;
    }

    // [JN] Older verions of Doom Shareware does not have M_NMARE patch,
    // so Nightmare is not available (at least in game menu).
    if (W_CheckNumForName(DEH_String("M_NMARE")) < 0)
    {
        NewDef.numitems--;
    }

    opldev = M_CheckParm("-opldev") > 0;
}

// [crispy] delete a savegame
static char *savegwarning;
static void M_ConfirmDeleteGameResponse (int key)
{
	free(savegwarning);

	if (key == key_menu_confirm)
	{
		char name[256];

		M_StringCopy(name, P_SaveGameFile(itemOn), sizeof(name));
		remove(name);

		if (itemOn == quickSaveSlot)
			quickSaveSlot = -1;

		M_ReadSaveStrings();
	}
}

void M_ConfirmDeleteGame (void)
{
	savegwarning =
	M_StringJoin("delete savegame\n\n", "\"", savegamestrings[itemOn], "\"", " ?\n\n",
	             PRESSYN, NULL);

	M_StartMessage(savegwarning, M_ConfirmDeleteGameResponse, true);
	messageToPrint = 2;
	S_StartSound(NULL,sfx_swtchn);
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
} key_names[] = KEY_NAMES_ARRAY;

static char *M_NameBind (int itemSetOn, int key)
{
    if (itemOn == itemSetOn && KbdIsBinding)
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
    if (key_crl_menu == key)         key_crl_menu         = 0;
    if (key_crl_reloadlevel == key)  key_crl_reloadlevel  = 0;
    if (key_crl_nextlevel == key)    key_crl_nextlevel    = 0;
    if (key_crl_demospeed == key)    key_crl_demospeed    = 0;
    if (key_crl_extendedhud == key)  key_crl_extendedhud  = 0;
    if (key_crl_spectator == key)    key_crl_spectator    = 0;
    if (key_crl_cameraup == key)     key_crl_cameraup     = 0;
    if (key_crl_cameradown == key)   key_crl_cameradown   = 0;
    if (key_crl_cameramoveto == key) key_crl_cameramoveto = 0;
    if (key_crl_freeze == key)       key_crl_freeze       = 0;
    if (key_crl_buddha == key)       key_crl_buddha       = 0;
    if (key_crl_notarget == key)     key_crl_notarget     = 0;
    if (key_crl_nomomentum == key)   key_crl_nomomentum   = 0;
    // Page 3
    if (key_crl_autorun == key)      key_crl_autorun      = 0;
    if (key_crl_novert == key)       key_crl_novert       = 0;
    if (key_crl_vilebomb == key)     key_crl_vilebomb     = 0;
    if (key_crl_vilefly == key)      key_crl_vilefly      = 0;
    if (key_crl_clearmax == key)     key_crl_clearmax     = 0;
    if (key_crl_movetomax == key)    key_crl_movetomax    = 0;
    if (key_crl_iddqd == key)        key_crl_iddqd        = 0;
    if (key_crl_idkfa == key)        key_crl_idkfa        = 0;
    if (key_crl_idfa == key)         key_crl_idfa         = 0;
    if (key_crl_idclip == key)       key_crl_idclip       = 0;
    if (key_crl_iddt == key)         key_crl_iddt         = 0;
    if (key_crl_mdk == key)          key_crl_mdk          = 0;
    // Page 4
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
    // Page 5
    if (key_map_toggle == key)       key_map_toggle       = 0;    
    // Do not override Automap binds in other pages.
    if (currentMenu == &CRLDef_Keybinds_5)
    {
        if (key_map_zoomin == key)       key_map_zoomin       = 0;
        if (key_map_zoomout == key)      key_map_zoomout      = 0;
        if (key_map_maxzoom == key)      key_map_maxzoom      = 0;
        if (key_map_follow == key)       key_map_follow       = 0;
        if (key_crl_map_rotate == key)   key_crl_map_rotate   = 0;
        if (key_crl_map_overlay == key)  key_crl_map_overlay  = 0;
        if (key_crl_map_sndprop == key)  key_crl_map_sndprop  = 0;
        if (key_map_grid == key)         key_map_grid         = 0;
        if (key_map_mark == key)         key_map_mark         = 0;
        if (key_map_clearmark == key)    key_map_clearmark    = 0;
    }
    // Page 6
    if (key_menu_help == key)        key_menu_help        = 0;
    if (key_menu_save == key)        key_menu_save        = 0;
    if (key_menu_load == key)        key_menu_load        = 0;
    if (key_menu_volume == key)      key_menu_volume      = 0;
    if (key_menu_detail == key)      key_menu_detail      = 0;
    if (key_menu_qsave == key)       key_menu_qsave       = 0;
    if (key_menu_endgame == key)     key_menu_endgame     = 0;
    if (key_menu_messages == key)    key_menu_messages    = 0;
    if (key_menu_qload == key)       key_menu_qload       = 0;
    if (key_menu_quit == key)        key_menu_quit        = 0;
    if (key_menu_gamma == key)       key_menu_gamma       = 0;
    if (key_spy == key)              key_spy              = 0;
    // Page 7
    if (key_pause == key)            key_pause            = 0;
    if (key_menu_screenshot == key)  key_menu_screenshot  = 0;
    if (key_message_refresh == key)  key_message_refresh  = 0;
    if (key_demo_quit == key)        key_demo_quit        = 0;
    if (key_multi_msg == key)        key_multi_msg        = 0;
    // Do not override Send To binds in other pages.
    if (currentMenu == &CRLDef_Keybinds_7)
    {
        if (key_multi_msgplayer[0] == key) key_multi_msgplayer[0] = 0;
        if (key_multi_msgplayer[1] == key) key_multi_msgplayer[1] = 0;
        if (key_multi_msgplayer[2] == key) key_multi_msgplayer[2] = 0;
        if (key_multi_msgplayer[3] == key) key_multi_msgplayer[3] = 0;
    }
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
        case 200:  key_crl_menu = key;          break;
        case 201:  key_crl_reloadlevel = key;   break;
        case 202:  key_crl_nextlevel = key;     break;
        case 203:  key_crl_demospeed = key;     break;
        case 204:  key_crl_extendedhud = key;   break;
        case 205:  key_crl_spectator = key;     break;
        case 206:  key_crl_cameraup = key;      break;
        case 207:  key_crl_cameradown = key;    break;
        case 208:  key_crl_cameramoveto = key;  break;
        case 209:  key_crl_freeze = key;        break;
        case 210:  key_crl_buddha = key;        break;
        case 211:  key_crl_notarget = key;      break;
        case 212:  key_crl_nomomentum = key;    break;
        // Page 3  
        case 300:  key_crl_autorun = key;       break;
        case 301:  key_crl_novert = key;        break;
        case 302:  key_crl_vilebomb = key;      break;
        case 303:  key_crl_vilefly = key;       break;
        case 304:  key_crl_clearmax = key;      break;
        case 305:  key_crl_movetomax = key;     break;
        case 306:  key_crl_iddqd = key;         break;
        case 307:  key_crl_idkfa = key;         break;
        case 308:  key_crl_idfa = key;          break;
        case 309:  key_crl_idclip = key;        break;
        case 310:  key_crl_iddt = key;          break;
        case 311:  key_crl_mdk = key;           break;
        // Page 4  
        case 400:  key_weapon1 = key;           break;
        case 401:  key_weapon2 = key;           break;
        case 402:  key_weapon3 = key;           break;
        case 403:  key_weapon4 = key;           break;
        case 404:  key_weapon5 = key;           break;
        case 405:  key_weapon6 = key;           break;
        case 406:  key_weapon7 = key;           break;
        case 407:  key_weapon8 = key;           break;
        case 408:  key_prevweapon = key;        break;
        case 409:  key_nextweapon = key;        break;
        // Page 5  
        case 500:  key_map_toggle = key;        break;
        case 501:  key_map_zoomin = key;        break;
        case 502:  key_map_zoomout = key;       break;
        case 503:  key_map_maxzoom = key;       break;
        case 504:  key_map_follow = key;        break;
        case 505:  key_crl_map_rotate = key;    break;
        case 506:  key_crl_map_overlay = key;   break;
        case 507:  key_crl_map_sndprop = key;   break;
        case 508:  key_map_grid = key;          break;
        case 509:  key_map_mark = key;          break;
        case 510:  key_map_clearmark = key;     break;
        // Page 6  
        case 600:  key_menu_help = key;         break;
        case 601:  key_menu_save = key;         break;
        case 602:  key_menu_load = key;         break;
        case 603:  key_menu_volume = key;       break;
        case 604:  key_menu_detail = key;       break;
        case 605:  key_menu_qsave = key;        break;
        case 606:  key_menu_endgame = key;      break;
        case 607:  key_menu_messages = key;     break;
        case 608:  key_menu_qload = key;        break;
        case 609:  key_menu_quit = key;         break;
        case 610:  key_menu_gamma = key;        break;
        case 611:  key_spy = key;               break;
        // Page 7
        case 700:  key_pause = key;             break;
        case 701:  key_menu_screenshot = key;   break;
        case 702:  key_message_refresh = key;   break;
        case 703:  key_demo_quit = key;         break;
        case 704:  key_multi_msg = key;         break;
        case 705:  key_multi_msgplayer[0] = key;  break;
        case 706:  key_multi_msgplayer[1] = key;  break;
        case 707:  key_multi_msgplayer[2] = key;  break;
        case 708:  key_multi_msgplayer[3] = key;  break;
    }
}

// -----------------------------------------------------------------------------
// M_ClearBind
//  [JN] Clear key bind on the line where cursor is placed (itemOn).
// -----------------------------------------------------------------------------

static void M_ClearBind (int itemOn)
{
    if (currentMenu == &CRLDef_Keybinds_1)
    {
        switch (itemOn)
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
    if (currentMenu == &CRLDef_Keybinds_2)
    {
        switch (itemOn)
        {
            case 0:   key_crl_menu = 0;         break;
            case 1:   key_crl_reloadlevel = 0;  break;
            case 2:   key_crl_nextlevel = 0;    break;
            case 3:   key_crl_demospeed = 0;    break;
            case 4:   key_crl_extendedhud = 0;  break;
            // Game modes title
            case 6:   key_crl_spectator = 0;    break;
            case 7:   key_crl_cameraup = 0;     break;
            case 8:   key_crl_cameradown = 0;   break;
            case 9:   key_crl_cameramoveto = 0; break;
            case 10:  key_crl_freeze = 0;       break;
            case 11:  key_crl_buddha = 0;       break;
            case 12:  key_crl_notarget = 0;     break;
            case 13:  key_crl_nomomentum = 0;   break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_3)
    {
        switch (itemOn)
        {
            case 0:   key_crl_autorun = 0;      break;
            case 1:   key_crl_novert = 0;       break;
            case 2:   key_crl_vilebomb = 0;     break;
            case 3:   key_crl_vilefly = 0;      break;
            // Visplanes MAX value title
            case 5:   key_crl_clearmax = 0;     break;
            case 6:   key_crl_movetomax = 0;    break;
            // Cheat shortcuts title
            case 8:   key_crl_iddqd = 0;        break;
            case 9:   key_crl_idkfa = 0;        break;
            case 10:  key_crl_idfa = 0;         break;
            case 11:  key_crl_idclip = 0;       break;
            case 12:  key_crl_iddt = 0;         break;
            case 13:  key_crl_mdk = 0;          break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_4)
    {
        switch (itemOn)
        {
            case 0:   key_weapon1 = 0;          break;
            case 1:   key_weapon2 = 0;          break;
            case 2:   key_weapon3 = 0;          break;
            case 3:   key_weapon4 = 0;          break;
            case 4:   key_weapon5 = 0;          break;
            case 5:   key_weapon6 = 0;          break;
            case 6:   key_weapon7 = 0;          break;
            case 7:   key_weapon8 = 0;          break;
            case 8:   key_prevweapon = 0;       break;
            case 9:   key_nextweapon = 0;       break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_5)
    {
        switch (itemOn)
        {
            case 0:   key_map_toggle = 0;       break;
            case 1:   key_map_zoomin = 0;       break;
            case 2:   key_map_zoomout = 0;      break;
            case 3:   key_map_maxzoom = 0;      break;
            case 4:   key_map_follow = 0;       break;
            case 5:   key_crl_map_rotate = 0;   break;
            case 6:   key_crl_map_overlay = 0;  break;
            case 7:   key_crl_map_sndprop = 0;  break;
            case 8:   key_map_grid = 0;         break;
            case 9:   key_map_mark = 0;         break;
            case 10:  key_map_clearmark = 0;    break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_6)
    {
        switch (itemOn)
        {
            case 0:   key_menu_help = 0;        break;
            case 1:   key_menu_save = 0;        break;
            case 2:   key_menu_load = 0;        break;
            case 3:   key_menu_volume = 0;      break;
            case 4:   key_menu_detail = 0;      break;
            case 5:   key_menu_qsave = 0;       break;
            case 6:   key_menu_endgame = 0;     break;
            case 7:   key_menu_messages = 0;    break;
            case 8:   key_menu_qload = 0;       break;
            case 9:   key_menu_quit = 0;        break;
            case 10:  key_menu_gamma = 0;       break;
            case 11:  key_spy = 0;              break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_7)
    {
        switch (itemOn)
        {
            case 0:   key_pause = 0;            break;
            case 1:   key_menu_screenshot = 0;  break;
            case 2:   key_message_refresh = 0;  break;
            case 3:   key_demo_quit = 0;        break;
            // Multiplayer title
            case 5:   key_multi_msg = 0;        break;
            case 6:   key_multi_msgplayer[0] = 0;  break;
            case 7:   key_multi_msgplayer[1] = 0;  break;
            case 8:   key_multi_msgplayer[2] = 0;  break;
            case 9:   key_multi_msgplayer[3] = 0;  break;
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
    key_crl_menu = '`';
    key_crl_reloadlevel = 0;
    key_crl_nextlevel = 0;
    key_crl_demospeed = 0;
    key_crl_extendedhud = 0;
    key_crl_spectator = 0;
    key_crl_cameraup = 0;
    key_crl_cameradown = 0;
    key_crl_cameramoveto = 0;
    key_crl_freeze = 0;
    key_crl_buddha = 0;
    key_crl_notarget = 0;
    key_crl_nomomentum = 0;
    // Page 3
    key_crl_autorun = KEY_CAPSLOCK;
    key_crl_novert = 0;
    key_crl_vilebomb = 0;
    key_crl_vilefly = 0;
    key_crl_clearmax = 0;
    key_crl_movetomax = 0;
    key_crl_iddqd = 0;
    key_crl_idkfa = 0;
    key_crl_idfa = 0;
    key_crl_idclip = 0;
    key_crl_iddt = 0;
    key_crl_mdk = 0;
    // Page 4
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
    // Page 5
    key_map_toggle = KEY_TAB;
    key_map_zoomin = '=';
    key_map_zoomout = '-';
    key_map_maxzoom = '0';
    key_map_follow = 'f';
    key_crl_map_rotate = 'r';
    key_crl_map_overlay = 'o';
    key_crl_map_sndprop = 'p';
    key_map_grid = 'g';
    key_map_mark = 'm';
    key_map_clearmark = 'c';
    // Page 6
    key_menu_help = KEY_F1;
    key_menu_save = KEY_F2;
    key_menu_load = KEY_F3;
    key_menu_volume = KEY_F4;
    key_menu_detail = KEY_F5;
    key_menu_qsave = KEY_F6;
    key_menu_endgame = KEY_F7;
    key_menu_messages = KEY_F8;
    key_menu_qload = KEY_F9;
    key_menu_quit = KEY_F10;
    key_menu_gamma = KEY_F11;
    key_spy = KEY_F12;
    // Page 7
    key_pause = KEY_PAUSE;
    key_menu_screenshot = KEY_PRTSCR;
    key_message_refresh = KEY_ENTER;
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

static byte *M_ColorizeBind (int itemSetOn, int key)
{
    if (itemOn == itemSetOn && KbdIsBinding)
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

static void M_DrawBindKey (int itemNum, int yPos, int key)
{
    M_WriteText(M_ItemRightAlign(M_NameBind(itemNum, key)),
                yPos,
                M_NameBind(itemNum, key),
                M_ColorizeBind(itemNum, key));
}

// -----------------------------------------------------------------------------
// M_DrawBindFooter
//  [JN] Draw footer in key binding pages with numeration.
// -----------------------------------------------------------------------------

static void M_DrawBindFooter (char *pagenum, boolean drawPages)
{
    M_WriteTextCentered(169, "PRESS ENTER TO BIND, DEL TO CLEAR", cr[CR_MENU_DARK1]);

    if (drawPages)
    {
        M_WriteText(CRL_MENU_LEFTOFFSET, 178, "< PGUP", cr[CR_MENU_DARK3]);
        M_WriteTextCentered(178, M_StringJoin("PAGE ", pagenum, "/7", NULL), cr[CR_MENU_DARK2]);
        M_WriteText(M_ItemRightAlign("PGDN >"), 178, "PGDN >", cr[CR_MENU_DARK3]);
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


static char *M_NameMouseBind (int itemSetOn, int btn)
{
    if (itemOn == itemSetOn && MouseIsBinding)
    {
        return "?";  // Means binding now
    }
    else
    {
        char  num[8]; 
        char *other_button;

        M_snprintf(num, 8, "%d", btn + 1);
        other_button = M_StringJoin("BUTTON #", num, NULL);

        switch (btn)
        {
            case -1:  return  "---";            break;  // Means empty
            case  0:  return  "LEFT BUTTON";    break;
            case  1:  return  "RIGHT BUTTON";   break;
            case  2:  return  "MIDDLE BUTTON";  break;
            case  3:  return  "WHEEL UP";       break;
            case  4:  return  "WHEEL DOWN";     break;
            default:  return  other_button;     break;
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
    if (mousebspeed == btn)       mousebspeed       = -1;
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
        case 1002:  mousebspeed = btn;        break;
        case 1003:  mousebstrafe = btn;       break;
        case 1004:  mousebbackward = btn;     break;
        case 1005:  mousebuse = btn;          break;
        case 1006:  mousebstrafeleft = btn;   break;
        case 1007:  mousebstraferight = btn;  break;
        case 1008:  mousebprevweapon = btn;   break;
        case 1009:  mousebnextweapon = btn;   break;
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
        case 2:  mousebspeed = -1;        break;
        case 3:  mousebstrafe = -1;       break;
        case 4:  mousebbackward = -1;     break;
        case 5:  mousebuse = -1;          break;
        case 6:  mousebstrafeleft = -1;   break;
        case 7:  mousebstraferight = -1;  break;
        case 8:  mousebprevweapon = -1;   break;
        case 9:  mousebnextweapon = -1;   break;
    }
}

// -----------------------------------------------------------------------------
// M_ColorizeMouseBind
//  [JN] Do mouse bind coloring.
// -----------------------------------------------------------------------------

static byte *M_ColorizeMouseBind (int itemSetOn, int btn)
{
    if (itemOn == itemSetOn && MouseIsBinding)
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

static void M_DrawBindButton (int itemNum, int yPos, int btn)
{
    M_WriteText(M_ItemRightAlign(M_NameMouseBind(itemNum, btn)),
                yPos,
                M_NameMouseBind(itemNum, btn),
                M_ColorizeMouseBind(itemNum, btn));
}

// -----------------------------------------------------------------------------
// M_ResetBinds
//  [JN] Reset all mouse binding to it's defaults.
// -----------------------------------------------------------------------------

static void M_ResetMouseBinds (void)
{
    mousebfire = 0;
    mousebforward = 2;
    mousebspeed = -1;
    mousebstrafe = 1;
    mousebbackward = -1;
    mousebuse = -1;
    mousebstrafeleft = -1;
    mousebstraferight = -1;
    mousebprevweapon = 4;
    mousebnextweapon = 3;
}
