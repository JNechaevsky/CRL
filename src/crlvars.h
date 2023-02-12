//
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


#pragma once


// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables.
// -----------------------------------------------------------------------------

extern int crl_startup_delay;
extern int crl_resize_delay;

// Game modes
extern int crl_spectating;
extern int crl_freeze;

// Widgets
extern int crl_widget_render;
extern int crl_widget_kis;
extern int crl_widget_time;
extern int crl_widget_coords;
extern int crl_widget_health;

// Drawing
extern int crl_hom_effect;
extern int crl_visplanes_drawing;

// Automap
extern int crl_automap_mode;
extern int crl_automap_rotate;
extern int crl_automap_overlay;

// Demos
extern int crl_demo_timer;
extern int crl_demo_timerdir;
extern int crl_demo_bar;

// QOL Features
extern int crl_uncapped_fps;
extern int crl_screenwipe;
extern int crl_default_skill;
extern int crl_colored_stbar;
extern int crl_revealed_secrets;
extern int crl_colorblind;


extern void CRL_BindVariables (void);
