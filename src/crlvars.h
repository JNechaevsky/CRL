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


#pragma once


// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables.
// -----------------------------------------------------------------------------

extern int crl_startup_delay;
extern int crl_resize_delay;

// Widgets
extern int crl_widget_render;
extern int crl_widget_kis;
extern int crl_widget_time;
extern int crl_widget_coords;

// Drawing
extern int crl_hom_effect;
extern int crl_visplanes_drawing;

// Game mode
extern int crl_spectating;

// Automap
extern int crl_automap_mode;
extern int crl_automap_overlay;

// QOL Features
extern int crl_screenwipe;
extern int crl_colored_stbar;
extern int crl_colorblind;


extern void CRL_BindVariables (void);
