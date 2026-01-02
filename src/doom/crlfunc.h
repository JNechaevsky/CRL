//
// Copyright(C) 1993-1996 Id Software, Inc.
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

extern char ID_Level_Time[64];

extern void ID_FormatWidgetTime (char *buf, size_t bufsize, int ticks, int mode);

extern void CRL_MoveTo_Camera (void);

extern void CRL_Clear_MAX (void);
extern void CRL_Get_MAX (void);
extern void CRL_MoveTo_MAX (void);

// [crispy] demo progress bar and timer widget
extern void CRL_DemoTimer (const int time);
extern void CRL_DemoBar (void);
extern int  defdemotics, deftotaldemotics;

extern void CRL_DrawTargetsHealth (void);
extern void CRL_DrawPlayerSpeed (void);

// Power-up counters:
extern int CRL_invul_counter;
extern int CRL_invis_counter;
extern int CRL_rad_counter;
extern int CRL_amp_counter;

