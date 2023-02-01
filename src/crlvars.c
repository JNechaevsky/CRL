//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2023 Julia Nechaevskaya
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

// Time to wait for the screen to settle on startup before starting the game (ms).
int crl_startup_delay = 35;
// Time to wait for the screen to be updated after resizing (ms).
int crl_resize_delay = 35;

// Widgets
int crl_widget_render = 1;
int crl_widget_kis = 0;
int crl_widget_time = 0;
int crl_widget_coords = 0;

// Drawing
int crl_hom_effect = 1;
int crl_visplanes_drawing = 0;

// Game mode
int crl_spectating = 0;

// Automap
int crl_automap_mode = 0;
int crl_automap_overlay = 0;

// QOL Features
int crl_uncapped_fps = 1;
int crl_screenwipe = 0;
int crl_colored_stbar = 0;
int crl_colorblind = 0;

// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables binding function.
// -----------------------------------------------------------------------------

void CRL_BindVariables (void)
{
    M_BindIntVariable("crl_startup_delay",              &crl_startup_delay);
    M_BindIntVariable("crl_resize_delay",               &crl_resize_delay);

    // Widgets
    M_BindIntVariable("crl_widget_render",              &crl_widget_render);
    M_BindIntVariable("crl_widget_kis",                 &crl_widget_kis);
    M_BindIntVariable("crl_widget_time",                &crl_widget_time);
    M_BindIntVariable("crl_widget_coords",              &crl_widget_coords);

    // Drawing
    M_BindIntVariable("crl_hom_effect",                 &crl_hom_effect);
    M_BindIntVariable("crl_visplanes_drawing",          &crl_visplanes_drawing);

    // Automap
    M_BindIntVariable("crl_automap_mode",               &crl_automap_mode);
    M_BindIntVariable("crl_automap_overlay",            &crl_automap_overlay);

    // QOL Features
    M_BindIntVariable("crl_uncapped_fps",               &crl_uncapped_fps);
    M_BindIntVariable("crl_screenwipe",                 &crl_screenwipe);
    M_BindIntVariable("crl_colored_stbar",              &crl_colored_stbar);
    M_BindIntVariable("crl_colorblind",                 &crl_colorblind);
}
