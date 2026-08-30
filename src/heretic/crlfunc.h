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

// [JN] Vanilla limit of solid segs on screen.
#define CRL_MAX_SOLIDSEGS 32

extern void CRL_MoveTo_Camera (void);

extern void CRL_Clear_ALL_MAX (void);
extern void CRL_Clear_SSG_MAX (void);
extern void CRL_Clear_SEG_MAX (void);
extern void CRL_Clear_OPN_MAX (void);
extern void CRL_Clear_PLN_MAX (void);
extern void CRL_MoveTo_SSG_MAX (void);
extern void CRL_MoveTo_SEG_MAX (void);
extern void CRL_MoveTo_OPN_MAX (void);
extern void CRL_MoveTo_PLN_MAX (void);
extern void CRL_Get_Render_MAX (CRL_Render_max_t *max);
extern void CRL_MoveTo_Render_MAX (const CRL_Render_max_t *max);

// [PN] Render counters' values.
extern void CRL_CounterValue_SPR (char *const value, size_t value_size);
extern void CRL_CounterValue_SSG (char *const value, size_t value_size, char *const max, size_t max_size);
extern void CRL_CounterValue_SEG (char *const value, size_t value_size, char *const max, size_t max_size);
extern void CRL_CounterValue_OPN (char *const value, size_t value_size, char *const max, size_t max_size);
extern void CRL_CounterValue_PLN (char *const value, size_t value_size, char *const max, size_t max_size);
extern int  CRL_GetTotalVisPlanes (void);

// [crispy] demo progress bar and timer widget
extern void CRL_DemoTimer (const int time);
extern void CRL_DemoBar (void);
extern int  defdemotics, deftotaldemotics;

extern void CRL_DrawTargetsHealth (void);
extern void CRL_DrawPlayerSpeed (void);

// Power-up counters:
extern int CRL_counter_tome;
extern int CRL_counter_ring;
extern int CRL_counter_shadow;
extern int CRL_counter_wings;
extern int CRL_counter_torch;
