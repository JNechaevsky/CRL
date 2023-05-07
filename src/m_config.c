//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//    Configuration file interface.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "SDL_filesystem.h"

#include "config.h"

#include "doomtype.h"
#include "doomkeys.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"

#include "z_zone.h"

//
// DEFAULTS
//

// Location where all configuration data is stored - 
// default.cfg, savegames, etc.

char *configdir;

static char *autoload_path = "";

// Default filenames for configuration files.

static char *default_main_config;

typedef enum 
{
    DEFAULT_INT,
    DEFAULT_INT_HEX,
    DEFAULT_STRING,
    DEFAULT_FLOAT,
    DEFAULT_KEY,
} default_type_t;

typedef struct
{
    // Name of the variable
    char *name;

    // Pointer to the location in memory of the variable
    union {
        int *i;
        char **s;
        float *f;
    } location;

    // Type of the variable
    default_type_t type;

    // If this is a key value, the original integer scancode we read from
    // the config file before translating it to the internal key value.
    // If zero, we didn't read this value from a config file.
    int untranslated;

    // The value we translated the scancode into when we read the 
    // config file on startup.  If the variable value is different from
    // this, it has been changed and needs to be converted; otherwise,
    // use the 'untranslated' value.
    int original_translated;

    // If true, this config variable has been bound to a variable
    // and is being used.
    boolean bound;
} default_t;

typedef struct
{
    default_t *defaults;
    int numdefaults;
    char *filename;
} default_collection_t;

#define CONFIG_VARIABLE_GENERIC(name, type) \
    { #name, {NULL}, type, 0, 0, false }

#define CONFIG_VARIABLE_KEY(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_KEY)
#define CONFIG_VARIABLE_INT(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_INT)
#define CONFIG_VARIABLE_INT_HEX(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_INT_HEX)
#define CONFIG_VARIABLE_FLOAT(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_FLOAT)
#define CONFIG_VARIABLE_STRING(name) \
    CONFIG_VARIABLE_GENERIC(name, DEFAULT_STRING)

//! @begin_config_file default

static default_t	doom_defaults_list[] =
{
    // [JN] Vanilla Doom config variables:

    CONFIG_VARIABLE_INT(mouse_sensitivity),
    CONFIG_VARIABLE_INT(sfx_volume),
    CONFIG_VARIABLE_INT(music_volume),
    CONFIG_VARIABLE_INT(show_messages),
    CONFIG_VARIABLE_KEY(key_right),
    CONFIG_VARIABLE_KEY(key_left),
    CONFIG_VARIABLE_KEY(key_up),
    CONFIG_VARIABLE_KEY(key_down),
    CONFIG_VARIABLE_KEY(key_strafeleft),
    CONFIG_VARIABLE_KEY(key_straferight),
    
    CONFIG_VARIABLE_KEY(key_useHealth),
    CONFIG_VARIABLE_KEY(key_jump),

    CONFIG_VARIABLE_KEY(key_flyup),
    CONFIG_VARIABLE_KEY(key_flydown),
    CONFIG_VARIABLE_KEY(key_flycenter),
    CONFIG_VARIABLE_KEY(key_lookup),
    CONFIG_VARIABLE_KEY(key_lookdown),
    CONFIG_VARIABLE_KEY(key_lookcenter),
    
    CONFIG_VARIABLE_KEY(key_invquery),
    CONFIG_VARIABLE_KEY(key_mission),
    CONFIG_VARIABLE_KEY(key_invPop),
    CONFIG_VARIABLE_KEY(key_invKey),
    CONFIG_VARIABLE_KEY(key_invHome),
    CONFIG_VARIABLE_KEY(key_invEnd),
    
    CONFIG_VARIABLE_KEY(key_invleft),
    CONFIG_VARIABLE_KEY(key_invright),
    CONFIG_VARIABLE_KEY(key_useartifact),

    CONFIG_VARIABLE_KEY(key_fire),
    CONFIG_VARIABLE_KEY(key_use),
    CONFIG_VARIABLE_KEY(key_strafe),
    CONFIG_VARIABLE_KEY(key_speed),
    CONFIG_VARIABLE_INT(use_mouse),
    CONFIG_VARIABLE_INT(mouseb_fire),
    CONFIG_VARIABLE_INT(mouseb_strafe),
    CONFIG_VARIABLE_INT(mouseb_forward),
    CONFIG_VARIABLE_INT(use_joystick),
    CONFIG_VARIABLE_INT(joyb_fire),
    CONFIG_VARIABLE_INT(joyb_strafe),
    CONFIG_VARIABLE_INT(joyb_use),
    CONFIG_VARIABLE_INT(joyb_speed),
    CONFIG_VARIABLE_INT(screenblocks),
    CONFIG_VARIABLE_INT(detaillevel),
    CONFIG_VARIABLE_INT(snd_channels),
    CONFIG_VARIABLE_INT(snd_musicdevice),
    CONFIG_VARIABLE_INT(snd_sfxdevice),
    CONFIG_VARIABLE_INT(snd_sbport),
    CONFIG_VARIABLE_INT(snd_sbirq),
    CONFIG_VARIABLE_INT(snd_sbdma),
    CONFIG_VARIABLE_INT(snd_mport),
    CONFIG_VARIABLE_INT(usegamma),
    CONFIG_VARIABLE_STRING(chatmacro0),
    CONFIG_VARIABLE_STRING(chatmacro1),
    CONFIG_VARIABLE_STRING(chatmacro2),
    CONFIG_VARIABLE_STRING(chatmacro3),
    CONFIG_VARIABLE_STRING(chatmacro4),
    CONFIG_VARIABLE_STRING(chatmacro5),
    CONFIG_VARIABLE_STRING(chatmacro6),
    CONFIG_VARIABLE_STRING(chatmacro7),
    CONFIG_VARIABLE_STRING(chatmacro8),
    CONFIG_VARIABLE_STRING(chatmacro9),

    // [JN] CRL-specific config variables:

    CONFIG_VARIABLE_STRING(video_driver),
    CONFIG_VARIABLE_STRING(window_position),
    CONFIG_VARIABLE_INT(fullscreen),
    CONFIG_VARIABLE_INT(window_position_x),
    CONFIG_VARIABLE_INT(window_position_y),
    CONFIG_VARIABLE_INT(video_display),
    CONFIG_VARIABLE_INT(aspect_ratio_correct),
    CONFIG_VARIABLE_INT(smooth_scaling),
    CONFIG_VARIABLE_INT(integer_scaling),
    CONFIG_VARIABLE_INT(vga_porch_flash),
    CONFIG_VARIABLE_INT(window_width),
    CONFIG_VARIABLE_INT(window_height),
    CONFIG_VARIABLE_INT(fullscreen_width),
    CONFIG_VARIABLE_INT(fullscreen_height),
    CONFIG_VARIABLE_INT(force_software_renderer),
    CONFIG_VARIABLE_INT(max_scaling_buffer_pixels),
    CONFIG_VARIABLE_INT(graphical_startup),
    CONFIG_VARIABLE_INT(show_endoom),
    CONFIG_VARIABLE_INT(show_diskicon),
    CONFIG_VARIABLE_INT(png_screenshots),
    CONFIG_VARIABLE_INT(snd_samplerate),
    CONFIG_VARIABLE_INT(snd_cachesize),
    CONFIG_VARIABLE_INT(snd_maxslicetime_ms),
    CONFIG_VARIABLE_INT(snd_pitchshift),
    CONFIG_VARIABLE_STRING(snd_musiccmd),
    CONFIG_VARIABLE_STRING(snd_dmxoption),
    CONFIG_VARIABLE_INT_HEX(opl_io_port),
    CONFIG_VARIABLE_INT(use_libsamplerate),
    CONFIG_VARIABLE_FLOAT(libsamplerate_scale),
    CONFIG_VARIABLE_STRING(autoload_path),

#ifdef HAVE_FLUIDSYNTH
    CONFIG_VARIABLE_INT(fsynth_chorus_active),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_depth),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_level),
    CONFIG_VARIABLE_INT(fsynth_chorus_nr),
    CONFIG_VARIABLE_FLOAT(fsynth_chorus_speed),
    CONFIG_VARIABLE_STRING(fsynth_midibankselect),
    CONFIG_VARIABLE_INT(fsynth_polyphony),
    CONFIG_VARIABLE_INT(fsynth_reverb_active),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_damp),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_level),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_roomsize),
    CONFIG_VARIABLE_FLOAT(fsynth_reverb_width),
    CONFIG_VARIABLE_STRING(fsynth_sf_path),
#endif // HAVE_FLUIDSYNTH

    CONFIG_VARIABLE_STRING(timidity_cfg_path),
    CONFIG_VARIABLE_STRING(gus_patch_path),
    CONFIG_VARIABLE_INT(gus_ram_kb),
    CONFIG_VARIABLE_INT(vanilla_savegame_limit),
    CONFIG_VARIABLE_INT(vanilla_demo_limit),
    CONFIG_VARIABLE_INT(vanilla_keyboard_mapping),
    CONFIG_VARIABLE_STRING(player_name),
    CONFIG_VARIABLE_INT(grabmouse),
    CONFIG_VARIABLE_INT(novert),
    CONFIG_VARIABLE_FLOAT(mouse_acceleration),
    CONFIG_VARIABLE_INT(mouse_threshold),
    CONFIG_VARIABLE_INT(mouseb_strafeleft),
    CONFIG_VARIABLE_INT(mouseb_straferight),
    CONFIG_VARIABLE_INT(mouseb_use),
    CONFIG_VARIABLE_INT(mouseb_backward),
    CONFIG_VARIABLE_INT(mouseb_prevweapon),
    CONFIG_VARIABLE_INT(mouseb_nextweapon),
    CONFIG_VARIABLE_INT(mouseb_invleft),
    CONFIG_VARIABLE_INT(mouseb_invright),
    CONFIG_VARIABLE_INT(mouseb_useartifact),
    CONFIG_VARIABLE_INT(dclick_use),
    CONFIG_VARIABLE_STRING(joystick_guid),
    CONFIG_VARIABLE_INT(joystick_index),
    CONFIG_VARIABLE_INT(joystick_x_axis),
    CONFIG_VARIABLE_INT(joystick_x_invert),
    CONFIG_VARIABLE_INT(joystick_y_axis),
    CONFIG_VARIABLE_INT(joystick_y_invert),
    CONFIG_VARIABLE_INT(joystick_strafe_axis),
    CONFIG_VARIABLE_INT(joystick_strafe_invert),
    CONFIG_VARIABLE_INT(joystick_physical_button0),
    CONFIG_VARIABLE_INT(joystick_physical_button1),
    CONFIG_VARIABLE_INT(joystick_physical_button2),
    CONFIG_VARIABLE_INT(joystick_physical_button3),
    CONFIG_VARIABLE_INT(joystick_physical_button4),
    CONFIG_VARIABLE_INT(joystick_physical_button5),
    CONFIG_VARIABLE_INT(joystick_physical_button6),
    CONFIG_VARIABLE_INT(joystick_physical_button7),
    CONFIG_VARIABLE_INT(joystick_physical_button8),
    CONFIG_VARIABLE_INT(joystick_physical_button9),
    CONFIG_VARIABLE_INT(joystick_physical_button10),
    CONFIG_VARIABLE_INT(joyb_strafeleft),
    CONFIG_VARIABLE_INT(joyb_straferight),
    CONFIG_VARIABLE_INT(joyb_menu_activate),
    CONFIG_VARIABLE_INT(joyb_toggle_automap),
    CONFIG_VARIABLE_INT(joyb_prevweapon),
    CONFIG_VARIABLE_INT(joyb_nextweapon),

    CONFIG_VARIABLE_KEY(key_pause),
    CONFIG_VARIABLE_KEY(key_menu_activate),
    CONFIG_VARIABLE_KEY(key_menu_up),
    CONFIG_VARIABLE_KEY(key_menu_down),
    CONFIG_VARIABLE_KEY(key_menu_left),
    CONFIG_VARIABLE_KEY(key_menu_right),
    CONFIG_VARIABLE_KEY(key_menu_back),
    CONFIG_VARIABLE_KEY(key_menu_forward),
    CONFIG_VARIABLE_KEY(key_menu_confirm),
    CONFIG_VARIABLE_KEY(key_menu_abort),
    CONFIG_VARIABLE_KEY(key_menu_help),
    CONFIG_VARIABLE_KEY(key_menu_save),
    CONFIG_VARIABLE_KEY(key_menu_load),
    CONFIG_VARIABLE_KEY(key_menu_volume),
    CONFIG_VARIABLE_KEY(key_menu_detail),
    CONFIG_VARIABLE_KEY(key_menu_qsave),
    CONFIG_VARIABLE_KEY(key_menu_endgame),
    CONFIG_VARIABLE_KEY(key_menu_messages),
    CONFIG_VARIABLE_KEY(key_menu_qload),
    CONFIG_VARIABLE_KEY(key_menu_quit),
    CONFIG_VARIABLE_KEY(key_menu_gamma),
    CONFIG_VARIABLE_KEY(key_spy),
    CONFIG_VARIABLE_KEY(key_menu_incscreen),
    CONFIG_VARIABLE_KEY(key_menu_decscreen),
    CONFIG_VARIABLE_KEY(key_menu_screenshot),
    CONFIG_VARIABLE_KEY(key_menu_del),
    CONFIG_VARIABLE_KEY(key_map_toggle),
    CONFIG_VARIABLE_KEY(key_map_north),
    CONFIG_VARIABLE_KEY(key_map_south),
    CONFIG_VARIABLE_KEY(key_map_east),
    CONFIG_VARIABLE_KEY(key_map_west),
    CONFIG_VARIABLE_KEY(key_map_zoomin),
    CONFIG_VARIABLE_KEY(key_map_zoomout),
    CONFIG_VARIABLE_KEY(key_map_maxzoom),
    CONFIG_VARIABLE_KEY(key_map_follow),
    CONFIG_VARIABLE_KEY(key_map_grid),
    CONFIG_VARIABLE_KEY(key_map_mark),
    CONFIG_VARIABLE_KEY(key_map_clearmark),
    CONFIG_VARIABLE_KEY(key_weapon1),
    CONFIG_VARIABLE_KEY(key_weapon2),
    CONFIG_VARIABLE_KEY(key_weapon3),
    CONFIG_VARIABLE_KEY(key_weapon4),
    CONFIG_VARIABLE_KEY(key_weapon5),
    CONFIG_VARIABLE_KEY(key_weapon6),
    CONFIG_VARIABLE_KEY(key_weapon7),
    CONFIG_VARIABLE_KEY(key_weapon8),
    CONFIG_VARIABLE_KEY(key_prevweapon),
    CONFIG_VARIABLE_KEY(key_nextweapon),
    
    CONFIG_VARIABLE_KEY(key_arti_quartz),
    CONFIG_VARIABLE_KEY(key_arti_urn),
    CONFIG_VARIABLE_KEY(key_arti_bomb),
    CONFIG_VARIABLE_KEY(key_arti_tome),
    CONFIG_VARIABLE_KEY(key_arti_ring),
    CONFIG_VARIABLE_KEY(key_arti_chaosdevice),
    CONFIG_VARIABLE_KEY(key_arti_shadowsphere),
    CONFIG_VARIABLE_KEY(key_arti_wings),
    CONFIG_VARIABLE_KEY(key_arti_torch),
    CONFIG_VARIABLE_KEY(key_arti_morph),
    
    CONFIG_VARIABLE_KEY(key_message_refresh),
    CONFIG_VARIABLE_KEY(key_demo_quit),
    CONFIG_VARIABLE_KEY(key_multi_msg),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer1),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer2),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer3),
    CONFIG_VARIABLE_KEY(key_multi_msgplayer4),

    CONFIG_VARIABLE_KEY(key_crl_menu),
    CONFIG_VARIABLE_KEY(key_crl_spectator),
    CONFIG_VARIABLE_KEY(key_crl_cameraup),
    CONFIG_VARIABLE_KEY(key_crl_cameradown),
    CONFIG_VARIABLE_KEY(key_crl_freeze),
    CONFIG_VARIABLE_KEY(key_crl_notarget),
    CONFIG_VARIABLE_KEY(key_crl_map_rotate),
    CONFIG_VARIABLE_KEY(key_crl_map_overlay),
    CONFIG_VARIABLE_KEY(key_crl_autorun),
    CONFIG_VARIABLE_KEY(key_crl_vilebomb),
    CONFIG_VARIABLE_KEY(key_crl_clearmax),
    CONFIG_VARIABLE_KEY(key_crl_movetomax),
    
    CONFIG_VARIABLE_KEY(key_crl_iddqd),
    CONFIG_VARIABLE_KEY(key_crl_idkfa),
    CONFIG_VARIABLE_KEY(key_crl_idfa),
    CONFIG_VARIABLE_KEY(key_crl_idclip),
    CONFIG_VARIABLE_KEY(key_crl_iddt),
    
    CONFIG_VARIABLE_KEY(key_crl_nextlevel),
    CONFIG_VARIABLE_KEY(key_crl_reloadlevel),
    CONFIG_VARIABLE_KEY(key_crl_demospeed),
    CONFIG_VARIABLE_KEY(key_crl_limits),

    // System and video
    CONFIG_VARIABLE_INT(crl_startup_delay),
    CONFIG_VARIABLE_INT(crl_resize_delay),
    CONFIG_VARIABLE_INT(crl_uncapped_fps),
    CONFIG_VARIABLE_INT(crl_fpslimit),
    CONFIG_VARIABLE_INT(crl_vsync),
    CONFIG_VARIABLE_INT(crl_showfps),
    CONFIG_VARIABLE_INT(crl_visplanes_drawing),
    CONFIG_VARIABLE_INT(crl_hom_effect),
    CONFIG_VARIABLE_INT(crl_gamma),
    CONFIG_VARIABLE_INT(crl_screen_size),
    CONFIG_VARIABLE_INT(crl_screenwipe),
    CONFIG_VARIABLE_INT(crl_text_shadows),
    CONFIG_VARIABLE_INT(crl_colorblind),

    // Widgets
    CONFIG_VARIABLE_INT(crl_widget_coords),
    CONFIG_VARIABLE_INT(crl_widget_playstate),
    CONFIG_VARIABLE_INT(crl_widget_render),
    CONFIG_VARIABLE_INT(crl_widget_kis),
    CONFIG_VARIABLE_INT(crl_widget_time),
    CONFIG_VARIABLE_INT(crl_widget_powerups),
    CONFIG_VARIABLE_INT(crl_widget_health),

    // Sound
    CONFIG_VARIABLE_INT(crl_monosfx),

    // Automap
    CONFIG_VARIABLE_INT(crl_automap_mode),
    CONFIG_VARIABLE_INT(crl_automap_secrets),
    CONFIG_VARIABLE_INT(crl_automap_rotate),
    CONFIG_VARIABLE_INT(crl_automap_overlay),    

    // Gameplay features
    CONFIG_VARIABLE_INT(crl_default_skill),
    CONFIG_VARIABLE_INT(crl_pistol_start),
    CONFIG_VARIABLE_INT(crl_colored_stbar),
    CONFIG_VARIABLE_INT(crl_revealed_secrets),
    CONFIG_VARIABLE_INT(crl_restore_targets),

    // Demos
    CONFIG_VARIABLE_INT(crl_demo_timer),
    CONFIG_VARIABLE_INT(crl_demo_timerdir),
    CONFIG_VARIABLE_INT(crl_demo_bar),
    CONFIG_VARIABLE_INT(crl_internal_demos),

    // Static limits
    CONFIG_VARIABLE_INT(crl_vanilla_limits),
    CONFIG_VARIABLE_INT(crl_prevent_zmalloc),
};

static default_collection_t doom_defaults =
{
    doom_defaults_list,
    arrlen(doom_defaults_list),
    NULL,
};

// Search a collection for a variable

static default_t *SearchCollection(default_collection_t *collection, char *name)
{
    int i;

    for (i=0; i<collection->numdefaults; ++i) 
    {
        if (!strcmp(name, collection->defaults[i].name))
        {
            return &collection->defaults[i];
        }
    }

    return NULL;
}

// Mapping from DOS keyboard scan code to internal key code (as defined
// in doomkey.h). I think I (fraggle) reused this from somewhere else
// but I can't find where. Anyway, notes:
//  * KEY_PAUSE is wrong - it's in the KEY_NUMLOCK spot. This shouldn't
//    matter in terms of Vanilla compatibility because neither of
//    those were valid for key bindings.
//  * There is no proper scan code for PrintScreen (on DOS machines it
//    sends an interrupt). So I added a fake scan code of 126 for it.
//    The presence of this is important so we can bind PrintScreen as
//    a screenshot key.
static const int scantokey[128] =
{
    0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6',
    '7',    '8',    '9',    '0',    '-',    '=',    KEY_BACKSPACE, 9,
    'q',    'w',    'e',    'r',    't',    'y',    'u',    'i',
    'o',    'p',    '[',    ']',    13,		KEY_RCTRL, 'a',    's',
    'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';',
    '\'',   '`',    KEY_RSHIFT,'\\',   'z',    'x',    'c',    'v',
    'b',    'n',    'm',    ',',    '.',    '/',    KEY_RSHIFT,KEYP_MULTIPLY,
    KEY_RALT,  ' ',  KEY_CAPSLOCK,KEY_F1,  KEY_F2,   KEY_F3,   KEY_F4,   KEY_F5,
    KEY_F6,   KEY_F7,   KEY_F8,   KEY_F9,   KEY_F10,  /*KEY_NUMLOCK?*/KEY_PAUSE,KEY_SCRLCK,KEY_HOME,
    KEY_UPARROW,KEY_PGUP,KEY_MINUS,KEY_LEFTARROW,KEYP_5,KEY_RIGHTARROW,KEYP_PLUS,KEY_END,
    KEY_DOWNARROW,KEY_PGDN,KEY_INS,KEY_DEL,0,   0,      0,      KEY_F11,
    KEY_F12,  0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      KEY_PRTSCR, 0
};


static void SaveDefaultCollection(default_collection_t *collection)
{
    default_t *defaults;
    int i, v;
    FILE *f;
	
    f = M_fopen(collection->filename, "w");
    if (!f)
	return; // can't write the file, but don't complain

    defaults = collection->defaults;
		
    for (i=0 ; i<collection->numdefaults ; i++)
    {
        int chars_written;

        // Ignore unbound variables

        if (!defaults[i].bound)
        {
            continue;
        }

        // Print the name and line up all values at 30 characters

        chars_written = fprintf(f, "%s ", defaults[i].name);

        for (; chars_written < 30; ++chars_written)
            fprintf(f, " ");

        // Print the value

        switch (defaults[i].type) 
        {
            case DEFAULT_KEY:

                // use the untranslated version if we can, to reduce
                // the possibility of screwing up the user's config
                // file
                
                v = *defaults[i].location.i;

                if (v == KEY_RSHIFT)
                {
                    // Special case: for shift, force scan code for
                    // right shift, as this is what Vanilla uses.
                    // This overrides the change check below, to fix
                    // configuration files made by old versions that
                    // mistakenly used the scan code for left shift.

                    v = 54;
                }
                else if (defaults[i].untranslated
                      && v == defaults[i].original_translated)
                {
                    // Has not been changed since the last time we
                    // read the config file.

                    v = defaults[i].untranslated;
                }
                else
                {
                    // search for a reverse mapping back to a scancode
                    // in the scantokey table

                    int s;

                    for (s=0; s<128; ++s)
                    {
                        if (scantokey[s] == v)
                        {
                            v = s;
                            break;
                        }
                    }
                }

	        fprintf(f, "%i", v);
                break;

            case DEFAULT_INT:
	        fprintf(f, "%i", *defaults[i].location.i);
                break;

            case DEFAULT_INT_HEX:
	        fprintf(f, "0x%x", *defaults[i].location.i);
                break;

            case DEFAULT_FLOAT:
                fprintf(f, "%f", *defaults[i].location.f);
                break;

            case DEFAULT_STRING:
	        fprintf(f,"\"%s\"", *defaults[i].location.s);
                break;
        }

        fprintf(f, "\n");
    }

    fclose (f);
}

// Parses integer values in the configuration file

static int ParseIntParameter(char *strparm)
{
    int parm;

    if (strparm[0] == '0' && strparm[1] == 'x')
        sscanf(strparm+2, "%x", &parm);
    else
        sscanf(strparm, "%i", &parm);

    return parm;
}

static void SetVariable(default_t *def, char *value)
{
    int intparm;

    // parameter found

    switch (def->type)
    {
        case DEFAULT_STRING:
            *def->location.s = M_StringDuplicate(value);
            break;

        case DEFAULT_INT:
        case DEFAULT_INT_HEX:
            *def->location.i = ParseIntParameter(value);
            break;

        case DEFAULT_KEY:

            // translate scancodes read from config
            // file (save the old value in untranslated)

            intparm = ParseIntParameter(value);
            def->untranslated = intparm;
            if (intparm >= 0 && intparm < 128)
            {
                intparm = scantokey[intparm];
            }
            else
            {
                intparm = 0;
            }

            def->original_translated = intparm;
            *def->location.i = intparm;
            break;

        case DEFAULT_FLOAT:
            *def->location.f = (float) atof(value);
            break;
    }
}

static void LoadDefaultCollection(default_collection_t *collection)
{
    FILE *f;
    default_t *def;
    char defname[80];
    char strparm[100];

    // read the file in, overriding any set defaults
    f = M_fopen(collection->filename, "r");

    if (f == NULL)
    {
        // File not opened, but don't complain. 
        // It's probably just the first time they ran the game.

        return;
    }

    while (!feof(f))
    {
        if (fscanf(f, "%79s %99[^\n]\n", defname, strparm) != 2)
        {
            // This line doesn't match

            continue;
        }

        // Find the setting in the list

        def = SearchCollection(collection, defname);

        if (def == NULL || !def->bound)
        {
            // Unknown variable?  Unbound variables are also treated
            // as unknown.

            continue;
        }

        // Strip off trailing non-printable characters (\r characters
        // from DOS text files)

        while (strlen(strparm) > 0 && !isprint(strparm[strlen(strparm)-1]))
        {
            strparm[strlen(strparm)-1] = '\0';
        }

        // Surrounded by quotes? If so, remove them.
        if (strlen(strparm) >= 2
         && strparm[0] == '"' && strparm[strlen(strparm) - 1] == '"')
        {
            strparm[strlen(strparm) - 1] = '\0';
            memmove(strparm, strparm + 1, sizeof(strparm) - 1);
        }

        SetVariable(def, strparm);
    }

    fclose (f);
}

// Set the default filenames to use for configuration files.

void M_SetConfigFilenames(char *main_config)
{
    default_main_config = main_config;
}

//
// M_SaveDefaults
//

void M_SaveDefaults (void)
{
    SaveDefaultCollection(&doom_defaults);
}

//
// Save defaults to alternate filenames
//

void M_SaveDefaultsAlternate(char *main)
{
    char *orig_main;

    // Temporarily change the filenames

    orig_main = doom_defaults.filename;

    doom_defaults.filename = main;

    M_SaveDefaults();

    // Restore normal filenames

    doom_defaults.filename = orig_main;
}

//
// M_LoadDefaults
//

void M_LoadDefaults (void)
{
    int i;
 
    // This variable is a special snowflake for no good reason.
    M_BindStringVariable("autoload_path", &autoload_path);
 
    // check for a custom default file

    //!
    // @arg <file>
    // @vanilla
    //
    // Load main configuration from the specified file, instead of the
    // default.
    //

    i = M_CheckParmWithArgs("-config", 1);

    if (i)
    {
	doom_defaults.filename = myargv[i+1];
	printf ("	default file: %s\n",doom_defaults.filename);
    }
    else
    {
        doom_defaults.filename
            = M_StringJoin(configdir, default_main_config, NULL);
    }

    printf("  saving config in %s\n", doom_defaults.filename);

    LoadDefaultCollection(&doom_defaults);
}

// Get a configuration file variable by its name

static default_t *GetDefaultForName(char *name)
{
    default_t *result;

    // Try the main list and the extras

    result = SearchCollection(&doom_defaults, name);

    // Not found? Internal error.

    if (result == NULL)
    {
        I_Error("Unknown configuration variable: '%s'", name);
    }

    return result;
}

//
// Bind a variable to a given configuration file variable, by name.
//

void M_BindIntVariable(char *name, int *location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_INT
        || variable->type == DEFAULT_INT_HEX
        || variable->type == DEFAULT_KEY);

    variable->location.i = location;
    variable->bound = true;
}

void M_BindFloatVariable(char *name, float *location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_FLOAT);

    variable->location.f = location;
    variable->bound = true;
}

void M_BindStringVariable(char *name, char **location)
{
    default_t *variable;

    variable = GetDefaultForName(name);
    assert(variable->type == DEFAULT_STRING);

    variable->location.s = location;
    variable->bound = true;
}

// Set the value of a particular variable; an API function for other
// parts of the program to assign values to config variables by name.

boolean M_SetVariable(char *name, char *value)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound)
    {
        return false;
    }

    SetVariable(variable, value);

    return true;
}

// Get the value of a variable.

int M_GetIntVariable(char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || (variable->type != DEFAULT_INT && variable->type != DEFAULT_INT_HEX))
    {
        return 0;
    }

    return *variable->location.i;
}

const char *M_GetStringVariable(char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || variable->type != DEFAULT_STRING)
    {
        return NULL;
    }

    return *variable->location.s;
}

float M_GetFloatVariable(char *name)
{
    default_t *variable;

    variable = GetDefaultForName(name);

    if (variable == NULL || !variable->bound
     || variable->type != DEFAULT_FLOAT)
    {
        return 0;
    }

    return *variable->location.f;
}

// Get the path to the default configuration dir to use, if NULL
// is passed to M_SetConfigDir.

static char *GetDefaultConfigDir(void)
{
#if !defined(_WIN32) || defined(_WIN32_WCE)

    // Configuration settings are stored in an OS-appropriate path
    // determined by SDL.  On typical Unix systems, this might be
    // ~/.local/share/chocolate-doom.  On Windows, we behave like
    // Vanilla Doom and save in the current directory.

    char *result;

    result = SDL_GetPrefPath("", PACKAGE_TARNAME);
    return result;
#endif /* #ifndef _WIN32 */
    return M_StringDuplicate("");
}

// 
// SetConfigDir:
//
// Sets the location of the configuration directory, where configuration
// files are stored - default.cfg, chocolate-doom.cfg, savegames, etc.
//

void M_SetConfigDir(char *dir)
{
    // Use the directory that was passed, or find the default.

    if (dir != NULL)
    {
        configdir = dir;
    }
    else
    {
        configdir = GetDefaultConfigDir();
    }

    if (strcmp(configdir, "") != 0)
    {
        printf("Using %s for configuration and saves\n", configdir);
    }

    // Make the directory if it doesn't already exist:

    M_MakeDirectory(configdir);
}

//
// Calculate the path to the directory to use to store save games.
// Creates the directory as necessary.
//

char *M_GetSaveGameDir(char *iwadname)
{
    char *savegamedir;
    char *topdir;
    int p;

    //!
    // @arg <directory>
    //
    // Specify a path from which to load and save games. If the directory
    // does not exist then it will automatically be created.
    //

    p = M_CheckParmWithArgs("-savedir", 1);
    if (p)
    {
        savegamedir = myargv[p + 1];
        if (!M_FileExists(savegamedir))
        {
            M_MakeDirectory(savegamedir);
        }

        // add separator at end just in case
        savegamedir = M_StringJoin(savegamedir, DIR_SEPARATOR_S, NULL);

        printf("Save directory changed to %s.\n", savegamedir);
    }
#ifdef _WIN32
    // In -cdrom mode, we write savegames to a specific directory
    // in addition to configs.

    else if (M_ParmExists("-cdrom"))
    {
        savegamedir = configdir;
    }
#endif
    // If not "doing" a configuration directory (Windows), don't "do"
    // a savegame directory, either.
    else if (!strcmp(configdir, ""))
    {
	savegamedir = M_StringDuplicate("");
    }
    else
    {
        // ~/.local/share/chocolate-doom/savegames

        topdir = M_StringJoin(configdir, "savegames", NULL);
        M_MakeDirectory(topdir);

        // eg. ~/.local/share/chocolate-doom/savegames/doom2.wad/

        savegamedir = M_StringJoin(topdir, DIR_SEPARATOR_S, iwadname,
                                   DIR_SEPARATOR_S, NULL);

        M_MakeDirectory(savegamedir);

        free(topdir);
    }

    return savegamedir;
}

//
// Calculate the path to the directory for autoloaded WADs/DEHs.
// Creates the directory as necessary.
//
char *M_GetAutoloadDir(const char *iwadname)
{
    char *result;

    if (autoload_path == NULL || strlen(autoload_path) == 0)
    {
        char *prefdir;

#ifdef _WIN32
        // [JN] On Windows, create "autoload" directory in program folder.
        prefdir = SDL_GetBasePath();
#else
        // [JN] On other OSes use system home folder.
        prefdir = SDL_GetPrefPath("", PACKAGE_TARNAME);
#endif

        if (prefdir == NULL)
        {
            printf("M_GetAutoloadDir: SDL_GetPrefPath failed\n");
            return NULL;
        }
        autoload_path = M_StringJoin(prefdir, "autoload", NULL);
        SDL_free(prefdir);
    }

    M_MakeDirectory(autoload_path);

    result = M_StringJoin(autoload_path, DIR_SEPARATOR_S, iwadname, NULL);
    M_MakeDirectory(result);

    // TODO: Add README file

    return result;
}