//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014-2017 RestlessRodent
// Copyright(C) 2018-2023 Julia Nechaevskaya
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
#include "doomstat.h"
#include "doomkeys.h"
#include "ct_chat.h"
#include "d_englsh.h"
#include "d_event.h"
#include "deh_str.h"
#include "i_input.h"
#include "m_controls.h"
#include "m_misc.h"
#include "p_local.h"
#include "s_sound.h"
#include "v_patch.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "sounds.h"

#include "crlcore.h"


#define QUEUESIZE       128
#define MESSAGESIZE     128
#define MESSAGELEN      265

#define CT_PLR_GREEN    1
#define CT_PLR_INDIGO   2
#define CT_PLR_BROWN    3
#define CT_PLR_RED      4
#define CT_PLR_ALL      5
#define CT_ESCAPE       6


patch_t *hu_font[HU_FONTSIZE];

boolean chatmodeon;
boolean altdown;
boolean shiftdown;

char *chat_macros[10] =
{
    HUSTR_CHATMACRO0,
    HUSTR_CHATMACRO1,
    HUSTR_CHATMACRO2,
    HUSTR_CHATMACRO3,
    HUSTR_CHATMACRO4,
    HUSTR_CHATMACRO5,
    HUSTR_CHATMACRO6,
    HUSTR_CHATMACRO7,
    HUSTR_CHATMACRO8,
    HUSTR_CHATMACRO9
};

char *player_names[] =
{
    HUSTR_PLRGREEN,
    HUSTR_PLRINDIGO,
    HUSTR_PLRBROWN,
    HUSTR_PLRRED
};

char *CT_FromPlrText[MAXPLAYERS] = {
    HUSTR_PLRGREEN,
    HUSTR_PLRINDIGO,
    HUSTR_PLRBROWN,
    HUSTR_PLRRED
};

static void CT_queueChatChar (char ch);
static void CT_ClearChatMessage (int player);
static void CT_AddChar(int player, char c);
static void CT_BackSpace(int player);

static int  head;
static int  tail;
static int  chat_sfx;  // [JN] Different chat sounds in Doom 1 and Doom 2.
static int  msgptr[MAXPLAYERS];
static int  msglen[MAXPLAYERS];
static int  chat_dest[MAXPLAYERS];
static char chat_msg[MAXPLAYERS][MESSAGESIZE];
static char plr_lastmsg[MAXPLAYERS][MESSAGESIZE + 9];  // add in the length of the pre-string
static byte ChatQueue[QUEUESIZE];

static int ChatFontBaseLump;


// -----------------------------------------------------------------------------
// CT_Init
// [JN] Load the heads-up font.
// -----------------------------------------------------------------------------

void CT_Init (void)
{
    int   j = HU_FONTSTART;
    char  buffer[9];

    head = 0;  // Initialize the queue index.
    tail = 0;
    chatmodeon = false;
    memset(ChatQueue, 0, QUEUESIZE);
    chat_sfx = gamemode == commercial ? sfx_radio : sfx_tink;

    for (int i = 0 ; i < HU_FONTSIZE ; i++)
    {
        DEH_snprintf(buffer, 9, "STCFN%.3d", j++);
        hu_font[i] = (patch_t *) W_CacheLumpName(buffer, PU_STATIC);
    }

    ChatFontBaseLump = W_GetNumForName(DEH_String("STCFN033"));
}

// -----------------------------------------------------------------------------
// CT_Stop
// -----------------------------------------------------------------------------

static void CT_Stop (void)
{
    chatmodeon = false;
    I_StopTextInput();
}

// -----------------------------------------------------------------------------
// ValidChatChar
// [JN] These keys are allowed by Vanilla Doom:
// -----------------------------------------------------------------------------
static const boolean ValidChatChar (const char c)
{
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '-'  || c == '+' || c == '='
        || (c >= '!' && c <= ')') || c == '@'  || c == '^' || c == '*'
        ||  c == ' ' ||  c == '[' || c == ']'  || c == '_'
        ||  c == ';' ||  c == ':' || c == '\'' || c == '\\'
        ||  c == ',' ||  c == '<' || c == '.'  || c == '>'
        ||  c == '/' ||  c == '?';
}

// -----------------------------------------------------------------------------
// CT_Responder
// -----------------------------------------------------------------------------

boolean CT_Responder (event_t *ev)
{
    int   sendto;
    const char *macro;

    /*
    if(BK_isKeyDown(ev, bk_show_message_list))
    {
        // TODO Show last N messages
        players[consoleplayer].messageTics = messages_timeout * TICRATE;
        return true;
    }
    */

    if (!netgame)
    {
        return false;
    }
    if (ev->data1 == KEY_LALT || ev->data2 == KEY_RALT)
    {
        altdown = (ev->type == ev_keydown);
        return false;
    }
    if (ev->data1 == KEY_RSHIFT)
    {
        shiftdown = (ev->type == ev_keydown);
        return false;
    }
    if (ev->type != ev_keydown)
    {
        return false;
    }
    if (!chatmodeon)
    {
        sendto = 0;

        if (ev->data1 == key_multi_msg)
        {
            sendto = CT_PLR_ALL;
        }
        else if (ev->data1 == key_multi_msgplayer[0])
        {
            sendto = CT_PLR_GREEN;
        }
        else if (ev->data1 == key_multi_msgplayer[1])
        {
            sendto = CT_PLR_INDIGO;
        }
        else if (ev->data1 == key_multi_msgplayer[2])
        {
            sendto = CT_PLR_BROWN;
        }
        else if (ev->data1 == key_multi_msgplayer[3])
        {
            sendto = CT_PLR_RED;
        }
        if (sendto == 0 || (sendto != CT_PLR_ALL && !playeringame[sendto - 1])
        ||  sendto == consoleplayer + 1)
        {
            return false;
        }

        CT_queueChatChar(sendto);
        chatmodeon = true;
        I_StartTextInput(25, 10, SCREENWIDTH, 18);

        return true;
    }
    else
    {
        if (altdown)
        {
            if (ev->data1 >= '0' && ev->data1 <= '9')
            {
                if (ev->data1 == '0')
                {
                    // macro 0 comes after macro 9
                    ev->data1 = '9' + 1;
                }
                macro = chat_macros[ev->data1 - SDL_SCANCODE_Z];
                CT_queueChatChar(KEY_ENTER);  // Send old message.
                CT_queueChatChar(chat_dest[consoleplayer]);  // Chose the dest.
                while (*macro)
                {
                    CT_queueChatChar(toupper(*macro++));
                }
                CT_queueChatChar(KEY_ENTER);  // Send it off...
                CT_Stop();
                return true;
            }
        }
        if (ev->data1 == KEY_ENTER)
        {
            CT_queueChatChar(KEY_ENTER);
            CT_Stop();
            return true;
        }
        else if (ev->data1 == KEY_ESCAPE)
        {
            CT_queueChatChar(CT_ESCAPE);
            CT_Stop();
            return true;
        }
        else if (ev->data1 == KEY_BACKSPACE)
        {
            CT_queueChatChar(KEY_BACKSPACE);
            return true;
        }
        else if (ValidChatChar(ev->data3))
        {
            CT_queueChatChar(toupper(ev->data3));
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// CT_Ticker
// -----------------------------------------------------------------------------

void CT_Ticker (void)
{
    int  i, j;
    char c;
    int  numplayers;

    for (i = 0 ; i < MAXPLAYERS ; i++)
    {
        if (!playeringame[i])
        {
            continue;
        }
        if ((c = players[i].cmd.chatchar) != 0)
        {
            if (c <= 5)
            {
                chat_dest[i] = c;
                continue;
            }
            else if (c == CT_ESCAPE)
            {
                CT_ClearChatMessage(i);
            }
            else if (c == KEY_ENTER)
            {
                numplayers = 0;
                for (j = 0; j < MAXPLAYERS; j++)
                {
                    numplayers += playeringame[j];
                }
                CT_AddChar(i, 0);  // Set the end of message character.

                // [JN] Always consolidate player name with message.
                M_StringCopy(plr_lastmsg[i], DEH_String(CT_FromPlrText[i]), sizeof(plr_lastmsg[i]));
                M_StringConcat(plr_lastmsg[i], chat_msg[i], sizeof(plr_lastmsg[i]));

                if (i != consoleplayer && (chat_dest[i] == consoleplayer + 1
                ||  chat_dest[i] == CT_PLR_ALL) && *chat_msg[i])
                {
                    CRL_SetMessage(&players[consoleplayer], plr_lastmsg[i], true, NULL);
                    S_StartSound(NULL, chat_sfx);
                }
                else if (i == consoleplayer && (*chat_msg[i]))
                {
                    if (numplayers > 1)
                    {
                        // [JN] Replicate sended message locally.
                        CRL_SetMessage(&players[consoleplayer], plr_lastmsg[i], true, NULL);
                        S_StartSound(NULL, chat_sfx);
                    }
                    else
                    {
                        CRL_SetMessage(&players[consoleplayer], CRL_NOPLAYERS, true, NULL);
                        S_StartSound(NULL, chat_sfx);
                    }
                }
                CT_ClearChatMessage(i);
            }
            else if (c == KEY_BACKSPACE)
            {
                CT_BackSpace(i);
            }
            else
            {
                CT_AddChar(i, c);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// CT_Drawer
// -----------------------------------------------------------------------------

void CT_Drawer (void)
{
    int x = 0;

    for (int i = 0 ; i < msgptr[consoleplayer] ; i++)
    {
        if (chat_msg[consoleplayer][i] < 33)
        {
            x += 4;
        }
        else
        {
            patch_t *patch = W_CacheLumpNum(ChatFontBaseLump + 
                                   chat_msg[consoleplayer][i] - 33,
                                   PU_STATIC);
            V_DrawPatch(x, 10, patch, "STCFN"); // todo - incorrect name
            x += patch->width;
        }
    }

    V_DrawPatch(x, 10, W_CacheLumpName  // [JN] Draw cursor.
                            (DEH_String("STCFN095"), PU_STATIC), "STCFN095");
}

// -----------------------------------------------------------------------------
// CT_queueChatChar
// -----------------------------------------------------------------------------

static void CT_queueChatChar (const char ch)
{
    if (((tail + 1) & (QUEUESIZE - 1)) == head)
    {
        return;  // The queue is full.
    }
    ChatQueue[tail] = ch;
    tail = (tail + 1) & (QUEUESIZE - 1);
}

// -----------------------------------------------------------------------------
// CT_dequeueChatChar
// -----------------------------------------------------------------------------

char CT_dequeueChatChar (void)
{
    byte temp;

    if (head == tail)
    {
        return 0;  // Queue is empty.
    }

    temp = ChatQueue[head];
    head = (head + 1) & (QUEUESIZE - 1);

    return temp;
}

// -----------------------------------------------------------------------------
// CT_AddChar
// -----------------------------------------------------------------------------

static void CT_AddChar (const int player, const char c)
{
    patch_t *patch;

    if (msgptr[player] + 1 >= MESSAGESIZE || msglen[player] >= MESSAGELEN)
    {
        return;  // Full.
    }

    chat_msg[player][msgptr[player]] = c;
    msgptr[player]++;

    if (c < 33)
    {
        msglen[player] += 4;
    }
    else
    {
        patch = W_CacheLumpNum(ChatFontBaseLump + c - 33, PU_CACHE);
        msglen[player] += patch->width;
    }
}

// -----------------------------------------------------------------------------
// CT_BackSpace
// Backs up a space, when the user hits (obviously) backspace.
// -----------------------------------------------------------------------------

static void CT_BackSpace (const int player)
{
    patch_t *patch;
    char c;

    if (msgptr[player] == 0)
    {
        return;  // Message is already blank.
    }

    msgptr[player]--;
    c = chat_msg[player][msgptr[player]];

    if (c < 33)
    {
        msglen[player] -= 6;
    }
    else
    {
        patch = W_CacheLumpNum(ChatFontBaseLump + c - 33, PU_CACHE);
        msglen[player] -= patch->width;
    }

    chat_msg[player][msgptr[player]] = 0;
}

// -----------------------------------------------------------------------------
// CT_ClearChatMessage
// Clears out the data for the chat message, but the player's message 
// is still saved in plrmsg.
// -----------------------------------------------------------------------------

static void CT_ClearChatMessage (const int player)
{
    memset(chat_msg[player], 0, MESSAGESIZE);
    msgptr[player] = 0;
    msglen[player] = 0;
}
