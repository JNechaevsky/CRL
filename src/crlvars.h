//
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


#pragma once


// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables.
// -----------------------------------------------------------------------------

// Compatibility
extern int vanilla_savegame_limit;
extern int vanilla_demo_limit;

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
extern int crl_colorblind;

// Game modes
extern int crl_spectating;
extern int crl_freeze;

// Widgets
extern int crl_widget_coords;
extern int crl_widget_playstate;
extern int crl_widget_render;
extern int crl_widget_maxvp;
extern int crl_widget_kis;
extern int crl_widget_time;
extern int crl_widget_powerups;
extern int crl_widget_health;

// Sound
extern int crl_monosfx;

// Automap
extern int crl_automap_mode;
extern int crl_automap_secrets;
extern int crl_automap_rotate;
extern int crl_automap_overlay;

// Gameplay features
extern int crl_default_skill;
extern int crl_pistol_start;
extern int crl_colored_stbar;
extern int crl_revealed_secrets;
extern int crl_restore_targets;

// Demos
extern int crl_demo_timer;
extern int crl_demo_timerdir;
extern int crl_demo_bar;
extern int crl_internal_demos;

// Static limits
extern int crl_vanilla_limits;
extern int crl_prevent_zmalloc;

// Mouse look
extern int crl_mouselook;

extern void CRL_BindVariables (void);
