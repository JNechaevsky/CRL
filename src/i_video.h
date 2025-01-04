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
// DESCRIPTION:
//	System specific interface stuff.
//


#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"

// Screen width and height.

#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#define SCREENAREA   (SCREENWIDTH * SCREENHEIGHT)

// Screen height used when aspect_ratio_correct=true.

#define SCREENHEIGHT_4_3 240

void *I_GetSDLWindow(void);
void *I_GetSDLRenderer(void);

typedef boolean (*grabmouse_callback_t)(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void);

void I_GraphicsCheckCommandLine(void);

void I_ShutdownGraphics(void);

void I_RenderReadPixels (byte **data, int *w, int *h);

// Takes full 8 bit values.
void I_SetPalette (byte* palette);
int I_GetPaletteIndex(int r, int g, int b);

void I_FinishUpdate (void);

void I_ReadScreen (pixel_t* scr);

void I_BeginRead (void);

void I_SetWindowTitle(const char *title);

void I_CheckIsScreensaver(void);
void I_SetGrabMouseCallback(grabmouse_callback_t func);

void I_DisplayFPSDots(boolean dots_on);
void I_BindVideoVariables(void);

void I_InitWindowTitle(void);
void I_RegisterWindowIcon(const unsigned int *icon, int width, int height);
void I_InitWindowIcon(void);

// Called before processing any tics in a frame (just after displaying a frame).
// Time consuming syncronous operations are performed here (joystick reading).

void I_StartFrame (void);

// Called before processing each tic in a frame.
// Quick syncronous operations are performed here.

void I_StartTic (void);

void I_StartDisplay (void); // [crispy]

// Enable the loading disk image displayed when reading from disk.

void I_EnableLoadingDisk(int xoffs, int yoffs);

extern char *video_driver;
extern boolean screenvisible;

extern int vanilla_keyboard_mapping;
extern boolean screensaver_mode;
extern pixel_t *I_VideoBuffer;

extern int screen_width;
extern int screen_height;
extern int fullscreen;
extern int aspect_ratio_correct;
extern int smooth_scaling;
extern int integer_scaling;
extern int vga_porch_flash;
extern int force_software_renderer;

extern char *window_position;
void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
extern unsigned int joywait;

extern int usemouse;

extern int menu_mouse_x;
extern int menu_mouse_y;
extern boolean menu_mouse_allow;
extern void I_ReInitCursorPosition (void);

extern void I_ToggleVsync (void);
extern void I_TogglePixelScaling (void);

extern boolean endoom_screen_active;

#endif
