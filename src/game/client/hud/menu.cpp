/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// menu.cpp
//
// generic menu handler
//
#include <string.h>
#include <stdio.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "menu.h"
#include "vgui/client_viewport.h"
#include "text_message.h"
#include "chat.h"

#define MAX_MENU_STRING 512
char g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

int KB_ConvertString(char *in, char **ppout);

DEFINE_HUD_ELEM(CHudMenu);

void CHudMenu::Init(void)
{
	BaseHudClass::Init();

	HookMessage<&CHudMenu::MsgFunc_ShowMenu>("ShowMenu");

	InitHudData();
}

void CHudMenu::InitHudData(void)
{
	m_fMenuDisplayed = 0;
	m_bitsValidSlots = 0;
	m_iFlags &= ~HUD_ACTIVE;
	Reset();
}

void CHudMenu::Reset(void)
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = FALSE;
}

void CHudMenu::VidInit(void)
{
}

/*=================================
  ParseEscapeToken

  Interprets the given escape token (backslash followed by a letter). The
  first character of the token must be a backslash.  The second character
  specifies the operation to perform:

   \w : White text (this is the default)
   \d : Dim (gray) text
   \y : Yellow text
   \r : Red text
   \R : Right-align (just for the remainder of the current line)
=================================*/

static int menu_r, menu_g, menu_b, menu_x, menu_ralign;

static inline const char *ParseEscapeToken(const char *token)
{
	if (*token != '\\')
		return token;

	token++;

	switch (*token)
	{
	case '\0':
		return token;

	case 'w':
		menu_r = 255;
		menu_g = 255;
		menu_b = 255;
		break;

	case 'd':
		menu_r = 100;
		menu_g = 100;
		menu_b = 100;
		break;

	case 'y':
		menu_r = 255;
		menu_g = 210;
		menu_b = 64;
		break;

	case 'r':
		menu_r = 210;
		menu_g = 24;
		menu_b = 0;
		break;

	case 'R':
		menu_x = ScreenWidth / 2;
		menu_ralign = TRUE;
		break;
	}

	return ++token;
}

void CHudMenu::Draw(float flTime)
{
	// check for if menu is set to disappear
	if (m_flShutoffTime > 0)
	{
		if (m_flShutoffTime <= gHUD.m_flTime)
		{ // times up, shutoff
			m_fMenuDisplayed = 0;
			m_iFlags &= ~HUD_ACTIVE;
			return;
		}
	}

	// don't draw the menu if the scoreboard is being shown
	if (g_pViewport && g_pViewport->IsScoreBoardVisible())
		return;

	// draw the menu, along the left-hand side of the screen
	const int lineHeight = gHUD.GetHudFontSize();

	// count the number of newlines
	int nlc = 0;
	int i;
	for (i = 0; i < MAX_MENU_STRING && g_szMenuString[i] != '\0'; i++)
	{
		if (g_szMenuString[i] == '\n')
			nlc++;
	}

	int y = GetStartY(nlc, lineHeight);

	menu_r = 255;
	menu_g = 255;
	menu_b = 255;
	menu_x = 20;
	menu_ralign = FALSE;

	const char *sptr = g_szMenuString;

	while (*sptr != '\0')
	{
		if (*sptr == '\\')
		{
			sptr = ParseEscapeToken(sptr);
		}
		else if (*sptr == '\n')
		{
			menu_ralign = FALSE;
			menu_x = SPR_RES_SCALED(20);
			y += lineHeight;

			sptr++;
		}
		else
		{
			char menubuf[80];
			const char *ptr = sptr;
			while (*sptr != '\0' && *sptr != '\n' && *sptr != '\\')
			{
				sptr++;
			}
			strncpy(menubuf, ptr, min((sptr - ptr), (int)sizeof(menubuf)));
			menubuf[min((sptr - ptr), (int)(sizeof(menubuf) - 1))] = '\0';

			if (menu_ralign)
			{
				// IMPORTANT: Right-to-left rendered text does not parse escape tokens!
				menu_x = gHUD.DrawHudStringReverseColorCodes(menu_x, y, 0, menubuf, menu_r, menu_g, menu_b);
			}
			else
			{
				menu_x = gHUD.DrawHudStringColorCodes(menu_x, y, 0, menubuf, menu_r, menu_g, menu_b);
			}
		}
	}

	return;
}

// selects an item from the menu
void CHudMenu::SelectMenuItem(int menu_item)
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ((menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item - 1))))
	{
		char szbuf[32];
		sprintf(szbuf, "menuselect %d\n", menu_item);
		EngineClientCmd(szbuf);

		// remove the menu
		m_fMenuDisplayed = 0;
		m_iFlags &= ~HUD_ACTIVE;
	}
}

int CHudMenu::GetStartY(int lineCount, int lineHeight)
{
	int height = (lineCount + 1) * lineHeight; // +1 to account for the last line missing a \n
	int top = (ScreenHeight / 2) - (height / 2); // Centered vertically
	int bottom = top + height;

	// Make sure menu doesn't occlude the chat
	int chatY = CHudChat::Get()->GetYPos();
	if (bottom > chatY)
	{
		int delta = bottom - chatY;
		top -= delta;
		bottom -= delta;
	}

	// Make sure the menu doesn't go out of bounds
	constexpr int MARGIN_TOP = 10;
	if (top < MARGIN_TOP)
	{
		top += MARGIN_TOP;
		bottom += MARGIN_TOP;
	}

	return top;
}

// Message handler for ShowMenu message
// takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, FALSE if it's the last string
//		string: menu string to display
// if this message is never received, then scores will simply be the combined totals of the players.
int CHudMenu::MsgFunc_ShowMenu(const char *pszName, int iSize, void *pbuf)
{
	char *temp = NULL;

	BEGIN_READ(pbuf, iSize);

	m_bitsValidSlots = READ_SHORT();
	int DisplayTime = READ_CHAR();
	int NeedMore = READ_BYTE();

	if (DisplayTime > 0)
		m_flShutoffTime = DisplayTime + gHUD.m_flTime;
	else
		m_flShutoffTime = -1;

	if (m_bitsValidSlots)
	{
		if (!m_fWaitingForMore) // this is the start of a new menu
		{
			strncpy(g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING);
		}
		else
		{ // append to the current menu string
			strncat(g_szPrelocalisedMenuString, READ_STRING(), MAX_MENU_STRING - strlen(g_szPrelocalisedMenuString));
		}
		g_szPrelocalisedMenuString[MAX_MENU_STRING - 1] = 0; // ensure null termination (strncat/strncpy does not)

		if (!NeedMore)
		{ // we have the whole string, so we can localise it now
			strcpy(g_szMenuString, CHudTextMessage::BufferedLocaliseTextString(g_szPrelocalisedMenuString));

			// Swap in characters
			if (KB_ConvertString(g_szMenuString, &temp))
			{
				strcpy(g_szMenuString, temp);
				free(temp);
			}
		}

		m_fMenuDisplayed = 1;
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		m_fMenuDisplayed = 0; // no valid slots means that the menu should be turned off
		m_iFlags &= ~HUD_ACTIVE;
	}

	m_fWaitingForMore = NeedMore;

	return 1;
}
