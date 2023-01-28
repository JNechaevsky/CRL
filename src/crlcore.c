// -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-
// ----------------------------------------------------------------------------
// Chocorenderlimits 2.0 <http://remood.org/?page=chocorenderlimits>
//   Copyright (C) 2014-2015 GhostlyDeath <ghostlydeath@remood.org>
//     For more credits, see AUTHORS.
// ----------------------------------------------------------------------------
// CRL is under the GNU General Public License v2 (or later), see COPYING.
// ----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
#if !defined(CHOCORENDERLIMITS_MAGIC_INCLUDE)

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "z_zone.h"
#include "v_video.h"
#include "m_argv.h"
#include "m_misc.h"    // [JN] M_snprintf
#include "tables.h"
#include "v_trans.h"   // [JN] Color translation

#include "crlcore.h"
#include "crlvars.h"


/** Backup. */
jmp_buf CRLJustIncaseBuf;

/**
 * Stat values.
 */
CRL_Value_t CRLStatSet[] =
{
	{
		"Full",
	},
	{
		"Overflows",
	},
	{
		"None",
	}
};

/**
 * Visplane values.
 */
CRL_Value_t CRLVisplaneSet[] =
{
	{
		"None",
	},
	{
		"Fill",
	},
	{
		"OverFill",
	},
	{
		"Border",
	},
	{
		"OverBorder",
	},
};

/**
 * Merging visplanes.
 */
CRL_Value_t CRLMergeVisplaneSet[] =
{
	{
		"Default"
	},
	{
		"No CHK"
	},
	{
		"No FND"
	},
	{
		"No CHK+FND"
	},
};

/**
 * Visplane limit.
 */
CRL_Value_t CRLVisplaneLimitSet[] =
{
	{
		"128 (Vanilla)"
	},
	{
		"4096 (CRL)"
	},
	{
		"32 (Quarter)"
	}
};

/**
 * Automap set.
 */
CRL_Value_t CRLMapSet[] =
{
	{
		"None",
	},
	{
		"Visplane Floor",
	},
	{
		"Visplane Ceil",
	},
};

/**
 * Spectate mode.
 */
CRL_Value_t CRLSpectate[] =
{
	{
		"Off",
	},
	{
		"On",
	},
};

/**
 * Colorblindness.
 */
CRL_Value_t CRLColorblind[] =
{
	{
		"None",
	},
	{
		"Red/Green",
	},
	{
		"Green/Blue",
	},
	{
		"Monochrome",
	},
};

/**
 * CRL Option menu and their values.
 */
CRL_Option_t CRLOptionSet[NUM_CRL_OPTIONS] =
{
	/** Stats. */
	{
		"Draw Stats",
		NUM_CRL_STAT,
		CRLStatSet
	},
	
	/** Visplanes. */
	{
		"VisPlanes",
		NUM_CRL_VIS,
		CRLVisplaneSet
	},
	
	/** Merge visplanes. */
	{
		"Merge VisPlanes",
		NUM_CRL_MERGE,
		CRLMergeVisplaneSet
	},
	
	/** Maximum visplane limit. */
	{
		"Max VisPlanes",
		NUM_CRL_MAXVISPLANES,
		CRLVisplaneLimitSet
	},
	
	/** Automap mode. */
	{
		"Automap Mode",
		NUM_CRL_MAP,
		CRLMapSet
	},
	
	/** Spectate. */
	{
		"Spectating",
		NUM_CRL_SPECTATE,
		CRLSpectate
	},
	
	/** Colorblind. */
	{
		"Colorblind",
		NUM_CRL_COLORBLIND,
		CRLColorblind
	},
};

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

/* V_ColorEntry_t -- HSV table */
typedef union V_ColorEntry_s
{
	struct
	{
		uint8_t R;
		uint8_t G;
		uint8_t B;
	} RGB;
	
	struct
	{
		uint8_t H;
		uint8_t S;
		uint8_t V;
	} HSV;
} V_ColorEntry_t;

static V_ColorEntry_t _rgbgamecolors[256];
static V_ColorEntry_t _hsvgamecolors[256];

/* V_HSVtoRGB() -- Convert HSV to RGB */
V_ColorEntry_t V_HSVtoRGB(const V_ColorEntry_t HSV)
{
	int R, G, B, H, S, V, P, Q, T, F;
	V_ColorEntry_t Ret;
	
	/* Get inital values */
	H = HSV.HSV.H;
	S = HSV.HSV.S;
	V = HSV.HSV.V;
	
	/* Gray Color? */
	if (!S)
	{
		R = G = B = V;
	}
	
	/* Real Color */
	else
	{
		// Calculate Hue Shift
		F = ((H % 60) * 255) / 60;
		H /= 60;
		
		// Calculate channel values
		P = (V * (256 - S)) / 256;
		Q = (V * (256 - (S * F) / 256)) / 256;
		T = (V * (256 - (S * (256 - F)) / 256)) / 256;
		
		switch (H)
		{
			case 0:
				R = V;
				G = T;
				B = P;
				break;
				
			case 1:
				R = Q;
				G = V;
				B = P;
				break;
				
			case 2:
				R = P;
				G = V;
				B = T;
				break;
				
			case 3:
				R = P;
				G = Q;
				B = V;
				break;
				
			case 4:
				R = T;
				G = P;
				B = V;
				break;
				
			default:
				R = V;
				G = P;
				B = Q;
				break;
		}
	}
	
	/* Set Return */
	Ret.RGB.R = R;
	Ret.RGB.G = G;
	Ret.RGB.B = B;
	
	/* Return */
	return Ret;
}

/* V_RGBtoHSV() -- Convert RGB to HSV */
V_ColorEntry_t V_RGBtoHSV(const V_ColorEntry_t RGB)
{
	V_ColorEntry_t Ret;
	uint8_t rMin, rMax, rDif;
	
	// Get min/max
	rMin = 255;
	rMax = 0;
	
	// Get RGB minimum
	if (RGB.RGB.R < rMin)
		rMin = RGB.RGB.R;
	if (RGB.RGB.G < rMin)
		rMin = RGB.RGB.G;
	if (RGB.RGB.B < rMin)
		rMin = RGB.RGB.B;
		
	// Get RGB maximum
	if (RGB.RGB.R > rMax)
		rMax = RGB.RGB.R;
	if (RGB.RGB.G > rMax)
		rMax = RGB.RGB.G;
	if (RGB.RGB.B > rMax)
		rMax = RGB.RGB.B;
		
	// Obtain value
	Ret.HSV.V = rMax;
	
	// Short circuit?
	if (Ret.HSV.V == 0)
	{
		Ret.HSV.H = Ret.HSV.S = 0;
		return Ret;
	}
	// Obtain difference
	rDif = rMax - rMin;
	
	// Obtain saturation
	Ret.HSV.S = (uint8_t)(((uint32_t)255 * (uint32_t)rDif) / (uint32_t)Ret.HSV.V);
	
	// Short circuit?
	if (Ret.HSV.S == 0)
	{
		Ret.HSV.H = 0;
		return Ret;
	}
	
	/* Obtain hue */
	if (rMax == RGB.RGB.R)
		Ret.HSV.H = 43 * (RGB.RGB.G - RGB.RGB.B) / rMax;
	else if (rMax == RGB.RGB.G)
		Ret.HSV.H = 85 + (43 * (RGB.RGB.B - RGB.RGB.R) / rMax);
	else
		Ret.HSV.H = 171 + (43 * (RGB.RGB.R - RGB.RGB.G) / rMax);
		
	return Ret;
}

/* V_BestHSVMatch() -- Best match between HSV for tables */
static size_t V_BestHSVMatch(const V_ColorEntry_t* const Table, const V_ColorEntry_t HSV)
{
	size_t i, Best;
	V_ColorEntry_t tRGB, iRGB;
	int32_t BestSqr, ThisSqr, Dr, Dg, Db;
	
	/* Check */
	if (!Table)
		return 0;
		
	/* Convert input to RGB */
	iRGB = V_HSVtoRGB(HSV);
	
	/* Loop colors */
	for (Best = 0, BestSqr = 0x7FFFFFFFUL, i = 0; i < 256; i++)
	{
		// Convert table entry to RGB
		tRGB = V_HSVtoRGB(Table[i]);
		
		// Perfect match?
		if (iRGB.RGB.R == tRGB.RGB.R && iRGB.RGB.B == tRGB.RGB.B && iRGB.RGB.G == tRGB.RGB.G)
			return i;
			
		// Distance of colors
		Dr = tRGB.RGB.R - iRGB.RGB.R;
		Dg = tRGB.RGB.G - iRGB.RGB.G;
		Db = tRGB.RGB.B - iRGB.RGB.B;
		ThisSqr = (Dr * Dr) + (Dg * Dg) + (Db * Db);
		
		// Closer?
		if (ThisSqr < BestSqr)
		{
			Best = i;
			BestSqr = ThisSqr;
		}
	}
	
	/* Fail */
	return Best;
}

/**
 * Returns the best RGB color match.
 *
 * @param __r Red.
 * @param __g Green.
 * @param __b Blue.
 * @return The best matching color.
 * @since 2015/12/17
 */
int CRL_BestRGBMatch(int __r, int __g, int __b)
{
	V_ColorEntry_t rgb, hsv;
	
	// Setup color request
	rgb.RGB.R = __r;
	rgb.RGB.G = __g;
	rgb.RGB.B = __b;
	
	// Convert to HSV
	hsv = V_RGBtoHSV(rgb);
	
	// Find best match
	return (int)V_BestHSVMatch(_hsvgamecolors, hsv);
}

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

CRL_Data_t CRLData;

uint8_t* CRLSurface = NULL;

void** CRLPlaneSurface = NULL;

static size_t _planesize;

static int _didcolors;

#define MAXSHADES 16
#define DARKSHADE 8
#define DARKMASK 7
static uint8_t _colormap[MAXSHADES][256];

static int* _usecolors;

static int _numcolors;

/** Visplane storage. */
#define MAXCOUNTPLANES 4096
static void* _planelist[MAXCOUNTPLANES];
static int _numplanes;

static int _maxplanes;

static int _frame;

static int _pulse;
static int _pulsewas;
static int _pulsestage;

/** HOM color table. */
#define HOMCOUNT 256
#define HOMQUAD (HOMCOUNT / 4)
static int _homtable[HOMCOUNT];

/** Camera position and orientation. */
static fixed_t _campos[3];
static uint32_t _camang;

static FILE* _crllogfile;

/** Brute force? The value here is the precision. */
int CRLBruteForce = 0;

// [JN] True if intercepts overflow has happened.
// Will be reset on level restart or save game loading.
boolean CRL_intercepts_overflow = false;

/** [JN] True if level contains Medusa bug. Used in P_LoadLineDefs. */
boolean CRL_level_have_medusa;

/**
 * Initializes things.
 */
void CRL_Init(int* __colorset, int __numcolors, int __pllim)
{
	int na;	
	
	// Make plane surface
	_planesize = SCREENAREA * sizeof(*CRLPlaneSurface);
	CRLPlaneSurface = Z_Malloc(_planesize, PU_STATIC, NULL);
	memset(CRLPlaneSurface, 0, _planesize);
	
	// Set colors
	_usecolors = __colorset;
	_numcolors = __numcolors;
	
	// Limits
	_maxplanes = __pllim;
	
	// Brute forcing?
	CRLBruteForce = 0;
	if ((na = M_CheckParm("-brute")) > 0)
	{
		// Passed in
		if (na + 1 < myargc)
			CRLBruteForce = strtol(myargv[na + 1], NULL, 10);
		
		// Use some default value
		if (CRLBruteForce <= 0)
			CRLBruteForce = 16;
		
		// Note it
		printf("Enabled brute force with granularity: %d\n", CRLBruteForce);
	}
}

/**
 * Reports the position of the camera.
 *
 * @param x The x position.
 * @param y The y position.
 * @param z The z position.
 * @param angle The angle used.
 */
void CRL_ReportPosition(fixed_t x, fixed_t y, fixed_t z, uint32_t angle)
{
	_campos[0] = x;
	_campos[1] = y;
	_campos[2] = z;
	_camang = angle;
}

/**
 * Prints report.
 */
extern int gametic;
void CRL_OutputReport(void)
{
	int i;
	CRLPlaneData_t plane;
	
#define FTOD(x) (((double)(x)) / 65536.0)
#define ANGLE1 0x00b60b60
	// Need to open file?
	if (_crllogfile == NULL)
		_crllogfile = fopen("logcrl.txt", "w+t");
	
	// Well, you cannot always be a winner
	if (_crllogfile == NULL)
		return;
	
	// Print report header
	fprintf(_crllogfile, "Report gt=%d\n", (int)gametic);
	
	// Position
	fprintf(_crllogfile, "\tX x=%08x f=%g\n", (int)_campos[0], FTOD(_campos[0]));
	fprintf(_crllogfile, "\tY x=%08x f=%g\n", (int)_campos[1], FTOD(_campos[1]));
	fprintf(_crllogfile, "\tZ x=%08x f=%g\n", (int)_campos[2], FTOD(_campos[2]));
	fprintf(_crllogfile, "\tA x=%08x f=%g\n", (int)_camang, ((double)_camang / (double)ANGLE1));
	
	// Print visplane information
	fprintf(_crllogfile, "\tVISCOUNT num=%d chk=%d fnd=%d\n",
		(int)_numplanes,
		(int)CRLData.numcheckplanes,
		(int)CRLData.numfindplanes);
	for (i = 0; i < _numplanes; i++)
	{
		// Load plane info
		memset(&plane, 0, sizeof(plane));
		GAME_IdentifyPlane(_planelist[i], &plane);
		
		// Print visplane information
		fprintf(_crllogfile, "\tVISPLANE num=%d id=%d type=%s seg=%d ssub=%d sect=%d what=%c\n",
			i,
			(int)plane.id,
			(plane.isf ? "FND" : "CHK"),
			(int)plane.emitlineid,
			(int)plane.emitsubid,
			(int)plane.emitsectid,
			(plane.onfloor ? 'F' : 'C'));
	}
#undef FTOD
#undef ANGLE1
}

static byte CRL_GetLightness(byte r, byte g, byte b)
{
	V_ColorEntry_t rgb, hsv;
	int rv;
	double is, iv, ol;
	
	// Convert to HSV
	rgb.RGB.R = r;
	rgb.RGB.G = g;
	rgb.RGB.B = b;
	hsv = V_RGBtoHSV(rgb);
	
	// Convert to double space
	is = (double)hsv.HSV.S / 255.0;
	iv = (double)hsv.HSV.V / 255.0;
	
	// Calculate values
	ol = (0.5 * iv) * (2.0 - is);
	
	// Retur brightness
	rv = ol * 255;
	if (rv > 255)
		rv = 255;
	else if (rv < 0)
		rv = 0;
	return rv;
}

static void CRL_AdjustRedGreen(byte* r, byte* g, byte* b)
{
	int x = CRL_GetLightness(*r, *g, 0);
	
	*r = x;
	*g = x;
}

static void CRL_AdjustGreenBlue(byte* r, byte* g, byte* b)
{
	int x = CRL_GetLightness(0, *g, *b);
	
	*g = x;
	*b = x;
}

static void CRL_AdjustMonochrome(byte* r, byte* g, byte* b)
{
	V_ColorEntry_t rgb, hsv;
	
	// Convert to HSV
	rgb.RGB.R = *r;
	rgb.RGB.G = *g;
	rgb.RGB.B = *b;
	hsv = V_RGBtoHSV(rgb);
	
	// Use value
	*r = hsv.HSV.V;
	*g = hsv.HSV.V;
	*b = hsv.HSV.V;
}

/**
 * Sets the game colors.
 *
 * @param __colors Colors to use, RGB.
 */
void CRL_SetColors(uint8_t* colors, void* ref)
{
	static int lastcolorblind = -1;
	static void* oldref = NULL;
	
	int i, j, darksplit, idx, hueoff, count, mi, bb, k;
	int nowcolorblind;
	int qr, qg, qb;
	uint8_t* x;
	V_ColorEntry_t vc;
	void (*adjustcolor)(byte* r, byte* g, byte* b);
	
	// Current color blindness used
	nowcolorblind = CRLOptionSet[CRL_COLORBLIND].curvalue;
	
	// Only if no colors were set or colorblindness changed
	if (!_didcolors || (lastcolorblind != nowcolorblind) || (ref != oldref) ||
		nowcolorblind >= 1)
	{
		// Do no more
		_didcolors = 1;
		lastcolorblind = nowcolorblind;
		oldref = ref;
		
		// Colorblind simulation, modify colors
		adjustcolor = NULL;
		switch (nowcolorblind)
		{
				// Red/green
			case CRL_COLORBLIND_RED_GREEN:
				adjustcolor = CRL_AdjustRedGreen;
				break;
			
				// Green/Blue
			case CRL_COLORBLIND_GREEN_BLUE:
				adjustcolor = CRL_AdjustGreenBlue;
				break;
				
				// Monochrome
			case CRL_COLORBLIND_MONOCHROME:
				adjustcolor = CRL_AdjustMonochrome;
				break;
			
				// Do not touch
			default:
				break;
		}
		
		// Adjust
		if (adjustcolor != NULL)
			for (i = 0; i < 768; i += 3)
				adjustcolor(&colors[i], &colors[i + 1], &colors[i + 2]);
		
		// go through them all
		x = colors;
		for (i = 0; i < 256; i++)
		{
			// Get colors
			vc.RGB.R = *(x++);
			vc.RGB.G = *(x++);
			vc.RGB.B = *(x++);
			
			// Set RGB
			_rgbgamecolors[i] = vc;
			_hsvgamecolors[i] = V_RGBtoHSV(vc);
		}
		
		// Find best hue colors for bright and dark
		darksplit = _numcolors >> 1;
		hueoff = 0;
		count = 0;
		for (i = 0; i < 256; i++)
		{
			// Dark color hue up
			for (j = 0; j < 2; j++)
			{
				// Get the index used here
				idx = _usecolors[(j * darksplit) + (i % darksplit)];
				vc = _hsvgamecolors[idx];
				
				// Increase count
				if (count++ >= darksplit)
				{
					hueoff += 16;
					count = 0;
				}
				
				// Do not mess with color
				if (hueoff == 0)
					_colormap[DARKSHADE * j][i] = idx;
				
				// Otherwise shift it slightly
				else
				{
					// Move hue over
					vc.HSV.H = ((int)vc.HSV.H + hueoff) & 0xFF;
					
					_colormap[DARKSHADE * j][i] = V_BestHSVMatch(_hsvgamecolors, vc);
				}
			}
			
			// Make it more red
			for (j = 1; j < DARKSHADE; j++)
				for (k = 0; k < 2; k++)
				{
					// Get major index to modify
					bb = DARKSHADE * k;
					mi = bb + j;
					
					// Get the color index
					idx = _colormap[bb][i];
					
					// Get base color used
					vc = _hsvgamecolors[idx];
					
					// Make it a pulsing red
					vc.HSV.H = 0;
					
					// Move color around a bit
					{
					double inv = (255.0 * ((double)j * (1.0 / (double)DARKSHADE)));
					if (inv < 0.0)
						inv = 0.0;
					else if (inv > 255.0)
					{
						inv = 255.0;
						
					}
					vc.HSV.V = (int)inv;
					}
					
					// Set new color
					_colormap[mi][i] = V_BestHSVMatch(_hsvgamecolors, vc);
				}
		}
	}
	
	// Initialize hom colors
	for (i = 0, k = 0; i < HOMCOUNT; k++)
		for (j = 0; j < HOMQUAD; j++)
		{
			// Use specific color ranges
			qr = (k == 0 || k == 3 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			qg = (k == 1 || k == 3 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			qb = (k == 2 ?
				((int)(255.0 * ((double)(j)) / ((double)(HOMQUAD)))) : 0);
			
			// Find it and put it in
			_homtable[i++] = CRL_BestRGBMatch(qr, qg, qb);
		}
}

/**
 * Starts the rendering of a new CRL, resetting any values.
 *
 * @param __err Frame error, 0 starts, < 0 ends OK, else renderer crashed.
 */
void CRL_ChangeFrame(int __err)
{
	CRL_Data_t old;	
	int fact, lim;
	
	// Clear state on zero
	if (__err == 0)
	{
		// Frame up
		int ntframe = (_frame++) >> 1;

		// Old for max stats
		memmove(&old, &CRLData, sizeof(CRLData));
		memset(&CRLData, 0, sizeof(CRLData));
		
		// Clear old plane surface
		memset(CRLPlaneSurface, 0, _planesize);
		
		// Plane set
		memset(_planelist, 0, sizeof(_planelist));
		_numplanes = 0;
		
		// Change pulse
		if (_pulsestage == 0)
		{
			if (ntframe > _pulsewas + 35)
			{
				_pulsewas = ntframe;
				_pulse = 0;
				_pulsestage++;
			}
		}
	
		// Pulse up or down
		else if (_pulsestage == 1 || _pulsestage == 2)
		{
			if (_pulsestage == 1)
			{
				fact = 1;
				lim = DARKMASK;
			}
			else
			{
				fact = -1;
				lim = 0;
			}
			
			// Will change color
			if (ntframe > _pulsewas + 4)
			{
				_pulsewas = ntframe;
				_pulse += fact;
				if (_pulse == lim)
					_pulsestage++;
				if (_pulsestage == 3)
				{
					_pulsestage = 0;
					_pulsewas = ntframe;
				}
			}
		}
	}
}

/**
 * Draws CRL stats.
 *
 * [JN] Extended to draw sprite and segment counters, simplified.
 */
void CRL_StatDrawer(void)
{
    // Medusa
    if (crl_medusa)
    {
        M_WriteText(0, 99, "MED:", cr[CR_GRAY]);

        if (CRL_level_have_medusa)
        {
            M_WriteText(32, 99, "FOUND", gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]);
        }
        else
        {
            M_WriteText(32, 99, "OK", cr[CR_GREEN]);
        }
    }

    // Intercepts (vanilla 128 + 61 for overflow emulation)
    if (crl_intercepts)
    {
        M_WriteText(0, 108, "INT:", cr[CR_GRAY]);
        
        if (CRL_intercepts_overflow)
        {
            M_WriteText(32, 108, "OVERFLOW", gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]);
        }
        else
        {
            M_WriteText(32, 108, "OK", cr[CR_GREEN]);
        }
    }

    // Segments (256 max)
    if (crl_solidsegs_counter)
	{
		char num[16];
		
        M_WriteText(0, 126, "SEG:", cr[CR_GRAY]);

		M_snprintf(num, 16, "%d", CRLData.numsegs);
		M_WriteText(32, 126, num, CRLData.numsegs >= 256 ?
                   (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
	}

    // Visplanes (CRL: 32, 128 or 4096 max)
    if (crl_visplanes_counter)
	{
		char num[32];
		
		M_WriteText(0, 135, "VIS:", cr[CR_GRAY]);
        
        if (crl_visplanes_counter == 1)
        {
            // Bief
            M_snprintf(num, 4, "%d", CRLData.numcheckplanes + CRLData.numfindplanes);
        }
        else
        {
            // Full
            M_snprintf(num, 32, "%d = %d CHK + %d FND",
                       CRLData.numcheckplanes + CRLData.numfindplanes,
                       CRLData.numcheckplanes, CRLData.numfindplanes);
        }
        
		M_WriteText(32, 135, num, (CRLData.numcheckplanes + CRLData.numfindplanes >= 128) ?
                   (gametic & 8 ? cr[CR_RED] : cr[CR_YELLOW]) : cr[CR_GREEN]);
	}


    /*
#define MAXLINE 256
	char line[MAXLINE];
	CRL_Option_t* op;
	int dm;
	int i;
	int x, y;
	
	// Get current draw mode option
	op = &CRLOptionSet[CRL_DRAWSTATS];
	dm = op->curvalue;
	
	// Not drawing anything
	if (dm == CRL_STAT_NONE)
		return;
	
	// X and y position
	x = 5;
	y = __lh - __fh;
	
	// Visplanes
	if (1)
	{
		// Visplane total
		i = CRLData.numcheckplanes + CRLData.numfindplanes;
		snprintf(line, MAXLINE, "VIS: %d = %d CHK + %d FND", i,
			CRLData.numcheckplanes, CRLData.numfindplanes);
			
		// Draw
		__dt(x, y, line);
		y -= __fh;
	}
    */
}

/**
 * Mark pixel that was drawn.
 *
 * @param __surface Target surface that gets it.
 * @param __what What was drawn here.
 * @param __drawp Where it was drawn.
 */
void CRL_MarkPixelP(void** __surface, void* __what, void* __drawp)
{
	// Set surface to what
	__surface[(uintptr_t)__drawp - (uintptr_t)CRLSurface] = __what;
}

/**
 * Colorize the current plane for same color on the view and the automap.
 *
 * @param __pl Plane to colorize.
 * @return Plane color
 */
static int CRL_ColorizeThisPlane(CRLPlaneData_t* __pl)
{
	// Initial color
	int id = 0;
	int shade = DARKSHADE * __pl->isf;
	
	// Above plane limit
	if (__pl->id >= _maxplanes)
		shade += _pulse & DARKMASK;
	
	// Make the colors consistent when running around to determine where
	// things come from
	// The best initial thing to do is use segs because most visplanes are
	// generated by the segs in view
	if (__pl->emitlineid >= 0)
		id = __pl->emitlineid;
	
	// Next the subsector, since not all visplanes may have a seg (like the
	// sky)
	else if (__pl->emitsubid >= 0)
		id = __pl->emitsubid;
	
	// Sectors can have tons of visplanes, so just use the ID
	else
		id = __pl->id;
	
	// Return color
	return _colormap[shade][id & 0xFF];
}

/**
 * Draw visplanes (underlay or overlay).
 */
void CRL_DrawVisPlanes(int __over)
{
	int dm, isover, x, y, i, isbord, c;
	void* is;
	CRLPlaneData_t pd;
	
	// Get visplane drawing mode
	dm = crl_visplanes_drawing;  // [JN] Use external config variable.
	
	// Drawing nothing
	if (dm == 0)
		return;
	
	// Overlay but not overlaying?
	isover = (dm == CRL_VIS_FILL_OVERLAY || dm == CRL_VIS_BORDER_OVERLAY);
	if (__over != isover)
		return;
	
	// Border colors
	isbord = (dm == CRL_VIS_BORDER || dm == CRL_VIS_BORDER_OVERLAY);
	
	// Go through all pixels and draw visplane if one is there
	memset(&pd, 0, sizeof(pd));
	for (i = 0, x = 0, y = 0; i < SCREENAREA; i++, x++)
	{
		// Increase y
		if (x >= SCREENWIDTH)
		{
			x = 0;
			y++;
		}
		
		// Get plane drawn here
		if (!(is = CRLPlaneSurface[i]))
			continue;
		
		// Border check
		if (isbord)
			if (x > 0 && x < SCREENWIDTH - 1 && y > 0 && y < SCREENHEIGHT - 1
				&& (is == CRLPlaneSurface[i - 1] &&
				is == CRLPlaneSurface[i + 1] &&
				is == CRLPlaneSurface[i - SCREENWIDTH] &&
				is == CRLPlaneSurface[i + SCREENWIDTH]))
			continue;
		
		// Get plane identity
		GAME_IdentifyPlane(is, &pd);
		
		// Color the plane
		c = CRL_ColorizeThisPlane(&pd);
		
		// Draw plane colors
		CRLSurface[i] = c;
	}
}

/**
 * Draws the automap.
 *
 * @param __fl Normal line.
 * @param __ml Map line.
 */
void CRL_DrawMap(void (*__fl)(int, int, int, int, int),
	void (*__ml)(int, int, int, int, int))
{
	CRL_Option_t* op;
	int i, dm, c, j;
	CRLPlaneData_t pd;
	CRLSegData_t sd;
	CRLSubData_t ud;
	
	op = &CRLOptionSet[CRL_MAPMODE];
	dm = op->curvalue;
	
	// Visplane emitting segs
	if (dm == CRL_MAP_VPFLOOR || dm == CRL_MAP_VPCEIL)
	{
		// Go through all planes
		for (i = 0; i < _numplanes; i++)
		{
			// Identify this plane
			GAME_IdentifyPlane(_planelist[i], &pd);
			
			// Floor/ceiling mismatch
			if ((!!(dm == CRL_MAP_VPFLOOR)) != (!!pd.onfloor))
				continue;
			
			// Color the plane
			c = CRL_ColorizeThisPlane(&pd);
			
			// Has an emitting line
			if (pd.emitline)
			{
				// Identify it
				GAME_IdentifySeg(pd.emitline, &sd);
				
				// Draw its position as some color
				__ml(c, sd.coords[0], sd.coords[1],
					sd.coords[2], sd.coords[3]);
			}
			
			// Has an emitting subsector, but no line
			else if (pd.emitsub)
			{
				// ID it
				GAME_IdentifySubSector(pd.emitsub, &ud);
				
				// Draw the subsector seg lines, the non implicit edges that
				// is.
				for (j = 0; j < ud.numlines; j++)
				{
					// Id seg
					GAME_IdentifySeg(ud.lines[j], &sd);
					
					// Draw it
					__ml(c, sd.coords[0], sd.coords[1],
						sd.coords[2], sd.coords[3]);
				}
			}
		}
	}
}

/**
 * Draw view.
 */
void CRL_ViewDrawer()
{
	// Do not really care if brute forcing
	if (CRLBruteForce)
		return;
	
	// Draw visplanes if overlayed
	CRL_DrawVisPlanes(1);
}

/**
 * Counts visplane.
 *
 * @param __key Visplane ID.
 * @param __chorf 0 = R_CheckPlane, 1 = R_FindPlane
 * @param __id ID Number.
 */
void CRL_CountPlane(void* __key, int __chorf, int __id)
{
	// Update count
	if (__chorf == 0)
		CRLData.numcheckplanes++;
	else
		CRLData.numfindplanes++;
	
	// Add to global list
	if (_numplanes < MAXCOUNTPLANES)
		_planelist[_numplanes++] = __key;
}

/**
 * Is spectating happening?
 *
 * @return {@code 1} if spectating is being done.
 * @since 2017/01/23
 */
int CRL_IsSpectating(void)
{
	if (CRLOptionSet[CRL_SPECTATE].curvalue == CRL_SPECTATE_ON)
		return 1;
	return 0;
}

/**
 * Impulses the camera.
 *
 * @param fwm Forward movement.
 * @param swm Sideways movement.
 * @param at Angle turning.
 */
void CRL_ImpulseCamera(int32_t fwm, int32_t swm, uint32_t at)
{
	// Do nothing if not spectating
	if (!CRL_IsSpectating())
		return;
	
	// Rotate camera first
	_camang += at << FRACBITS;
	
	// Forward movement
    at = _camang >> ANGLETOFINESHIFT;
    _campos[0] += FixedMul(fwm * 32768, finecosine[at]); 
    _campos[1] += FixedMul(fwm * 32768, finesine[at]);
    
    // Sideways movement
    at = (_camang - ANG90) >> ANGLETOFINESHIFT;
    _campos[0] += FixedMul(swm * 32768, finecosine[at]); 
    _campos[1] += FixedMul(swm * 32768, finesine[at]);
}

/**
 * Returns the camera position.
 */
void CRL_GetCameraPos(int32_t* x, int32_t* y, int32_t* z, uint32_t* a)
{
	*x = _campos[0];
	*y = _campos[1];
	*z = _campos[2];
	*a = _camang;
}

/**
 * Returns the maximum number of visplanes.
 *
 * @return The visplane limit.
 * @since 2015/12/17
 */
int CRL_MaxVisPlanes(void)
{
	if (CRLOptionSet[CRL_MAXVISPLANES].curvalue == CRL_VISLIMIT_VANILLA)
		return 128;
	else if (CRLOptionSet[CRL_MAXVISPLANES].curvalue == CRL_VISLIMIT_QUARTER)
		return 32;
	return 4096;
}

/**
 * Draws the HOM detection background.
 *
 * @param __x X position.
 * @param __y Y position,
 * @param __w Width.
 * @param __h Height.
 * @since 2015/12/17
 */
void CRL_DrawHOMBack(int __x, int __y, int __w, int __h)
{
	static int tic;
	int usecolor;
	
	// Do not really care if brute forcing
	if (CRLBruteForce)
		return;
	
	// Color to use
	usecolor = _homtable[(++tic) & (HOMCOUNT - 1)];
	
	// Draw it
	V_DrawFilledBox(__x, __y, __w, __h, usecolor);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#else

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// This is the magical include area so that the various different games with
// vastly different data structures can share the same code.

// For progress
#include <time.h>

// Just in case
#include "crlcore.h"

// This across all ports has all the render stuff visible
#include "doom/r_local.h"

// eye dee sawftwear kan speil
#define numvertices numvertexes

/**
 * Returns the larger of two fixed_t.
 *
 * @param __a A.
 * @param __b B.
 * @return The larger value.
 * @since 2015/12/19
 */
static fixed_t CRL_MaxFixed(fixed_t __a, fixed_t __b)
{
	if (__a > __b)
		return __a;
	return __b;
}
/**
 * Returns the smaller of two fixed_t.
 *
 * @param __a A.
 * @param __b B.
 * @return The smaller value.
 * @since 2015/12/19
 */
static fixed_t CRL_MinFixed(fixed_t __a, fixed_t __b)
{
	if (__a < __b)
		return __a;
	return __b;
}

/**
 * Returns a subsector but only if it is in an actual sector.
 *
 * Taken from Doom Legacy.
 *
 * @since 2015/12/20
 */
subsector_t* CRL_IsPointInSubsector(fixed_t x, fixed_t y)
{
	node_t* node;
	int side;
	int nodenum, i;
	subsector_t* ret;
	
	// single subsector is a special case
	if (!numnodes)
		return subsectors;
		
	nodenum = numnodes - 1;
	
	while (!(nodenum & NF_SUBSECTOR))
	{
		node = &nodes[nodenum];
		side = R_PointOnSide(x, y, node);
		nodenum = node->children[side];
	}
	
	ret = &subsectors[nodenum & ~NF_SUBSECTOR];
	for (i = 0; i < ret->numlines; i++)
	{
		if (R_PointOnSegSide(x, y, &segs[ret->firstline + i]))
			return 0;
	}
	
	return ret;
}

/**
 * Returns the size of the current map.
 *
 * @return The current map size.
 * @since 2015/12/19
 */
CRLRect_t CRL_MapSize(void)
{
	CRLRect_t rv;
	int i;
	fixed_t x1, y1, x2, y2;
	
	// Start with bad values that are way out of range
	x1 = y1 = 2147483647;
	x2 = y2 = -2147483647;
	
	// Go through all level vertices
	for (i = 0; i < numvertices; i++)
	{
		vertex_t* v = &vertexes[i];
		
		// Update X
		x1 = CRL_MinFixed(v->x, x1);
		x2 = CRL_MaxFixed(v->x, x2);
		
		// Update Y
		y1 = CRL_MinFixed(v->y, y1);
		y2 = CRL_MaxFixed(v->y, y2);
	}
	
	// Setup return value
	rv.x = x1;
	rv.y = y1;
	rv.w = (x2 - x1);
	rv.h = (y2 - y1);
	
	// Return it
	return rv;
}

/**
 * This is the brute force loop.
 *
 * @since 2015/12/19
 */
void CRL_BruteForceLoop(void)
{
#define EVERY 64
	CRLRect_t mapdim;
	fixed_t addi;
	fixed_t x, y, ex, ey;
	int i, firsttime, a;
	subsector_t* ss;
	sector_t* sect;
	int drawnpos, explorecount, totalcount;
	int starttime, nowtime, timediff, eta;
	
	// Get the size of the map
	mapdim = CRL_MapSize();
	
	// Granularity addition
	addi = (fixed_t)CRLBruteForce << FRACBITS;
	
	// End positions
	ex = mapdim.x + mapdim.w;
	ey = mapdim.y + mapdim.h;
	
	// Use fullscreen
	R_ExecuteSetViewSize();
	
	// There is always a first time for everything
	// Otherwise crashes will occur when R_RenderPlayerView is called without
	// a D_Display calling it.
	firsttime = 1;
	
	// Positions drawn
	drawnpos = 0;
	
	// Guess exploration size
	explorecount = 0;
	totalcount = ((mapdim.w >> FRACBITS) / CRLBruteForce) *
		((mapdim.h >> FRACBITS) / CRLBruteForce);
	
	// Start time
	starttime = I_GetTimeMS();
	
	// Go through the map
	i = 0;
	for (y = mapdim.y; y < ey; y += addi)
		for (x = mapdim.x; x < ex; x += addi, i++)
		{
			// If this position is not in any subsector (well technically a
			// position will always be in a subsector) then do not bother at
			// all because the player will usually never be in said position
			// unless they glide out of the level or the map developer is bad
			// at making maps.
			ss = CRL_IsPointInSubsector(x, y);
			
			// Explored this area
			explorecount++;
			
			// Not in one, so stop
			if (ss == NULL)
			{
				i--;
				continue;
			}
			
			// Get sector
			sect = ss->sector;
			
			// If the sector ceiling is below or at the floor, do not bother
			if (sect->ceilingheight <= sect->floorheight)
				continue;
			
			// Move player to this position
			players[consoleplayer].mo->x = x;
			players[consoleplayer].mo->y = y;
			
			// Place the player position in the middle
			players[consoleplayer].mo->z = FixedDiv(
				sect->ceilingheight + sect->floorheight, 2 << FRACBITS);
			
			// Viewheight to player Z
			players[consoleplayer].viewheight = players[consoleplayer].mo->z;
			
			// Look in all 4 directions
			for (a = 0; a < 4; a++)
			{
				// Set angle
				players[consoleplayer].mo->angle = ANG90 * (angle_t)a;
			
				// The first frame must be a D_Display, otherwise boom
				if (firsttime)
				{
					D_Display();
					firsttime = 0;
				}
				
				// Just render
				else
				{
					// Render
					R_RenderPlayerView(&players[consoleplayer]);
					
					// Print progress
					if (i >= EVERY)
					{
						// Later on
						i = -1;
						
						// Get the current time
						nowtime = I_GetTimeMS();
						
						// Time difference
						timediff = nowtime - starttime;
						
						// Estimated end time
						eta = 0;
					
						// Progress helps, and stderr also has no buffering
						fprintf(stderr, "Drawn %-12d (%12d of %-12d) %12dms, "
							"ETA %12dms\r",
							drawnpos, explorecount, totalcount, timediff, eta);
					}
				}
				
				// Drew it so count it
				drawnpos++;
			}
		}
#undef EVERY
}

///////////////////////////////////////////////////////////////////////////////

#endif /* CHOCORENDERLIMITS_MAGIC_INCLUDE */

