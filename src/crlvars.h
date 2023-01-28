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

// Detectors

extern int crl_medusa;
extern int crl_intercepts;

// Drawing functions

extern int crl_solidsegs_counter;
extern int crl_visplanes_counter;
extern int crl_visplanes_drawing;
extern int crl_visplanes_merge;

// QOL Features

extern int crl_screenwipe;


extern void CRL_BindVariables (void);
