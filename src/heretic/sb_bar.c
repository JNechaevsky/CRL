//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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


#include "doomdef.h"
#include "deh_str.h"
#include "i_swap.h"
#include "i_timer.h"
#include "m_controls.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_trans.h"
#include "v_video.h"
#include "am_map.h"
#include "ct_chat.h"

#include "crlcore.h"
#include "crlvars.h"


// Public Data

boolean DebugSound;             // debug flag for displaying sound info

boolean inventory;
int ArtifactFlash;
int curpos;
int inv_ptr;
int playerkeys = 0;
int sb_palette = 0;  // [JN] Externalazied variable of current palette index.
int SB_state = -1;

// Private Data

static int ChainWiggle;
static int FontBNumBase;
static int HealthMarker;
static int oldammo = -1;
static int oldarmor = -1;
static int oldarti = 0;
static int oldartiCount = 0;
static int oldfrags = -9999;
static int oldhealth = -1;
static int oldkeys = -1;
static int oldlife = -1;
static int oldweapon = -1;
static int playpalette;
static int spinbooklump;
static int spinflylump;

static patch_t *PatchARMCLEAR;
static patch_t *PatchBARBACK;
static patch_t *PatchBLACKSQ;
static patch_t *PatchCHAIN;
static patch_t *PatchCHAINBACK;
static patch_t *PatchINumbers[10];
static patch_t *PatchINVBAR;
static patch_t *PatchINVLFGEM1;
static patch_t *PatchINVLFGEM2;
static patch_t *PatchINVRTGEM1;
static patch_t *PatchINVRTGEM2;
static patch_t *PatchLIFEGEM;
static patch_t *PatchLTFACE;
static patch_t *PatchLTFCTOP;
static patch_t *PatchNEGATIVE;
static patch_t *PatchRTFACE;
static patch_t *PatchRTFCTOP;
static patch_t *PatchSELECTBOX;
static patch_t *PatchSmNumbers[10];
static patch_t *PatchSTATBAR;
static player_t *CPlayer;







//---------------------------------------------------------------------------
//
// PROC DrINumber
//
// Draws a three digit number.
//
//---------------------------------------------------------------------------

static void DrINumber(signed int val, int x, int y)
{
    patch_t *patch;
    int oldval;

    oldval = val;
    if (val < 0)
    {
        if (val < -9)
        {
            V_DrawPatch(x + 1, y + 1, W_CacheLumpName(DEH_String("LAME"), PU_CACHE));
        }
        else
        {
            val = -val;
            V_DrawPatch(x + 18, y, PatchINumbers[val]);
            V_DrawPatch(x + 9, y, PatchNEGATIVE);
        }
        return;
    }
    if (val > 99)
    {
        patch = PatchINumbers[val / 100];
        V_DrawPatch(x, y, patch);
    }
    val = val % 100;
    if (val > 9 || oldval > 99)
    {
        patch = PatchINumbers[val / 10];
        V_DrawPatch(x + 9, y, patch);
    }
    val = val % 10;
    patch = PatchINumbers[val];
    V_DrawPatch(x + 18, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC DrBNumber
//
// Draws a three digit number using FontB
//
//---------------------------------------------------------------------------

static void DrBNumber(signed int val, int x, int y)
{
    patch_t *patch;
    int xpos;
    int oldval;

    oldval = val;
    xpos = x;
    if (val < 0)
    {
        val = 0;
    }
    if (val > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 100, PU_CACHE);
        V_DrawShadowedPatchRaven(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 100;
    xpos += 12;
    if (val > 9 || oldval > 99)
    {
        patch = W_CacheLumpNum(FontBNumBase + val / 10, PU_CACHE);
        V_DrawShadowedPatchRaven(xpos + 6 - SHORT(patch->width) / 2, y, patch);
    }
    val = val % 10;
    xpos += 12;
    patch = W_CacheLumpNum(FontBNumBase + val, PU_CACHE);
    V_DrawShadowedPatchRaven(xpos + 6 - SHORT(patch->width) / 2, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC SB_Init
//
//---------------------------------------------------------------------------

void SB_Init(void)
{
    int i;
    int startLump;

    PatchLTFACE = W_CacheLumpName(DEH_String("LTFACE"), PU_STATIC);
    PatchRTFACE = W_CacheLumpName(DEH_String("RTFACE"), PU_STATIC);
    PatchBARBACK = W_CacheLumpName(DEH_String("BARBACK"), PU_STATIC);
    PatchINVBAR = W_CacheLumpName(DEH_String("INVBAR"), PU_STATIC);
    PatchCHAIN = W_CacheLumpName(DEH_String("CHAIN"), PU_STATIC);
    if (deathmatch)
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("STATBAR"), PU_STATIC);
    }
    else
    {
        PatchSTATBAR = W_CacheLumpName(DEH_String("LIFEBAR"), PU_STATIC);
    }
    if (!netgame)
    {                           // single player game uses red life gem
        PatchLIFEGEM = W_CacheLumpName(DEH_String("LIFEGEM2"), PU_STATIC);
    }
    else
    {
        PatchLIFEGEM = W_CacheLumpNum(W_GetNumForName(DEH_String("LIFEGEM0"))
                                      + consoleplayer, PU_STATIC);
    }
    PatchLTFCTOP = W_CacheLumpName(DEH_String("LTFCTOP"), PU_STATIC);
    PatchRTFCTOP = W_CacheLumpName(DEH_String("RTFCTOP"), PU_STATIC);
    PatchSELECTBOX = W_CacheLumpName(DEH_String("SELECTBOX"), PU_STATIC);
    PatchINVLFGEM1 = W_CacheLumpName(DEH_String("INVGEML1"), PU_STATIC);
    PatchINVLFGEM2 = W_CacheLumpName(DEH_String("INVGEML2"), PU_STATIC);
    PatchINVRTGEM1 = W_CacheLumpName(DEH_String("INVGEMR1"), PU_STATIC);
    PatchINVRTGEM2 = W_CacheLumpName(DEH_String("INVGEMR2"), PU_STATIC);
    PatchBLACKSQ = W_CacheLumpName(DEH_String("BLACKSQ"), PU_STATIC);
    PatchARMCLEAR = W_CacheLumpName(DEH_String("ARMCLEAR"), PU_STATIC);
    PatchCHAINBACK = W_CacheLumpName(DEH_String("CHAINBACK"), PU_STATIC);
    startLump = W_GetNumForName(DEH_String("IN0"));
    for (i = 0; i < 10; i++)
    {
        PatchINumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    PatchNEGATIVE = W_CacheLumpName(DEH_String("NEGNUM"), PU_STATIC);
    FontBNumBase = W_GetNumForName(DEH_String("FONTB16"));
    startLump = W_GetNumForName(DEH_String("SMALLIN0"));
    for (i = 0; i < 10; i++)
    {
        PatchSmNumbers[i] = W_CacheLumpNum(startLump + i, PU_STATIC);
    }
    playpalette = W_GetNumForName(DEH_String("PLAYPAL"));
    spinbooklump = W_GetNumForName(DEH_String("SPINBK0"));
    spinflylump = W_GetNumForName(DEH_String("SPFLY0"));
}

//---------------------------------------------------------------------------
//
// PROC SB_Ticker
//
//---------------------------------------------------------------------------

void SB_Ticker(void)
{
    int delta;
    int curHealth;

    if (leveltime & 1)
    {
        ChainWiggle = P_Random() & 1;
    }
    curHealth = players[consoleplayer].mo->health;
    if (curHealth < 0)
    {
        curHealth = 0;
    }
    if (curHealth < HealthMarker)
    {
        delta = (HealthMarker - curHealth) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 8)
        {
            delta = 8;
        }
        HealthMarker -= delta;
    }
    else if (curHealth > HealthMarker)
    {
        delta = (curHealth - HealthMarker) >> 2;
        if (delta < 1)
        {
            delta = 1;
        }
        else if (delta > 8)
        {
            delta = 8;
        }
        HealthMarker += delta;
    }

    // [JN] Update CRL_Widgets_t data.
    CPlayer = &players[displayplayer];

    CRLWidgets.kills = CPlayer->killcount;
    CRLWidgets.totalkills = totalkills;
    CRLWidgets.items = CPlayer->itemcount;
    CRLWidgets.totalitems = totalitems;
    CRLWidgets.secrets = CPlayer->secretcount;
    CRLWidgets.totalsecrets = totalsecret;

    CRLWidgets.x = CPlayer->mo->x;
    CRLWidgets.y = CPlayer->mo->y;
    CRLWidgets.ang = CPlayer->mo->angle;

    // Do red-/gold-shifts from damage/items
    if (!crl_spectating)
    {
        SB_PaletteFlash();
    }
}





//---------------------------------------------------------------------------
//
// PROC DrSmallNumber
//
// Draws a small two digit number.
//
//---------------------------------------------------------------------------

static void DrSmallNumber(int val, int x, int y)
{
    patch_t *patch;

    if (val == 1)
    {
        return;
    }
    if (val > 9)
    {
        patch = PatchSmNumbers[val / 10];
        V_DrawPatch(x, y, patch);
    }
    val = val % 10;
    patch = PatchSmNumbers[val];
    V_DrawPatch(x + 4, y, patch);
}

//---------------------------------------------------------------------------
//
// PROC ShadeLine
//
//---------------------------------------------------------------------------

static void ShadeLine(int x, int y, int height, int shade)
{
    byte *dest;
    byte *shades;

    shades = colormaps + 9 * 256 + shade * 2 * 256;
    dest = I_VideoBuffer + y * SCREENWIDTH + x;
    while (height--)
    {
        *(dest) = *(shades + *dest);
        dest += SCREENWIDTH;
    }
}

//---------------------------------------------------------------------------
//
// PROC ShadeChain
//
//---------------------------------------------------------------------------

static void ShadeChain(void)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        ShadeLine(277 + i, 190, 10, i / 2);
        ShadeLine(19 + i, 190, 10, 7 - (i / 2));
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawSoundInfo
//
// Displays sound debugging information.
//
//---------------------------------------------------------------------------

static void DrawSoundInfo(void)
{
    int i;
    SoundInfo_t s;
    ChanInfo_t *c;
    char text[32];
    int x;
    int y;
    int xPos[7] = { 1, 75, 112, 156, 200, 230, 260 };

    if (leveltime & 16)
    {
        MN_DrTextA(DEH_String("*** SOUND DEBUG INFO ***"), xPos[0], 20, NULL);
    }
    S_GetChannelInfo(&s);
    if (s.channelCount == 0)
    {
        return;
    }
    x = 0;
    MN_DrTextA(DEH_String("NAME"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.T"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.X"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("MO.Y"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("ID"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("PRI"), xPos[x++], 30, NULL);
    MN_DrTextA(DEH_String("DIST"), xPos[x++], 30, NULL);
    for (i = 0; i < s.channelCount; i++)
    {
        c = &s.chan[i];
        x = 0;
        y = 40 + i * 10;
        if (c->mo == NULL)
        {                       // Channel is unused
            MN_DrTextA(DEH_String("------"), xPos[0], y, NULL);
            continue;
        }
        M_snprintf(text, sizeof(text), "%s", c->name);
        M_ForceUppercase(text);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->type);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->x >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->mo->y >> FRACBITS);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->id);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->priority);
        MN_DrTextA(text, xPos[x++], y, NULL);
        M_snprintf(text, sizeof(text), "%d", c->distance);
        MN_DrTextA(text, xPos[x++], y, NULL);
    }
}

//---------------------------------------------------------------------------
//
// PROC SB_Drawer
//
//---------------------------------------------------------------------------

// [crispy] Needed to support widescreen status bar.
void SB_ForceRedraw(void)
{
    SB_state = -1;
}





// -----------------------------------------------------------------------------
// SB_MainBarColor
// [crispy] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

enum
{
    hudcolor_ammo,
    hudcolor_health,
    hudcolor_frags,
    hudcolor_armor
} hudcolor_t;

static byte *const SB_MainBarColor (const int i)
{
    if (!crl_colored_stbar)
    {
        return NULL;
    }

    switch (i)
    {
        case hudcolor_ammo:
        {
            if (wpnlev1info[CPlayer->readyweapon].ammo == am_noammo)
            {
                return NULL;
            }
            else
            {
                const int ammo = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
                const int fullammo = CPlayer->maxammo[wpnlev1info[CPlayer->readyweapon].ammo];

                if (ammo < fullammo/4)
                    return cr[CR_RED];
                else if (ammo < fullammo/2)
                    return cr[CR_YELLOW];
                else
                    return cr[CR_GREEN];
            }
            break;
        }
        case hudcolor_health:
        {
            const int health = CPlayer->health;

            // [crispy] Invulnerability powerup and God Mode cheat turn Health values gray
            if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                return cr[CR_WHITE];
            else if (health >= 67)
                return cr[CR_GREEN];
            else if (health >= 34)
                return cr[CR_YELLOW];
            else
                return cr[CR_RED];
            break;
        }
        case hudcolor_frags:
        {
            const int frags = CPlayer->frags[consoleplayer];

            if (frags < 0)
                return cr[CR_RED];
            else if (frags == 0)
                return cr[CR_YELLOW];
            else
                return cr[CR_GREEN];
            break;
        }
        case hudcolor_armor:
        {
	    // [crispy] Invulnerability powerup and God Mode cheat turn Armor values gray
	    if (CPlayer->cheats & CF_GODMODE || CPlayer->powers[pw_invulnerability])
                return cr[CR_WHITE];
	    // [crispy] color by armor type
	    else if (CPlayer->armortype >= 2)
                return cr[CR_YELLOW];
	    else if (CPlayer->armortype == 1)
                return cr[CR_LIGHTGRAY];
	    else if (CPlayer->armortype == 0)
                return cr[CR_RED];
            break;
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// CRL_ReloadPalette
// -----------------------------------------------------------------------------

void CRL_ReloadPalette (void)
{
    byte *pal = (byte *) W_CacheLumpNum (playpalette, PU_CACHE)+sb_palette*768;
    I_SetPalette (pal);
}

// sets the new palette based upon current values of player->damagecount
// and player->bonuscount
void SB_PaletteFlash(void)
{
    int palette;

    CPlayer = &players[consoleplayer];

    if (CPlayer->damagecount)
    {
        // [JN] A11Y - Palette flash effects.
        // For A11Y, fix missing first pain palette index for smoother effect.
        // [PN] Simplified pain palette logic and reduced redundancy.

        const int offset     = (CPlayer->damagecount + 7) >> 3;
        const int maxOffset  = NUMREDPALS - (crl_a11y_pal_flash ? 0 : 1);
        const int startIndex = STARTREDPALS + (crl_a11y_pal_flash ? -1 : 0);

        // [PN] Select offset, clamped to allowed range
        palette = startIndex + (offset < NUMREDPALS ? offset : maxOffset);

        // [PN] Aadditional limits for flash mode
        if      (crl_a11y_pal_flash == 1 && palette > 5) palette = 5;
        else if (crl_a11y_pal_flash == 2 && palette > 2) palette = 2;
        else if (crl_a11y_pal_flash == 3)                palette = 0;
    }
    else if (CPlayer->bonuscount)
    {
        // [JN] A11Y - Palette flash effects.
        // For A11Y, fix missing first bonus palette index for smoother effect.
        // [PN] Simplified bonus palette logic and reduced redundancy.

        const int offset     = (CPlayer->bonuscount + 7) >> 3;
        const int maxOffset  = NUMBONUSPALS - (crl_a11y_pal_flash ? 0 : 1);
        const int startIndex = STARTBONUSPALS + (crl_a11y_pal_flash ? -1 : 0);

        // [PN] Select offset, clamped to allowed range
        palette = startIndex + (offset < NUMBONUSPALS ? offset : maxOffset);

        // [PN] Aadditional limits for flash mode
        if      (crl_a11y_pal_flash == 1 && palette > 11) palette = 11;
        else if (crl_a11y_pal_flash == 2 && palette >  9) palette =  9;
        else if (crl_a11y_pal_flash == 3)                 palette =  0;
    }
    else
    {
        palette = 0;
    }
    if (palette != sb_palette)
    {
        sb_palette = palette;
        CRL_ReloadPalette();
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawCommonBar
//
//---------------------------------------------------------------------------

static void DrawCommonBar(void)
{
    int chainY;
    int chainYY;
    int healthPos;

    V_DrawPatch(0, 148, PatchLTFCTOP);
    V_DrawPatch(290, 148, PatchRTFCTOP);

    if (oldhealth != HealthMarker)
    {
        oldhealth = HealthMarker;
        healthPos = HealthMarker;
        if (healthPos < 0)
        {
            healthPos = 0;
        }
        if (healthPos > 100)
        {
            healthPos = 100;
        }
        healthPos = (healthPos * 256) / 100;
        chainY =
            (HealthMarker == CPlayer->mo->health) ? 191 : 191 + ChainWiggle;
        // [JN] TODO - something wrong with chain wiggling.
        chainYY = 
            (HealthMarker == CPlayer->mo->health) ? chainY : 191;
        V_DrawPatch(0, 190, PatchCHAINBACK);
        V_DrawPatch(2 + (healthPos % 17), chainYY, PatchCHAIN);
        V_DrawPatch(17 + healthPos, chainYY, PatchLIFEGEM);
        V_DrawPatch(0, 190, PatchLTFACE);
        V_DrawPatch(276, 190, PatchRTFACE);
        ShadeChain();
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

static const char patcharti[][10] = {
    {"ARTIBOX"},                // none
    {"ARTIINVU"},               // invulnerability
    {"ARTIINVS"},               // invisibility
    {"ARTIPTN2"},               // health
    {"ARTISPHL"},               // superhealth
    {"ARTIPWBK"},               // tomeofpower
    {"ARTITRCH"},               // torch
    {"ARTIFBMB"},               // firebomb
    {"ARTIEGGC"},               // egg
    {"ARTISOAR"},               // fly
    {"ARTIATLP"}                // teleport
};

static const char ammopic[][10] = {
    {"INAMGLD"},
    {"INAMBOW"},
    {"INAMBST"},
    {"INAMRAM"},
    {"INAMPNX"},
    {"INAMLOB"}
};

static void DrawMainBar(void)
{
    int i;
    int temp;

    // Ready artifact
    if (ArtifactFlash)
    {
        V_DrawPatch(180, 161, PatchBLACKSQ);

        temp = W_GetNumForName(DEH_String("useartia")) + ArtifactFlash - 1;

        V_DrawPatch(182, 161, W_CacheLumpNum(temp, PU_CACHE));
        ArtifactFlash--;
        oldarti = -1;           // so that the correct artifact fills in after the flash
    }
    else if (oldarti != CPlayer->readyArtifact
             || oldartiCount != CPlayer->inventory[inv_ptr].count)
    {
        V_DrawPatch(180, 161, PatchBLACKSQ);
        if (CPlayer->readyArtifact > 0)
        {
            V_DrawPatch(179, 160,
                        W_CacheLumpName(DEH_String(patcharti[CPlayer->readyArtifact]),
                                        PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 201, 182);
        }
        oldarti = CPlayer->readyArtifact;
        oldartiCount = CPlayer->inventory[inv_ptr].count;
    }

    // Frags
    if (deathmatch)
    {
        temp = 0;
        for (i = 0; i < MAXPLAYERS; i++)
        {
            temp += CPlayer->frags[i];
        }
        if (temp != oldfrags)
        {
            V_DrawPatch(57, 171, PatchARMCLEAR);
            dp_translation = SB_MainBarColor(hudcolor_frags);
            DrINumber(temp, 61, 170);
            dp_translation = NULL;
            oldfrags = temp;
        }
    }
    else
    {
        temp = HealthMarker;
        if (temp < 0)
        {
            temp = 0;
        }
        else if (temp > 100)
        {
            temp = 100;
        }
        if (oldlife != temp)
        {
            oldlife = temp;
            V_DrawPatch(57, 171, PatchARMCLEAR);
            dp_translation = SB_MainBarColor(hudcolor_health);
            DrINumber(temp, 61, 170);
            dp_translation = NULL;
        }
    }

    // Keys
    if (oldkeys != playerkeys)
    {
        if (CPlayer->keys[key_yellow])
        {
            V_DrawPatch(153, 164, W_CacheLumpName(DEH_String("ykeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_green])
        {
            V_DrawPatch(153, 172, W_CacheLumpName(DEH_String("gkeyicon"), PU_CACHE));
        }
        if (CPlayer->keys[key_blue])
        {
            V_DrawPatch(153, 180, W_CacheLumpName(DEH_String("bkeyicon"), PU_CACHE));
        }
        oldkeys = playerkeys;
    }
    // Ammo
    temp = CPlayer->ammo[wpnlev1info[CPlayer->readyweapon].ammo];
    if (oldammo != temp || oldweapon != CPlayer->readyweapon)
    {
        V_DrawPatch(108, 161, PatchBLACKSQ);
        if (temp && CPlayer->readyweapon > 0 && CPlayer->readyweapon < 7)
        {
            dp_translation = SB_MainBarColor(hudcolor_ammo);
            DrINumber(temp, 109, 162);
            dp_translation = NULL;
            V_DrawPatch(111, 172,
                        W_CacheLumpName(DEH_String(ammopic[CPlayer->readyweapon - 1]),
                                        PU_CACHE));
        }
        oldammo = temp;
        oldweapon = CPlayer->readyweapon;
    }

    // Armor
    if (oldarmor != CPlayer->armorpoints)
    {
        V_DrawPatch(224, 171, PatchARMCLEAR);
        dp_translation = SB_MainBarColor(hudcolor_armor);
        DrINumber(CPlayer->armorpoints, 228, 170);
        dp_translation = NULL;
        oldarmor = CPlayer->armorpoints;
    }
}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

static void DrawInventoryBar(void)
{
    const char *patch;
    int i;
    int x;

    x = inv_ptr - curpos;
    V_DrawPatch(34, 160, PatchINVBAR);
    for (i = 0; i < 7; i++)
    {
        //V_DrawPatch(50+i*31, 160, W_CacheLumpName("ARTIBOX", PU_CACHE));
        if (CPlayer->inventorySlotNum > x + i
            && CPlayer->inventory[x + i].type != arti_none)
        {
            patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);

            V_DrawPatch(50 + i * 31, 160, W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31, 182);
        }
    }
    V_DrawPatch(50 + curpos * 31, 189, PatchSELECTBOX);
    if (x != 0)
    {
        V_DrawPatch(38, 159, !(leveltime & 4) ? PatchINVLFGEM1 : PatchINVLFGEM2);
    }
    if (CPlayer->inventorySlotNum - x > 7)
    {
        V_DrawPatch(269, 159, !(leveltime & 4) ? PatchINVRTGEM1 : PatchINVRTGEM2);
    }
}

static void DrawFullScreenStuff(void)
{
    const char *patch;
    int i;
    int x;
    int temp;

    dp_translation = SB_MainBarColor(hudcolor_health);
    if (CPlayer->mo->health > 0)
    {
        DrBNumber(CPlayer->mo->health, 5, 180);
    }
    else
    {
        DrBNumber(0, 5, 180);
    }
    dp_translation = NULL;
    if (deathmatch)
    {
        temp = 0;
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i])
            {
                temp += CPlayer->frags[i];
            }
        }
        dp_translation = SB_MainBarColor(hudcolor_frags);
        DrINumber(temp, 45, 185);
        dp_translation = NULL;
    }
    if (!inventory)
    {
        if (CPlayer->readyArtifact > 0)
        {
            patch = DEH_String(patcharti[CPlayer->readyArtifact]);
            V_DrawTLPatch(286, 170, W_CacheLumpName(DEH_String("ARTIBOX"), PU_CACHE));
            V_DrawPatch(286, 170, W_CacheLumpName(patch, PU_CACHE));
            DrSmallNumber(CPlayer->inventory[inv_ptr].count, 307, 192);
        }
    }
    else
    {
        x = inv_ptr - curpos;
        for (i = 0; i < 7; i++)
        {
            V_DrawTLPatch(50 + i * 31, 168,
                          W_CacheLumpName(DEH_String("ARTIBOX"), PU_CACHE));
            if (CPlayer->inventorySlotNum > x + i
                && CPlayer->inventory[x + i].type != arti_none)
            {
                patch = DEH_String(patcharti[CPlayer->inventory[x + i].type]);
                V_DrawPatch(50 + i * 31, 168,
                            W_CacheLumpName(patch, PU_CACHE));
                DrSmallNumber(CPlayer->inventory[x + i].count, 69 + i * 31,
                              190);
            }
        }
        V_DrawPatch(50 + curpos * 31, 197, PatchSELECTBOX);
        if (x != 0)
        {
            V_DrawPatch(38, 167, !(leveltime & 4) ? PatchINVLFGEM1 : PatchINVLFGEM2);
        }
        if (CPlayer->inventorySlotNum - x > 7)
        {
            V_DrawPatch(269, 167, !(leveltime & 4) ? PatchINVRTGEM1 : PatchINVRTGEM2);
        }
    }
}

// -----------------------------------------------------------------------------
// SB_AmmoWidgetColor
// [plums] return ammo/health/armor widget color
// -----------------------------------------------------------------------------

enum
{
    ammowidgetcolor_ammo,
    ammowidgetcolor_weapon
} ammowidgetcolor_t;

static byte *const SB_AmmoWidgetColor (int i, weapontype_t weapon)
{
    switch (i)
    {
        case ammowidgetcolor_ammo:
        {
            const int ammo = CPlayer->ammo[wpnlev1info[weapon].ammo];
            const int fullammo = CPlayer->maxammo[wpnlev1info[weapon].ammo];

            if (crl_ammo_widget_colors != 1 && crl_ammo_widget_colors != 2)
            {
                return cr[CR_GRAY];
            }

            if (ammo < fullammo/4)
                return cr[CR_RED];
            else if (ammo < fullammo/2)
                return cr[CR_YELLOW];
            else
                return cr[CR_GREEN];
        }
        case ammowidgetcolor_weapon:
        {
            // always color the weapon letter if ammo widget coloring is OFF
            if ((crl_ammo_widget_colors != 1 && crl_ammo_widget_colors != 3) ||
                CPlayer->weaponowned[weapon] == true)
            {
                switch (weapon)
                {
                    case wp_goldwand:   return cr[CR_YELLOW];
                    case wp_crossbow:   return cr[CR_GREEN];
                    case wp_blaster:    return cr[CR_BLUE2];
                    case wp_skullrod:   return cr[CR_RED];
                    case wp_phoenixrod: return cr[CR_ORANGE];
                    case wp_mace:       return cr[CR_LIGHTGRAY];
                    default:            return cr[CR_GRAY];
                }
            }
            else
            {
                return cr[CR_GRAY];
            }
        }
    }

    return NULL;
}

// -----------------------------------------------------------------------------
// SB_Drawer
// -----------------------------------------------------------------------------

void SB_Drawer(void)
{
    int frame;
    static boolean hitCenterFrame;

    // Sound info debug stuff
    if (DebugSound == true)
    {
        DrawSoundInfo();
    }
    CPlayer = &players[consoleplayer];
    if (viewheight == SCREENHEIGHT && (!automapactive || crl_automap_overlay))
    {
        DrawFullScreenStuff();
        SB_state = -1;
    }
    else
    {
        // [JN] CRL - always do full status bar update, as we drawing
        // everything below automap for proper render counter values.
        SB_state = -1;

        if (SB_state == -1)
        {
            V_DrawPatch(0, 158, PatchBARBACK);
            if (players[consoleplayer].cheats & CF_GODMODE)
            {
                V_DrawPatch(16, 167,
                            W_CacheLumpName(DEH_String("GOD1"), PU_CACHE));
                V_DrawPatch(287, 167,
                            W_CacheLumpName(DEH_String("GOD2"), PU_CACHE));
            }
            oldhealth = -1;
        }
        DrawCommonBar();
        if (!inventory)
        {
            if (SB_state != 0)
            {
                // Main interface
                V_DrawPatch(34, 160, PatchSTATBAR);
                oldarti = 0;
                oldammo = -1;
                oldarmor = -1;
                oldweapon = -1;
                oldfrags = -9999;       //can't use -1, 'cuz of negative frags
                oldlife = -1;
                oldkeys = -1;
            }
            DrawMainBar();
            SB_state = 0;
        }
        else
        {
            if (SB_state != 1)
            {
                V_DrawPatch(34, 160, PatchINVBAR);
            }
            DrawInventoryBar();
            SB_state = 1;
        }
    }

    // Flight icons
    if (CPlayer->powers[pw_flight])
    {
        if (CPlayer->powers[pw_flight] > BLINKTHRESHOLD
            || !(CPlayer->powers[pw_flight] & 16))
        {
            frame = (leveltime / 3) & 15;
            if (CPlayer->mo->flags2 & MF2_FLY)
            {
                if (hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(20, 17, W_CacheLumpNum(spinflylump + 15,
                                                       PU_CACHE));
                }
                else
                {
                    V_DrawPatch(20, 17, W_CacheLumpNum(spinflylump + frame,
                                                       PU_CACHE));
                    hitCenterFrame = false;
                }
            }
            else
            {
                if (!hitCenterFrame && (frame != 15 && frame != 0))
                {
                    V_DrawPatch(20, 17, W_CacheLumpNum(spinflylump + frame,
                                                       PU_CACHE));
                    hitCenterFrame = false;
                }
                else
                {
                    V_DrawPatch(20, 17, W_CacheLumpNum(spinflylump + 15,
                                                       PU_CACHE));
                    hitCenterFrame = true;
                }
            }
        }
    }

    if (CPlayer->powers[pw_weaponlevel2] && !CPlayer->chickenTics)
    {
        int spinbook_x = 300; // [crispy]

        // [JN] Shift tome icon left if fps counter or demo timer is active.
        if (crl_showfps
        || (demoplayback && (crl_demo_timer == 1 || crl_demo_timer == 3))
        || (demorecording && (crl_demo_timer == 2 || crl_demo_timer == 3)))
        {
            spinbook_x -= 70;
        }

        if (CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
        || !(CPlayer->powers[pw_weaponlevel2] & 16))
        {
            frame = (leveltime / 3) & 15;
            V_DrawPatch(spinbook_x, 17,
                        W_CacheLumpNum(spinbooklump + frame, PU_CACHE));
        }
    }

    // [JN] Ammo widget.
    if (crl_ammo_widget && crl_extended_hud)
    {
        char str[16];

        // [PN] Parallel arrays for slot data (0-5).
        const weapontype_t weapons[6] = { wp_goldwand, wp_crossbow, wp_blaster, wp_skullrod, wp_phoenixrod, wp_mace };
        const int ammo_types[6]       = { am_goldwand, am_crossbow, am_blaster, am_skullrod, am_phoenixrod, am_mace };
        const char *const labels[6]   = { "W", "E", "D", "H", "P", "M" };

        byte *ammo_widget_weapon_colors[6];
        byte *ammo_widget_ammo_colors[6];

        // [JN] Move widgets slightly down when using a fullscreen status bar.
        int yy = (crl_screen_size > 10 && (!automapactive || crl_automap_overlay)) ? 13 : 0;

        // [PN] Cache ammo-widget colors for all weapon slots once per draw pass.
        for (int i = 0; i < 6; i++)
        {
            ammo_widget_weapon_colors[i] = SB_AmmoWidgetColor(ammowidgetcolor_weapon, weapons[i]);
            ammo_widget_ammo_colors[i]   = SB_AmmoWidgetColor(ammowidgetcolor_ammo, weapons[i]);
        }

        // [PN] Enable translucency once for both modes.
        dp_translucent = crl_ammo_widget_translucent;

        for (int i = 0; i < 6; i++)
        {
            const int current_yy = 95 + (i * 10) + yy;

            // [PN] Calculate weapon icon X: 282 for Brief (1), 251 for Full.
            const int label_xx = ((crl_ammo_widget == 1) ? 282 : 251);
            MN_DrTextA(labels[i], label_xx, current_yy, ammo_widget_weapon_colors[i]);

            if (crl_ammo_widget == 1) // Brief
            {
                sprintf(str, "%d", CPlayer->ammo[ammo_types[i]]);
                MN_DrTextA(str, 293, current_yy, ammo_widget_ammo_colors[i]);
            }
            else // Full
            {
                sprintf(str, "%d/", CPlayer->ammo[ammo_types[i]]);
                MN_DrTextA(str, 293 - MN_TextAWidth(str), current_yy, ammo_widget_ammo_colors[i]);

                sprintf(str, "%d", CPlayer->maxammo[ammo_types[i]]);
                MN_DrTextA(str, 293, current_yy, ammo_widget_ammo_colors[i]);
            }
        }

        dp_translucent = false;
    }
}


// =============================================================================
//
//                              CHEAT FUNCTIONS
//
// =============================================================================

#define FULL_CHEAT_CHECK if(netgame || demorecording || demoplayback){return;}
#define SAFE_CHEAT_CHECK if(netgame || demorecording){return;}

typedef struct Cheat_s
{
    void (*func) (player_t *const player, struct Cheat_s *const cheat);
    cheatseq_t *seq;
} Cheat_t;

static void CheatWaitFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    // [JN] If user types "id", activate timer to prevent
    // other than typing actions in G_Responder.
    player->cheatTics = TICRATE * 2;
}

static void CheatGodFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    // [crispy] dead players are first respawned at the current position
    mapthing_t mt = {0};

    if (player->playerstate == PST_DEAD)
    {
        angle_t an;

        mt.x = player->mo->x >> FRACBITS;
        mt.y = player->mo->y >> FRACBITS;
        mt.angle = (player->mo->angle + ANG45/2)*(uint64_t)45/ANG45;
        mt.type = consoleplayer + 1;
        P_SpawnPlayer(&mt);

        // [crispy] spawn a teleport fog
        an = player->mo->angle >> ANGLETOFINESHIFT;
        P_SpawnMobj(player->mo->x + 20 * finecosine[an],
                    player->mo->y + 20 * finesine[an],
                    player->mo->z + TELEFOGHEIGHT, MT_TFOG);
        S_StartSound(NULL, sfx_telept);

        // [crispy] fix reviving as "zombie" if god mode was already enabled
        if (player->mo)
        {
            player->mo->health = MAXHEALTH;
        }
        player->health = MAXHEALTH;
        player->lookdir = 0;
    }

    player->cheats ^= CF_GODMODE;
    CT_SetMessage(player, DEH_String(player->cheats & CF_GODMODE ?
                  TXT_CHEATGODON : TXT_CHEATGODOFF), false, NULL);
    SB_state = -1;
    player->cheatTics = 1;
}

static void CheatWeaponsFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    int i;

    player->armorpoints = 200;
    player->armortype = 2;
    if (!player->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for (i = 0; i < NUMWEAPONS - 1; i++)
    {
        player->weaponowned[i] = true;
    }
    if (gamemode == shareware)
    {
        player->weaponowned[wp_skullrod] = false;
        player->weaponowned[wp_phoenixrod] = false;
        player->weaponowned[wp_mace] = false;
    }
    for (i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATWEAPONS), false, NULL);
    player->cheatTics = 1;
}

static void CheatWeapKeysFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    int i;

    player->armorpoints = 200;
    player->armortype = 2;
    if (!player->backpack)
    {
        for (i = 0; i < NUMAMMO; i++)
        {
            player->maxammo[i] *= 2;
        }
        player->backpack = true;
    }
    for (i = 0; i < NUMWEAPONS - 1; i++)
    {
        player->weaponowned[i] = true;
    }
    if (gamemode == shareware)
    {
        player->weaponowned[wp_skullrod] = false;
        player->weaponowned[wp_phoenixrod] = false;
        player->weaponowned[wp_mace] = false;
    }
    for (i = 0; i < NUMAMMO; i++)
    {
        player->ammo[i] = player->maxammo[i];
    }
    player->keys[key_yellow] = true;
    player->keys[key_green] = true;
    player->keys[key_blue] = true;
    playerkeys = 7;             // Key refresh flags
    CT_SetMessage(player, DEH_String(TXT_CHEATWEAPKEYS), false, NULL);
    player->cheatTics = 1;
}

static void CheatChoppersFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    player->weaponowned[wp_gauntlets] = true;
    player->powers[pw_invulnerability] = true;
    CT_SetMessage(player, DEH_String(TXT_CHOPPERS), false, NULL);
}

static void CheatKeysFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    player->keys[key_yellow] = true;
    player->keys[key_green] = true;
    player->keys[key_blue] = true;
    playerkeys = 7;             // Key refresh flags
    CT_SetMessage(player, DEH_String(TXT_CHEATKEYS), false, NULL);
    player->cheatTics = 1;
}

static void CheatNoClipFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_NOCLIP;
    CT_SetMessage(player, DEH_String(player->cheats & CF_NOCLIP ?
                  TXT_CHEATNOCLIPON : TXT_CHEATNOCLIPOFF), false, NULL);
    player->cheatTics = 1;
}

static void CheatWarpFunc (player_t *const player, Cheat_t *const cheat)
{
    // [JN] Safe to use IDCLEV/ENGAGE while demo playback.
    SAFE_CHEAT_CHECK;

    char args[2];

    cht_GetParam(cheat->seq, args);

    const int episode = args[0] - '0';
    const int map = args[1] - '0';
    if (D_ValidEpisodeMap(heretic, gamemode, episode, map))
    {
        // [crisp] allow IDCLEV during demo playback and warp to the requested map
        if (demoplayback)
        {
            demowarp = map;
            nodrawers = true;
            singletics = true;

            if (map <= gamemap)
            {
                G_DoPlayDemo();
            }
        }
        else
        {
            G_DeferedInitNew(gameskill, episode, map);
            player->cheatTics = 1;
        }
    }
}

static void CheatMDKFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    // Do an overflow-safe trace to get target.
    P_AimLineAttack (player->mo, player->mo->angle, MISSILERANGE, true);

    if (linetarget)
    {
        // Got one, deal damage equal to it's health.
        P_DamageMobj (linetarget, NULL, NULL, player->targetsheath);
        CT_SetMessage(player, "TARGET KILLED", false, NULL);
    }
    else
    {
        // No target found, just inform.
        CT_SetMessage(player, "TARGET NOT FOUND", false, NULL);
    }
}

static void CheatArtifact1Func (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS1), false, NULL);
}

static void CheatArtifact2Func (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS2), false, NULL);
}

static void CheatArtifact3Func (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    char args[2];
    int i;

    cht_GetParam(cheat->seq, args);
    const int type = args[0] - 'a' + 1;
    const int count = args[1] - '0';
    if (type == 26 && count == 0)
    {                           // All artifacts
        for (i = arti_none + 1; i < NUMARTIFACTS; i++)
        {
            if (gamemode == shareware 
             && (i == arti_superhealth || i == arti_teleport))
            {
                continue;
            }
            for (int j = 0; j < 16; j++)
            {
                P_GiveArtifact(player, i, NULL);
            }
        }
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS3), false, NULL);
    }
    else if (type > arti_none && type < NUMARTIFACTS
             && count > 0 && count < 10)
    {
        if (gamemode == shareware
        && (type == arti_superhealth || type == arti_teleport))
        {
            CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTSFAIL), false, NULL);
            return;
        }
        for (i = 0; i < count; i++)
        {
            P_GiveArtifact(player, type, NULL);
        }
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS3), false, NULL);
    }
    else
    {                           // Bad input
        CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTSFAIL), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatArtifactAllFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    for (int i = arti_none + 1; i < NUMARTIFACTS; i++)
    {
        for (int j = 0; j < 16; j++)
        {
            P_GiveArtifact(player, i, NULL);
        }
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATARTIFACTS4), false, NULL);
    player->cheatTics = 1;
}

static void CheatPowerFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    if (player->powers[pw_weaponlevel2])
    {
        player->powers[pw_weaponlevel2] = 0;
        CT_SetMessage(player, DEH_String(TXT_CHEATPOWEROFF), false, NULL);
    }
    else
    {
        P_UseArtifact(player, arti_tomeofpower);
        CT_SetMessage(player, DEH_String(TXT_CHEATPOWERON), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatHealthFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    if (player->chickenTics)
    {
        player->health = player->mo->health = MAXCHICKENHEALTH;
    }
    else
    {
        player->health = player->mo->health = MAXHEALTH;
    }
    CT_SetMessage(player, DEH_String(TXT_CHEATHEALTH), false, NULL);
    player->cheatTics = 1;
}

static void CheatChickenFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    if (player->chickenTics)
    {
        if (P_UndoPlayerChicken(player))
        {
            CT_SetMessage(player, DEH_String(TXT_CHEATCHICKENOFF), false, NULL);
        }
    }
    else if (P_ChickenMorphPlayer(player))
    {
        CT_SetMessage(player, DEH_String(TXT_CHEATCHICKENON), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatMassacreFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    P_Massacre();
    CT_SetMessage(player, DEH_String(TXT_CHEATMASSACRE), false, NULL);
    player->cheatTics = 1;
}

static int SB_Cheat_Massacre (const boolean explode)
{
    int killcount = 0;
    int amount;
    mobj_t *mo;
    thinker_t *think;

    for (think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        if (think->function != P_MobjThinker)
        {                       // Not a mobj thinker
            continue;
        }
        mo = (mobj_t *) think;
        amount = explode ? 10000 : mo->health;
        if ((mo->flags & MF_COUNTKILL) && (mo->health > 0))
        {
            P_DamageMobj(mo, NULL, NULL, amount);
            killcount++;
        }
    }
    return killcount;
}

static void CheatTNTEMFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    static char buf[52];

    const int killcount = SB_Cheat_Massacre(true);

    M_snprintf(buf, sizeof(buf), "MONSTERS KILLED: %d", killcount);

    CT_SetMessage(player, buf, false, NULL);
    player->cheatTics = 1;
}

static void CheatKILLEMFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;

    static char buf[52];

    const int killcount = SB_Cheat_Massacre(false);

    M_snprintf(buf, sizeof(buf), "MONSTERS KILLED: %d", killcount);

    CT_SetMessage(player, buf, false, NULL);
    player->cheatTics = 1;
}

static void CheatIDMUSFunc (player_t *const player, Cheat_t *const cheat)
{
    char buf[3];

    // [JN] Harmless cheat, always allow.

    // [JN] Prevent impossible selection.
    const int maxnum = gamemode == retail     ? 47 :  // 5 episodes
                       gamemode == registered ? 26 :  // 3 episodes
                                                 8 ;  // 1 episode (shareware)

    cht_GetParam(cheat->seq, buf);
    const int musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');

    if (((buf[0]-'1')*9 + buf[1]-'1') > maxnum)
    {
        CT_SetMessage(player, DEH_String(TXT_NOMUS), false, NULL);
    }
    else
    {
        S_StartSong(musnum, true);
        // [JN] jff 3/17/98 remember idmus number for restore
        // idmusnum = musnum;
        CT_SetMessage(player, DEH_String(TXT_MUS), false, NULL);
    }
    player->cheatTics = 1;
}

static void CheatAMapFunc (player_t *const player, Cheat_t *const cheat)
{
    // [JN] Harmless cheat, always allow.
    ravmap_cheating = (ravmap_cheating + 1) % 3;
    player->cheatTics = 1;
}

// -----------------------------------------------------------------------------
// SB_CheatRevealThing
//  [PN] Cycles automap camera through matching things.
// -----------------------------------------------------------------------------

static void SB_CheatRevealThing (const int flags, const boolean alive_only, int *last_index)
{
    thinker_t *think;
    mobj_t *first_match = NULL;
    mobj_t *selected = NULL;
    int match_index = 0;
    int selected_index = -1;
    const int target_index = *last_index + 1;

    if (!automapactive)
    {
        return;
    }

    for (think = thinkercap.next; think != &thinkercap; think = think->next)
    {
        if (think->function != P_MobjThinker)
        {
            continue;
        }

        mobj_t *mo = (mobj_t *) think;

        if (!(mo->flags & flags))
        {
            continue;
        }

        if (alive_only && mo->health <= 0)
        {
            continue;
        }

        if (!first_match)
        {
            first_match = mo;
        }

        if (match_index == target_index)
        {
            selected = mo;
            selected_index = match_index;
            break;
        }

        ++match_index;
    }

    if (!selected && first_match)
    {
        selected = first_match;
        selected_index = 0;
    }

    if (selected)
    {
        am_followplayer = 0;
        AM_SetMapCenter(selected->x, selected->y);
        *last_index = selected_index;
    }
    else
    {
        *last_index = -1;
    }
}

// -----------------------------------------------------------------------------
// CheatRevealKillFunc
//  [PN] Cycles automap camera through alive monsters.
// -----------------------------------------------------------------------------

static void CheatRevealKillFunc (player_t *const player, Cheat_t *const cheat)
{
    static int last_index = -1;
    static int last_episode = -1;
    static int last_map = -1;

    if (deathmatch)
    {
        return;
    }

    if (last_episode != gameepisode || last_map != gamemap)
    {
        last_episode = gameepisode;
        last_map = gamemap;
        last_index = -1;
    }

    SB_CheatRevealThing(MF_COUNTKILL, true, &last_index);
    player->cheatTics = 1;
}

// -----------------------------------------------------------------------------
// CheatRevealItemFunc
//  [PN] Cycles automap camera through countable items.
// -----------------------------------------------------------------------------

static void CheatRevealItemFunc (player_t *const player, Cheat_t *const cheat)
{
    static int last_index = -1;
    static int last_episode = -1;
    static int last_map = -1;

    if (deathmatch)
    {
        return;
    }

    if (last_episode != gameepisode || last_map != gamemap)
    {
        last_episode = gameepisode;
        last_map = gamemap;
        last_index = -1;
    }

    SB_CheatRevealThing(MF_COUNTITEM, false, &last_index);
    player->cheatTics = 1;
}

// -----------------------------------------------------------------------------
// SB_IsSecretSector
//  [PN] Treat both live and already-revealed secret sectors as valid targets.
// -----------------------------------------------------------------------------

static boolean SB_IsSecretSector (const sector_t *sec)
{
    return sec->special == 9 || sec->oldspecial == 9;
}

// -----------------------------------------------------------------------------
// CheatRevealSecretFunc
//  [PN] Cycles automap camera through secret sectors.
// -----------------------------------------------------------------------------

static void CheatRevealSecretFunc (player_t *const player, Cheat_t *const cheat)
{
    static int last_secret = -1;
    static int last_episode = -1;
    static int last_map = -1;

    if (deathmatch || !automapactive || numsectors <= 0)
    {
        return;
    }

    if (last_episode != gameepisode || last_map != gamemap)
    {
        last_episode = gameepisode;
        last_map = gamemap;
        last_secret = -1;
    }

    for (int step = 0; step < numsectors; ++step)
    {
        int i = last_secret + 1 + step;
        i %= numsectors;

        if (SB_IsSecretSector(&sectors[i])
        && sectors[i].linecount > 0
        && sectors[i].lines != NULL
        && sectors[i].lines[0] != NULL
        && sectors[i].lines[0]->v1 != NULL)
        {
            am_followplayer = 0;
            AM_SetMapCenter(sectors[i].lines[0]->v1->x, sectors[i].lines[0]->v1->y);
            last_secret = i;
            break;
        }
    }

    player->cheatTics = 1;
}



static void CheatIDMYPOSFunc (player_t *const player, Cheat_t *const cheat)
{
    static char buf[52];

    // [JN] Harmless cheat, always allow.
    M_snprintf(buf, sizeof(buf), "ANG=0X%X;X,Y=(0X%X,0X%X)",
               players[displayplayer].mo->angle,
               players[displayplayer].mo->x,
               players[displayplayer].mo->y);
    CT_SetMessage(player, buf, false, NULL);
    player->cheatTics = 1;
}

static void CheatFREEZEFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    crl_freeze ^= 1;
    CT_SetMessage(&players[consoleplayer], crl_freeze ?
                 CRL_FREEZE_ON : CRL_FREEZE_OFF, false, NULL);
    player->cheatTics = 1;
}

static void CheatNOTARGETFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_NOTARGET;
    P_ForgetPlayer(player);
    CT_SetMessage(player, player->cheats & CF_NOTARGET ?
                  CRL_NOTARGET_ON : CRL_NOTARGET_OFF, false, NULL);
    player->cheatTics = 1;
}

static void CheatBUDDHAFunc (player_t *const player, Cheat_t *const cheat)
{
    FULL_CHEAT_CHECK;
    player->cheats ^= CF_BUDDHA;
    CT_SetMessage(player, player->cheats & CF_BUDDHA ?
                  CRL_BUDDHA_ON : CRL_BUDDHA_OFF, false, NULL);
    player->cheatTics = 1;
}

// -----------------------------------------------------------------------------
//
// CHEAT CODES
//
// -----------------------------------------------------------------------------

#define CHEAT_SEQ(str, params) str, sizeof(str)-1, params, 0, 0, ""

static Cheat_t Cheats[] = {
    { CheatWaitFunc,        &(cheatseq_t){ CHEAT_SEQ("id", 0) } },
    // Toggle god mode
    { CheatGodFunc,         &(cheatseq_t){ CHEAT_SEQ("iddqd", 0) } },
    { CheatGodFunc,         &(cheatseq_t){ CHEAT_SEQ("quicken", 0) } },
    { CheatGodFunc,         &(cheatseq_t){ CHEAT_SEQ("satan", 0) } },
    // Get all weapons and ammo
    { CheatWeaponsFunc,     &(cheatseq_t){ CHEAT_SEQ("idfa", 0) } },
    { CheatWeaponsFunc,     &(cheatseq_t){ CHEAT_SEQ("rambo", 0) } },
    { CheatWeaponsFunc,     &(cheatseq_t){ CHEAT_SEQ("nra", 0) } },
    // Get all weapons and keys
    { CheatWeapKeysFunc,    &(cheatseq_t){ CHEAT_SEQ("idkfa", 0) } },
    // Get Gauntlets of the Necromancer
    { CheatChoppersFunc,    &(cheatseq_t){ CHEAT_SEQ("idchoppers", 0) } },
    // Get all keys
    { CheatKeysFunc,        &(cheatseq_t){ CHEAT_SEQ("idka", 0) } },
    { CheatKeysFunc,        &(cheatseq_t){ CHEAT_SEQ("skel", 0) } },
    { CheatKeysFunc,        &(cheatseq_t){ CHEAT_SEQ("locksmith", 0) } },
    // Toggle no clipping mode
    { CheatNoClipFunc,      &(cheatseq_t){ CHEAT_SEQ("idclip", 0) } },
    { CheatNoClipFunc,      &(cheatseq_t){ CHEAT_SEQ("idspispopd", 0) } },
    { CheatNoClipFunc,      &(cheatseq_t){ CHEAT_SEQ("kitty", 0) } },
    { CheatNoClipFunc,      &(cheatseq_t){ CHEAT_SEQ("casper", 0) } },
    // Warp to new level
    { CheatWarpFunc,        &(cheatseq_t){ CHEAT_SEQ("idclev",   2) } },
    { CheatWarpFunc,        &(cheatseq_t){ CHEAT_SEQ("engage",   2) } },
    { CheatWarpFunc,        &(cheatseq_t){ CHEAT_SEQ("visit",    2) } },
    // MDK
    { CheatMDKFunc,         &(cheatseq_t){ CHEAT_SEQ("mdk", 0) } },
    // Artifacts
    { CheatArtifact1Func,   &(cheatseq_t){ CHEAT_SEQ("gimme",    0) } },
    { CheatArtifact2Func,   &(cheatseq_t){ CHEAT_SEQ("gimme",    1) } },
    { CheatArtifact3Func,   &(cheatseq_t){ CHEAT_SEQ("gimme",    2) } },
    { CheatArtifact1Func,   &(cheatseq_t){ CHEAT_SEQ("idbehold", 0) } },
    { CheatArtifact2Func,   &(cheatseq_t){ CHEAT_SEQ("idbehold", 1) } },
    { CheatArtifact3Func,   &(cheatseq_t){ CHEAT_SEQ("idbehold", 2) } },
    // Get all artifacts
    { CheatArtifactAllFunc, &(cheatseq_t){ CHEAT_SEQ("indiana", 0) } },
    // Toggle tome of power
    { CheatPowerFunc,       &(cheatseq_t){ CHEAT_SEQ("shazam", 0) } },
    // Get full health
    { CheatHealthFunc,      &(cheatseq_t){ CHEAT_SEQ("ponce", 0) } },
    { CheatHealthFunc,      &(cheatseq_t){ CHEAT_SEQ("clubmed", 0) } },
    // Turn to a chicken
    { CheatChickenFunc,     &(cheatseq_t){ CHEAT_SEQ("cockadoodledoo", 0) } },
    { CheatChickenFunc,     &(cheatseq_t){ CHEAT_SEQ("deliverance", 0) } },
    // Kill all monsters
    { CheatTNTEMFunc,       &(cheatseq_t){ CHEAT_SEQ("tntem", 0) } },
    { CheatKILLEMFunc,      &(cheatseq_t){ CHEAT_SEQ("killem", 0) } },
    { CheatMassacreFunc,    &(cheatseq_t){ CHEAT_SEQ("massacre", 0) } },
    { CheatMassacreFunc,    &(cheatseq_t){ CHEAT_SEQ("butcher", 0) } },
    // [JN] IDMUS
    { CheatIDMUSFunc,       &(cheatseq_t){ CHEAT_SEQ("idmus", 2) } },
    // Reveal all map
    { CheatAMapFunc,        &(cheatseq_t){ CHEAT_SEQ("iddt", 0) } },
    { CheatAMapFunc,        &(cheatseq_t){ CHEAT_SEQ("ravmap", 0) } },
    { CheatAMapFunc,        &(cheatseq_t){ CHEAT_SEQ("mapsco", 0) } },
    // [PN] Woof-style automap reveal helpers
    { CheatRevealKillFunc,   &(cheatseq_t){ CHEAT_SEQ("iddkt", 0) } },
    { CheatRevealItemFunc,   &(cheatseq_t){ CHEAT_SEQ("iddit", 0) } },
    { CheatRevealSecretFunc, &(cheatseq_t){ CHEAT_SEQ("iddst", 0) } },
    // [JN] IDMYPOS coords
    { CheatIDMYPOSFunc,     &(cheatseq_t){ CHEAT_SEQ("idmypos", 0) } },
    { CheatIDMYPOSFunc,     &(cheatseq_t){ CHEAT_SEQ("where", 0) } },
    // [JN] ID-specific modes
    { CheatFREEZEFunc,      &(cheatseq_t){ CHEAT_SEQ("freeze", 0) } },
    { CheatNOTARGETFunc,    &(cheatseq_t){ CHEAT_SEQ("notarget", 0) } },
    { CheatBUDDHAFunc,      &(cheatseq_t){ CHEAT_SEQ("buddha", 0) } },
    { NULL, NULL }
};

//--------------------------------------------------------------------------
//
// FUNC HandleCheats
//
// Returns true if the caller should eat the key.
//
//--------------------------------------------------------------------------

static boolean HandleCheats(const byte key)
{
    /* [crispy] check for nightmare/netgame per cheat, to allow "harmless" cheats
    ** [JN] Allow in nightmare and for dead player (can be resurrected).
    if (netgame || gameskill == sk_nightmare)
    {                           // Can't cheat in a net-game, or in nightmare mode
        return (false);
    }
    */

    boolean eat = false;
    for (int i = 0; Cheats[i].func != NULL; i++)
    {
        if (cht_CheckCheat(Cheats[i].seq, key))
        {
            Cheats[i].func(&players[consoleplayer], &Cheats[i]);
            // [JN] Do not play sound after typing just "ID".
            if (Cheats[i].func != CheatWaitFunc)
            {
                S_StartSound(NULL, sfx_dorcls);
            }
        }
        // [PN] The word is typed and the digits are still coming in: show how
        // this game calls the current and the next level, so it is clear what
        // to type.
        else if (!netgame && !demorecording
        &&      Cheats[i].func == CheatWarpFunc
        &&      Cheats[i].seq->chars_read >= Cheats[i].seq->sequence_len)
        {
            static char buf[52];
            int         epsd, map;

            if (G_NextLevel(&epsd, &map))
            {
                M_snprintf(buf, sizeof(buf), "CURRENT: E%dM%d, NEXT: E%dM%d",
                           gameepisode, gamemap, epsd, map);
            }
            else
            {
                M_snprintf(buf, sizeof(buf), "CURRENT: E%dM%d", gameepisode, gamemap);
            }

            CT_SetMessage(&players[consoleplayer], buf, false, NULL);
        }
    }

    // [PN] The shield raised by "id" lasts two seconds, and a slow typist may
    // still be entering the digits when it runs out ("engage" and "visit" do
    // not raise it at all). Keep the shield up until they arrive.
    if (!netgame && !demorecording)
    {
        for (int i = 0; Cheats[i].func != NULL; i++)
        {
            if (Cheats[i].seq->chars_read >= Cheats[i].seq->sequence_len)
            {
                players[consoleplayer].cheatTics = TICRATE * 2;
                break;
            }
        }
    }
    return (eat);
}

// -----------------------------------------------------------------------------
// SB_Responder
// -----------------------------------------------------------------------------

boolean SB_Responder (const event_t *const event)
{
    if (event->type == ev_keydown)
    {
        if (HandleCheats(event->data1))
        {                       // Need to eat the key
            return (true);
        }

        // [JN] CRL - handle cheat keybind shortcuts:
        if (event->data1 == key_crl_iddqd || event->data1 == key_crl_iddqd2)
        {
            CheatGodFunc(&players[consoleplayer], &Cheats[0]);
            return (true);
        }
        if (event->data1 == key_crl_idkfa || event->data1 == key_crl_idkfa2)
        {
            CheatWeapKeysFunc(&players[consoleplayer], &Cheats[2]);
            return (true);
        }
        if (event->data1 == key_crl_idfa || event->data1 == key_crl_idfa2)
        {
            CheatWeaponsFunc(&players[consoleplayer], &Cheats[2]);
            return (true);
        }
        if (event->data1 == key_crl_idclip || event->data1 == key_crl_idclip2)
        {
            CheatNoClipFunc(&players[consoleplayer], &Cheats[1]);
            return (true);
        }
        if (event->data1 == key_crl_iddt || event->data1 == key_crl_iddt2)
        {
            ravmap_cheating++;
            if (ravmap_cheating > 2)
            {
                ravmap_cheating = 0;
            }
            return (true);
        }
        if (event->data1 == key_crl_mdk || event->data1 == key_crl_mdk2)
        {
            CheatMDKFunc(&players[consoleplayer], &Cheats[1]);
            return (true);
        }
    }
    return (false);
}