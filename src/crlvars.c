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


#include "m_config.h"  // [JN] M_BindIntVariable


// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables.
// -----------------------------------------------------------------------------

// Compatibility
int vanilla_savegame_limit = 1;

// System and video
int crl_startup_delay = 35;
int crl_resize_delay = 35;
int crl_uncapped_fps = 0;
int crl_fpslimit = 0;
int crl_vsync = 1;
int crl_showfps = 0;
int crl_visplanes_drawing = 0;
int crl_hom_effect = 1;
int crl_gamma = 10;
int crl_menu_shading = 12;
int crl_level_brightness = 0;
int crl_msg_critical = 1;
int crl_screen_size = 10;
int crl_screenwipe = 0;
int crl_text_shadows = 0;

// Game modes
int crl_spectating = 0;
int crl_freeze = 0;

// Widgets
int crl_extended_hud = 1;
int crl_widget_playstate = 2;
int crl_widget_render = 1;
int crl_widget_maxvp = 0;
int crl_widget_kis = 0;
int crl_widget_kis_format = 0;
int crl_widget_kis_items = 1;
int crl_widget_time = 0;
int crl_widget_coords = 0;
int crl_widget_speed = 0;
int crl_widget_powerups = 0;
int crl_widget_health = 0;

// Sound
int crl_monosfx = 0;

// Automap
int crl_automap_mode = 0;
int crl_automap_secrets = 0;
int crl_automap_rotate = 0;
int crl_automap_overlay = 0;
int crl_automap_shading = 0;
int crl_automap_sndprop = 0;

// Gameplay features
int crl_default_skill = 2;
int crl_pistol_start = 0;
int crl_colored_stbar = 0;
int crl_revealed_secrets = 0;
int crl_restore_targets = 0;
int crl_death_use_action = 0;

// Demos
int crl_demo_timer = 0;
int crl_demo_timerdir = 0;
int crl_demo_bar = 0;
int crl_internal_demos = 1;

// Miscellaneous
int crl_a11y_invul = 0;
int crl_a11y_move_bob = 20;
int crl_a11y_weapon_bob = 20;
int crl_colorblind = 0;
int crl_autoload_wad = 1;
int crl_autoload_deh = 1;
int crl_menu_highlight = 2;
int crl_menu_esc_key = 0;

// Static limits
int crl_vanilla_limits = 1;

// Mouse look
int crl_mouselook = 0;

// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables binding function.
// -----------------------------------------------------------------------------

void CRL_BindVariables (void)
{
    // Compatibility
    M_BindIntVariable("vanilla_savegame_limit",         &vanilla_savegame_limit);

    // System and video
    M_BindIntVariable("crl_startup_delay",              &crl_startup_delay);
    M_BindIntVariable("crl_resize_delay",               &crl_resize_delay);
    M_BindIntVariable("crl_uncapped_fps",               &crl_uncapped_fps);
    M_BindIntVariable("crl_fpslimit",                   &crl_fpslimit);
    M_BindIntVariable("crl_vsync",                      &crl_vsync);
    M_BindIntVariable("crl_showfps",                    &crl_showfps);
    M_BindIntVariable("crl_visplanes_drawing",          &crl_visplanes_drawing);
    M_BindIntVariable("crl_hom_effect",                 &crl_hom_effect);
    M_BindIntVariable("crl_gamma",                      &crl_gamma);
    M_BindIntVariable("crl_menu_shading",               &crl_menu_shading);
    M_BindIntVariable("crl_level_brightness",           &crl_level_brightness);
    M_BindIntVariable("crl_msg_critical",               &crl_msg_critical);
    M_BindIntVariable("crl_screen_size",                &crl_screen_size);
    M_BindIntVariable("crl_screenwipe",                 &crl_screenwipe);
    M_BindIntVariable("crl_text_shadows",               &crl_text_shadows);

    // Widgets
    M_BindIntVariable("crl_extended_hud",               &crl_extended_hud);
    M_BindIntVariable("crl_widget_playstate",           &crl_widget_playstate);
    M_BindIntVariable("crl_widget_render",              &crl_widget_render);
    M_BindIntVariable("crl_widget_maxvp",               &crl_widget_maxvp);
    M_BindIntVariable("crl_widget_kis",                 &crl_widget_kis);
    M_BindIntVariable("crl_widget_kis_format",          &crl_widget_kis_format);
    M_BindIntVariable("crl_widget_kis_items",           &crl_widget_kis_items);
    M_BindIntVariable("crl_widget_time",                &crl_widget_time);
    M_BindIntVariable("crl_widget_coords",              &crl_widget_coords);
    M_BindIntVariable("crl_widget_speed",               &crl_widget_speed);
    M_BindIntVariable("crl_widget_powerups",            &crl_widget_powerups);
    M_BindIntVariable("crl_widget_health",              &crl_widget_health);

    // Sound
    M_BindIntVariable("crl_monosfx",                    &crl_monosfx);

    // Automap
    M_BindIntVariable("crl_automap_mode",               &crl_automap_mode);
    M_BindIntVariable("crl_automap_secrets",            &crl_automap_secrets);
    M_BindIntVariable("crl_automap_rotate",             &crl_automap_rotate);
    M_BindIntVariable("crl_automap_overlay",            &crl_automap_overlay);
    M_BindIntVariable("crl_automap_shading",            &crl_automap_shading);
    M_BindIntVariable("crl_automap_sndprop",            &crl_automap_sndprop);

    // Gameplay features
    M_BindIntVariable("crl_default_skill",              &crl_default_skill);
    M_BindIntVariable("crl_pistol_start",               &crl_pistol_start);
    M_BindIntVariable("crl_colored_stbar",              &crl_colored_stbar);
    M_BindIntVariable("crl_revealed_secrets",           &crl_revealed_secrets);
    M_BindIntVariable("crl_restore_targets",            &crl_restore_targets);
    M_BindIntVariable("crl_death_use_action",           &crl_death_use_action);

    // Demos
    M_BindIntVariable("crl_demo_timer",                 &crl_demo_timer);
    M_BindIntVariable("crl_demo_timerdir",              &crl_demo_timerdir);
    M_BindIntVariable("crl_demo_bar",                   &crl_demo_bar);
    M_BindIntVariable("crl_internal_demos",             &crl_internal_demos);

    // Miscellaneous
    M_BindIntVariable("crl_a11y_invul",                 &crl_a11y_invul);
    M_BindIntVariable("crl_a11y_move_bob",              &crl_a11y_move_bob);
    M_BindIntVariable("crl_a11y_weapon_bob",            &crl_a11y_weapon_bob);
    M_BindIntVariable("crl_colorblind",                 &crl_colorblind);
    M_BindIntVariable("crl_autoload_wad",               &crl_autoload_wad);
    M_BindIntVariable("crl_autoload_deh",               &crl_autoload_deh);
    M_BindIntVariable("crl_menu_highlight",             &crl_menu_highlight);
    M_BindIntVariable("crl_menu_esc_key",               &crl_menu_esc_key);

    // Static limits
    M_BindIntVariable("crl_vanilla_limits",             &crl_vanilla_limits);

    // Mouse look
    M_BindIntVariable("crl_mouselook",                  &crl_mouselook);
}
