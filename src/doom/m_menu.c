//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
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
// DESCRIPTION:
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>

#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"
#include "d_main.h"
#include "deh_main.h"
#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
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
#include "ct_chat.h"
#include "v_trans.h"

#include "crlcore.h"
#include "crlvars.h"


extern int			show_endoom;

//
// defaulted values
//
int			mouseSensitivity = 5;

// Show messages has default, 0 = off, 1 = on
int			showMessages = 1;
	

// Blocky mode, has default, 0 = high, 1 = normal
int			detailLevel = 0;
int			screenblocks = 10;

// temp for screenblocks (0-9)
int			screenSize;

// -1 = no quicksave slot picked!
static int quickSaveSlot;

 // 1 = message to be printed
static int messageToPrint;
// ...and here is the message string!
static char *messageString;

// message x & y
static int messageLastMenuActive;

// timed message = no input from user
static boolean messageNeedsInput;

static void (*messageRoutine)(int response);

static char gammamsg[14][32] =
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
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

// we are going to be entering a savegame string
static int saveStringEnter;              
static int saveSlot;	   // which slot to save in
static int saveCharIndex;  // which char we're editing
static boolean joypadSave = false; // was the save action initiated by joypad?

// old save description before edit
static char saveOldString[SAVESTRINGSIZE];  

boolean			menuactive;

#define SKULLXOFF		-32
#define LINEHEIGHT       16
#define LINEHEIGHT_SMALL 9

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
} menuitem_t;



typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    menuitem_t*		menuitems;	// menu items
    void		(*routine)();	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
    boolean		smallFont;  // [JN] If true, use small font
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
static const int M_StringHeight(char *string);
static void M_StartMessage(char *string,void *routine,boolean input);
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
    {1,"M_OPTION",M_Options,'o'},
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
    false
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
    false
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
    false
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
    &MainDef,
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
    false
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
    false
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
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,64,
    0,
    false
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
    67,37,
    0,
    false
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
    67,37,
    0,
    false
};

// =============================================================================
// [JN] CRL custom menu
// =============================================================================

#define CRL_MENU_TOPOFFSET     (36)
#define CRL_MENU_LEFTOFFSET    (48)
#define CRL_MENU_RIGHTOFFSET   (SCREENWIDTH - CRL_MENU_LEFTOFFSET)

#define CRL_MENU_LEFTOFFSET_SML    (72)
#define CRL_MENU_RIGHTOFFSET_SML   (SCREENWIDTH - CRL_MENU_LEFTOFFSET_SML)

static player_t *player;

//static void M_ChooseCRL_Main (int choice);
static void M_DrawCRL_Main (void);
static void M_CRL_Spectating (int choice);
static void M_CRL_Freeze (int choice);
static void M_CRL_NoTarget (int choice);
static void M_CRL_NoMomentum (int choice);

static void M_ChooseCRL_Video (int choice);
static void M_DrawCRL_Video (void);
static void M_CRL_UncappedFPS (int choice);
static void M_CRL_VisplanesDraw (int choice);
static void M_CRL_HOMDraw (int choice);
static void M_CRL_ScreenWipe (int choice);
static void M_CRL_TextShadows (int choice);
static void M_CRL_ShowENDOOM (int choice);
static void M_CRL_Colorblind (int choice);

static void M_ChooseCRL_Sound (int choice);
static void M_DrawCRL_Sound (void);
static void M_CRL_SFXSystem (int choice);
static void M_CRL_MusicSystem (int choice);
static void M_CRL_SFXMode (int choice);
static void M_CRL_PitchShift (int choice);

static void M_ChooseCRL_Widgets (int choice);
static void M_DrawCRL_Widgets (void);
static void M_CRL_Widget_Coords (int choice);
static void M_CRL_Widget_Playstate (int choice);
static void M_CRL_Widget_Render (int choice);
static void M_CRL_Widget_KIS (int choice);
static void M_CRL_Widget_Time (int choice);
static void M_CRL_Widget_Powerups (int choice);
static void M_CRL_Widget_Health (int choice);
static void M_CRL_Automap (int choice);
static void M_CRL_Automap_Secrets (int choice);

static void M_ChooseCRL_Gameplay (int choice);
static void M_DrawCRL_Gameplay (void);
static void M_CRL_DefaulSkill (int choice);
static void M_CRL_PistolStart (int choice);
static void M_CRL_ColoredSTBar (int choice);
static void M_CRL_RevealedSecrets (int choice);
static void M_CRL_DemoTimer (int choice);
static void M_CRL_TimerDirection (int choice);
static void M_CRL_ProgressBar (int choice);

#ifdef _WIN32
static void M_ChooseCRL_Console (int choice);
#endif

static void M_ShadeBackground (void)
{
    // [JN] Shade background while in CRL menu.
    for (int y = 0; y < SCREENWIDTH * SCREENHEIGHT; y++)
    {
        I_VideoBuffer[y] = colormaps[12 * 256 + I_VideoBuffer[y]];
    }
}

enum
{
    m_crl_01,    // 18
    m_crl_02,    // 27
    m_crl_03,    // 36
    m_crl_04,    // 45
    m_crl_05,    // 54
    m_crl_06,    // 63
    m_crl_07,    // 72
    m_crl_08,    // 81
    m_crl_09,    // 90
    m_crl_10,    // 99
    m_crl_11,    // 108
    m_crl_12,    // 117
    m_crl_13,    // 126
    m_crl_14,    // 135
    m_crl_15,    // 144
    m_crl_end
} crl1_e;

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

static byte *DefSkillColor (const int skill)
{
    return
        skill == 0 ? cr[CR_OLIVE]     :
        skill == 1 ? cr[CR_DARKGREEN] :
        skill == 2 ? cr[CR_GREEN]     :
        skill == 3 ? cr[CR_YELLOW]    :
        skill == 4 ? cr[CR_ORANGE]    :
                     cr[CR_RED];
}

static char *const DefSkillName[5] = 
{
    "IMTYTD" ,
    "HNTR"   ,
    "HMP"    ,
    "UV"     ,
    "NM"     
};


//
// Main Menu
//

static menuitem_t CRLMenu_Main[]=
{
    { 2, "SPECTATOR MODE",       M_CRL_Spectating,      's'},
    { 2, "FREEZE MODE",          M_CRL_Freeze,          'f'},
    { 2, "NO TARGET MODE",       M_CRL_NoTarget,        'n'},
    { 2, "NO MOMENTUM MODE",     M_CRL_NoMomentum,      'n'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 1, "VIDEO OPTIONS",        M_ChooseCRL_Video,     'v'},
    { 1, "SOUND OPTIONS",        M_ChooseCRL_Sound,     's'},
    { 1, "WIDGETS AND AUTOMAP",  M_ChooseCRL_Widgets,   'w'},
    { 1, "GAMEPLAY FEATURES",    M_ChooseCRL_Gameplay,  'g'},
#ifdef _WIN32
    { 2, "SHOW CONSOLE WINDOW",  M_ChooseCRL_Console,   's'},
#else
    {-1, "", 0, '\0'},
#endif
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'}
};

static menu_t CRLDef_Main =
{
    m_crl_end,
    NULL,  // [JN] Do not jump back to main Doom menu (&MainDef).
    CRLMenu_Main,
    M_DrawCRL_Main,
    CRL_MENU_LEFTOFFSET_SML, CRL_MENU_TOPOFFSET,
    0,
    true
};

// static void M_ChooseCRL_Main (int choice)
// {
//     M_SetupNextMenu (&CRLDef_Main);
// }

static void M_DrawCRL_Main (void)
{
    static char str[32];

    M_ShadeBackground();
    M_WriteTextCentered(27, "MAIN MENU", cr[CR_YELLOW]);

    // Spectating
    sprintf(str, crl_spectating ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET_SML - M_StringWidth(str), 36, str,
                 crl_spectating ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Freeze
    sprintf(str, !singleplayer ? "N/A" :
            crl_freeze ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET_SML - M_StringWidth(str), 45, str,
                 !singleplayer ? cr[CR_DARKRED] :
                 crl_freeze ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // No target
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOTARGET ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET_SML - M_StringWidth(str), 54, str,
                 !singleplayer ? cr[CR_DARKRED] :
                 player->cheats & CF_NOTARGET ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // No momentum
    sprintf(str, !singleplayer ? "N/A" :
            player->cheats & CF_NOMOMENTUM ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET_SML - M_StringWidth(str), 63, str,
                 !singleplayer ? cr[CR_DARKRED] :
                 player->cheats & CF_NOMOMENTUM ? cr[CR_GREEN] : cr[CR_DARKRED]);

    M_WriteTextCentered(81, "SETTINGS", cr[CR_YELLOW]);

#ifdef _WIN32
    // Show console window
    sprintf(str, crl_console ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET_SML - M_StringWidth(str), 126, str,
                 crl_console ? cr[CR_GREEN] : cr[CR_DARKRED]);
#endif

    // NEXT PAGE >
    // M_WriteText(CRL_MENU_LEFTOFFSET, 153, "NEXT PAGE >", cr[CR_WHITE]);
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

static void M_CRL_NoTarget (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_NOTARGET;
}

static void M_CRL_NoMomentum (int choice)
{
    if (!singleplayer)
    {
        return;
    }

    player->cheats ^= CF_NOMOMENTUM;
}

#ifdef _WIN32
static void M_ChooseCRL_Console (int choice)
{
    CRL_ToggleWindowsConsole();
    CRL_ShowWindowsConsole();
}
#endif

// -----------------------------------------------------------------------------
// Video options
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Video[]=
{
    { 2, "UNCAPPED FRAMERATE",  M_CRL_UncappedFPS,    'u'},
    { 2, "VISPLANES DRAWING",   M_CRL_VisplanesDraw,  'v'},
    { 2, "HOM EFFECT",          M_CRL_HOMDraw,        'h'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "SCREEN WIPE EFFECT",  M_CRL_ScreenWipe,     's'},
    { 2, "TEXT CASTS SHADOWS",  M_CRL_TextShadows,    't'},
    { 2, "SHOW ENDOOM SCREEN",  M_CRL_ShowENDOOM,     's'},
    { 2, "COLORBLIND",          M_CRL_Colorblind,     'c'},
    {-1, "", 0, '\0'},
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
    true
};

static void M_ChooseCRL_Video (int choice)
{
    M_SetupNextMenu (&CRLDef_Video);
}

static void M_DrawCRL_Video (void)
{
    static char str[32];

    M_ShadeBackground();
    M_WriteTextCentered(27, "VIDEO OPTIONS", cr[CR_YELLOW]);

    // Uncapped framerate
    sprintf(str, crl_uncapped_fps ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 36, str, 
                 crl_uncapped_fps ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Visplanes drawing mode
    sprintf(str, crl_visplanes_drawing == 0 ? "NORMAL" :
                 crl_visplanes_drawing == 1 ? "FILL" :
                 crl_visplanes_drawing == 2 ? "OVERFILL" :
                 crl_visplanes_drawing == 3 ? "BORDER" : "OVERBORDER");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 45, str,
                 crl_visplanes_drawing > 0 ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // HOM effect
    sprintf(str, crl_hom_effect == 0 ? "OFF" :
                 crl_hom_effect == 1 ? "MULTICOLOR" : "BLACK");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 54, str,
                 crl_hom_effect > 0 ? cr[CR_GREEN] : cr[CR_DARKRED]);

    M_WriteTextCentered(72, "MISCELLANEOUS", cr[CR_YELLOW]);

    // Screen wipe effect
    sprintf(str, crl_screenwipe ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 81, str, 
                 crl_screenwipe ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Text casts shadows
    sprintf(str, crl_text_shadows ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 90, str, 
                 crl_text_shadows ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Screen wipe effect
    sprintf(str, show_endoom ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 99, str, 
                 show_endoom ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Colorblind
    sprintf(str, crl_colorblind == 1 ? "RED/GREEN" :
                 crl_colorblind == 2 ? "BLUE/YELLOW" :
                 crl_colorblind == 3 ? "MONOCHROME" : "NONE");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 108, str, 
                 crl_colorblind > 0 ? cr[CR_GREEN] : cr[CR_DARKRED]);
}

static void M_CRL_UncappedFPS (int choice)
{
    crl_uncapped_fps ^= 1;
    // [JN] Skip weapon bobbing interpolation for next frame.
    pspr_interp = false;
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
    crl_screenwipe ^= 1;
}

static void M_CRL_TextShadows (int choice)
{
    crl_text_shadows ^= 1;
}

static void M_CRL_ShowENDOOM (int choice)
{
    show_endoom ^= 1;
}

static void M_CRL_Colorblind (int choice)
{
    crl_colorblind = M_INT_Slider(crl_colorblind, 0, 3, choice);
    CRL_ReloadPalette();
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
    {-1, "", 0, '\0'},
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
    true
};

static void M_ChooseCRL_Sound (int choice)
{
    M_SetupNextMenu (&CRLDef_Sound);
}

static void M_DrawCRL_Sound (void)
{
    static char str[16];

    M_ShadeBackground();
    M_WriteTextCentered(27, "VOLUME", cr[CR_YELLOW]);

    M_DrawThermo(46, 45, 16, sfxVolume);
    sprintf(str,"%d", sfxVolume);
    M_WriteText (192, 48, str, !snd_sfxdevice ? cr[CR_DARKRED] :
                                    sfxVolume ? NULL : cr[CR_DARK]);

    M_DrawThermo(46, 72, 16, musicVolume);
    sprintf(str,"%d", musicVolume);
    M_WriteText (192, 75, str, !snd_musicdevice ? cr[CR_DARKRED] :
                                    musicVolume ? NULL : cr[CR_DARK]);

    M_WriteTextCentered(90, "SOUND SYSTEM", cr[CR_YELLOW]);

    // SFX playback
    sprintf(str, snd_sfxdevice == 0 ? "DISABLED"    :
                 snd_sfxdevice == 1 ? "PC SPEAKER"  :
                 snd_sfxdevice == 3 ? "DIGITAL SFX" :
                                      "UNKNOWN");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 99, str,
                 snd_sfxdevice ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Music playback
    sprintf(str, snd_musicdevice == 0 ? "DISABLED" :
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "")) ? "OPL2 SYNTH" : 
                (snd_musicdevice == 3 && !strcmp(snd_dmxoption, "-opl3")) ? "OPL3 SYNTH" : 
                 snd_musicdevice == 5 ? "GUS (EMULATED)" :
                 snd_musicdevice == 8 ? "NATIVE MIDI" :
                                        "UNKNOWN");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 108, str,
                 snd_musicdevice ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Pitch-shifted sounds
    sprintf(str, !crl_monosfx ? "STEREO" : "MONO");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 117, str,
                 !crl_monosfx ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Pitch-shifted sounds
    sprintf(str, snd_pitchshift ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 126, str,
                 snd_pitchshift ? cr[CR_GREEN] : cr[CR_DARKRED]);
}

static void M_CRL_SFXSystem (int choice)
{
    switch (choice)
    {
        case 0:
            if (snd_sfxdevice == 0)
                snd_sfxdevice = 3;
            else
            if (snd_sfxdevice == 3)
                snd_sfxdevice = 1;
            else
            if (snd_sfxdevice == 1)
                snd_sfxdevice = 0;
            break;
        case 1:
            if (snd_sfxdevice == 0)
                snd_sfxdevice = 1;
            else
            if (snd_sfxdevice == 1)
                snd_sfxdevice = 3;
            else
            if (snd_sfxdevice == 3)
                snd_sfxdevice = 0;
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
    I_InitSound(true);

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
    I_InitSound(true);

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

// -----------------------------------------------------------------------------
// Widgets and Automap
// -----------------------------------------------------------------------------

static menuitem_t CRLMenu_Widgets[]=
{
    { 2, "PLAYER COORDS",       M_CRL_Widget_Coords,     'p'},
    { 2, "PLAYSTATE COUNTERS",  M_CRL_Widget_Playstate,  'r'},
    { 2, "RENDER COUNTERS",     M_CRL_Widget_Render,     'r'},
    { 2, "KIS STATS/FRAGS",     M_CRL_Widget_KIS,        'k'},
    { 2, "LEVEL TIME",          M_CRL_Widget_Time,       'l'},
    { 2, "POWERUP TIMERS",      M_CRL_Widget_Powerups,   'p'},
    { 2, "TARGET'S HEALTH",     M_CRL_Widget_Health,     't'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "DRAWING MODE",        M_CRL_Automap,           'a'},
    { 2, "MARK SECRET SECTORS", M_CRL_Automap_Secrets,   'm'},
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
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
    true
};

static void M_ChooseCRL_Widgets (int choice)
{
    M_SetupNextMenu (&CRLDef_Widgets);
}

static void M_DrawCRL_Widgets (void)
{
    static char str[32];

    M_ShadeBackground();
    M_WriteTextCentered(27, "WIDGETS", cr[CR_YELLOW]);

    // Player coords
    sprintf(str, crl_widget_coords ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 36, str,
                 crl_widget_coords ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Playstate counters
    sprintf(str, crl_widget_playstate == 1 ? "ON" :
                 crl_widget_playstate == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 45, str,
                 crl_widget_playstate == 1 ? cr[CR_GREEN] :
                 crl_widget_playstate == 2 ? cr[CR_DARKGREEN] : cr[CR_DARKRED]);

    // Rendering counters
    sprintf(str, crl_widget_render == 1 ? "ON" :
                 crl_widget_render == 2 ? "OVERFLOWS" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 54, str,
                 crl_widget_render == 1 ? cr[CR_GREEN] :
                 crl_widget_render == 2 ? cr[CR_DARKGREEN] : cr[CR_DARKRED]);

    // K/I/S stats
    sprintf(str, crl_widget_kis ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 63, str, 
                 crl_widget_kis ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Level time
    sprintf(str, crl_widget_time ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 72, str,
                 crl_widget_time ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Powerup timers
    sprintf(str, crl_widget_powerups ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 81, str,
                 crl_widget_powerups ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Target's health
    sprintf(str, crl_widget_health == 1 ? "TOP" : 
                 crl_widget_health == 2 ? "BOTTOM" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 90, str,
                 crl_widget_health ? cr[CR_GREEN] : cr[CR_DARKRED]);

    M_WriteTextCentered(108, "AUTOMAP", cr[CR_YELLOW]);

    // Drawing mode
    sprintf(str, crl_automap_mode == 1 ? "FLOOR VISPLANES" :
                 crl_automap_mode == 2 ? "CEILING VISPLANES" : "NORMAL");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 117, str,
                 crl_automap_mode ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Mark secret sectors
    sprintf(str, crl_automap_secrets ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 126, str,
                 crl_automap_secrets ? cr[CR_GREEN] : cr[CR_DARKRED]);
}

static void M_CRL_Widget_Coords (int choice)
{
    crl_widget_coords ^= 1;
}

static void M_CRL_Widget_Playstate (int choice)
{
    crl_widget_playstate = M_INT_Slider(crl_widget_playstate, 0, 2, choice);
}

static void M_CRL_Widget_Render (int choice)
{
    crl_widget_render = M_INT_Slider(crl_widget_render, 0, 2, choice);
}

static void M_CRL_Widget_KIS (int choice)
{
    crl_widget_kis ^= 1;
}

static void M_CRL_Widget_Time (int choice)
{
    crl_widget_time ^= 1;
}

static void M_CRL_Widget_Powerups (int choice)
{
    crl_widget_powerups ^= 1;
}

static void M_CRL_Widget_Health (int choice)
{
    crl_widget_health = M_INT_Slider(crl_widget_health, 0, 2, choice);
}

static void M_CRL_Automap (int choice)
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
    {-1, "", 0, '\0'},
    {-1, "", 0, '\0'},
    { 2, "SHOW DEMO TIMER",          M_CRL_DemoTimer,        's'},
    { 2, "TIMER DIRECTION",          M_CRL_TimerDirection,   't'},
    { 2, "SHOW PROGRESS BAR",        M_CRL_ProgressBar,      's'},
    {-1, "", 0, '\0'},
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
    CRL_MENU_LEFTOFFSET, CRL_MENU_TOPOFFSET,
    0,
    true
};

static void M_ChooseCRL_Gameplay (int choice)
{
    M_SetupNextMenu (&CRLDef_Gameplay);
}

static void M_DrawCRL_Gameplay (void)
{
    static char str[32];

    M_ShadeBackground();
    M_WriteTextCentered(27, "GAMEPLAY FEATURES", cr[CR_YELLOW]);

    // Default skill level
    DEH_snprintf(str, sizeof(str), DefSkillName[crl_default_skill]);
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 36, str, 
                 DefSkillColor(crl_default_skill));

    // Pistol start game mode
    sprintf(str, crl_pistol_start ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 45, str, 
                 crl_pistol_start ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Colored status bar
    sprintf(str, crl_colored_stbar ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 54, str, 
                 crl_colored_stbar ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Report revealed secrets
    sprintf(str, crl_revealed_secrets ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 63, str, 
                 crl_revealed_secrets ? cr[CR_GREEN] : cr[CR_DARKRED]);

    M_WriteTextCentered(81, "DEMOS", cr[CR_YELLOW]);

    // Demo timer
    sprintf(str, crl_demo_timer == 1 ? "PLAYBACK" : 
                 crl_demo_timer == 2 ? "RECORDING" : 
                 crl_demo_timer == 3 ? "ALWAYS" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 90, str, 
                 crl_demo_timer ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Timer direction
    sprintf(str, crl_demo_timerdir ? "BACKWARD" : "FORWARD");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 99, str, 
                 crl_demo_timer ? cr[CR_GREEN] : cr[CR_DARKRED]);

    // Progress bar
    sprintf(str, crl_demo_bar ? "ON" : "OFF");
    M_WriteText (CRL_MENU_RIGHTOFFSET - M_StringWidth(str), 108, str, 
                 crl_demo_bar ? cr[CR_GREEN] : cr[CR_DARKRED]);
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
    crl_revealed_secrets ^= 1;
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
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
	fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
	LoadMenu[i].status = 1;
    }
}


//
// M_LoadGame & Cie.
//
static void M_DrawLoad(void)
{
    int             i;
	
    V_DrawPatch(72, 12, W_CacheLumpName(DEH_String("M_LOADG"), PU_CACHE));

    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }
}



//
// Draw border for the savegame description
//
static void M_DrawSaveLoadBorder(int x,int y)
{
    int             i;
	
    V_DrawPatch(x - 8, y + 7, W_CacheLumpName(DEH_String("M_LSLEFT"), PU_CACHE));
	
    for (i = 0;i < 24;i++)
    {
	V_DrawPatch(x, y + 7, W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
	x += 8;
    }

    V_DrawPatch(x, y + 7, W_CacheLumpName(DEH_String("M_LSRGHT"), PU_CACHE));
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
	
    V_DrawPatch(72, 12, W_CacheLumpName(DEH_String("M_SAVEG"), PU_CACHE));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i], NULL);
    }
	
    if (saveStringEnter)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_", NULL);
    }
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
    M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE - 1,
               "JOYSTICK SLOT %i", itemOn + 1);
    joypadSave = false;
}

//
// User wants to save. Start string input for M_Responder
//
static void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    saveStringEnter = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(saveOldString,savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave)
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
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
}



//
// Read This Menus - optional second page.
//
static void M_DrawReadThis2(void)
{
    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
}

static void M_DrawReadThisCommercial(void)
{
    V_DrawPatch(0, 0, W_CacheLumpName(DEH_String("HELP"), PU_CACHE));
}


//
// Change Sfx & Music volumes
//
static void M_DrawSound(void)
{
    V_DrawPatch(60, 38, W_CacheLumpName(DEH_String("M_SVOL"), PU_CACHE));

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
		 16,sfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
		 16,musicVolume);
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
    V_DrawPatch(94, 2, W_CacheLumpName(DEH_String("M_DOOM"), PU_CACHE));
}




//
// M_NewGame
//
static void M_DrawNewGame(void)
{
    V_DrawShadowedPatch(96, 14, W_CacheLumpName(DEH_String("M_NEWG"), PU_CACHE));
    V_DrawShadowedPatch(54, 38, W_CacheLumpName(DEH_String("M_SKILL"), PU_CACHE));
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
    V_DrawShadowedPatch(54, 38, W_CacheLumpName(DEH_String("M_EPISOD"), PU_CACHE));
}

static void M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm)
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
    V_DrawPatch(108, 15, W_CacheLumpName(DEH_String("M_OPTTTL"), PU_CACHE));
	
    V_DrawPatch(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		        W_CacheLumpName(DEH_String(detailNames[detailLevel]), PU_CACHE));

    V_DrawPatch(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                W_CacheLumpName(DEH_String(msgNames[showMessages]), PU_CACHE));

    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity);

    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,screenSize);
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
    if (key != key_menu_confirm)
	return;
		
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    players[consoleplayer].messageTics = 1;
    players[consoleplayer].criticalmessageTics = 1;
    players[consoleplayer].message = NULL;
    players[consoleplayer].criticalmessage = NULL;
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
    if (key != key_menu_confirm)
	return;
    if (!netgame && show_endoom) // [JN] Play exit SFX only if ENDOOM is enabled
    {
	if (gamemode == commercial)
	    S_StartSound(NULL,quitsounds2[(gametic>>2)&7]);
	else
	    S_StartSound(NULL,quitsounds[(gametic>>2)&7]);
	I_WaitVBL(105);
    }
    I_Quit ();
}


static char *M_SelectEndMessage(void)
{
    char **endmsg;

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

    R_SetViewSize (screenblocks, detailLevel);

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
	if (screenSize > 0)
	{
	    screenblocks--;
	    screenSize--;
	}
	break;
      case 1:
	if (screenSize < 8)
	{
	    screenblocks++;
	    screenSize++;
	}
	break;
    }
	

    R_SetViewSize (screenblocks, detailLevel);
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

    xx = x;
    V_DrawShadowedPatch(xx, y, W_CacheLumpName(DEH_String("M_THERML"), PU_CACHE));
    xx += 8;
    for (i=0;i<thermWidth;i++)
    {
	V_DrawShadowedPatch(xx, y, W_CacheLumpName(DEH_String("M_THERMM"), PU_CACHE));
	xx += 8;
    }
    V_DrawShadowedPatch(xx, y, W_CacheLumpName(DEH_String("M_THERMR"), PU_CACHE));

    V_DrawPatch((x + 8) + thermDot * 8, y, W_CacheLumpName(DEH_String("M_THERMO"), PU_CACHE));
}

static void
M_StartMessage
( char*		string,
  void*		routine,
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
static const int M_StringHeight(char* string)
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

        V_DrawShadowedPatch(cx, cy, hu_font[c]);
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
    int w, c, cx, cy, width;

    ch = string;
    width = 0;
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
            width += 7;
            continue;
        }

        w = SHORT (hu_font[c]->width);
        width += w;
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
        width += w;

        if (cx+w > SCREENWIDTH)
        {
            break;
        }
        
        V_DrawShadowedPatch(cx, cy, hu_font[c]);
        cx += w;
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
	    mousey += ev->data3;
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
		
	    if (ev->data1&1)
	    {
		key = key_menu_forward;
		mousewait = I_GetTime() + 15;
	    }
			
	    if (ev->data1&2)
	    {
		key = key_menu_back;
		mousewait = I_GetTime() + 15;
	    }

	    // [crispy] scroll menus with mouse wheel
	    if (mousebprevweapon >= 0 && ev->data1 & (1 << mousebprevweapon))
	    {
		key = key_menu_down;
		mousewait = I_GetTime() + 1;
	    }
	    else
	    if (mousebnextweapon >= 0 && ev->data1 & (1 << mousebnextweapon))
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
             && key != key_menu_confirm && key != key_menu_abort)
            {
                return false;
            }
	}

	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine)
	    messageRoutine(key);

	menuactive = false;
	S_StartSound(NULL,sfx_swtchx);
	return true;
    }

    if ((devparm && key == key_menu_help) ||
        (key != 0 && key == key_menu_screenshot))
    {
	S_StartSound(NULL,sfx_tink);    // [JN] Add audible feedback
	G_ScreenShot ();
	return true;
    }

    // F-Keys
    if (!menuactive)
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
        else if (key == key_menu_gamma)    // gamma toggle
        {
	    usegamma++;
	    if (usegamma > 13)
		usegamma = 0;
	    CRL_SetMessage(&players[consoleplayer], DEH_String(gammamsg[usegamma]), false, NULL);
            I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
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
    extern void I_OPL_DevMessages(char *, size_t);
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
    char               *name;
    int			start;

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
            // [JN] Highlight menu item on which the cursor is positioned.
            M_WriteText (x, y, name, itemOn == i ? cr[CR_BRIGHT] : NULL);
            y += LINEHEIGHT_SMALL;
        }
        else
        {
            if (name[0])
            {
                V_DrawShadowedPatch(x, y, W_CacheLumpName(name, PU_CACHE));
            }
            y += LINEHEIGHT;
        }
    }

    if (currentMenu->smallFont)
    {
        // [JN] Draw blinking * symbol.
        M_WriteText(x - 10, currentMenu->y + itemOn * LINEHEIGHT_SMALL, "*",
                    whichSkull ? cr[CR_BRIGHT] : NULL);
    }
    else
    {
        // DRAW SKULL
        V_DrawPatch(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
	    	        W_CacheLumpName(DEH_String(skullName[whichSkull]), PU_CACHE));
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
    screenSize = screenblocks - 3;
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

void M_ConfirmDeleteGame ()
{
	savegwarning =
	M_StringJoin("delete savegame\n\n", "\"", savegamestrings[itemOn], "\"", " ?\n\n",
	             PRESSYN, NULL);

	M_StartMessage(savegwarning, M_ConfirmDeleteGameResponse, true);
	messageToPrint = 2;
	S_StartSound(NULL,sfx_swtchn);
}
