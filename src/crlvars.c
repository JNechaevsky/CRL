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

// Detectors

int crl_medusa = 0;
int crl_intercepts = 0;

// Drawing functions

int crl_solidsegs_counter = 1;

// QOL Features

// Screen wiping effect.
int crl_screenwipe = 0;

// -----------------------------------------------------------------------------
// [JN] CRL-specific config variables binding function.
// -----------------------------------------------------------------------------

void CRL_BindVariables (void)
{
    M_BindIntVariable("crl_startup_delay",              &crl_startup_delay);
    M_BindIntVariable("crl_resize_delay",               &crl_resize_delay);

    // Detectors

    M_BindIntVariable("crl_medusa",                     &crl_medusa);
    M_BindIntVariable("crl_intercepts",                 &crl_intercepts);

    // Drawing functions

    M_BindIntVariable("crl_solidsegs_counter",          &crl_solidsegs_counter);

    // QOL Features

    M_BindIntVariable("crl_screenwipe",                 &crl_screenwipe);
}
