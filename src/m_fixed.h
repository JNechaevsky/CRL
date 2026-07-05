//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2022 Ryan Krafnick
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
//	Fixed point arithemtics, implementation.
//


#ifndef __M_FIXED__
#define __M_FIXED__




//
// Fixed point, 32bit as 16.16.
//
#define FRACBITS		16
#define FRACUNIT		(1<<FRACBITS)
#define FRACMASK		(FRACUNIT-1)
#define FIXED2DOUBLE(x)	(x / (double)FRACUNIT)

typedef int fixed_t;

fixed_t FixedMul	(fixed_t a, fixed_t b);
fixed_t FixedDiv	(fixed_t a, fixed_t b);

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#ifndef BETWEEN
#define BETWEEN(l,u,x) (((l)>(x))?(l):((x)>(u))?(u):(x))
#endif

// -----------------------------------------------------------------------------
// Coordinate formatting helpers
// -----------------------------------------------------------------------------

typedef struct
{
    int negative;
    int base;
    int frac;
} split_fixed_t;

typedef struct
{
    int base;
    int frac;
} split_angle_t;

inline static split_fixed_t SplitFixed(fixed_t x)
{
    split_fixed_t result;

    result.negative = x < 0;
    result.base = x >> FRACBITS;
    result.frac = x & FRACMASK;

    if (result.negative && result.frac)
    {
        result.base++;
        result.frac = FRACUNIT - result.frac;
    }

    return result;
}

inline static split_angle_t SplitAngle(unsigned x)
{
    split_angle_t result;

    result.base = x >> (FRACBITS + 8);
    result.frac = (x >> FRACBITS) & 0xFF;

    return result;
}

#endif
