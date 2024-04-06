//
// Copyright(C) 1993-1996 Id Software, Inc.
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

// [FG] support up to 8 pages of savegames
int savepage = 0;
static const int savepage_max = 7;

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
    
    char	name[32];
    
    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
    // hotkey in menu
    char	alphaKey;			
    
    // [JN] Menu item timer for glowing effect.
    short   tics;
} menuitem_t;


// [JN] Small cursor timer for glowing effect.
static short   cursor_tics = 0;
static boolean cursor_direction = false;

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

static short itemOn;            // menu item skull is on
static short skullAnimCounter;  // skull animation counter
static short whichSkull;        // which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
static char *skullName[2] = {"M_SKULL1","M_SKULL2"};

// current menudef
static menu_t *currentMenu;

//
// PROTOTYPES
//
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
static void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
static const int M_StringHeight(const char *string);
static void M_StartMessage(const char *string,void (*routine)(int),boolean input);
static void M_ClearMenus (void);


//
// DOOM MENU
//

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
    {1,"M_NGAME",M_NewGame,'n'},
    {1,"M_OPTION",M_ChooseCRL_Main,'o'},
    {1,"M_LOADG",M_LoadGame,'l'},
    {1,"M_SAVEG",M_SaveGame,'s'},
    // Another hickup with Special edition.
    {1,"M_RDTHIS",M_ReadThis,'r'},
    {1,"M_QUITG",M_QuitDOOM,'q'}
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
    {1,"M_EPI1", M_Episode,'k'},
    {1,"M_EPI2", M_Episode,'t'},
    {1,"M_EPI3", M_Episode,'i'},
    {1,"M_EPI4", M_Episode,'t'}
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
    {1,"M_JKILL",	M_ChooseSkill, 'i'},
    {1,"M_ROUGH",	M_ChooseSkill, 'h'},
    {1,"M_HURT",	M_ChooseSkill, 'h'},
    {1,"M_ULTRA",	M_ChooseSkill, 'u'},
    {1,"M_NMARE",	M_ChooseSkill, 'n'}
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
    {1,"M_ENDGAM",	M_EndGame,'e'},
    {1,"M_MESSG",	M_ChangeMessages,'m'},
    {1,"M_DETAIL",	M_ChangeDetail,'g'},
    {2,"M_SCRNSZ",	M_SizeDisplay,'s'},
    {-1,"",0,'\0'},
    {2,"M_MSENS",	M_ChangeSensitivity,'m'},
    {-1,"",0,'\0'},
    {1,"M_SVOL",	M_Sound,'s'}
};

static menu_t OptionsDef =
{
    opt_end,
    &CRLDef_Main,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0,
    false
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
    {1,"",M_ReadThis2,0}
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
    {1,"",M_FinishReadThis,0}
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
    {2,"M_SFXVOL",M_SfxVol,'s'},
    {-1,"",0,'\0'},
    {2,"M_MUSVOL",M_MusicVol,'m'},
    {-1,"",0,'\0'}
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
    {1,"", M_LoadSelect,'1'},
    {1,"", M_LoadSelect,'2'},
    {1,"", M_LoadSelect,'3'},
    {1,"", M_LoadSelect,'4'},
    {1,"", M_LoadSelect,'5'},
    {1,"", M_LoadSelect,'6'},
    {1,"", M_LoadSelect,'7'},
    {1,"", M_LoadSelect,'8'}
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
    {1,"", M_SaveSelect,'1'},
    {1,"", M_SaveSelect,'2'},
    {1,"", M_SaveSelect,'3'},
    {1,"", M_SaveSelect,'4'},
    {1,"", M_SaveSelect,'5'},
    {1,"", M_SaveSelect,'6'},
    {1,"", M_SaveSelect,'7'},
    {1,"", M_SaveSelect,'8'}
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

#define CRL_MENU_TOPOFFSET         (34)
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
static void M_CRL_Colorblind (int choice);

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
static void M_CRL_Controls_Sensivity (int choice);
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
static void M_Bind_FreezeMode (int choice);
static void M_Bind_BuddhaMode (int choice);
static void M_Bind_NotargetMode (int choice);
static void M_Bind_NomomentumMode (int choice);

static void M_DrawCRL_Keybinds_3 (void);
static void M_Bind_AlwaysRun (int choice);
static void M_Bind_NoVert (int choice);
static void M_Bind_VileBomb (int choice);
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
static void M_CRL_Widget_Coords (int choice);
static void M_CRL_Widget_Time (int choice);
static void M_CRL_Widget_Powerups (int choice);
static void M_CRL_Widget_Health (int choice);
static void M_CRL_Automap_Rotate (int choice);
static void M_CRL_Automap_Overlay (int choice);
static void M_CRL_Automap_Shading (int choice);
static void M_CRL_Automap_Drawing (int choice);
static void M_CRL_Automap_Secrets (int choice);

static void M_ChooseCRL_Gameplay (int choice);
static void M_DrawCRL_Gameplay (void);
static void M_CRL_DefaulSkill (int choice);
static void M_CRL_PistolStart (int choice);
static void M_CRL_ColoredSTBar (int choice);
static void M_CRL_RevealedSecrets (int choice);
static void M_CRL_RestoreTargets (int choice);
static void M_CRL_DemoTimer (int choice);
static void M_CRL_TimerDirection (int choice);
static void M_CRL_ProgressBar (int choice);
static void M_CRL_InternalDemos (int choice);

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

// Utility function for scrolling pages by arrows / PG keys.
static void M_ScrollPages (boolean direction)
{
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
    else if (currentMenu == &CRLDef_Keybinds_1) M_SetupNextMenu(direction ? &CRLDef_Keybinds_2 : &CRLDef_Keybinds_7);
    else if (currentMenu == &CRLDef_Keybinds_2) M_SetupNextMenu(direction ? &CRLDef_Keybinds_3 : &CRLDef_Keybinds_1);
    else if (currentMenu == &CRLDef_Keybinds_3) M_SetupNextMenu(direction ? &CRLDef_Keybinds_4 : &CRLDef_Keybinds_2);
    else if (currentMenu == &CRLDef_Keybinds_4) M_SetupNextMenu(direction ? &CRLDef_Keybinds_5 : &CRLDef_Keybinds_3);
    else if (currentMenu == &CRLDef_Keybinds_5) M_SetupNextMenu(direction ? &CRLDef_Keybinds_6 : &CRLDef_Keybinds_4);
    else if (currentMenu == &CRLDef_Keybinds_6) M_SetupNextMenu(direction ? &CRLDef_Keybinds_7 : &CRLDef_Keybinds_5);
    else if (currentMenu == &CRLDef_Keybinds_7) M_SetupNextMenu(direction ? &CRLDef_Keybinds_1 : &CRLDef_Keybinds_6);

    // Play sound.
    S_StartSound(NULL, sfx_pstop);
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

        st_fullupdate = true;
    }
}

// [JN] Fill background with FLOOR4_8 flat.
static void M_FillBackground (void)
{
    const byte *src = W_CacheLumpName("FLOOR4_8", PU_CACHE);
    pixel_t *dest = I_VideoBuffer;

    for (int y = 0 ; y < SCREENHEIGHT; y++)
    {
        for (int x = 0; x < SCREENWIDTH; x++)
        {
            *dest++ = src[((y & 63) << 6) + (x & 63)];
        }
    }
}

enum
{
    m_crl_01,    // 16
    m_crl_02,    // 25
    m_crl_03,    // 34
    m_crl_04,    // 43
    m_crl_05,    // 52
    m_crl_06,    // 61
    m_crl_07,    // 70
    m_crl_08,    // 79
    m_crl_09,    // 88
    m_crl_10,    // 97
    m_crl_11,    // 106
    m_crl_12,    // 115
    m_crl_13,    // 124
    m_crl_14,    // 133
    m_crl_15,    // 142
    m_crl_end
} crl1_e;

static byte *M_Line_Glow (const int tics)
{
    return
        tics == 5 ? cr[CR_MENU_BRIGHT5] :
        tics == 4 ? cr[CR_MENU_BRIGHT4] :
        tics == 3 ? cr[CR_MENU_BRIGHT3] :
        tics == 2 ? cr[CR_MENU_BRIGHT2] :
        tics == 1 ? cr[CR_MENU_BRIGHT1] : NULL;
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
    { 2, "SPECTATOR MODE",       M_CRL_Spectating,      's'},
    { 2, "FREEZE MODE",          M_CRL_Freeze,          'f'},
    { 2, "BUDDHA MODE",          M_CRL_Buddha,          'f'},
    { 2, "NO TARGET MODE",       M_CRL_NoTarget,        'n'},
    { 2, "NO MOMENTUM MODE",     M_CRL_NoMomentum,      'n'},
    {-1, "", 0, '\0'},
    { 1, "VIDEO OPTIONS",        M_ChooseCRL_Video,     'v'},
    { 1, "DISPLAY OPTIONS",      M_ChooseCRL_Display,   'd'},
    { 1, "SOUND OPTIONS",        M_ChooseCRL_Sound,     's'},
    { 1, "CONTROL SETTINGS",     M_ChooseCRL_Controls,  'c'},
    { 1, "WIDGETS AND AUTOMAP",  M_ChooseCRL_Widgets,   'w'},
    { 1, "GAMEPLAY FEATURES",    M_ChooseCRL_Gameplay,  'g'},
    { 1, "STATIC ENGINE LIMITS", M_ChooseCRL_Limits,    's'},
    { 1, "VANILLA OPTIONS MENU", M_Options,             'v'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Main =
{
    m_crl_end,
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

    M_ShadeBackground();
    M_WriteTextCentered(25, "MAIN MENU", cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(0, crl_spectating ? GLOW_GREEN : GLOW_DARKRED));

    // Freeze
    sprintf(str, !singleplayer ? "N/A" :
            crl_freeze ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(1, !singleplayer ? GLOW_DARKRED :
                             crl_freeze ? GLOW_GREEN : GLOW_DARKRED));

    // Buddha
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_BUDDHA ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(2, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_BUDDHA ? GLOW_GREEN : GLOW_DARKRED));

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(3, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOTARGET ? GLOW_GREEN : GLOW_DARKRED));

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(4, !singleplayer ? GLOW_DARKRED :
                             player->cheats & CF_NOMOMENTUM ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(79, "SETTINGS", cr[CR_YELLOW]);
}

static void M_CRL_Spectating (int choice)
{
    crl_spectating ^= 1;
    pspr_interp = false;
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
    { 2, "UNCAPPED FRAMERATE",  M_CRL_UncappedFPS,    'u'},
    { 2, "FRAMERATE LIMIT",     M_CRL_LimitFPS,       'f'},
    { 2, "ENABLE VSYNC",        M_CRL_VSync,          'e'},
    { 2, "SHOW FPS COUNTER",    M_CRL_ShowFPS,        's'},
    { 2, "PIXEL SCALING",       M_CRL_PixelScaling,   'p'},
    { 2, "VISPLANES DRAWING",   M_CRL_VisplanesDraw,  'v'},
    { 2, "HOM EFFECT",          M_CRL_HOMDraw,        'h'},
    {-1, "", 0, '\0'},
    { 2, "SCREEN WIPE EFFECT",  M_CRL_ScreenWipe,     's'},
    { 2, "SHOW ENDOOM SCREEN",  M_CRL_ShowENDOOM,     's'},
    { 2, "COLORBLIND",          M_CRL_Colorblind,     'c'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Video =
{
    m_crl_end,
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

    M_ShadeBackground();

    M_WriteTextCentered(25, "VIDEO OPTIONS", cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str, 
                 M_Item_Glow(0, crl_uncapped_fps ? GLOW_GREEN : GLOW_DARKRED));

    // Framerate limit
    sprintf(str, !crl_uncapped_fps ? "35" :
                 crl_fpslimit ? "%d" : "NONE", crl_fpslimit);
    M_WriteText (M_ItemRightAlign(str), 43, str, 
                 !crl_uncapped_fps ? cr[CR_DARKRED] :
                 M_Item_Glow(1, crl_fpslimit ? GLOW_GREEN : GLOW_DARKRED));

    // Enable vsync
    sprintf(str, crl_vsync ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str, 
                 M_Item_Glow(2, crl_vsync ? GLOW_GREEN : GLOW_DARKRED));

    // Show FPS counter
    sprintf(str, crl_showfps ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str, 
                 M_Item_Glow(3, crl_showfps ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (itemOn == 3 && !crl_extended_hud)
    {
        M_WriteTextCentered(151, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }

    // Pixel scaling
    sprintf(str, smooth_scaling ? "SMOOTH" : "SHARP");
    M_WriteText (M_ItemRightAlign(str), 70, str, 
                 M_Item_Glow(4, smooth_scaling ? GLOW_GREEN : GLOW_DARKRED));

    // Visplanes drawing mode
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(5, crl_visplanes_drawing ? GLOW_GREEN : GLOW_DARKRED));

    // HOM effect
    sprintf(str, crl_hom_effect == 1 ? "MULTICOLOR" :
                 crl_hom_effect == 2 ? "BLACK" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(6, crl_hom_effect ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(97, "MISCELLANEOUS", cr[CR_YELLOW]);

    // Screen wipe effect
    sprintf(str, crl_screenwipe == 1 ? "ON" :
                 crl_screenwipe == 2 ? "FAST" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(8, crl_screenwipe ? GLOW_GREEN : GLOW_DARKRED));

    // Screen ENDOOM screen
    sprintf(str, show_endoom ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 115, str, 
                 M_Item_Glow(9, show_endoom ? GLOW_GREEN : GLOW_DARKRED));

    // Colorblind
    sprintf(str, crl_colorblind == 1 ? "RED/GREEN" :
                 crl_colorblind == 2 ? "BLUE/YELLOW" :
                 crl_colorblind == 3 ? "MONOCHROME" : "NONE");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(10, crl_colorblind ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_UncappedFPS (int choice)
{
    crl_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
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
    crl_visplanes_drawing = M_INT_Slider(crl_visplanes_drawing, 0, 4, choice);
}

static void M_CRL_HOMDraw (int choice)
{
    crl_hom_effect = M_INT_Slider(crl_hom_effect, 0, 2, choice);
}

static void M_CRL_ScreenWipe (int choice)
{
    crl_screenwipe = M_INT_Slider(crl_screenwipe, 0, 2, choice);
}

static void M_CRL_ShowENDOOM (int choice)
{
    show_endoom ^= 1;
}

static void M_CRL_Colorblind (int choice)
{
    crl_colorblind = M_INT_Slider(crl_colorblind, 0, 3, choice);

    // [JN] 1 - always do a full palette reset when colorblind is changed.
    I_SetPalette ((byte *)W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE) + st_palette * 768, 1);
}

// -----------------------------------------------------------------------------
// Display options
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Display[]=
{
    { 2, "GAMMA-CORRECTION",         M_CRL_Gamma,            'g'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "MENU BACKGROUND SHADING",  M_CRL_MenuBgShading,    'm'},
    { 2, "EXTRA LEVEL BRIGHTNESS",   M_CRL_LevelBrightness,  'e'},
    {-1, "", 0, '\0'},
    { 2, "MESSAGES ENABLED",         M_ChangeMessages,       'm'},
    { 2, "CRITICAL MESSAGE",         M_CRL_MsgCritical,      'c'},
    { 2, "TEXT CAST SHADOWS",        M_CRL_TextShadows,      't'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Display =
{
    m_crl_end,
    &CRLDef_Main,
    CRLMenu_Display,
    M_DrawCRL_Display,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
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

    // Temporary unshade background while changing gamma-correction.
    if (shade_wait < I_GetTime())
    {
        M_ShadeBackground();
    }

    M_WriteTextCentered(25, "DISPLAY OPTIONS", cr[CR_YELLOW]);

    // Gamma-correction slider and num
    M_DrawThermo(46, 44, 15, crl_gamma);
    M_WriteText (184, 47, gammalvls[crl_gamma][1],
                           M_Item_Glow(0, GLOW_UNCOLORED));

    // Menu background shading
    sprintf(str, crl_menu_shading ? "%d" : "OFF", crl_menu_shading);
    M_WriteText (M_ItemRightAlign(str), 61, str, 
                 M_Item_Glow(3, crl_menu_shading ? GLOW_GREEN : GLOW_DARKRED));

    // Extra level brightness
    sprintf(str, crl_level_brightness ? "%d" : "OFF", crl_level_brightness);
    M_WriteText (M_ItemRightAlign(str), 70, str, 
                 M_Item_Glow(4, crl_level_brightness ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(79, "MESSAGES SETTINGS", cr[CR_YELLOW]);

    // Messages enabled
    sprintf(str, showMessages ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str, 
                 M_Item_Glow(6, showMessages ? GLOW_GREEN : GLOW_DARKRED));

    // Critical message style
    sprintf(str, crl_msg_critical ? "BLINKING" : "STATIC");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(7, crl_msg_critical ? GLOW_GREEN : GLOW_DARKRED));
    // Show nice preview-reminder :)
    if (itemOn == 7)
    {
        CRL_SetMessageCritical("CRL_REMINDER:", "CRITICAL MESSAGES ARE ALWAYS ENABLED!", 2);
    }

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str, 
                 M_Item_Glow(8, crl_text_shadows ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_Gamma (int choice)
{
    shade_wait = I_GetTime() + TICRATE;

    switch (choice)
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

    CRL_ReloadPalette();
}

static void M_CRL_MenuBgShading (int choice)
{
    switch (choice)
    {
        case 0:
            if (crl_menu_shading)
                crl_menu_shading--;
            break;
        case 1:
            if (crl_menu_shading < 24)
                crl_menu_shading++;
        default:
            break;
    }
}

static void M_CRL_LevelBrightness (int choice)
{
    switch (choice)
    {
        case 0:
            if (crl_level_brightness)
                crl_level_brightness--;
            break;
        case 1:
            if (crl_level_brightness < 8)
                crl_level_brightness++;
        default:
            break;
    }
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
    { 2, "SFX VOLUME",            M_SfxVol,           's'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "MUSIC VOLUME",          M_MusicVol,         'm'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "SFX PLAYBACK",          M_CRL_SFXSystem,    's'},
    { 2, "MUSIC PLAYBACK",        M_CRL_MusicSystem,  'm'},
    { 2, "SOUNDS EFFECTS MODE",   M_CRL_SFXMode,      's'},
    { 2, "PITCH-SHIFTED SOUNDS",  M_CRL_PitchShift,   'p'},
    { 2, "NUMBER OF SFX TO MIX",  M_CRL_SFXChannels,  'n'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Sound =
{
    m_crl_end,
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

    M_ShadeBackground();

    M_WriteTextCentered(25, "VOLUME", cr[CR_YELLOW]);

    M_DrawThermo(46, 44, 16, sfxVolume);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (192, 47, str, M_Item_Glow(0, GLOW_UNCOLORED));

    M_DrawThermo(46, 71, 16, musicVolume);
    sprintf(str,"%d", musicVolume);
    M_WriteText (192, 74, str, M_Item_Glow(3, GLOW_UNCOLORED));

    M_WriteTextCentered(88, "SOUND SYSTEM", cr[CR_YELLOW]);

    // SFX playback
    sprintf(str, snd_sfxdevice == 0 ? "DISABLED"    :
                 snd_sfxdevice == 1 ? "PC SPEAKER"  :
                 snd_sfxdevice == 3 ? "DIGITAL SFX" :
                                      "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(7, snd_sfxdevice ? GLOW_GREEN : GLOW_DARKRED));

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ? "GUS (EMULATED)" :
                 snd_musicdevice == 8 ? "NATIVE MIDI" :
                 snd_musicdevice == 11 ? "FLUIDSYNTH" :
                                        "UNKNOWN");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(8, snd_musicdevice ? GLOW_GREEN : GLOW_DARKRED));

    // Sound effects mode
    sprintf(str, crl_monosfx ? "MONO" : "STEREO");
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(9, crl_monosfx ? GLOW_DARKRED : GLOW_GREEN));

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(10, snd_pitchshift ? GLOW_GREEN : GLOW_DARKRED));

    // Number of SFX to mix
    sprintf(str, "%i", snd_channels);
    M_WriteText (M_ItemRightAlign(str), 133, str,
                 M_Item_Glow(11, snd_channels == 8 ? GLOW_GREEN :
                                 snd_channels == 1 ? GLOW_DARKRED : GLOW_DARKGREEN));

    // Inform if GUS patches patch isn't set.
    if (itemOn == 8 && snd_musicdevice == 5 && strcmp(gus_patch_path, "") == 0)
    {
        M_WriteTextCentered(142, "\"GUS_PATCH_PATH\" VARIABLE IS NOT SET,\n", cr[CR_GRAY]);
        M_WriteTextCentered(151, "PLEASE EDIT IT IN DEFAULT.CFG FILE.", cr[CR_GRAY]);
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
    S_ChangeMusic(crl_musicnum, true);
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
    S_ChangeMusic(crl_musicnum, true);
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
    switch (choice)
    {
        case 0:
            if (snd_channels > 1)
                snd_channels--;
            break;
        case 1:
            if (snd_channels < 8)
                snd_channels++;
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// Control settings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Controls[]=
{
    { 1, "KEYBOARD BINDINGS",            M_Choose_CRL_Keybinds,       'k'},
    { 1, "MOUSE BINDINGS",               M_ChooseCRL_MouseBinds,      'm'},
    {-1, "", 0, '\0'},
    { 2, "SENSIVITY",                    M_CRL_Controls_Sensivity,    's'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "ACCELERATION",                 M_CRL_Controls_Acceleration, 'a'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "ACCELERATION THRESHOLD",       M_CRL_Controls_Threshold,    'a'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "VERTICAL MOUSE MOVEMENT",      M_CRL_Controls_NoVert,       'v'},
    { 2, "DOUBLE CLICK ACTS AS \"USE\"", M_CRL_Controls_DblClck,      'd'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Controls =
{
    m_crl_end,
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

    M_ShadeBackground();
    
    M_WriteTextCentered(25, "BINDINGS", cr[CR_YELLOW]);
    
    M_WriteTextCentered(52, "MOUSE CONFIGURATION", cr[CR_YELLOW]);

    M_DrawThermo(46, 72, 10, mouseSensitivity);
    sprintf(str,"%d", mouseSensitivity);
    M_WriteText (144, 75, str, M_Item_Glow(3, mouseSensitivity > 9 ? GLOW_GREEN :
                                              mouseSensitivity < 1 ? GLOW_DARKRED : GLOW_UNCOLORED));

    M_DrawThermo(46, 98, 12, (mouse_acceleration * 3) - 3);
    sprintf(str,"%.1f", mouse_acceleration);
    M_WriteText (160, 101, str, M_Item_Glow(6, GLOW_UNCOLORED));

    M_DrawThermo(46, 125, 15, mouse_threshold / 2);
    sprintf(str,"%d", mouse_threshold);
    M_WriteText (184, 128, str, M_Item_Glow(9, mouse_threshold == 0 ? GLOW_DARKRED : GLOW_UNCOLORED));

    // Vertical mouse movement
    sprintf(str, novert ? "OFF" : "ON");
    M_WriteText (M_ItemRightAlign(str), 142, str,
                 M_Item_Glow(12, novert ? GLOW_DARKRED : GLOW_GREEN));

    // Double click acts as "use"
    sprintf(str, dclick_use ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 151, str,
                 M_Item_Glow(13, dclick_use ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_Controls_Sensivity (int choice)
{
    switch (choice)
    {
        case 0:
        if (mouseSensitivity)
            mouseSensitivity--;
        break;
        case 1:
        if (mouseSensitivity < 255) // [crispy] extended range
            mouseSensitivity++;
        break;
    }
}

static void M_CRL_Controls_Acceleration (int choice)
{
    char buf[9];

    switch (choice)
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
}

static void M_CRL_Controls_Threshold (int choice)
{
    switch (choice)
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
    { 1, "MOVE FORWARD",   M_Bind_MoveForward,   'm' },
    { 1, "MOVE BACKWARD",  M_Bind_MoveBackward,  'm' },
    { 1, "TURN LEFT",      M_Bind_TurnLeft,      't' },
    { 1, "TURN RIGHT",     M_Bind_TurnRight,     't' },
    { 1, "STRAFE LEFT",    M_Bind_StrafeLeft,    's' },
    { 1, "STRAFE RIGHT",   M_Bind_StrafeRight,   's' },
    { 1, "SPEED ON",       M_Bind_SpeedOn,       's' },
    { 1, "STRAFE ON",      M_Bind_StrafeOn,      's' },
    { 1, "180 DEGREE TURN",M_Bind_180Turn,       '1' },
    {-1, "",               0,                    '\0'},  // ACTION
    { 1, "FIRE/ATTACK",    M_Bind_FireAttack,    'f' },
    { 1, "USE",            M_Bind_Use,           'u' },
    {-1, "",               0,                    '\0'},
    {-1, "",               0,                    '\0'},
    {-1, "",               0,                    '\0'},
    {-1, "",               0,                    '\0'}
};

static menu_t CRLDef_Keybinds_1 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_up);
    M_DrawBindKey(1, 43, key_down);
    M_DrawBindKey(2, 52, key_left);
    M_DrawBindKey(3, 61, key_right);
    M_DrawBindKey(4, 70, key_strafeleft);
    M_DrawBindKey(5, 79, key_straferight);
    M_DrawBindKey(6, 88, key_speed);
    M_DrawBindKey(7, 97, key_strafe);
    M_DrawBindKey(8, 106, key_180turn);

    M_WriteTextCentered(115, "ACTION", cr[CR_YELLOW]);

    M_DrawBindKey(10, 124, key_fire);
    M_DrawBindKey(11, 133, key_use);

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
    { 1, "MAIN CRL MENU",       M_Bind_CRLmenu,        'm'  },
    { 1, "RESTART LEVEL/DEMO",  M_Bind_RestartLevel,   'r'  },
    { 1, "GO TO NEXT LEVEL",    M_Bind_NextLevel,      'g'  },
    { 1, "DEMO FAST-FORWARD",   M_Bind_FastForward,    'd'  },
    { 1, "TOGGLE EXTENDED HUD", M_Bind_ExtendedHUD,    's'  },
    {-1, "",                    0,                     '\0' },  // GAME MODES
    { 1, "SPECTATOR MODE",      M_Bind_SpectatorMode,  's'  },
    { 1, "- MOVE CAMERA UP",    M_Bind_CameraUp,       'm'  },
    { 1, "- MOVE CAMERA DOWN",  M_Bind_CameraDown,     'm'  },
    { 1, "FREEZE MODE",         M_Bind_FreezeMode,     'f'  },
    { 1, "BUDDHA MODE",         M_Bind_BuddhaMode,     'b'  },
    { 1, "NO TARGET MODE",      M_Bind_NotargetMode,   'n'  },
    { 1, "NO MOMENTUM MODE",    M_Bind_NomomentumMode, 'n'  },
    {-1, "",                    0,                     '\0' },
    {-1, "",                    0,                     '\0' },
    {-1, "",                    0,                     '\0' },
    {-1, "",                    0,                     '\0' }
};

static menu_t CRLDef_Keybinds_2 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "CRL CONTROLS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_crl_menu);
    M_DrawBindKey(1, 43, key_crl_reloadlevel);
    M_DrawBindKey(2, 52, key_crl_nextlevel);
    M_DrawBindKey(3, 61, key_crl_demospeed);
    M_DrawBindKey(4, 70, key_crl_extendedhud);

    M_WriteTextCentered(79, "GAME MODES", cr[CR_YELLOW]);

    M_DrawBindKey(6, 88, key_crl_spectator);
    M_DrawBindKey(7, 97, key_crl_cameraup);
    M_DrawBindKey(8, 106, key_crl_cameradown);
    M_DrawBindKey(9, 115, key_crl_freeze);
    M_DrawBindKey(10, 124, key_crl_buddha);
    M_DrawBindKey(11, 133, key_crl_notarget);
    M_DrawBindKey(12, 142, key_crl_nomomentum);

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

static void M_Bind_FreezeMode (int choice)
{
    M_StartBind(208);  // key_crl_freeze
}

static void M_Bind_BuddhaMode (int choice)
{
    M_StartBind(209);  // key_crl_buddha
}

static void M_Bind_NotargetMode (int choice)
{
    M_StartBind(210);  // key_crl_notarget
}

static void M_Bind_NomomentumMode (int choice)
{
    M_StartBind(211);  // key_crl_nomomentum
}

// -----------------------------------------------------------------------------
// Keybinds 3
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_3[]=
{
    { 1, "ALWAYS RUN",              M_Bind_AlwaysRun,  'a'  },
    { 1, "VERTICAL MOUSE MOVEMENT", M_Bind_NoVert,     'v'  },
    { 1, "ARCH-VILE JUMP",          M_Bind_VileBomb,   'a'  },
    {-1, "",                        0,                 '\0' },  // VISPLANES MAX VALUE
    { 1, "CLEAR MAX",               M_Bind_ClearMAX,   'c'  },
    { 1, "MOVE TO MAX ",            M_Bind_MoveToMAX,  'm'  },
    {-1, "",                        0,                 '\0' },  // CHEAT SHORTCUTS
    { 1, "IDDQD",                   M_Bind_IDDQD,      'i'  },
    { 1, "IDKFA",                   M_Bind_IDKFA,      'i'  },
    { 1, "IDFA",                    M_Bind_IDFA,       'i'  },
    { 1, "IDCLIP",                  M_Bind_IDCLIP,     'i'  },
    { 1, "IDDT",                    M_Bind_IDDT,       'i'  },
    { 1, "MDK",                     M_Bind_MDK,        'm'  },
    {-1, "",                        0,                 '\0' },
    {-1, "",                        0,                 '\0' },
    {-1, "",                        0,                 '\0' }
};

static menu_t CRLDef_Keybinds_3 =
{
    m_crl_end,
    &CRLDef_Controls,
    CRLMenu_Keybinds_3,
    M_DrawCRL_Keybinds_3,
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true, true, true,
};

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

static void M_Bind_ClearMAX (int choice)
{
    M_StartBind(303);  // key_crl_clearmax
}

static void M_Bind_MoveToMAX (int choice)
{
    M_StartBind(304);  // key_crl_movetomax
}

static void M_Bind_IDDQD (int choice)
{
    M_StartBind(305);  // key_crl_iddqd
}

static void M_Bind_IDKFA (int choice)
{
    M_StartBind(306);  // key_crl_idkfa
}

static void M_Bind_IDFA (int choice)
{
    M_StartBind(307);  // key_crl_idfa
}

static void M_Bind_IDCLIP (int choice)
{
    M_StartBind(308);  // key_crl_idclip
}

static void M_Bind_IDDT (int choice)
{
    M_StartBind(309);  // key_crl_iddt
}

static void M_Bind_MDK (int choice)
{
    M_StartBind(310);  // key_crl_mdk
}

static void M_DrawCRL_Keybinds_3 (void)
{
    st_fullupdate = true;
    Keybinds_Cur = 2;

    M_FillBackground();

    M_WriteTextCentered(25, "MOVEMENT", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_crl_autorun);
    M_DrawBindKey(1, 43, key_crl_novert);
    M_DrawBindKey(2, 52, key_crl_vilebomb);

    M_WriteTextCentered(61, "VISPLANES MAX VALUE", cr[CR_YELLOW]);

    M_DrawBindKey(4, 70, key_crl_clearmax);
    M_DrawBindKey(5, 79, key_crl_movetomax);

    M_WriteTextCentered(88, "CHEAT SHORTCUTS", cr[CR_YELLOW]);

    M_DrawBindKey(7, 97, key_crl_iddqd);
    M_DrawBindKey(8, 106, key_crl_idkfa);
    M_DrawBindKey(9, 115, key_crl_idfa);
    M_DrawBindKey(10, 124, key_crl_idclip);
    M_DrawBindKey(11, 133, key_crl_iddt);
    M_DrawBindKey(12, 142, key_crl_mdk);

    M_DrawBindFooter("3", true);
}

// -----------------------------------------------------------------------------
// Keybinds 4
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_4[]=
{
    { 1, "WEAPON 1",        M_Bind_Weapon1,    'w'  },
    { 1, "WEAPON 2",        M_Bind_Weapon2,    'w'  },
    { 1, "WEAPON 3",        M_Bind_Weapon3,    'w'  },
    { 1, "WEAPON 4",        M_Bind_Weapon4,    'w'  },
    { 1, "WEAPON 5",        M_Bind_Weapon5,    'w'  },
    { 1, "WEAPON 6",        M_Bind_Weapon6,    'w'  },
    { 1, "WEAPON 7",        M_Bind_Weapon7,    'w'  },
    { 1, "WEAPON 8",        M_Bind_Weapon8,    'w'  },
    { 1, "PREVIOUS WEAPON", M_Bind_PrevWeapon, 'p'  },
    { 1, "NEXT WEAPON",     M_Bind_NextWeapon, 'n'  },
    {-1, "",                0,                 '\0' },
    {-1, "",                0,                 '\0' },
    {-1, "",                0,                 '\0' },
    {-1, "",                0,                 '\0' },
    {-1, "",                0,                 '\0' },
    {-1, "",                0,                 '\0' }
};

static menu_t CRLDef_Keybinds_4 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "WEAPONS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 35, key_weapon1);
    M_DrawBindKey(1, 43, key_weapon2);
    M_DrawBindKey(2, 52, key_weapon3);
    M_DrawBindKey(3, 61, key_weapon4);
    M_DrawBindKey(4, 70, key_weapon5);
    M_DrawBindKey(5, 79, key_weapon6);
    M_DrawBindKey(6, 88, key_weapon7);
    M_DrawBindKey(7, 97, key_weapon8);
    M_DrawBindKey(8, 106, key_prevweapon);
    M_DrawBindKey(9, 115, key_nextweapon);

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
    { 1, "TOGGLE MAP",        M_Bind_ToggleMap,    't'  },
    { 1, "ZOOM IN",           M_Bind_ZoomIn,       'z'  },
    { 1, "ZOOM OUT",          M_Bind_ZoomOut,      'z'  },
    { 1, "MAXIMUM ZOOM OUT",  M_Bind_MaxZoom,      'm'  },
    { 1, "FOLLOW MODE",       M_Bind_FollowMode,   'f'  },
    { 1, "ROTATE MODE",       M_Bind_RotateMode,   'r'  },
    { 1, "OVERLAY MODE",      M_Bind_OverlayMode,  'o'  },
    { 1, "TOGGLE GRID",       M_Bind_ToggleGrid,   't'  },
    { 1, "MARK LOCATION",     M_Bind_AddMark,      'm'  },
    { 1, "CLEAR ALL MARKS",   M_Bind_ClearMarks,   'c'  },
    {-1, "",                  0,                   '\0' },
    {-1, "",                  0,                   '\0' },
    {-1, "",                  0,                   '\0' },
    {-1, "",                  0,                   '\0' },
    {-1, "",                  0,                   '\0' },
    {-1, "",                  0,                   '\0' }
};

static menu_t CRLDef_Keybinds_5 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "AUTOMAP", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_map_toggle);
    M_DrawBindKey(1, 43, key_map_zoomin);
    M_DrawBindKey(2, 52, key_map_zoomout);
    M_DrawBindKey(3, 61, key_map_maxzoom);
    M_DrawBindKey(4, 70, key_map_follow);
    M_DrawBindKey(5, 79, key_crl_map_rotate);
    M_DrawBindKey(6, 88, key_crl_map_overlay);
    M_DrawBindKey(7, 97, key_map_grid);
    M_DrawBindKey(8, 106, key_map_mark);
    M_DrawBindKey(9, 115, key_map_clearmark);

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

static void M_Bind_ToggleGrid (int choice)
{
    M_StartBind(507);  // key_map_grid
}

static void M_Bind_AddMark (int choice)
{
    M_StartBind(508);  // key_map_mark
}

static void M_Bind_ClearMarks (int choice)
{
    M_StartBind(509);  // key_map_clearmark
}

// -----------------------------------------------------------------------------
// Keybinds 6
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Keybinds_6[]=
{
    { 1, "HELP SCREEN",      M_Bind_HelpScreen,      'h'  },
    { 1, "SAVE GAME",        M_Bind_SaveGame,        's'  },
    { 1, "LOAD GAME",        M_Bind_LoadGame,        'l'  },
    { 1, "SOUND VOLUME",     M_Bind_SoundVolume,     's'  },
    { 1, "TOGGLE DETAIL",    M_Bind_ToggleDetail,    't'  },
    { 1, "QUICK SAVE",       M_Bind_QuickSave,       'q'  },
    { 1, "END GAME",         M_Bind_EndGame,         'e'  },
    { 1, "TOGGLE MESSAGES",  M_Bind_ToggleMessages,  't'  },
    { 1, "QUICK LOAD",       M_Bind_QuickLoad,       'q'  },
    { 1, "QUIT GAME",        M_Bind_QuitGame,        'q'  },
    { 1, "TOGGLE GAMMA",     M_Bind_ToggleGamma,     't'  },
    { 1, "MULTIPLAYER SPY",  M_Bind_MultiplayerSpy,  'm'  },
    {-1, "",                 0,                      '\0' },
    {-1, "",                 0,                      '\0' },
    {-1, "",                 0,                      '\0' },
    {-1, "",                 0,                      '\0' }
};

static menu_t CRLDef_Keybinds_6 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "FUNCTION KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_menu_help);
    M_DrawBindKey(1, 43, key_menu_save);
    M_DrawBindKey(2, 52, key_menu_load);
    M_DrawBindKey(3, 61, key_menu_volume);
    M_DrawBindKey(4, 70, key_menu_detail);
    M_DrawBindKey(5, 79, key_menu_qsave);
    M_DrawBindKey(6, 88, key_menu_endgame);
    M_DrawBindKey(7, 97, key_menu_messages);
    M_DrawBindKey(8, 106, key_menu_qload);
    M_DrawBindKey(9, 115, key_menu_quit);
    M_DrawBindKey(10, 124, key_menu_gamma);
    M_DrawBindKey(11, 133, key_spy);

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
    { 1, "PAUSE GAME",             M_Bind_Pause,           'p'  },
    { 1, "SAVE A SCREENSHOT",      M_Bind_SaveScreenshot,  's'  },
    { 1, "DISPLAY LAST MESSAGE",   M_Bind_LastMessage,     'd'  },
    { 1, "FINISH DEMO RECORDING",  M_Bind_FinishDemo,      'f'  },
    {-1, "",                       0,                      '\0' },  // MULTIPLAYER
    { 1, "SEND MESSAGE",           M_Bind_SendMessage,     's'  },
    { 1, "- TO PLAYER 1",          M_Bind_ToPlayer1,       '1'  },
    { 1, "- TO PLAYER 2",          M_Bind_ToPlayer2,       '2'  },
    { 1, "- TO PLAYER 3",          M_Bind_ToPlayer3,       '3'  },
    { 1, "- TO PLAYER 4",          M_Bind_ToPlayer4,       '4'  },
    {-1, "",                       0,                      '\0' },
    { 1, "RESET BINDINGS TO DEFAULT", M_Bind_Reset,        'r'  },
    {-1, "",                       0,                      '\0' },
    {-1, "",                       0,                      '\0' },
    {-1, "",                       0,                      '\0' },
    {-1, "",                       0,                      '\0' }
};

static menu_t CRLDef_Keybinds_7 =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "SHORTCUT KEYS", cr[CR_YELLOW]);

    M_DrawBindKey(0, 34, key_pause);
    M_DrawBindKey(1, 43, key_menu_screenshot);
    M_DrawBindKey(2, 52, key_message_refresh);
    M_DrawBindKey(3, 61, key_demo_quit);

    M_WriteTextCentered(70, "MULTIPLAYER", cr[CR_YELLOW]);

    M_DrawBindKey(5, 79, key_multi_msg);
    M_DrawBindKey(6, 88, key_multi_msgplayer[0]);
    M_DrawBindKey(7, 97, key_multi_msgplayer[1]);
    M_DrawBindKey(8, 106, key_multi_msgplayer[2]);
    M_DrawBindKey(9, 115, key_multi_msgplayer[3]);

    M_WriteTextCentered(124, "RESET", cr[CR_YELLOW]);

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

    M_StartMessage(resetwarning, M_Bind_ResetResponse, true);
}

// -----------------------------------------------------------------------------
// Mouse bindings
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_MouseBinds[]=
{
    { 1, "FIRE/ATTACK",    M_Bind_M_FireAttack,    'f' },
    { 1, "MOVE FORWARD",   M_Bind_M_MoveForward,   'm' },
    { 1, "STRAFE ON",      M_Bind_M_StrafeOn,      's' },
    { 1, "MOVE BACKWARD",  M_Bind_M_MoveBackward,  'm' },
    { 1, "USE",            M_Bind_M_Use,           'u' },
    { 1, "STRAFE LEFT",    M_Bind_M_StrafeLeft,    's' },
    { 1, "STRAFE RIGHT",   M_Bind_M_StrafeRight,   's' },
    { 1, "PREV WEAPON",    M_Bind_M_PrevWeapon,    'p' },
    { 1, "NEXT WEAPON",    M_Bind_M_NextWeapon,    'n' },
    {-1, "",               0,                      '\0'},
    { 1, "RESET BINDINGS TO DEFAULT", M_Bind_M_Reset, 'r'  },
    {-1, "",               0,                      '\0'},
    {-1, "",               0,                      '\0'},
    {-1, "",               0,                      '\0'},
    {-1, "",               0,                      '\0'},
    {-1, "",               0,                      '\0'}
};

static menu_t CRLDef_MouseBinds =
{
    m_crl_end,
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

    M_WriteTextCentered(25, "MOUSE BINDINGS", cr[CR_YELLOW]);

    M_DrawBindButton(0, 34, mousebfire);
    M_DrawBindButton(1, 43, mousebforward);
    M_DrawBindButton(2, 52, mousebstrafe);
    M_DrawBindButton(3, 61, mousebbackward);
    M_DrawBindButton(4, 70, mousebuse);
    M_DrawBindButton(5, 79, mousebstrafeleft);
    M_DrawBindButton(6, 88, mousebstraferight);
    M_DrawBindButton(7, 97, mousebprevweapon);
    M_DrawBindButton(8, 106, mousebnextweapon);

    M_WriteTextCentered(115, "RESET", cr[CR_YELLOW]);

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

static void M_Bind_M_StrafeOn (int choice)
{
    M_StartMouseBind(1002);  // mousebstrafe
}

static void M_Bind_M_MoveBackward (int choice)
{
    M_StartMouseBind(1003);  // mousebbackward
}

static void M_Bind_M_Use (int choice)
{
    M_StartMouseBind(1004);  // mousebuse
}

static void M_Bind_M_StrafeLeft (int choice)
{
    M_StartMouseBind(1005);  // mousebstrafeleft
}

static void M_Bind_M_StrafeRight (int choice)
{
    M_StartMouseBind(1006);  // mousebstraferight
}

static void M_Bind_M_PrevWeapon (int choice)
{
    M_StartMouseBind(1007);  // mousebprevweapon
}

static void M_Bind_M_NextWeapon (int choice)
{
    M_StartMouseBind(1008);  // mousebnextweapon
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

    M_StartMessage(resetwarning, M_Bind_M_ResetResponse, true);
}

// -----------------------------------------------------------------------------
// Widgets and Automap
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Widgets[]=
{
    { 2, "RENDER COUNTERS",       M_CRL_Widget_Render,     'r'},
    { 2, "MAX OVERFLOW STYLE",    M_CRL_Widget_MAX,        'r'},
    { 2, "PLAYSTATE COUNTERS",    M_CRL_Widget_Playstate,  'r'},
    { 2, "KIS STATS/FRAGS",       M_CRL_Widget_KIS,        'k'},
    { 2, "LEVEL/DM TIMER",        M_CRL_Widget_Time,       'l'},
    { 2, "PLAYER COORDS",         M_CRL_Widget_Coords,     'p'},
    { 2, "POWERUP TIMERS",        M_CRL_Widget_Powerups,   'p'},
    { 2, "TARGET'S HEALTH",       M_CRL_Widget_Health,     't'},
    {-1, "", 0, '\0'},
    { 2, "ROTATE MODE",           M_CRL_Automap_Rotate,    'r'},
    { 2, "OVERLAY MODE",          M_CRL_Automap_Overlay,   'o'},
    { 2, "OVERLAY SHADING LEVEL", M_CRL_Automap_Shading,   'o'},
    { 2, "DRAWING MODE",          M_CRL_Automap_Drawing,   'd'},
    { 2, "MARK SECRET SECTORS",   M_CRL_Automap_Secrets,   'm'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Widgets =
{
    m_crl_end,
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

    M_ShadeBackground();
    M_WriteTextCentered(25, "WIDGETS", cr[CR_YELLOW]);

    // Rendering counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(0, crl_widget_render == 1 ? GLOW_GREEN :
                                crl_widget_render == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // MAX overflow style
    sprintf(str, crl_widget_maxvp == 1 ? "BLINKING 1" :
                 crl_widget_maxvp == 2 ? "BLINKING 2" : "STATIC");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(1, crl_widget_maxvp == 1 ? (gametic &  8 ? GLOW_YELLOW : GLOW_GREEN) :
                                crl_widget_maxvp == 2 ? (gametic & 16 ? GLOW_YELLOW : GLOW_GREEN) : GLOW_YELLOW));

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(2, crl_widget_playstate == 1 ? GLOW_GREEN :
                                crl_widget_playstate == 2 ? GLOW_DARKGREEN : GLOW_DARKRED));

    // K/I/S stats
    sprintf(str, crl_widget_kis == 1 ? "ON" :
                 crl_widget_kis == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(3, crl_widget_kis ? GLOW_GREEN : GLOW_DARKRED));

    // Level time
    sprintf(str, crl_widget_time == 1 ? "ON" : 
                 crl_widget_time == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(4, crl_widget_time ? GLOW_GREEN : GLOW_DARKRED));

    // Player coords
    sprintf(str, crl_widget_coords == 1 ? "ON" :
                 crl_widget_coords == 2 ? "AUTOMAP" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 79, str,
                 M_Item_Glow(5, crl_widget_coords ? GLOW_GREEN : GLOW_DARKRED));

    // Powerup timers
    sprintf(str, crl_widget_powerups ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(6, crl_widget_powerups ? GLOW_GREEN : GLOW_DARKRED));

    // Target's health
    sprintf(str, crl_widget_health == 1 ? "TOP" :
                 crl_widget_health == 2 ? "TOP+NAME" :
                 crl_widget_health == 3 ? "BOTTOM" :
                 crl_widget_health == 4 ? "BOTTOM+NAME" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(7, crl_widget_health ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (itemOn < 8 && !crl_extended_hud)
    {
        M_WriteTextCentered(160, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }

    M_WriteTextCentered(106, "AUTOMAP", cr[CR_YELLOW]);

    // Rotate mode
    sprintf(str, crl_automap_rotate ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(9, crl_automap_rotate ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay mode
    sprintf(str, crl_automap_overlay ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 124, str,
                 M_Item_Glow(10, crl_automap_overlay ? GLOW_GREEN : GLOW_DARKRED));

    // Overlay shading level
    sprintf(str,"%d", crl_automap_shading);
    M_WriteText (M_ItemRightAlign(str), 133, str,
                 M_Item_Glow(11, !crl_automap_overlay ? GLOW_DARKRED :
                                  crl_automap_shading ==  0 ? GLOW_RED :
                                  crl_automap_shading == 12 ? GLOW_YELLOW : GLOW_GREEN));

    // Drawing mode
    sprintf(str, crl_automap_mode == 1 ? "FLOOR VISPLANES" :
                 crl_automap_mode == 2 ? "CEILING VISPLANES" : "NORMAL");
    M_WriteText (M_ItemRightAlign(str), 142, str,
                 M_Item_Glow(12, crl_automap_mode ? GLOW_GREEN : GLOW_DARKRED));

    // Mark secret sectors
    sprintf(str, crl_automap_secrets ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 151, str,
                 M_Item_Glow(13, crl_automap_secrets ? GLOW_GREEN : GLOW_DARKRED));
}

static void M_CRL_Widget_Render (int choice)
{
    crl_widget_render = M_INT_Slider(crl_widget_render, 0, 2, choice);
}

static void M_CRL_Widget_MAX (int choice)
{
    crl_widget_maxvp = M_INT_Slider(crl_widget_maxvp, 0, 2, choice);
}

static void M_CRL_Widget_Playstate (int choice)
{
    crl_widget_playstate = M_INT_Slider(crl_widget_playstate, 0, 2, choice);
}

static void M_CRL_Widget_KIS (int choice)
{
    crl_widget_kis = M_INT_Slider(crl_widget_kis, 0, 2, choice);
}

static void M_CRL_Widget_Coords (int choice)
{
    crl_widget_coords = M_INT_Slider(crl_widget_coords, 0, 2, choice);
}

static void M_CRL_Widget_Time (int choice)
{
    crl_widget_time = M_INT_Slider(crl_widget_time, 0, 2, choice);
}

static void M_CRL_Widget_Powerups (int choice)
{
    crl_widget_powerups ^= 1;
}

static void M_CRL_Widget_Health (int choice)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 4, choice);
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
    switch (choice)
    {
        case 0:
            if (crl_automap_shading)
                crl_automap_shading--;
            break;
        case 1:
            if (crl_automap_shading < 12)
                crl_automap_shading++;
        default:
            break;
    }
}

static void M_CRL_Automap_Drawing (int choice)
{
    crl_automap_mode = M_INT_Slider(crl_automap_mode, 0, 2, choice);
}

static void M_CRL_Automap_Secrets (int choice)
{
    crl_automap_secrets ^= 1;
}

// -----------------------------------------------------------------------------
// Gameplay features
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Gameplay[]=
{
    { 2, "DEFAULT SKILL LEVEL",      M_CRL_DefaulSkill,      'd'},
    { 2, "PISTOL START GAME MODE",   M_CRL_PistolStart,      'p'},
    { 2, "COLORED STATUS BAR",       M_CRL_ColoredSTBar,     'c'},
    { 2, "REPORT REVEALED SECRETS",  M_CRL_RevealedSecrets,  'r'},
    { 2, "RESTORE MONSTER TARGETS",  M_CRL_RestoreTargets,   'r'},
    {-1, "", 0, '\0'},
    { 2, "SHOW DEMO TIMER",          M_CRL_DemoTimer,        's'},
    { 2, "TIMER DIRECTION",          M_CRL_TimerDirection,   't'},
    { 2, "SHOW PROGRESS BAR",        M_CRL_ProgressBar,      's'},
    { 2, "PLAY INTERNAL DEMOS",      M_CRL_InternalDemos,    'p'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Gameplay =
{
    m_crl_end,
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

    M_ShadeBackground();

    M_WriteTextCentered(25, "GAMEPLAY FEATURES", cr[CR_YELLOW]);

    // Default skill level
    M_snprintf(str, sizeof(str), "%s", DefSkillName[crl_default_skill]);
    M_WriteText (M_ItemRightAlign(str), 34, str, 
                 M_Item_Glow(0, DefSkillColor(crl_default_skill)));

    // Pistol start game mode
    sprintf(str, crl_pistol_start ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(1, crl_pistol_start ? GLOW_GREEN : GLOW_DARKRED));

    // Colored status bar
    sprintf(str, crl_colored_stbar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 52, str,
                 M_Item_Glow(2, crl_colored_stbar ? GLOW_GREEN : GLOW_DARKRED));

    // Report revealed secrets
    sprintf(str, crl_revealed_secrets == 1 ? "TOP" :
                 crl_revealed_secrets == 2 ? "CENTERED" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 61, str,
                 M_Item_Glow(3, crl_revealed_secrets ? GLOW_GREEN : GLOW_DARKRED));

    // Restore monster target from savegames
    sprintf(str, crl_restore_targets ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 70, str,
                 M_Item_Glow(4, crl_restore_targets ? GLOW_GREEN : GLOW_DARKRED));

    M_WriteTextCentered(79, "DEMOS", cr[CR_YELLOW]);

    // Demo timer
    sprintf(str, crl_demo_timer == 1 ? "PLAYBACK" : 
                 crl_demo_timer == 2 ? "RECORDING" : 
                 crl_demo_timer == 3 ? "ALWAYS" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 88, str,
                 M_Item_Glow(6, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Timer direction
    sprintf(str, crl_demo_timerdir ? "BACKWARD" : "FORWARD");
    M_WriteText (M_ItemRightAlign(str), 97, str,
                 M_Item_Glow(7, crl_demo_timer ? GLOW_GREEN : GLOW_DARKRED));

    // Progress bar
    sprintf(str, crl_demo_bar ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 106, str,
                 M_Item_Glow(8, crl_demo_bar ? GLOW_GREEN : GLOW_DARKRED));

    // Print informatime message if extended HUD is off.
    if (itemOn > 5 && itemOn < 9 && !crl_extended_hud)
    {
        M_WriteTextCentered(151, "HIDDEN WHILE EXTENDED HUD IS OFF", cr[CR_GRAY]);
    }

    // Play internal demos
    sprintf(str, crl_internal_demos ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 115, str,
                 M_Item_Glow(9, crl_internal_demos ? GLOW_DARKRED : GLOW_GREEN));
}

static void M_CRL_DefaulSkill (int choice)
{
    crl_default_skill = M_INT_Slider(crl_default_skill, 0, 4, choice);
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
    crl_revealed_secrets = M_INT_Slider(crl_revealed_secrets, 0, 2, choice);
}

static void M_CRL_RestoreTargets (int choice)
{
    crl_restore_targets ^= 1;
}

static void M_CRL_DemoTimer (int choice)
{
    crl_demo_timer = M_INT_Slider(crl_demo_timer, 0, 3, choice);
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
// Static engine limits
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Limits[]=
{
    { 2, "SAVE GAME LIMIT WARNING",    M_CRL_SaveSizeWarning, 's'},
    { 2, "RENDER LIMITS LEVEL",        M_CRL_Limits,          'r'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Limits =
{
    m_crl_end,
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

    M_ShadeBackground();

    M_WriteTextCentered(25, "STATIC ENGINE LIMITS", cr[CR_YELLOW]);

    // Savegame limit warning
    sprintf(str, vanilla_savegame_limit ? "ON" : "OFF");
    M_WriteText (M_ItemRightAlign(str), 34, str,
                 M_Item_Glow(0, vanilla_savegame_limit ? GLOW_GREEN : GLOW_DARKRED));

    // Level of the limits
    sprintf(str, crl_vanilla_limits ? "VANILLA" : "DOOM-PLUS");
    M_WriteText (M_ItemRightAlign(str), 43, str,
                 M_Item_Glow(1, crl_vanilla_limits ? GLOW_RED : GLOW_GREEN));

    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16,  61, "MAXVISPLANES",  cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16,  70, "MAXDRAWSEGS",   cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16,  79, "MAXVISSPRITES", cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16,  88, "MAXOPENINGS",   cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16,  97, "MAXPLATS",      cr[CR_GRAY]);
    M_WriteText (CRL_MENU_LEFTOFFSET_SML+16, 106, "MAXLINEANIMS",  cr[CR_GRAY]);

    if (crl_vanilla_limits)
    {
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("128"),    61,   "128", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("256"),    70,   "256", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("128"),    79,   "128", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("20480"),  88, "20480", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("30"),     97,    "30", cr[CR_RED]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("64"),    106,    "64", cr[CR_RED]);
    }
    else
    {
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("1024"),   61,  "1024", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("2048"),   70,  "2048", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("1024"),   79,  "1024", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("65536"),  88, "65536", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("7680"),   97,  "7680", cr[CR_GREEN]);
        M_WriteText (CRL_MENU_RIGHTOFFSET_SML-16 - M_StringWidth("16384"), 106, "16384", cr[CR_GREEN]);
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

    // [JN] Print "modified" (or created initially) time of
    // savegame file in YYYY-MM-DD HH:MM:SS format.
    if (LoadMenu[itemOn].status)
    {
        struct stat filestat;
        char filedate[32];

        stat(P_SaveGameFile(itemOn), &filestat);
        strftime(filedate, sizeof(filedate), "%Y-%m-%d %X", localtime(&filestat.st_mtime));
        M_WriteTextCentered(160, filedate, cr[CR_MENU_DARK2]);
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
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
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
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i+7);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_", NULL);
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
	M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
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

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (sfx_vol + 1), 16, sfxVolume);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (226, 83, str, sfxVolume ? NULL : cr[CR_DARKRED]);

    M_DrawThermo(SoundDef.x, SoundDef.y + LINEHEIGHT * (music_vol + 1), 16, musicVolume);
    sprintf(str,"%d", musicVolume);
    M_WriteText (226, 115, str, musicVolume ? NULL : cr[CR_DARKRED]);
}

static void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

static void M_SfxVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (sfxVolume)
	    sfxVolume--;
	break;
      case 1:
	if (sfxVolume < 15)
	    sfxVolume++;
	break;
    }
	
    S_SetSfxVolume(sfxVolume * 8);
}

static void M_MusicVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (musicVolume)
	    musicVolume--;
	break;
      case 1:
	if (musicVolume < 15)
	    musicVolume++;
	break;
    }
	
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
	
    V_DrawShadowedPatch(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		        W_CacheLumpName(DEH_String(detailNames[detailLevel]), PU_CACHE),
                                DEH_String(detailNames[detailLevel]));

    V_DrawShadowedPatch(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                W_CacheLumpName(DEH_String(msgNames[showMessages]), PU_CACHE),
                                DEH_String(msgNames[showMessages]));

    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,crl_screen_size-3);
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
    // warning: unused parameter `int choice'
    choice = 0;
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
    choice = 0;
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
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

static void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

static void M_FinishReadThis(int choice)
{
    choice = 0;
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
    choice = 0;
    detailLevel = 1 - detailLevel;

    R_SetViewSize (crl_screen_size, detailLevel);

    if (!detailLevel)
	CRL_SetMessage(&players[consoleplayer], DEH_String(DETAILHI), false, NULL);
    else
	CRL_SetMessage(&players[consoleplayer], DEH_String(DETAILLO), false, NULL);
}




static void M_SizeDisplay(int choice)
{
    switch(choice)
    {
      case 0:
	if (crl_screen_size > 3)
	{
	    crl_screen_size--;
	}
	break;
      case 1:
	if (crl_screen_size < 13)
	{
	    crl_screen_size++;
	}
	break;
    }
	

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
  int	thermDot )
{
    int		xx;
    int		i;
    const char	*m_therml = DEH_String("M_THERML");
    const char	*m_thermm = DEH_String("M_THERMM");
    const char	*m_thermr = DEH_String("M_THERMR");
    const char	*m_thermo = DEH_String("M_THERMO");

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
const int M_StringWidth(const char* string)
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
static const int M_StringHeight(const char* string)
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

        c = c - HU_FONTSTART;

        if (c < 0 || c> HU_FONTSIZE)
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

    if (gamestate == GS_LEVEL)
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
            S_StartSound(NULL,sfx_swtchn);
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
	    if (mousey < lasty-30)
	    {
		key = key_menu_down;
		mousewait = I_GetTime() + 5;
		mousey = lasty -= 30;
	    }
	    else if (mousey > lasty+30)
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
        if (MouseIsBinding && ev->data1)
        {
            M_CheckMouseBind(SDL_mouseButton);
            M_DoMouseBind(btnToBind, SDL_mouseButton);
            btnToBind = 0;
            MouseIsBinding = false;
            mousewait = I_GetTime() + 15;
            return true;
        }

	    if (ev->data1&1)
	    {
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
			
	    if (ev->data1&2)
	    {
		if (messageToPrint && messageNeedsInput)
		{
		key = key_menu_abort;  // [JN] Cancel by right mouse button.
		}
		else
		if (saveStringEnter)
		{
		key = key_menu_abort;
		saveStringEnter = 0;
		}
		else
		{
		key = key_menu_back;
		}
		mousewait = I_GetTime() + 5;
	    }

	    // [crispy] scroll menus with mouse wheel
	    // [JN] Hardcoded to always use mouse wheel up/down.
	    if (/*mousebprevweapon >= 0 &&*/ ev->data1 & (1 << 4 /*mousebprevweapon*/))
	    {
		key = key_menu_down;
		mousewait = I_GetTime() + 1;
	    }
	    else
	    if (/*mousebnextweapon >= 0 &&*/ ev->data1 & (1 << 3 /*mousebnextweapon*/))
	    {
		key = key_menu_up;
		mousewait = I_GetTime() + 1;
	    }
	}
	else
	{
	    if (ev->type == ev_keydown)
	    {
		key = ev->data1;
		ch = ev->data2;
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
            M_StringCopy(savegamestrings[saveSlot], saveOldString,
                         SAVESTRINGSIZE);
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

	    if (ch >= 32 && ch <= 127 &&
		saveCharIndex < SAVESTRINGSIZE-1 &&
		M_StringWidth(savegamestrings[saveSlot]) <
		(SAVESTRINGSIZE-2)*8)
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
            if (key != ' ' && key != KEY_ESCAPE
             && key != key_menu_confirm && key != key_menu_abort
             // [JN] Allow to confirm nightmare, end game and quit by pressing Enter.
             && key != key_menu_forward)
            {
                return false;
            }
	}

	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(key);

	// [JN] Do not close Save/Load menu after deleting a savegame.
	if (currentMenu != &SaveDef && currentMenu != &LoadDef
	// [JN] Do not close Options menu after pressing "N" in End Game.
	&&  currentMenu != &CRLDef_Main
	// [JN] Do not close bindings menu keyboard/mouse binds reset.
	&&  currentMenu != &CRLDef_Keybinds_7 && currentMenu != &CRLDef_MouseBinds)
	menuactive = false;
	S_StartSound(NULL,sfx_swtchx);
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

    if ((devparm && key == key_menu_help) ||
        (key != 0 && key == key_menu_screenshot))
    {
	S_StartSound(NULL,sfx_tink);    // [JN] Add audible feedback
	G_ScreenShot ();
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
	    S_StartSound(NULL,sfx_stnmov);
	    return true;
	}
        else if (key == key_menu_incscreen) // Screen size up
        {
	    if (automapactive)
		return false;
	    M_SizeDisplay(1);
	    S_StartSound(NULL,sfx_stnmov);
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
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
        else if (key == key_menu_save)     // Save
        {
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_SaveGame(0);
	    return true;
        }
        else if (key == key_menu_load)     // Load
        {
	    M_StartControlPanel();
	    S_StartSound(NULL,sfx_swtchn);
	    M_LoadGame(0);
	    return true;
        }
        else if (key == key_menu_volume)   // Sound Volume
        {
	    M_StartControlPanel ();
	    currentMenu = &SoundDef;
	    itemOn = sfx_vol;
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
	}
        else if (key == key_menu_detail)   // Detail toggle
        {
	    M_ChangeDetail(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
        }
        else if (key == key_menu_qsave)    // Quicksave
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickSave();
	    return true;
        }
        else if (key == key_menu_endgame)  // End game
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_EndGame(0);
	    return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
	    M_ChangeMessages(0);
	    S_StartSound(NULL,sfx_swtchn);
	    return true;
        }
        else if (key == key_menu_qload)    // Quickload
        {
	    S_StartSound(NULL,sfx_swtchn);
	    M_QuickLoad();
	    return true;
        }
        else if (key == key_menu_quit)     // Quit DOOM
        {
	    S_StartSound(NULL,sfx_swtchn);
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
        if (++crl_gamma > 14)
        {
            crl_gamma = 0;
        }
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
		
		S_StartSound(NULL,sfx_swtchn);
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
        // Move down to next item

        do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item

	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartSound(NULL,sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_left)
    {
	// [JN] Go to previous-left menu by pressing Left Arrow.
	if (currentMenu->ScrollAR)
	{
	    M_ScrollPages(false);
	}
        // Slide slider left

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
    }
    else if (key == key_menu_right)
    {
	// [JN] Go to next-right menu by pressing Right Arrow.
	if (currentMenu->ScrollAR)
	{
	    M_ScrollPages(true);
	}
        // Slide slider right

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartSound(NULL,sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    currentMenu->lastOn = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
	    {
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
		S_StartSound(NULL,sfx_stnmov);
	    }
	    else
	    {
		currentMenu->menuitems[itemOn].routine(itemOn);
		S_StartSound(NULL,sfx_pistol);
	    }
	}
	return true;
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu

	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu

	currentMenu->lastOn = itemOn;
	if (currentMenu->prevMenu)
	{
	    currentMenu = currentMenu->prevMenu;
	    itemOn = currentMenu->lastOn;
	    S_StartSound(NULL,sfx_swtchn);
	}
	// [JN] Close menu if pressed "back" in Doom main or CRL main menu.
	else
	if (currentMenu == &MainDef || currentMenu == &CRLDef_Main)
	{
	    S_StartSound(NULL,sfx_swtchx);
	    M_ClearMenus();
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
		S_StartSound(NULL,sfx_pstop);
		return true;
	    }
        }

	for (i = 0;i <= itemOn;i++)
        {
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartSound(NULL,sfx_pstop);
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
    itemOn = currentMenu->lastOn;   // JDC
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

    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	M_ShadeBackground();
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
	return;

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i=0;i<max;i++)
    {
        name = DEH_String(currentMenu->menuitems[i].name);

        if (currentMenu->smallFont)
        {
            if (itemOn == i)
            {
                // [JN] Highlight menu item on which the cursor is positioned.
                M_WriteText (x, y, name, cr[CR_MENU_BRIGHT5]);
            }
            else
            {
                // [JN] Apply fading effect in M_Ticker.
                M_WriteText (x, y, name, M_Line_Glow(currentMenu->menuitems[i].tics));
            }
            y += CRL_MENU_LINEHEIGHT_SMALL;
        }
        else
        {
            if (name[0])
            {
                V_DrawShadowedPatch(x, y, W_CacheLumpName(name, PU_CACHE), name);
            }
            y += LINEHEIGHT;
        }
    }

    if (currentMenu->smallFont)
    {
        // [JN] Draw glowing * symbol.
        M_WriteText(x - CRL_MENU_CURSOR_OFFSET, currentMenu->y + itemOn * CRL_MENU_LINEHEIGHT_SMALL,
                    "*", M_Cursor_Glow(cursor_tics));
    }
    else
    {
        // DRAW SKULL
        V_DrawShadowedPatch(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
                            W_CacheLumpName(DEH_String(skullName[whichSkull]), PU_CACHE),
                            DEH_String(skullName[whichSkull]));
    }
}


//
// M_ClearMenus
//
static void M_ClearMenus (void)
{
    menuactive = 0;
}




//
// M_SetupNextMenu
//
static void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
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

    if (currentMenu->smallFont)
    {
        for (int i = 0 ; i < currentMenu->numitems ; i++)
        {
            if (itemOn == i)
            {
                // Keep menu item bright
                currentMenu->menuitems[i].tics = 5;
            }
            else
            {
                // Decrease tics for glowing effect
                currentMenu->menuitems[i].tics--;
            }
        }
    }
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
    if (key_crl_freeze == key)       key_crl_freeze       = 0;
    if (key_crl_buddha == key)       key_crl_buddha       = 0;
    if (key_crl_notarget == key)     key_crl_notarget     = 0;
    if (key_crl_nomomentum == key)   key_crl_nomomentum   = 0;
    // Page 3
    if (key_crl_autorun == key)      key_crl_autorun      = 0;
    if (key_crl_novert == key)       key_crl_novert       = 0;
    if (key_crl_vilebomb == key)     key_crl_vilebomb     = 0;
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
        case 208:  key_crl_freeze = key;        break;
        case 209:  key_crl_buddha = key;        break;
        case 210:  key_crl_notarget = key;      break;
        case 211:  key_crl_nomomentum = key;    break;
        // Page 3  
        case 300:  key_crl_autorun = key;       break;
        case 301:  key_crl_novert = key;        break;
        case 302:  key_crl_vilebomb = key;      break;
        case 303:  key_crl_clearmax = key;      break;
        case 304:  key_crl_movetomax = key;     break;
        case 305:  key_crl_iddqd = key;         break;
        case 306:  key_crl_idkfa = key;         break;
        case 307:  key_crl_idfa = key;          break;
        case 308:  key_crl_idclip = key;        break;
        case 309:  key_crl_iddt = key;          break;
        case 310:  key_crl_mdk = key;           break;
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
        case 507:  key_map_grid = key;          break;
        case 508:  key_map_mark = key;          break;
        case 509:  key_map_clearmark = key;     break;
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
            case 9:   key_crl_freeze = 0;       break;
            case 10:  key_crl_buddha = 0;       break;
            case 11:  key_crl_notarget = 0;     break;
            case 12:  key_crl_nomomentum = 0;   break;
        }
    }
    if (currentMenu == &CRLDef_Keybinds_3)
    {
        switch (itemOn)
        {
            case 0:   key_crl_autorun = 0;      break;
            case 1:   key_crl_novert = 0;       break;
            case 2:   key_crl_vilebomb = 0;     break;
            // Visplanes MAX value title
            case 4:   key_crl_clearmax = 0;     break;
            case 5:   key_crl_movetomax = 0;    break;
            // Cheat shortcuts title
            case 7:   key_crl_iddqd = 0;        break;
            case 8:   key_crl_idkfa = 0;        break;
            case 9:   key_crl_idfa = 0;         break;
            case 10:  key_crl_idclip = 0;       break;
            case 11:  key_crl_iddt = 0;         break;
            case 12:  key_crl_mdk = 0;          break;
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
            case 7:   key_map_grid = 0;         break;
            case 8:   key_map_mark = 0;         break;
            case 9:   key_map_clearmark = 0;    break;
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
    key_crl_freeze = 0;
    key_crl_buddha = 0;
    key_crl_notarget = 0;
    key_crl_nomomentum = 0;
    // Page 3
    key_crl_autorun = KEY_CAPSLOCK;
    key_crl_novert = 0;
    key_crl_vilebomb = 0;
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
    M_WriteTextCentered(162, "PRESS ENTER TO BIND, DEL TO CLEAR",  cr[CR_MENU_DARK1]);

    if (drawPages)
    {
        M_WriteText(CRL_MENU_LEFTOFFSET, 171, "< PGUP", cr[CR_MENU_DARK3]);
        M_WriteTextCentered(171, M_StringJoin("PAGE ", pagenum, "/7", NULL), cr[CR_MENU_DARK2]);
        M_WriteText(M_ItemRightAlign("PGDN >"), 171, "PGDN >", cr[CR_MENU_DARK3]);
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
    mousebstrafe = 1;
    mousebbackward = -1;
    mousebuse = -1;
    mousebstrafeleft = -1;
    mousebstraferight = -1;
    mousebprevweapon = 4;
    mousebnextweapon = 3;
}
