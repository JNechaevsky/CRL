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
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//	Line Tag handling. Line and Sector triggers.
//


#include <stdlib.h>
#include "doomstat.h"
#include "deh_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "w_wad.h"
#include "p_local.h"
#include "g_game.h"
#include "s_sound.h"
#include "d_englsh.h"
#include "v_trans.h"

#include "crlcore.h"
#include "crlvars.h"


//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//
typedef struct
{
    boolean	istexture;
    int		picnum;
    int		basepic;
    int		numpics;
    int		speed;
    
} anim_t;

//
//      source animation definition
//
typedef struct
{
    int 	istexture;	// if false, it is a flat
    char	endname[9];
    char	startname[9];
    int		speed;
} animdef_t;



#define MAXANIMS                32


//
// P_InitPicAnims
//

// Floor/ceiling animation sequences,
//  defined by first and last frame,
//  i.e. the flat (64x64 tile) name to
//  be used.
// The full animation sequence is given
//  using all the flats between the start
//  and end entry, in the order found in
//  the WAD file.
//
animdef_t		animdefs[] =
{
    {false,	"NUKAGE3",	"NUKAGE1",	8},
    {false,	"FWATER4",	"FWATER1",	8},
    {false,	"SWATER4",	"SWATER1", 	8},
    {false,	"LAVA4",	"LAVA1",	8},
    {false,	"BLOOD3",	"BLOOD1",	8},

    // DOOM II flat animations.
    {false,	"RROCK08",	"RROCK05",	8},		
    {false,	"SLIME04",	"SLIME01",	8},
    {false,	"SLIME08",	"SLIME05",	8},
    {false,	"SLIME12",	"SLIME09",	8},

    {true,	"BLODGR4",	"BLODGR1",	8},
    {true,	"SLADRIP3",	"SLADRIP1",	8},

    {true,	"BLODRIP4",	"BLODRIP1",	8},
    {true,	"FIREWALL",	"FIREWALA",	8},
    {true,	"GSTFONT3",	"GSTFONT1",	8},
    {true,	"FIRELAVA",	"FIRELAV3",	8},
    {true,	"FIREMAG3",	"FIREMAG1",	8},
    {true,	"FIREBLU2",	"FIREBLU1",	8},
    {true,	"ROCKRED3",	"ROCKRED1",	8},

    {true,	"BFALL4",	"BFALL1",	8},
    {true,	"SFALL4",	"SFALL1",	8},
    {true,	"WFALL4",	"WFALL1",	8},
    {true,	"DBRAIN4",	"DBRAIN1",	8},
	
    {-1,        "",             "",             0},
};

anim_t		anims[MAXANIMS];
anim_t*		lastanim;


//
//      Animating line specials
//
#define MAXLINEANIMS            16385

short		numlinespecials;
line_t*		linespeciallist[MAXLINEANIMS];



void P_InitPicAnims (void)
{
    int		i;

    
    //	Init animation
    lastanim = anims;
    for (i=0 ; animdefs[i].istexture != -1 ; i++)
    {
        const char *startname, *endname;

        startname = DEH_String(animdefs[i].startname);
        endname = DEH_String(animdefs[i].endname);

	if (animdefs[i].istexture)
	{
	    // different episode ?
	    if (R_CheckTextureNumForName(startname) == -1)
		continue;	

	    lastanim->picnum = R_TextureNumForName(endname);
	    lastanim->basepic = R_TextureNumForName(startname);
	}
	else
	{
	    if (W_CheckNumForName(startname) == -1)
		continue;

	    lastanim->picnum = R_FlatNumForName(endname);
	    lastanim->basepic = R_FlatNumForName(startname);
	}

	lastanim->istexture = animdefs[i].istexture;
	lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

	if (lastanim->numpics < 2)
	    I_Error ("P_InitPicAnims: bad cycle from %s to %s",
		     startname, endname);
	
	lastanim->speed = animdefs[i].speed;
	lastanim++;
    }
	
}



//
// UTILITIES
//



//
// getSide()
// Will return a side_t*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
side_t*
getSide
( int		currentSector,
  int		line,
  int		side )
{
    return &sides[ (sectors[currentSector].lines[line])->sidenum[side] ];
}


//
// getSector()
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
sector_t*
getSector
( int		currentSector,
  int		line,
  int		side )
{
    return sides[ (sectors[currentSector].lines[line])->sidenum[side] ].sector;
}


//
// twoSided()
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
int
twoSided
( int	sector,
  int	line )
{
    return (sectors[sector].lines[line])->flags & ML_TWOSIDED;
}




//
// getNextSector()
// Return sector_t * of sector next to current.
// NULL if not two-sided line
//
sector_t*
getNextSector
( line_t*	line,
  const sector_t*	sec )
{
    if (!(line->flags & ML_TWOSIDED))
	return NULL;
		
    if (line->frontsector == sec)
	return line->backsector;
	
    return line->frontsector;
}



//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindLowestFloorSurrounding(sector_t* sec)
{
    int			i;
    line_t*		check;
    const sector_t*		other;
    fixed_t		floor = sec->floorheight;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sec->lines[i];
	other = getNextSector(check,sec);

	if (!other)
	    continue;
	
	if (other->floorheight < floor)
	    floor = other->floorheight;
    }
    return floor;
}



//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec)
{
    int			i;
    line_t*		check;
    const sector_t*		other;
    fixed_t		floor = -500*FRACUNIT;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sec->lines[i];
	other = getNextSector(check,sec);
	
	if (!other)
	    continue;
	
	if (other->floorheight > floor)
	    floor = other->floorheight;
    }
    return floor;
}



//
// P_FindNextHighestFloor
// FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
// Note: this should be doable w/o a fixed array.

// Thanks to entryway for the Vanilla overflow emulation.

// 20 adjoining sectors max!
#define MAX_ADJOINING_SECTORS     20

fixed_t
P_FindNextHighestFloor
( sector_t* sec,
  int       currentheight )
{
    int         i;
    int         h;
    int         min;
    line_t*     check;
    const sector_t*   other;
    fixed_t     height = currentheight;
    fixed_t     heightlist[MAX_ADJOINING_SECTORS + 2];

    for (i=0, h=0; i < sec->linecount; i++)
    {
        check = sec->lines[i];
        other = getNextSector(check,sec);

        if (!other)
            continue;
        
        if (other->floorheight > height)
        {
            // Emulation of memory (stack) overflow
            if (h == MAX_ADJOINING_SECTORS + 1)
            {
                height = other->floorheight;
            }
            else if (h == MAX_ADJOINING_SECTORS + 2)
            {
                // Fatal overflow: game crashes at 22 sectors
                I_Error("Sector with more than 22 adjoining sectors. "
                        "Vanilla will crash here");
            }

            heightlist[h++] = other->floorheight;
        }
    }
    
    // Find lowest height in list
    if (!h)
    {
        return currentheight;
    }
        
    min = heightlist[0];
    
    // Range checking? 
    for (i = 1; i < h; i++)
    {
        if (heightlist[i] < min)
        {
            min = heightlist[i];
        }
    }

    return min;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t
P_FindLowestCeilingSurrounding(sector_t* sec)
{
    int			i;
    line_t*		check;
    const sector_t*		other;
    fixed_t		height = INT_MAX;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sec->lines[i];
	other = getNextSector(check,sec);

	if (!other)
	    continue;

	if (other->ceilingheight < height)
	    height = other->ceilingheight;
    }
    return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t	P_FindHighestCeilingSurrounding(sector_t* sec)
{
    int		i;
    line_t*	check;
    const sector_t*	other;
    fixed_t	height = 0;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sec->lines[i];
	other = getNextSector(check,sec);

	if (!other)
	    continue;

	if (other->ceilingheight > height)
	    height = other->ceilingheight;
    }
    return height;
}



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
int
P_FindSectorFromLineTag
( const line_t*	line,
  int		start )
{
    int	i;
	
    for (i=start+1;i<numsectors;i++)
	if (sectors[i].tag == line->tag)
	    return i;
    
    return -1;
}




//
// Find minimum light from an adjacent sector
//
int
P_FindMinSurroundingLight
( sector_t*	sector,
  int		max )
{
    int		i;
    int		min;
    line_t*	line;
    const sector_t*	check;
	
    min = max;
    for (i=0 ; i < sector->linecount ; i++)
    {
	line = sector->lines[i];
	check = getNextSector(line,sector);

	if (!check)
	    continue;

	if (check->lightlevel < min)
	    min = check->lightlevel;
    }
    return min;
}



//
// EVENTS
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_CrossSpecialLine - TRIGGER
// Called every time a thing origin is about
//  to cross a line with a non 0 special.
//
void
P_CrossSpecialLine
( int		linenum,
  int		side,
  mobj_t*	thing )
{
    line_t*	line;
    int		ok;

    line = &lines[linenum];
    
    if (gameversion <= exe_doom_1_2)
    {
        if (line->special > 98 && line->special != 104)
        {
            return;
        }
    }
    else
    {
        //	Triggers that other things can activate
        if (!thing->player)
        {
            // Things that should NOT trigger specials...
            switch(thing->type)
            {
                case MT_ROCKET:
                case MT_PLASMA:
                case MT_BFG:
                case MT_TROOPSHOT:
                case MT_HEADSHOT:
                case MT_BRUISERSHOT:
                    return;

                default: break;
            }
        }
    }

    if (!thing->player)
    {
	ok = 0;
	switch(line->special)
	{
	  case 39:	// TELEPORT TRIGGER
	  case 97:	// TELEPORT RETRIGGER
	  case 125:	// TELEPORT MONSTERONLY TRIGGER
	  case 126:	// TELEPORT MONSTERONLY RETRIGGER
	  case 4:	// RAISE DOOR
	  case 10:	// PLAT DOWN-WAIT-UP-STAY TRIGGER
	  case 88:	// PLAT DOWN-WAIT-UP-STAY RETRIGGER
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }

    
    // Note: could use some const's here.
    switch (line->special)
    {
	// TRIGGERS.
	// All from here to RETRIGGERS.
      case 2:
	// Open Door
	EV_DoDoor(line,vld_open);
	line->special = 0;
	break;

      case 3:
	// Close Door
	EV_DoDoor(line,vld_close);
	line->special = 0;
	break;

      case 4:
	// Raise Door
	EV_DoDoor(line,vld_normal);
	line->special = 0;
	break;
	
      case 5:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
	line->special = 0;
	break;
	
      case 6:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
	line->special = 0;
	break;
	
      case 8:
	// Build Stairs
	EV_BuildStairs(line,build8);
	line->special = 0;
	break;
	
      case 10:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
	line->special = 0;
	break;
	
      case 12:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
	line->special = 0;
	break;
	
      case 13:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
	line->special = 0;
	break;
	
      case 16:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
	line->special = 0;
	break;
	
      case 17:
	// Start Light Strobing
	EV_StartLightStrobing(line);
	line->special = 0;
	break;
	
      case 19:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
	line->special = 0;
	break;
	
      case 22:
	// Raise floor to nearest height and change texture
	EV_DoPlat(line,raiseToNearestAndChange,0);
	line->special = 0;
	break;
	
      case 25:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
	line->special = 0;
	break;
	
      case 30:
	// Raise floor to shortest texture height
	//  on either side of lines.
	EV_DoFloor(line,raiseToTexture);
	line->special = 0;
	break;
	
      case 35:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
	line->special = 0;
	break;
	
      case 36:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
	line->special = 0;
	break;
	
      case 37:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
	line->special = 0;
	break;
	
      case 38:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
	line->special = 0;
	break;
	
      case 39:
	// TELEPORT!
	EV_Teleport( line, side, thing );
	line->special = 0;
	break;

      case 40:
	// RaiseCeilingLowerFloor
	EV_DoCeiling( line, raiseToHighest );
	EV_DoFloor( line, lowerFloorToLowest );
	line->special = 0;
	break;
	
      case 44:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
	line->special = 0;
	break;
	
      case 52:
	// EXIT!
	G_ExitLevel ();
	break;
	
      case 53:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
	line->special = 0;
	break;
	
      case 54:
	// Platform Stop
	EV_StopPlat(line);
	line->special = 0;
	break;

      case 56:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
	line->special = 0;
	break;

      case 57:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
	line->special = 0;
	break;
	
      case 58:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
	line->special = 0;
	break;

      case 59:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
	line->special = 0;
	break;
	
      case 104:
	// Turn lights off in sector(tag)
	EV_TurnTagLightsOff(line);
	line->special = 0;
	break;
	
      case 108:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
	line->special = 0;
	break;
	
      case 109:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
	line->special = 0;
	break;
	
      case 100:
	// Build Stairs Turbo 16
	EV_BuildStairs(line,turbo16);
	line->special = 0;
	break;
	
      case 110:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
	line->special = 0;
	break;

      case 119:
	// Raise floor to nearest surr. floor
	EV_DoFloor(line,raiseFloorToNearest);
	line->special = 0;
	break;
	
      case 121:
	// Blazing PlatDownWaitUpStay
	EV_DoPlat(line,blazeDWUS,0);
	line->special = 0;
	break;
	
      case 124:
	// Secret EXIT
	G_SecretExitLevel ();
	break;
		
      case 125:
	// TELEPORT MonsterONLY
	if (!thing->player)
	{
	    EV_Teleport( line, side, thing );
	    line->special = 0;
	}
	break;
	
      case 130:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
	line->special = 0;
	break;
	
      case 141:
	// Silent Ceiling Crush & Raise
	EV_DoCeiling(line,silentCrushAndRaise);
	line->special = 0;
	break;
	
	// RETRIGGERS.  All from here till end.
      case 72:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
	break;

      case 73:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
	break;

      case 74:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
	break;
	
      case 75:
	// Close Door
	EV_DoDoor(line,vld_close);
	break;
	
      case 76:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
	break;
	
      case 77:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
	break;
	
      case 79:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
	break;
	
      case 80:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
	break;
	
      case 81:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
	break;
	
      case 82:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
	break;
	
      case 83:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
	break;

      case 84:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
	break;

      case 86:
	// Open Door
	EV_DoDoor(line,vld_open);
	break;
	
      case 87:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
	break;
	
      case 88:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
	break;
	
      case 89:
	// Platform Stop
	EV_StopPlat(line);
	break;
	
      case 90:
	// Raise Door
	EV_DoDoor(line,vld_normal);
	break;
	
      case 91:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
	break;
	
      case 92:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
	break;
	
      case 93:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
	break;
	
      case 94:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
	break;
	
      case 95:
	// Raise floor to nearest height
	// and change texture.
	EV_DoPlat(line,raiseToNearestAndChange,0);
	break;
	
      case 96:
	// Raise floor to shortest texture height
	// on either side of lines.
	EV_DoFloor(line,raiseToTexture);
	break;
	
      case 97:
	// TELEPORT!
	EV_Teleport( line, side, thing );
	break;
	
      case 98:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
	break;

      case 105:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
	break;
	
      case 106:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
	break;

      case 107:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
	break;

      case 120:
	// Blazing PlatDownWaitUpStay.
	EV_DoPlat(line,blazeDWUS,0);
	break;
	
      case 126:
	// TELEPORT MonsterONLY.
	if (!thing->player)
	    EV_Teleport( line, side, thing );
	break;
	
      case 128:
	// Raise To Nearest Floor
	EV_DoFloor(line,raiseFloorToNearest);
	break;
	
      case 129:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
	break;
    }
}



//
// P_ShootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void
P_ShootSpecialLine
( const mobj_t*	thing,
  line_t*	line )
{
    int		ok;
    
    //	Impacts that other things can activate.
    if (!thing->player)
    {
	ok = 0;
	switch(line->special)
	{
	  case 46:
	    // OPEN DOOR IMPACT
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }

    switch(line->special)
    {
      case 24:
	// RAISE FLOOR
	EV_DoFloor(line,raiseFloor);
	P_ChangeSwitchTexture(line,0);
	break;
	
      case 46:
	// OPEN DOOR
	EV_DoDoor(line,vld_open);
	P_ChangeSwitchTexture(line,1);
	break;
	
      case 47:
	// RAISE FLOOR NEAR AND CHANGE
	EV_DoPlat(line,raiseToNearestAndChange,0);
	P_ChangeSwitchTexture(line,0);
	break;
    }
}



//
// P_PlayerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t* player)
{
    sector_t*	sector;
	
    sector = player->mo->subsector->sector;

    // Falling, not all the way down yet?
    if (player->mo->z != sector->floorheight)
	return;	

    // Has hitten ground.
    switch (sector->special)
    {
      case 5:
	// HELLSLIME DAMAGE
	if (!player->powers[pw_ironfeet] && !crl_freeze)
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 10);
	break;
	
      case 7:
	// NUKAGE DAMAGE
	if (!player->powers[pw_ironfeet])
	    if (!(leveltime&0x1f) && !crl_freeze)
		P_DamageMobj (player->mo, NULL, NULL, 5);
	break;
	
      case 16:
	// SUPER HELLSLIME DAMAGE
      case 4:
	// STROBE HURT
	if (!player->powers[pw_ironfeet]
	    || (P_Random()<5) )
	{
	    if (!(leveltime&0x1f) && !crl_freeze)
		P_DamageMobj (player->mo, NULL, NULL, 20);
	}
	break;
			
      case 9:
	// SECRET SECTOR
	player->secretcount++;
	sector->oldspecial = sector->special;
	sector->special = 0;
	// [JN] CRL - "A secret is revelaed!" message.
	if (crl_revealed_secrets)
	{
	    // [crispy] play DSSECRET if available
	    int sfx_id;  
        
	    sfx_id = I_GetSfxLumpNum(&S_sfx[sfx_secret]) != -1 ? sfx_secret :
	             I_GetSfxLumpNum(&S_sfx[sfx_getpow]) != -1 ? sfx_getpow : -1;

	    if (crl_revealed_secrets == 1)  // [JN] Top
	    {
	        CRL_SetMessage(player, DEH_String(CRL_SECRET_FOUND), true, cr[CR_YELLOW]);
	    }
	    else
	    if (crl_revealed_secrets == 2)  // [JN] Centered
	    {
	        CRL_SetMessageCentered(player, DEH_String(CRL_SECRET_FOUND));
	    }

	    if (sfx_id != -1)
        {
	        S_StartSound(NULL, sfx_id);
	    }
	}
	break;
			
      case 11:
	// EXIT SUPER DAMAGE! (for E1M8 finale)
	player->cheats &= ~CF_GODMODE;

	if (!(leveltime&0x1f))
	    P_DamageMobj (player->mo, NULL, NULL, 20);

	if (player->health <= 10)
	    G_ExitLevel();
	break;
			
      default:
      {
	    char secnum[7];

	    sprintf(secnum, "%i", sector->special);

	    // [JN] CRL - do not crash, print an in-game instead.
	    CRL_SetMessageCritical("P_PlayerInSpecialSector:",
                               M_StringJoin("unknown special ", secnum,
                               " (vanilla crashes here)", NULL), 2);
      }
	break;
    };
}




//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
boolean		levelTimer;
int		levelTimeCount;

void P_UpdateSpecials (void)
{
    anim_t*	anim;
    int		pic;
    int		i;
    line_t*	line;

    
    //	LEVEL TIMER
    if (levelTimer == true)
    {
	levelTimeCount--;
	if (!levelTimeCount)
	    G_ExitLevel();
    }
    
    //	ANIMATE FLATS AND TEXTURES GLOBALLY
    for (anim = anims ; anim < lastanim ; anim++)
    {
	for (i=anim->basepic ; i<anim->basepic+anim->numpics ; i++)
	{
	    pic = anim->basepic + ( (leveltime/anim->speed + i)%anim->numpics );
	    if (anim->istexture)
		texturetranslation[i] = pic;
	    else
		flattranslation[i] = pic;
	}
    }

    
    //	ANIMATE LINE SPECIALS
    for (i = 0; i < numlinespecials; i++)
    {
	line = linespeciallist[i];
	switch(line->special)
	{
	  case 48:
	    // EFFECT FIRSTCOL SCROLL +
	    // [crispy] smooth texture scrolling
	    sides[line->sidenum[0]].basetextureoffset += FRACUNIT;
	    sides[line->sidenum[0]].textureoffset =
	    sides[line->sidenum[0]].basetextureoffset;
	    break;
	  case 85:
	    // [JN] (Boom) Scroll Texture Right
	    // [crispy] smooth texture scrolling
	    sides[line->sidenum[0]].basetextureoffset -= FRACUNIT;
	    sides[line->sidenum[0]].textureoffset =
	    sides[line->sidenum[0]].basetextureoffset;
	    break;
	}
    }

    
    //	DO BUTTONS
    for (i = 0; i < MAXBUTTONS; i++)
	if (buttonlist[i].btimer)
	{
	    buttonlist[i].btimer--;
	    if (!buttonlist[i].btimer)
	    {
		switch(buttonlist[i].where)
		{
		  case top:
		    sides[buttonlist[i].line->sidenum[0]].toptexture =
			buttonlist[i].btexture;
		    break;
		    
		  case middle:
		    sides[buttonlist[i].line->sidenum[0]].midtexture =
			buttonlist[i].btexture;
		    break;
		    
		  case bottom:
		    sides[buttonlist[i].line->sidenum[0]].bottomtexture =
			buttonlist[i].btexture;
		    break;
		}
		S_StartSound(&buttonlist[i].soundorg,sfx_swtchn);
		memset(&buttonlist[i],0,sizeof(button_t));
		// [JN] CRL - decrease buttons counter.
		CRL_buttons_counter--;
	    }
	}

    // [JN] CRL - Sound propagation mode﻿ for automap.
    // If enabled, scan through all map lines and reduce line timers.
    if (crl_automap_sndprop)
    {
        for (i = 0 ; i < numlines ; i++)
        {
            if (lines[i].sndprop_tics)
            {
                lines[i].sndprop_tics--;
            }
        }
    }

    // [crispy] draw fuzz effect independent of rendering frame rate
    R_SetFuzzPosTic();
}

// [crispy] smooth texture scrolling
void R_InterpolateTextureOffsets (void)
{
	if (crl_uncapped_fps && leveltime > oldleveltime)
	{
		int i;

		for (i = 0; i < numlinespecials; i++)
		{
			const line_t *const line = linespeciallist[i];
			side_t *const side = &sides[line->sidenum[0]];

			if (line->special == 48)
			{
				side->textureoffset = side->basetextureoffset + fractionaltic;
			}
			else
			if (line->special == 85)
			{
				side->textureoffset = side->basetextureoffset - fractionaltic;
			}
		}
	}
}

//
// Donut overrun emulation
//
// Derived from the code from PrBoom+.  Thanks go to Andrey Budko (entryway)
// as usual :-)
//

#define DONUT_FLOORHEIGHT_DEFAULT 0x00000000
#define DONUT_FLOORPIC_DEFAULT 0x16

static void DonutOverrun(fixed_t *s3_floorheight, short *s3_floorpic,
                         line_t *line, sector_t *pillar_sector)
{
    static int first = 1;
    static int tmp_s3_floorheight;
    static int tmp_s3_floorpic;

    if (first)
    {
        int p;

        // This is the first time we have had an overrun.
        first = 0;

        // Default values
        tmp_s3_floorheight = DONUT_FLOORHEIGHT_DEFAULT;
        tmp_s3_floorpic = DONUT_FLOORPIC_DEFAULT;

        //!
        // @category compat
        // @arg <x> <y>
        //
        // Use the specified magic values when emulating behavior caused
        // by memory overruns from improperly constructed donuts.
        // In Vanilla Doom this can differ depending on the operating
        // system.  The default (if this option is not specified) is to
        // emulate the behavior when running under Windows 98.

        p = M_CheckParmWithArgs("-donut", 2);

        if (p > 0)
        {
            // Dump of needed memory: (fixed_t)0000:0000 and (short)0000:0008
            //
            // C:\>debug
            // -d 0:0
            //
            // DOS 6.22:
            // 0000:0000    (57 92 19 00) F4 06 70 00-(16 00)
            // DOS 7.1:
            // 0000:0000    (9E 0F C9 00) 65 04 70 00-(16 00)
            // Win98:
            // 0000:0000    (00 00 00 00) 65 04 70 00-(16 00)
            // DOSBox under XP:
            // 0000:0000    (00 00 00 F1) ?? ?? ?? 00-(07 00)

            M_StrToInt(myargv[p + 1], &tmp_s3_floorheight);
            M_StrToInt(myargv[p + 2], &tmp_s3_floorpic);

            if (tmp_s3_floorpic >= numflats)
            {
                fprintf(stderr,
                        "DonutOverrun: The second parameter for \"-donut\" "
                        "switch should be greater than 0 and less than number "
                        "of flats (%d). Using default value (%d) instead. \n",
                        numflats, DONUT_FLOORPIC_DEFAULT);
                tmp_s3_floorpic = DONUT_FLOORPIC_DEFAULT;
            }
        }
    }

    /*
    fprintf(stderr,
            "Linedef: %d; Sector: %d; "
            "New floor height: %d; New floor pic: %d\n",
            line->iLineID, pillar_sector->iSectorID,
            tmp_s3_floorheight >> 16, tmp_s3_floorpic);
     */

    *s3_floorheight = (fixed_t) tmp_s3_floorheight;
    *s3_floorpic = (short) tmp_s3_floorpic;
}


//
// Special Stuff that can not be categorized
//
int EV_DoDonut(line_t*	line)
{
    sector_t*		s1;
    sector_t*		s2;
    const sector_t*		s3;
    int			secnum;
    int			rtn;
    int			i;
    floormove_t*	floor;
    fixed_t s3_floorheight;
    short s3_floorpic;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	s1 = &sectors[secnum];

	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (s1->specialdata)
	    continue;

	rtn = 1;
	s2 = getNextSector(s1->lines[0],s1);

        // Vanilla Doom does not check if the linedef is one sided.  The
        // game does not crash, but reads invalid memory and causes the
        // sector floor to move "down" to some unknown height.
        // DOSbox prints a warning about an invalid memory access.
        //
        // I'm not sure exactly what invalid memory is being read.  This
        // isn't something that should be done, anyway.
        // Just print a warning and return.

        if (s2 == NULL)
        {
            fprintf(stderr,
                    "EV_DoDonut: linedef had no second sidedef! "
                    "Unexpected behavior may occur in Vanilla Doom. \n");
	    break;
        }

	for (i = 0; i < s2->linecount; i++)
	{
	    s3 = s2->lines[i]->backsector;

	    if (s3 == s1)
		continue;

            if (s3 == NULL)
            {
                // e6y
                // s3 is NULL, so
                // s3->floorheight is an int at 0000:0000
                // s3->floorpic is a short at 0000:0008
                // Trying to emulate

                fprintf(stderr,
                        "EV_DoDonut: WARNING: emulating buffer overrun due to "
                        "NULL back sector. "
                        "Unexpected behavior may occur in Vanilla Doom.\n");

                DonutOverrun(&s3_floorheight, &s3_floorpic, line, s1);
            }
            else
            {
                s3_floorheight = s3->floorheight;
                s3_floorpic = s3->floorpic;
            }

	    //	Spawn rising slime
	    floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	    P_AddThinker (&floor->thinker);
	    s2->specialdata = floor;
	    floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
	    floor->type = donutRaise;
	    floor->crush = false;
	    floor->direction = 1;
	    floor->sector = s2;
	    floor->speed = FLOORSPEED / 2;
	    floor->texture = s3_floorpic;
	    floor->newspecial = 0;
	    floor->floordestheight = s3_floorheight;
	    
	    //	Spawn lowering donut-hole
	    floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	    P_AddThinker (&floor->thinker);
	    s1->specialdata = floor;
	    floor->thinker.function.acp1 = (actionf_p1) T_MoveFloor;
	    floor->type = lowerFloor;
	    floor->crush = false;
	    floor->direction = -1;
	    floor->sector = s1;
	    floor->speed = FLOORSPEED / 2;
	    floor->floordestheight = s3_floorheight;
	    break;
	}
    }
    return rtn;
}



//
// SPECIAL SPAWNING
//

//
// P_SpawnSpecials
// After the map has been loaded, scan for specials
//  that spawn thinkers
//



// Parses command line parameters.
void P_SpawnSpecials (void)
{
    sector_t*	sector;
    int		i;

    // See if -TIMER was specified.

    if (timelimit > 0 && deathmatch)
    {
        levelTimer = true;
        levelTimeCount = timelimit * 60 * TICRATE;
    }
    else
    {
	levelTimer = false;
    }

    //	Init special SECTORs.
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	if (!sector->special)
	    continue;
	
	switch (sector->special)
	{
	  case 1:
	    // FLICKERING LIGHTS
	    P_SpawnLightFlash (sector);
	    break;

	  case 2:
	    // STROBE FAST
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    break;
	    
	  case 3:
	    // STROBE SLOW
	    P_SpawnStrobeFlash(sector,SLOWDARK,0);
	    break;
	    
	  case 4:
	    // STROBE FAST/DEATH SLIME
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    sector->special = 4;
	    break;
	    
	  case 8:
	    // GLOWING LIGHT
	    P_SpawnGlowingLight(sector);
	    break;
	  case 9:
	    // SECRET SECTOR
	    totalsecret++;
	    break;
	    
	  case 10:
	    // DOOR CLOSE IN 30 SECONDS
	    P_SpawnDoorCloseIn30 (sector);
	    break;
	    
	  case 12:
	    // SYNC STROBE SLOW
	    P_SpawnStrobeFlash (sector, SLOWDARK, 1);
	    break;

	  case 13:
	    // SYNC STROBE FAST
	    P_SpawnStrobeFlash (sector, FASTDARK, 1);
	    break;

	  case 14:
	    // DOOR RAISE IN 5 MINUTES
	    P_SpawnDoorRaiseIn5Mins (sector, i);
	    break;
	    
        case 17:
            // first introduced in official v1.4 beta
            if (gameversion > exe_doom_1_2)
            {
                P_SpawnFireFlicker(sector);
            }
            break;
	}
    }

    
    //	Init line EFFECTs
    numlinespecials = 0;
    for (i = 0;i < numlines; i++)
    {
	switch(lines[i].special)
	{
	  case 48:
            if (numlinespecials >= MAXLINEANIMS)
            {
                I_Error("Too many scrolling wall linedefs! "
                        "(Vanilla limit is 64)");
            }
	    // EFFECT FIRSTCOL SCROLL+
	    linespeciallist[numlinespecials] = &lines[i];
	    numlinespecials++;
	    CRL_lineanims_counter++;

	    if (CRL_lineanims_counter > CRL_MaxAnims)
	    {
	        // [JN] Print in-game warning.
	        CRL_SetMessageCritical("P_SpawnSpecials:",
                                   M_StringJoin("Too many scrollers! (", CRL_LimitsName,
                                   " crashes here)", NULL), MESSAGETICS);
	    }

	    break;
	}
    }

    
    //	Init other misc stuff
    for (i = 0;i < MAXCEILINGS;i++)
	activeceilings[i] = NULL;

    for (i = 0;i < MAXPLATS;i++)
	activeplats[i] = NULL;
    
    for (i = 0;i < MAXBUTTONS;i++)
	memset(&buttonlist[i],0,sizeof(button_t));

    // UNUSED: no horizonal sliders.
    //	P_InitSlidingDoorFrames();
}
