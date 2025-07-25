//
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


#pragma once


// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables.
// -----------------------------------------------------------------------------

// Compatibility
extern int vanilla_savegame_limit;

// System and video
extern int crl_startup_delay;
extern int crl_resize_delay;
extern int crl_uncapped_fps;
extern int crl_fpslimit;
extern int crl_vsync;
extern int crl_showfps;
extern int crl_visplanes_drawing;
extern int crl_hom_effect;
extern int crl_gamma;
extern int crl_menu_shading;
extern int crl_level_brightness;
extern int crl_msg_critical;
extern int crl_screen_size;
extern int crl_screenwipe;
extern int crl_text_shadows;

// Game modes
extern int crl_spectating;
extern int crl_freeze;

// Widgets
extern int crl_extended_hud;
extern int crl_widget_playstate;
extern int crl_widget_render;
extern int crl_widget_maxvp;
extern int crl_widget_kis;
extern int crl_widget_kis_format;
extern int crl_widget_kis_items;
extern int crl_widget_time;
extern int crl_widget_coords;
extern int crl_widget_speed;
extern int crl_widget_powerups;
extern int crl_widget_health;

// Sound
extern int crl_monosfx;

// Automap
extern int crl_automap_mode;
extern int crl_automap_secrets;
extern int crl_automap_rotate;
extern int crl_automap_overlay;
extern int crl_automap_shading;
extern int crl_automap_sndprop;

// Gameplay features
extern int crl_default_skill;
extern int crl_pistol_start;
extern int crl_colored_stbar;
extern int crl_revealed_secrets;
extern int crl_restore_targets;
extern int crl_death_use_action;

// Demos
extern int crl_demo_timer;
extern int crl_demo_timerdir;
extern int crl_demo_bar;
extern int crl_internal_demos;

// Miscellaneous
extern int crl_a11y_invul;
extern int crl_a11y_pal_flash;
extern int crl_a11y_move_bob;
extern int crl_a11y_weapon_bob;
extern int crl_colorblind;
extern int crl_autoload_wad;
extern int crl_autoload_deh;
extern int crl_menu_highlight;
extern int crl_menu_esc_key;
extern int crl_confirm_quit;

// Static limits
extern int crl_vanilla_limits;

// Mouse look
extern int crl_mouselook;

extern void CRL_BindVariables (void);
