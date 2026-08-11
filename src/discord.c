// SONIC ROBO BLAST 2 KART
//-----------------------------------------------------------------------------
// Copyright (C) 2018-2020 by Sally "TehRealSalt" Cochenour.
// Copyright (C) 2018-2020 by Kart Krew.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  discord.h
/// \brief Discord Rich Presence handling

#ifdef HAVE_DISCORDRPC

#include <time.h>

#include "i_system.h"
#include "netcode/d_clisrv.h"
#include "netcode/d_netcmd.h"
#include "netcode/i_net.h"
#include "netcode/mserv.h" // cv_masterserver_room_id
#include "netcode/i_tcp.h" // current_port
#include "netcode/server_connection.h" // current_port
#include "g_game.h"
#include "p_tick.h"
#include "m_menu.h" // gametype_cons_t
#include "r_things.h" // skins
#include "z_zone.h"
#include "byteptr.h"
#include "stun.h"
#include "fastcmp.h"

#include "discord.h"
#include "doomdef.h"

// Feel free to provide your own, if you care enough to create another Discord app for this :P
#define DISCORD_APPID "1536516464440508446"

// length of IP strings
#define IP_SIZE 21

static void Discordrp_OnChange(void);

consvar_t cv_discordrp = CVAR_INIT ("discordrp", "On", CV_SAVE|CV_CALL|CV_CLIENT, CV_OnOff, Discordrp_OnChange);
consvar_t cv_discordstreamer = CVAR_INIT ("discordstreamer", "Off", CV_SAVE|CV_CLIENT, CV_OnOff, NULL);

struct discordInfo_s discordInfo;

discordRequest_t *discordRequestList = NULL;

static char self_ip[IP_SIZE+1];

/*--------------------------------------------------
 *	const char *DRPC_HideUsername(const char *input)
 *
 *		See header file for description.
 * --------------------------------------------------*/
const char *DRPC_HideUsername(const char *input)
{
	static char buffer[5];
	int i;

	buffer[0] = input[0];

	for (i = 1; i < 4; ++i)
	{
		buffer[i] = '.';
	}

	buffer[4] = '\0';
	return buffer;
}

boolean drpc_init = false;

/*--------------------------------------------------
	static char *DRPC_XORIPString(const char *input)

		Simple XOR encryption/decryption. Not complex or
		very secretive because we aren't sending anything
		that isn't easily accessible via our Master Server anyway.
--------------------------------------------------*/
static char *DRPC_XORIPString(const char *input)
{
	const UINT8 xor[IP_SIZE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};
	char *output = malloc(sizeof(char) * (IP_SIZE+1));
	UINT8 i;

	for (i = 0; i < IP_SIZE; i++)
	{
		char xorinput;

		if (!input[i])
			break;

		xorinput = input[i] ^ xor[i];

		if (xorinput < 32 || xorinput > 126)
		{
			xorinput = input[i];
		}

		output[i] = xorinput;
	}

	output[i] = '\0';

	return output;
}

/*--------------------------------------------------
	static void DRPC_HandleReady(const DiscordUser *user)

		Callback function, ran when the game connects to Discord.

	Input Arguments:-
		user - Struct containing Discord user info.

	Return:-
		None
--------------------------------------------------*/
static void DRPC_HandleReady(const DiscordUser *user)
{
	if (cv_discordstreamer.value)
	{
		CONS_Printf("Discord: connection successful\n");
	}
	else
	{
		CONS_Printf("Discord: connected to %s (%s)\n", user->username, user->userId);
	}
}

/*--------------------------------------------------
	static void DRPC_HandleDisconnect(int err, const char *msg)

		Callback function, ran when disconnecting from Discord.

	Input Arguments:-
		err - Error type
		msg - Error message

	Return:-
		None
--------------------------------------------------*/
static void DRPC_HandleDisconnect(int err, const char *msg)
{
	CONS_Printf("Discord: disconnected (%d: %s)\n", err, msg);
}

/*--------------------------------------------------
	static void DRPC_HandleError(int err, const char *msg)

		Callback function, ran when Discord outputs an error.

	Input Arguments:-
		err - Error type
		msg - Error message

	Return:-
		None
--------------------------------------------------*/
static void DRPC_HandleError(int err, const char *msg)
{
	CONS_Alert(CONS_WARNING, "Discord error (%d: %s)\n", err, msg);
}

/*--------------------------------------------------
	static void DRPC_HandleJoin(const char *secret)

		Callback function, ran when Discord wants to
		connect a player to the game via a channel invite
		or a join request.

	Input Arguments:-
		secret - Value that links you to the server.

	Return:-
		None
--------------------------------------------------*/
static void DRPC_HandleJoin(const char *secret)
{
	char *ip = DRPC_XORIPString(secret);
	CONS_Printf("Connecting to %s via Discord\n", ip);
	M_ClearMenus(true); //Don't have menus open during connection screen
	if (demoplayback && titledemo)
		G_CheckDemoStatus(); //Stop the title demo, so that the connect command doesn't error if a demo is playing
	COM_BufAddText(va("connect \"%s\"\n", ip));
	free(ip);
}

/*--------------------------------------------------
	void DRPC_RemoveRequest(discordRequest_t *removeRequest)

		See header file for description.
--------------------------------------------------*/
void DRPC_RemoveRequest(discordRequest_t *removeRequest)
{
	if (removeRequest->prev != NULL)
	{
		removeRequest->prev->next = removeRequest->next;
	}

	if (removeRequest->next != NULL)
	{
		removeRequest->next->prev = removeRequest->prev;

		if (removeRequest == discordRequestList)
		{
			discordRequestList = removeRequest->next;
		}
	}
	else
	{
		if (removeRequest == discordRequestList)
		{
			discordRequestList = NULL;
		}
	}

	Z_Free(removeRequest->username);
#if 0
	Z_Free(removeRequest->discriminator);
#endif
	Z_Free(removeRequest->userID);
	Z_Free(removeRequest);
}

/*--------------------------------------------------
	void DRPC_Init(void)

		See header file for description.
--------------------------------------------------*/
void DRPC_Init(void)
{
	if (drpc_init || !cv_discordrp.value) return;

	drpc_init = true;
	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));

	handlers.ready = DRPC_HandleReady;
	handlers.disconnected = DRPC_HandleDisconnect;
	handlers.errored = DRPC_HandleError;
	handlers.joinGame = DRPC_HandleJoin;

	Discord_Initialize(DISCORD_APPID, &handlers, 1, NULL);
	I_AddExitFunc(DRPC_Shutdown);
	DRPC_UpdatePresence();
}

void DRPC_Shutdown(void)
{
	if (!drpc_init)
	{
		CONS_Printf("DiscordRPC never started\n");
		return;
	}

	CONS_Printf("Shutting down DiscordRPC\n");

	Discord_Shutdown();
	drpc_init = false;
}

static void Discordrp_OnChange(void)
{
	if (cv_discordrp.value)
	{
		DRPC_UpdatePresence();
	}
	else
	{
		DRPC_Shutdown();
	}
}

/*--------------------------------------------------
	static void DRPC_GotServerIP(UINT32 address)

		Callback triggered by successful STUN response.

	Input Arguments:-
		address - IPv4 address of this machine, in network byte order.

	Return:-
		None
--------------------------------------------------*/
static void DRPC_GotServerIP(UINT32 address)
{
	const unsigned char * p = (const unsigned char *)&address;
	sprintf(self_ip, "%u.%u.%u.%u:%u", p[0], p[1], p[2], p[3], current_port);
}

/*--------------------------------------------------
	static const char *DRPC_GetServerIP(void)

		Retrieves the IP address of the server that you're
		connected to. Will attempt to use STUN for getting your
		own IP address.
--------------------------------------------------*/
static const char *DRPC_GetServerIP(void)
{
	const char *address;

	// If you're connected
	if (I_GetNodeAddress && (address = I_GetNodeAddress(servernode)) != NULL)
	{
		if (!fastcmp(address, "self"))
		{
			// We're not the server, so we could successfully get the IP!
			// No need to do anything else :)
			return address;
		}
	}

	if (self_ip[0])
	{
		return self_ip;
	}
	else
	{
		// There happens to be a good way to get it after all! :D
		STUN_bind(DRPC_GotServerIP);
		return NULL;
	}
}

/*--------------------------------------------------
	void DRPC_EmptyRequests(void)

		Empties the request list. Any existing requests
		will get an ignore reply.
--------------------------------------------------*/
static void DRPC_EmptyRequests(void)
{
	while (discordRequestList != NULL)
	{
		Discord_Respond(discordRequestList->userID, DISCORD_REPLY_IGNORE);
		DRPC_RemoveRequest(discordRequestList);
	}
}

/*--------------------------------------------------
	void DRPC_UpdatePresence(void)

		See header file for description.
--------------------------------------------------*/
void DRPC_UpdatePresence(void)
{
	if (!cv_discordrp.value) return;

	if (!drpc_init) DRPC_Init();

	char detailstr[48+1];

	char mapimg[8+1];
	char mapname[5+21+21+2+1];

	char charimg[4+SKINNAMESIZE+1];
	char charname[11+SKINNAMESIZE+1];

	boolean joinSecretSet = false;

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	if (dedicated)
	{
		return;
	}

	if (!cv_discordrp.value)
	{
		// User doesn't want to show their game information, so update with empty presence.
		// This just shows that they're playing SRB2Kart. (If that's too much, then they should disable game activity :V)
		DRPC_EmptyRequests();
		Discord_UpdatePresence(&discordPresence);
		return;
	}

#ifdef DEVELOP
	// This way, we can use the invite feature in-dev, but not have snoopers seeing any potential secrets! :P
	discordPresence.largeImageKey = "miscdevelop";
	discordPresence.largeImageText = "No peeking!";
	discordPresence.state = "Testing the game";

	DRPC_EmptyRequests();
	Discord_UpdatePresence(&discordPresence);
	return;
#endif // DEVELOP

	// Server info
	if (netgame && Playing())
	{
#ifdef MASTERSERVER
		if (gamestate != GS_TITLESCREEN)
			discordPresence.state = "Playing a netgame";
#endif

		discordPresence.partyId = server_context; // Thanks, whoever gave us Mumble support, for implementing the EXACT thing Discord wanted for this field!
		discordPresence.partySize = D_NumPlayers(); // Players in server
		discordPresence.partyMax = cv_maxplayers.value; // Max players
	}
	else
	{
		// Reset discord info if you're not in a place that uses it!
		// Important for if you join a server that compiled without HAVE_DISCORDRPC,
		// so that you don't ever end up using bad information from another server.
		memset(&discordInfo, 0, sizeof(discordInfo));

		// Offline info
		if (Playing())
			discordPresence.state = "Singleplayer";
		else if (demoplayback && !titledemo)
			discordPresence.state = "Watching Replay";
		else
			discordPresence.state = "In the menus";
	}

	// Gametype info
	if ((gamestate == GS_LEVEL || gamestate == GS_INTERMISSION) && Playing())
	{
		if (modeattacking)
			discordPresence.details = "Time Attack";
		else
		{
			snprintf(detailstr, 48, "%s | %s",
				(netgame && Playing()) ? cv_servername.string : "", gametype_cons_t[gametype].strvalue
			);
			discordPresence.details = detailstr;
		}
	}

	if ((gamestate == GS_LEVEL || gamestate == GS_INTERMISSION) // Map info
		&& !(demoplayback && titledemo))
	{
		if ((gamemap >= 1 && gamemap <= 60) // supported race maps
			|| (gamemap >= 136 && gamemap <= 164)) // supported battle maps
		{
			snprintf(mapimg, 8, "%s", G_BuildMapName(gamemap));
			strlwr(mapimg);
			discordPresence.largeImageKey = mapimg; // Map image
		}
		else if (mapheaderinfo[gamemap-1]->menuflags & LF2_HIDEINMENU)
		{
			// Hell map, use the method that got you here :P
			discordPresence.largeImageKey = "miscdice";
		}
		else
		{
			// This is probably a custom map!
			discordPresence.largeImageKey = "mapcustom";
		}

		if (mapheaderinfo[gamemap-1]->menuflags & LF2_HIDEINMENU)
		{
			// Hell map, hide the name
			discordPresence.largeImageText = "Map: ???";
		}
		else
		{
			// Map name on tool tip
			char *title = G_BuildMapTitle(gamemap);
			if (title)
			{
				snprintf(mapname, 48, "Map: %s", title);
				Z_Free(title);
			}
			else
				snprintf(mapname, 48, "Map: UNKNOWN");

			discordPresence.largeImageText = mapname;
		}

		if (gamestate == GS_LEVEL && Playing())
		{
			const time_t currentTime = time(NULL);
			const time_t mapTimeStart = currentTime - (leveltime / TICRATE);

			discordPresence.startTimestamp = mapTimeStart;

			if (timelimitintics > 0)
			{
				const time_t mapTimeEnd = mapTimeStart + (timelimitintics / TICRATE);
				discordPresence.endTimestamp = mapTimeEnd;
			}
		}
	}
	else
	{
		discordPresence.largeImageKey = "misctitle";
		discordPresence.largeImageText = "Title Screen";
	}

	if (joinSecretSet == false)
	{
		// Not able to join? Flush the request list, if it exists.
		DRPC_EmptyRequests();
	}

	Discord_UpdatePresence(&discordPresence);
}

#endif // HAVE_DISCORDRPC
