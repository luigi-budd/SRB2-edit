// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1993-1996 by id Software, Inc.
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2024 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  g_game.c
/// \brief game loop functions, events handling

#include "doomdef.h"
#include "console.h"
#include "d_main.h"
#include "d_player.h"
#include "netcode/d_clisrv.h"
#include "netcode/net_command.h"
#include "f_finale.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "i_time.h"
#include "i_system.h"
#include "am_map.h"
#include "m_random.h"
#include "p_local.h"
#include "r_draw.h"
#include "r_main.h"
#include "s_sound.h"
#include "g_game.h"
#include "g_demo.h"
#include "m_cheat.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_argv.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "z_zone.h"
#include "i_video.h"
#include "byteptr.h"
#include "i_joy.h"
#include "r_local.h"
#include "r_skins.h"
#include "y_inter.h"
#include "v_video.h"
#include "lua_hook.h"
#include "b_bot.h"
#include "m_cond.h" // condition sets
#include "lua_script.h"
#include "r_fps.h" // frame interpolation/uncapped
#include "screen.h" // BASEVID*

#include "lua_hud.h"
#include "lua_libs.h"

gameaction_t gameaction;
gamestate_t gamestate = GS_NULL;
UINT8 ultimatemode = false;

INT32 pickedchar;

boolean botingame;
UINT8 botskin;
UINT16 botcolor;

JoyType_t Joystick;
JoyType_t Joystick2;

// 1024 bytes is plenty for a savegame
#define SAVEGAMESIZE (1024)

char gamedatafilename[64] = "gamedata.dat";
char timeattackfolder[64] = "main";
char customversionstring[32] = "\0";

static void G_DoCompleted(void);
static void G_DoStartContinue(void);
static void G_DoContinued(void);
static void G_DoWorldDone(void);

char   mapmusname[7]; // Music name
UINT16 mapmusflags; // Track and reset bit
UINT32 mapmusposition; // Position to jump to

INT16 gamemap = 1;
UINT32 maptol;
UINT8 globalweather = 0;
INT32 curWeather = PRECIP_NONE;
INT32 cursaveslot = 0; // Auto-save 1p savegame slot
//INT16 lastmapsaved = 0; // Last map we auto-saved at
INT16 lastmaploaded = 0; // Last map the game loaded
UINT8 gamecomplete = 0;

marathonmode_t marathonmode = 0;
tic_t marathontime = 0;

UINT8 numgameovers = 0; // for startinglives balance
SINT8 startinglivesbalance[maxgameovers+1] = {3, 5, 7, 9, 12, 15, 20, 25, 30, 40, 50, 75, 99, 0x7F};

UINT16 mainwads = 0;
boolean modifiedgame; // Set if homebrew PWAD stuff has been added.
boolean savemoddata = false;
boolean usedCheats = false; // Set when a gamedata-preventing cheat command is used.
UINT8 paused;
UINT8 modeattacking = ATTACKING_NONE;
boolean disableSpeedAdjust = false;
boolean imcontinuing = false;
boolean runemeraldmanager = false;
UINT16 emeraldspawndelay = 60*TICRATE;

// menu demo things
UINT8  numDemos      = 0;
UINT32 demoDelayTime = 15*TICRATE;
UINT32 demoIdleTime  = 3*TICRATE;

boolean netgame; // only true if packets are broadcast
boolean multiplayer;
boolean playeringame[MAXPLAYERS];
boolean addedtogame;
player_t players[MAXPLAYERS];

INT32 consoleplayer; // player taking events and displaying
INT32 displayplayer; // view being displayed
INT32 secondarydisplayplayer; // for splitscreen

tic_t gametic;
tic_t levelstarttic; // gametic at level start
UINT32 ssspheres; // old special stage
INT16 lastmap; // last level you were at (returning from special stages)
tic_t timeinmap; // Ticker for time spent in level (used for levelcard display)

INT16 spstage_start, spmarathon_start;
INT16 sstage_start, sstage_end, smpstage_start, smpstage_end;

INT16 titlemap = 0;
boolean hidetitlepics = false;
INT16 bootmap; //bootmap for loading a map on startup

INT16 tutorialmap = 0; // map to load for tutorial
boolean tutorialmode = false; // are we in a tutorial right now?
INT32 tutorialgcs = gcs_custom; // which control scheme is loaded?
INT32 tutorialusemouse = 0; // store cv_usemouse user value
INT32 tutorialfreelook = 0; // store cv_alwaysfreelook user value
INT32 tutorialmousemove = 0; // store cv_mousemove user value
INT32 tutorialanalog = 0; // store cv_analog[0] user value

boolean looptitle = false;

UINT16 skincolor_redteam = SKINCOLOR_RED;
UINT16 skincolor_blueteam = SKINCOLOR_BLUE;
UINT16 skincolor_redring = SKINCOLOR_SALMON;
UINT16 skincolor_bluering = SKINCOLOR_CORNFLOWER;

tic_t countdowntimer = 0;
boolean countdowntimeup = false;
boolean exitfadestarted = false;

cutscene_t *cutscenes[128];
textprompt_t *textprompts[MAX_PROMPTS];

INT16 nextmapoverride;
UINT8 skipstats;
INT16 nextgametype = -1;

// Pointers to each CTF flag
mobj_t *redflag;
mobj_t *blueflag;
// Pointers to CTF spawn location
mapthing_t *rflagpoint;
mapthing_t *bflagpoint;

struct quake quake;

// Map Header Information
mapheader_t* mapheaderinfo[NUMMAPS] = {NULL};

static boolean exitgame = false;
static boolean retrying = false;
static boolean retryingmodeattack = false;

boolean stagefailed = false; // Used for GEMS BONUS? Also to see if you beat the stage.

UINT16 emeralds;
INT32 luabanks[NUM_LUABANKS];
UINT32 token; // Number of tokens collected in a level
UINT32 tokenlist; // List of tokens collected
boolean gottoken; // Did you get a token? Used for end of act
INT32 tokenbits; // Used for setting token bits

// Old Special Stage
INT32 sstimer; // Time allotted in the special stage

UINT32 bluescore, redscore; // CTF and Team Match team scores

// ring count... for PERFECT!
INT32 nummaprings = 0;

// Elminates unnecessary searching.
boolean CheckForBustableBlocks;
boolean CheckForBouncySector;
boolean CheckForQuicksand;
boolean CheckForMarioBlocks;
boolean CheckForFloatBob;
boolean CheckForReverseGravity;

// Powerup durations
UINT16 invulntics = 20*TICRATE;
UINT16 sneakertics = 20*TICRATE;
UINT16 flashingtics = 3*TICRATE;
UINT16 tailsflytics = 8*TICRATE;
UINT16 underwatertics = 30*TICRATE;
UINT16 spacetimetics = 11*TICRATE + (TICRATE/2);
UINT16 extralifetics = 4*TICRATE;
UINT16 nightslinktics = 2*TICRATE;

INT32 gameovertics = 11*TICRATE;

UINT8 ammoremovaltics = 2*TICRATE;

UINT8 use1upSound = 0;
UINT8 maxXtraLife = 2; // Max extra lives from rings
UINT8 useContinues = 0; // Set to 1 to enable continues outside of no-save scenarioes
UINT8 shareEmblems = 0; // Set to 1 to share all picked up emblems in multiplayer

UINT8 introtoplay;
UINT8 creditscutscene;
UINT8 useBlackRock = 1;

// Emerald locations
mobj_t *hunt1;
mobj_t *hunt2;
mobj_t *hunt3;

UINT32 countdown, countdown2; // for racing

fixed_t gravity;

INT16 autobalance; //for CTF team balance
INT16 teamscramble; //for CTF team scramble
INT16 scrambleplayers[MAXPLAYERS]; //for CTF team scramble
INT16 scrambleteams[MAXPLAYERS]; //for CTF team scramble
INT16 scrambletotal; //for CTF team scramble
INT16 scramblecount; //for CTF team scramble

INT32 cheats; //for multiplayer cheat commands

tic_t hidetime;

typedef struct joystickvector2_s
{
	INT32 xaxis;
	INT32 yaxis;
} joystickvector2_t;

boolean precache = true; // if true, load all graphics at start

INT16 prevmap, nextmap;

// Analog Control
static void UserAnalog_OnChange(void);
static void UserAnalog2_OnChange(void);
static void Analog_OnChange(void);
static void Analog2_OnChange(void);
static void DirectionChar_OnChange(void);
static void DirectionChar2_OnChange(void);
static void AutoBrake_OnChange(void);
static void AutoBrake2_OnChange(void);
void SendWeaponPref(void);
void SendWeaponPref2(void);

static CV_PossibleValue_t crosshair_cons_t[] = {{0, "Off"}, {1, "Cross"}, {2, "Angle"}, {3, "Point"}, {0, NULL}};
static CV_PossibleValue_t joyaxis_cons_t[] = {{0, "None"},
{1, "X-Axis"}, {2, "Y-Axis"}, {-1, "X-Axis-"}, {-2, "Y-Axis-"},
#if JOYAXISSET > 1
{3, "Z-Axis"}, {4, "X-Rudder"}, {-3, "Z-Axis-"}, {-4, "X-Rudder-"},
#endif
#if JOYAXISSET > 2
{5, "Y-Rudder"}, {6, "Z-Rudder"}, {-5, "Y-Rudder-"}, {-6, "Z-Rudder-"},
#endif
#if JOYAXISSET > 3
{7, "U-Axis"}, {8, "V-Axis"}, {-7, "U-Axis-"}, {-8, "V-Axis-"},
#endif
 {0, NULL}};
#if JOYAXISSET > 4
"More Axis Sets"
#endif

// don't mind me putting these here, I was lazy to figure out where else I could put those without blowing up the compiler.

// it automatically becomes compact with 20+ players, but if you like it, I guess you can turn that on!
consvar_t cv_compactscoreboard= CVAR_INIT ("compactscoreboard", "Off", CV_SAVE, CV_OnOff, NULL);

// chat timer thingy
static CV_PossibleValue_t chattime_cons_t[] = {{5, "MIN"}, {999, "MAX"}, {0, NULL}};
consvar_t cv_chattime = CVAR_INIT ("chattime", "8", CV_SAVE, chattime_cons_t, NULL);

// chatwidth
static CV_PossibleValue_t chatwidth_cons_t[] = {{64, "MIN"}, {300, "MAX"}, {0, NULL}};
consvar_t cv_chatwidth = CVAR_INIT ("chatwidth", "150", CV_SAVE, chatwidth_cons_t, NULL);

// chatheight
static CV_PossibleValue_t chatheight_cons_t[] = {{6, "MIN"}, {22, "MAX"}, {0, NULL}};
consvar_t cv_chatheight= CVAR_INIT ("chatheight", "8", CV_SAVE, chatheight_cons_t, NULL);

// chat notifications (do you want to hear beeps? I'd understand if you didn't.)
consvar_t cv_chatnotifications= CVAR_INIT ("chatnotifications", "On", CV_SAVE, CV_OnOff, NULL);

// chat spam protection (why would you want to disable that???)
consvar_t cv_chatspamprotection= CVAR_INIT ("chatspamprotection", "On", CV_SAVE|CV_NETVAR, CV_OnOff, NULL);
consvar_t cv_chatspamspeed= CVAR_INIT ("chatspamspeed", "35", CV_SAVE|CV_NETVAR, CV_Unsigned, NULL);
consvar_t cv_chatspamburst= CVAR_INIT ("chatspamburst", "3", CV_SAVE|CV_NETVAR, CV_Unsigned, NULL);

// minichat text background
consvar_t cv_chatbacktint = CVAR_INIT ("chatbacktint", "On", CV_SAVE, CV_OnOff, NULL);

// old shit console chat. (mostly exists for stuff like terminal, not because I cared if anyone liked the old chat.)
static CV_PossibleValue_t consolechat_cons_t[] = {{0, "Window"}, {1, "Console"}, {2, "Window (Hidden)"}, {0, NULL}};
consvar_t cv_consolechat = CVAR_INIT ("chatmode", "Window", CV_SAVE, consolechat_cons_t, NULL);

// customizable chat
static CV_PossibleValue_t chatx_cons_t[] = {{-BASEVIDWIDTH/2, "MIN"}, {BASEVIDWIDTH, "MAX"}, {0, NULL}};
static CV_PossibleValue_t chaty_cons_t[] = {{-BASEVIDHEIGHT/2, "MIN"}, {BASEVIDHEIGHT, "MAX"}, {0, NULL}};
static CV_PossibleValue_t chats1_cons_t[] = {{V_SNAPTOLEFT, "Left"}, {V_SNAPTORIGHT, "Right"}, {0, NULL}};
static CV_PossibleValue_t chats2_cons_t[] = {{V_SNAPTOTOP, "Top"}, {V_SNAPTOBOTTOM, "Bottom"}, {0, NULL}};
consvar_t cv_chatx = CVAR_INIT ("chatx", "13", CV_SAVE, chatx_cons_t, NULL);
consvar_t cv_chaty = CVAR_INIT ("chaty", "169", CV_SAVE, chaty_cons_t, NULL);
consvar_t cv_chats1 = CVAR_INIT ("chatleftrightsnapping", "Left", CV_SAVE, chats1_cons_t, NULL);
consvar_t cv_chats2 = CVAR_INIT ("chatupdownsnapping", "Bottom", CV_SAVE, chats2_cons_t, NULL);

// Pause game upon window losing focus
consvar_t cv_pauseifunfocused = CVAR_INIT ("pauseifunfocused", "Yes", CV_SAVE, CV_YesNo, NULL);

consvar_t cv_instantretry = CVAR_INIT ("instantretry", "No", CV_SAVE, CV_YesNo, NULL);

consvar_t cv_crosshair = CVAR_INIT ("crosshair", "Cross", CV_SAVE, crosshair_cons_t, NULL);
consvar_t cv_crosshair2 = CVAR_INIT ("crosshair2", "Cross", CV_SAVE, crosshair_cons_t, NULL);
consvar_t cv_crosshair_invert = CVAR_INIT ("crosshair_invert", "No", CV_SAVE, CV_YesNo, NULL);
consvar_t cv_crosshair2_invert = CVAR_INIT ("crosshair2_invert", "No", CV_SAVE, CV_YesNo, NULL);
consvar_t cv_invertmouse = CVAR_INIT ("invertmouse", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_alwaysfreelook = CVAR_INIT ("alwaysmlook", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_invertmouse2 = CVAR_INIT ("invertmouse2", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_alwaysfreelook2 = CVAR_INIT ("alwaysmlook2", "On", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_chasefreelook = CVAR_INIT ("chasemlook", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_chasefreelook2 = CVAR_INIT ("chasemlook2", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_mousemove = CVAR_INIT ("mousemove", "Off", CV_SAVE, CV_OnOff, NULL);
consvar_t cv_mousemove2 = CVAR_INIT ("mousemove2", "Off", CV_SAVE, CV_OnOff, NULL);

// previously "analog", "analog2", "useranalog", and "useranalog2", invalidating 2.1-era copies of config.cfg
// changed because it'd be nice to see people try out our actually good controls with gamepads now autobrake exists
consvar_t cv_analog[2] = {
	CVAR_INIT ("sessionanalog", "Off", CV_CALL|CV_NOSHOWHELP, CV_OnOff, Analog_OnChange),
	CVAR_INIT ("sessionanalog2", "Off", CV_CALL|CV_NOSHOWHELP, CV_OnOff, Analog2_OnChange),
};
consvar_t cv_useranalog[2] = {
	CVAR_INIT ("configanalog", "On", CV_SAVE|CV_CALL|CV_NOSHOWHELP, CV_OnOff, UserAnalog_OnChange),
	CVAR_INIT ("configanalog2", "On", CV_SAVE|CV_CALL|CV_NOSHOWHELP, CV_OnOff, UserAnalog2_OnChange),
};

// deez New User eXperiences
// Dont push this to git
static CV_PossibleValue_t directionchar_cons_t[] = {{0, "Camera"}, {1, "Movement"}, {2, "Simple Locked"}, {0, NULL}};
consvar_t cv_directionchar[2] = {
	CVAR_INIT ("directionchar", "Movement", CV_SAVE|CV_CALL|CV_ALLOWLUA, directionchar_cons_t, DirectionChar_OnChange),
	CVAR_INIT ("directionchar2", "Movement", CV_SAVE|CV_CALL|CV_ALLOWLUA, directionchar_cons_t, DirectionChar2_OnChange),
};
consvar_t cv_autobrake = CVAR_INIT ("autobrake", "On", CV_SAVE|CV_CALL, CV_OnOff, AutoBrake_OnChange);
consvar_t cv_autobrake2 = CVAR_INIT ("autobrake2", "On", CV_SAVE|CV_CALL, CV_OnOff, AutoBrake2_OnChange);

// hi here's some new controls
static CV_PossibleValue_t zerotoone_cons_t[] = {{0, "MIN"}, {FRACUNIT, "MAX"}, {0, NULL}};
consvar_t cv_cam_shiftfacing[2] = {
	CVAR_INIT ("cam_shiftfacingchar", "0.375", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
	CVAR_INIT ("cam2_shiftfacingchar", "0.375", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
};
consvar_t cv_cam_turnfacing[2] = {
	CVAR_INIT ("cam_turnfacingchar", "0.25", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
	CVAR_INIT ("cam2_turnfacingchar", "0.25", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
};
consvar_t cv_cam_turnfacingability[2] = {
	CVAR_INIT ("cam_turnfacingability", "0.125", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
	CVAR_INIT ("cam2_turnfacingability", "0.125", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
};
consvar_t cv_cam_turnfacingspindash[2] = {
	CVAR_INIT ("cam_turnfacingspindash", "0.25", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
	CVAR_INIT ("cam2_turnfacingspindash", "0.25", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
};
consvar_t cv_cam_turnfacinginput[2] = {
	CVAR_INIT ("cam_turnfacinginput", "0.375", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
	CVAR_INIT ("cam2_turnfacinginput", "0.375", CV_FLOAT|CV_SAVE|CV_ALLOWLUA, zerotoone_cons_t, NULL),
};

static CV_PossibleValue_t centertoggle_cons_t[] = {{0, "Hold"}, {1, "Toggle"}, {2, "Sticky Hold"}, {0, NULL}};
consvar_t cv_cam_centertoggle[2] = {
	CVAR_INIT ("cam_centertoggle", "Hold", CV_SAVE|CV_ALLOWLUA, centertoggle_cons_t, NULL),
	CVAR_INIT ("cam2_centertoggle", "Hold", CV_SAVE|CV_ALLOWLUA, centertoggle_cons_t, NULL),
};

static CV_PossibleValue_t lockedinput_cons_t[] = {{0, "Strafe"}, {1, "Turn"}, {0, NULL}};
consvar_t cv_cam_lockedinput[2] = {
	CVAR_INIT ("cam_lockedinput", "Strafe", CV_SAVE|CV_ALLOWLUA, lockedinput_cons_t, NULL),
	CVAR_INIT ("cam2_lockedinput", "Strafe", CV_SAVE|CV_ALLOWLUA, lockedinput_cons_t, NULL),
};

static CV_PossibleValue_t lockedassist_cons_t[] = {
	{0, "Off"},
	{LOCK_BOSS, "Bosses"},
	{LOCK_BOSS|LOCK_ENEMY, "Enemies"},
	{LOCK_BOSS|LOCK_INTERESTS, "Interests"},
	{LOCK_BOSS|LOCK_ENEMY|LOCK_INTERESTS, "Full"},
	{0, NULL}
};
consvar_t cv_cam_lockonboss[2] = {
	CVAR_INIT ("cam_lockaimassist", "Full", CV_SAVE|CV_ALLOWLUA, lockedassist_cons_t, NULL),
	CVAR_INIT ("cam2_lockaimassist", "Full", CV_SAVE|CV_ALLOWLUA, lockedassist_cons_t, NULL),
};

consvar_t cv_moveaxis = CVAR_INIT ("joyaxis_move", "Y-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_sideaxis = CVAR_INIT ("joyaxis_side", "X-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_lookaxis = CVAR_INIT ("joyaxis_look", "X-Rudder-", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_turnaxis = CVAR_INIT ("joyaxis_turn", "Z-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_jumpaxis = CVAR_INIT ("joyaxis_jump", "None", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_spinaxis = CVAR_INIT ("joyaxis_spin", "None", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_fireaxis = CVAR_INIT ("joyaxis_fire", "Z-Rudder", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_firenaxis = CVAR_INIT ("joyaxis_firenormal", "Z-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_deadzone = CVAR_INIT ("joy_deadzone", "0.125", CV_FLOAT|CV_SAVE, zerotoone_cons_t, NULL);
consvar_t cv_digitaldeadzone = CVAR_INIT ("joy_digdeadzone", "0.25", CV_FLOAT|CV_SAVE, zerotoone_cons_t, NULL);

consvar_t cv_moveaxis2 = CVAR_INIT ("joyaxis2_move", "Y-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_sideaxis2 = CVAR_INIT ("joyaxis2_side", "X-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_lookaxis2 = CVAR_INIT ("joyaxis2_look", "X-Rudder-", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_turnaxis2 = CVAR_INIT ("joyaxis2_turn", "Z-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_jumpaxis2 = CVAR_INIT ("joyaxis2_jump", "None", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_spinaxis2 = CVAR_INIT ("joyaxis2_spin", "None", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_fireaxis2 = CVAR_INIT ("joyaxis2_fire", "Z-Rudder", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_firenaxis2 = CVAR_INIT ("joyaxis2_firenormal", "Z-Axis", CV_SAVE, joyaxis_cons_t, NULL);
consvar_t cv_deadzone2 = CVAR_INIT ("joy_deadzone2", "0.125", CV_FLOAT|CV_SAVE, zerotoone_cons_t, NULL);
consvar_t cv_digitaldeadzone2 = CVAR_INIT ("joy_digdeadzone2", "0.25", CV_FLOAT|CV_SAVE, zerotoone_cons_t, NULL);

player_t *seenplayer; // player we're aiming at right now

// now automatically allocated in D_RegisterClientCommands
// so that it doesn't have to be updated depending on the value of MAXPLAYERS
char player_names[MAXPLAYERS][MAXPLAYERNAME+1];

INT32 player_name_changes[MAXPLAYERS];

INT16 rw_maximums[NUM_WEAPONS] =
{
	800, // MAX_INFINITY
	400, // MAX_AUTOMATIC
	100, // MAX_BOUNCE
	50,  // MAX_SCATTER
	100, // MAX_GRENADE
	50,  // MAX_EXPLOSION
	50   // MAX_RAIL
};

// Allocation for time and nights data
void G_AllocMainRecordData(INT16 i, gamedata_t *data)
{
	if (!data->mainrecords[i])
		data->mainrecords[i] = Z_Malloc(sizeof(recorddata_t), PU_STATIC, NULL);
	memset(data->mainrecords[i], 0, sizeof(recorddata_t));
}

void G_AllocNightsRecordData(INT16 i, gamedata_t *data)
{
	if (!data->nightsrecords[i])
		data->nightsrecords[i] = Z_Malloc(sizeof(nightsdata_t), PU_STATIC, NULL);
	memset(data->nightsrecords[i], 0, sizeof(nightsdata_t));
}

// MAKE SURE YOU SAVE DATA BEFORE CALLING THIS
void G_ClearRecords(gamedata_t *data)
{
	INT16 i;
	for (i = 0; i < NUMMAPS; ++i)
	{
		if (data->mainrecords[i])
		{
			Z_Free(data->mainrecords[i]);
			data->mainrecords[i] = NULL;
		}
		if (data->nightsrecords[i])
		{
			Z_Free(data->nightsrecords[i]);
			data->nightsrecords[i] = NULL;
		}
	}
}

// For easy retrieval of records
UINT32 G_GetBestScore(INT16 map, gamedata_t *data)
{
	if (!data->mainrecords[map-1])
		return 0;

	return data->mainrecords[map-1]->score;
}

tic_t G_GetBestTime(INT16 map, gamedata_t *data)
{
	if (!data->mainrecords[map-1] || data->mainrecords[map-1]->time <= 0)
		return (tic_t)UINT32_MAX;

	return data->mainrecords[map-1]->time;
}

UINT16 G_GetBestRings(INT16 map, gamedata_t *data)
{
	if (!data->mainrecords[map-1])
		return 0;

	return data->mainrecords[map-1]->rings;
}

UINT32 G_GetBestNightsScore(INT16 map, UINT8 mare, gamedata_t *data)
{
	if (!data->nightsrecords[map-1])
		return 0;

	return data->nightsrecords[map-1]->score[mare];
}

tic_t G_GetBestNightsTime(INT16 map, UINT8 mare, gamedata_t *data)
{
	if (!data->nightsrecords[map-1] || data->nightsrecords[map-1]->time[mare] <= 0)
		return (tic_t)UINT32_MAX;

	return data->nightsrecords[map-1]->time[mare];
}

UINT8 G_GetBestNightsGrade(INT16 map, UINT8 mare, gamedata_t *data)
{
	if (!data->nightsrecords[map-1])
		return 0;

	return data->nightsrecords[map-1]->grade[mare];
}

// For easy adding of NiGHTS records
void G_AddTempNightsRecords(player_t *player, UINT32 pscore, tic_t ptime, UINT8 mare)
{
	const UINT8 playerID = player - players;

	I_Assert(player != NULL);

	ntemprecords[playerID].score[mare] = pscore;
	ntemprecords[playerID].grade[mare] = P_GetGrade(pscore, gamemap, mare - 1);
	ntemprecords[playerID].time[mare] = ptime;

	// Update nummares
	// Note that mare "0" is overall, mare "1" is the first real mare
	if (ntemprecords[playerID].nummares < mare)
		ntemprecords[playerID].nummares = mare;
}

//
// G_SetMainRecords
//
// Update replay files/data, etc. for Record Attack
// See G_SetNightsRecords for NiGHTS Attack.
//
static void G_SetMainRecords(gamedata_t *data, player_t *player)
{
	UINT8 earnedEmblems;

	I_Assert(player != NULL);

	// Record new best time
	if (!data->mainrecords[gamemap-1])
		G_AllocMainRecordData(gamemap-1, data);

	if (player->recordscore > data->mainrecords[gamemap-1]->score)
		data->mainrecords[gamemap-1]->score = player->recordscore;

	if ((data->mainrecords[gamemap-1]->time == 0) || (player->realtime < data->mainrecords[gamemap-1]->time))
		data->mainrecords[gamemap-1]->time = player->realtime;

	if ((UINT16)(player->rings) > data->mainrecords[gamemap-1]->rings)
		data->mainrecords[gamemap-1]->rings = (UINT16)(player->rings);

	if (modeattacking)
	{
		const size_t glen = strlen(srb2home)+1+strlen("replay")+1+strlen(timeattackfolder)+1+strlen("MAPXX")+1;
		char *gpath;
		char lastdemo[256], bestdemo[256];

		// Save demo!
		bestdemo[255] = '\0';
		lastdemo[255] = '\0';
		G_SetDemoTime(player->realtime, player->recordscore, (UINT16)(player->rings));
		G_CheckDemoStatus();

		I_mkdir(va("%s"PATHSEP"replay", srb2home), 0755);
		I_mkdir(va("%s"PATHSEP"replay"PATHSEP"%s", srb2home, timeattackfolder), 0755);

		if ((gpath = malloc(glen)) == NULL)
			I_Error("Out of memory for replay filepath\n");

		sprintf(gpath,"%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s", srb2home, timeattackfolder, G_BuildMapName(gamemap));
		snprintf(lastdemo, 255, "%s-%s-last.lmp", gpath, skins[cv_chooseskin.value-1]->name);

		if (FIL_FileExists(lastdemo))
		{
			UINT8 *buf;
			size_t len = FIL_ReadFile(lastdemo, &buf);

			snprintf(bestdemo, 255, "%s-%s-time-best.lmp", gpath, skins[cv_chooseskin.value-1]->name);
			if (!FIL_FileExists(bestdemo) || G_CmpDemoTime(bestdemo, lastdemo) & 1)
			{ // Better time, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW RECORD TIME!"), M_GetText("Saved replay as"), bestdemo);
			}

			snprintf(bestdemo, 255, "%s-%s-score-best.lmp", gpath, skins[cv_chooseskin.value-1]->name);
			if (!FIL_FileExists(bestdemo) || (G_CmpDemoTime(bestdemo, lastdemo) & (1<<1)))
			{ // Better score, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW HIGH SCORE!"), M_GetText("Saved replay as"), bestdemo);
			}

			snprintf(bestdemo, 255, "%s-%s-rings-best.lmp", gpath, skins[cv_chooseskin.value-1]->name);
			if (!FIL_FileExists(bestdemo) || (G_CmpDemoTime(bestdemo, lastdemo) & (1<<2)))
			{ // Better rings, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW MOST RINGS!"), M_GetText("Saved replay as"), bestdemo);
			}

			//CONS_Printf("%s '%s'\n", M_GetText("Saved replay as"), lastdemo);

			Z_Free(buf);
		}
		free(gpath);
	}

	// Check emblems when level data is updated
	if ((earnedEmblems = M_CheckLevelEmblems(data)))
	{
		CONS_Printf(M_GetText("\x82" "Earned %hu emblem%s for Record Attack records.\n"), (UINT16)earnedEmblems, earnedEmblems > 1 ? "s" : "");
	}

	// Update timeattack menu's replay availability.
	Nextmap_OnChange();
}

static void G_SetNightsRecords(gamedata_t *data, player_t *player)
{
	nightsdata_t *const ntemprecord = &ntemprecords[player - players];
	UINT32 totalscore = 0;
	tic_t totaltime = 0;
	INT32 i;

	UINT8 earnedEmblems;

	if (!ntemprecord->nummares)
	{
		return;
	}

	// Set overall
	{
		UINT8 totalrank = 0, realrank = 0;

		for (i = 1; i <= ntemprecord->nummares; ++i)
		{
			totalscore += ntemprecord->score[i];
			totalrank += ntemprecord->grade[i];
			totaltime += ntemprecord->time[i];
		}

		// Determine overall grade
		realrank = (UINT8)((FixedDiv((fixed_t)totalrank << FRACBITS, ntemprecord->nummares << FRACBITS) + (FRACUNIT/2)) >> FRACBITS);

		// You need ALL rainbow As to get a rainbow A overall
		if (realrank == GRADE_S && (totalrank / ntemprecord->nummares) != GRADE_S)
		{
			realrank = GRADE_A;
		}

		ntemprecord->score[0] = totalscore;
		ntemprecord->grade[0] = realrank;
		ntemprecord->time[0] = totaltime;
	}

	// Now take all temp records and put them in the actual records
	{
		nightsdata_t *maprecords;

		if (!data->nightsrecords[gamemap-1])
		{
			G_AllocNightsRecordData(gamemap-1, data);
		}

		maprecords = data->nightsrecords[gamemap-1];

		if (maprecords->nummares != ntemprecord->nummares)
		{
			maprecords->nummares = ntemprecord->nummares;
		}

		for (i = 0; i < ntemprecord->nummares + 1; ++i)
		{
			if (maprecords->score[i] < ntemprecord->score[i])
				maprecords->score[i] = ntemprecord->score[i];
			if (maprecords->grade[i] < ntemprecord->grade[i])
				maprecords->grade[i] = ntemprecord->grade[i];
			if (!maprecords->time[i] || maprecords->time[i] > ntemprecord->time[i])
				maprecords->time[i] = ntemprecord->time[i];
		}
	}

	memset(&ntemprecords[player - players], 0, sizeof(nightsdata_t));

	if (modeattacking)
	{
		const size_t glen = strlen(srb2home)+1+strlen("replay")+1+strlen(timeattackfolder)+1+strlen("MAPXX")+1;
		char *gpath;
		char lastdemo[256], bestdemo[256];

		// Save demo!
		bestdemo[255] = '\0';
		lastdemo[255] = '\0';
		G_SetDemoTime(totaltime, totalscore, 0);
		G_CheckDemoStatus();

		I_mkdir(va("%s"PATHSEP"replay", srb2home), 0755);
		I_mkdir(va("%s"PATHSEP"replay"PATHSEP"%s", srb2home, timeattackfolder), 0755);

		if ((gpath = malloc(glen)) == NULL)
			I_Error("Out of memory for replay filepath\n");

		sprintf(gpath,"%s"PATHSEP"replay"PATHSEP"%s"PATHSEP"%s", srb2home, timeattackfolder, G_BuildMapName(gamemap));
		snprintf(lastdemo, 255, "%s-%s-last.lmp", gpath, skins[cv_chooseskin.value-1]->name);

		if (FIL_FileExists(lastdemo))
		{
			UINT8 *buf;
			size_t len = FIL_ReadFile(lastdemo, &buf);

			snprintf(bestdemo, 255, "%s-%s-time-best.lmp", gpath, skins[cv_chooseskin.value-1]->name);
			if (!FIL_FileExists(bestdemo) || G_CmpDemoTime(bestdemo, lastdemo) & 1)
			{ // Better time, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW RECORD TIME!"), M_GetText("Saved replay as"), bestdemo);
			}

			snprintf(bestdemo, 255, "%s-%s-score-best.lmp", gpath, skins[cv_chooseskin.value-1]->name);
			if (!FIL_FileExists(bestdemo) || (G_CmpDemoTime(bestdemo, lastdemo) & (1<<1)))
			{ // Better score, save this demo.
				if (FIL_FileExists(bestdemo))
					remove(bestdemo);
				FIL_WriteFile(bestdemo, buf, len);
				CONS_Printf("\x83%s\x80 %s '%s'\n", M_GetText("NEW HIGH SCORE!"), M_GetText("Saved replay as"), bestdemo);
			}

			//CONS_Printf("%s '%s'\n", M_GetText("Saved replay as"), lastdemo);

			Z_Free(buf);
		}
		free(gpath);
	}

	if ((earnedEmblems = M_CheckLevelEmblems(data)))
	{
		CONS_Printf(M_GetText("\x82" "Earned %hu emblem%s for NiGHTS records.\n"), (UINT16)earnedEmblems, earnedEmblems > 1 ? "s" : "");
	}

	// If the mare count changed, this will update the score display
	Nextmap_OnChange();
}

// for consistency among messages: this modifies the game and removes savemoddata.
void G_SetGameModified(boolean silent)
{
	if (modifiedgame && !savemoddata)
		return;

	modifiedgame = true;
	savemoddata = false;

	if (!silent)
		CONS_Alert(CONS_NOTICE, M_GetText("Game must be restarted to play Record Attack.\n"));

	// If in record attack recording, cancel it.
	if (modeattacking)
		M_EndModeAttackRun();
	else if (marathonmode)
		Command_ExitGame_f();
}

void G_SetUsedCheats(boolean silent)
{
	if (usedCheats)
		return;

	usedCheats = true;

	if (!silent)
		CONS_Alert(CONS_NOTICE, M_GetText("Game must be restarted to save progress.\n"));

	// If in record attack recording, cancel it.
	if (modeattacking)
		M_EndModeAttackRun();
	else if (marathonmode)
		Command_ExitGame_f();
}

/** Builds an original game map name from a map number.
  * The complexity is due to MAPA0-MAPZZ.
  *
  * \param map Map number.
  * \return Pointer to a static buffer containing the desired map name.
  * \sa M_MapNumber
  */
const char *G_BuildMapName(INT32 map)
{
	static char mapname[10] = "MAPXX"; // internal map name (wad resource name)

	I_Assert(map > 0);
	I_Assert(map <= NUMMAPS);

	if (map < 100)
		sprintf(&mapname[3], "%.2d", map);
	else
	{
		mapname[3] = (char)('A' + (char)((map - 100) / 36));
		if ((map - 100) % 36 < 10)
			mapname[4] = (char)('0' + (char)((map - 100) % 36));
		else
			mapname[4] = (char)('A' + (char)((map - 100) % 36) - 10);
		mapname[5] = '\0';
	}

	return mapname;
}

/** Clips the console player's mouse aiming to the current view.
  * Used whenever the player view is changed manually.
  *
  * \param aiming Pointer to the vertical angle to clip.
  * \return Short version of the clipped angle for building a ticcmd.
  */
INT16 G_ClipAimingPitch(INT32 *aiming)
{
	INT32 limitangle;

	limitangle = ANGLE_90 - 1;

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (INT16)((*aiming)>>16);
}

INT16 G_SoftwareClipAimingPitch(INT32 *aiming)
{
	INT32 limitangle;

	// note: the current software mode implementation doesn't have true perspective
	limitangle = ANGLE_90 - ANG10; // Some viewing fun, but not too far down...

	if (*aiming > limitangle)
		*aiming = limitangle;
	else if (*aiming < -limitangle)
		*aiming = -limitangle;

	return (INT16)((*aiming)>>16);
}

INT32 JoyAxis(joyaxis_e axissel)
{
	INT32 retaxis;
	INT32 axisval;
	boolean flp = false;

	//find what axis to get
	switch (axissel)
	{
		case JA_TURN:
			axisval = cv_turnaxis.value;
			break;
		case JA_MOVE:
			axisval = cv_moveaxis.value;
			break;
		case JA_LOOK:
			axisval = cv_lookaxis.value;
			break;
		case JA_STRAFE:
			axisval = cv_sideaxis.value;
			break;
		case JA_JUMP:
			axisval = cv_jumpaxis.value;
			break;
		case JA_SPIN:
			axisval = cv_spinaxis.value;
			break;
		case JA_FIRE:
			axisval = cv_fireaxis.value;
			break;
		case JA_FIRENORMAL:
			axisval = cv_firenaxis.value;
			break;
		default:
			return 0;
	}

	if (axisval < 0) //odd -axises
	{
		axisval = -axisval;
		flp = true;
	}
	if (axisval > JOYAXISSET*2 || axisval == 0) //not there in array or None
		return 0;

	if (axisval%2)
	{
		axisval /= 2;
		retaxis = joyxmove[axisval];
	}
	else
	{
		axisval--;
		axisval /= 2;
		retaxis = joyymove[axisval];
	}

	if (retaxis < (-JOYAXISRANGE))
		retaxis = -JOYAXISRANGE;
	if (retaxis > (+JOYAXISRANGE))
		retaxis = +JOYAXISRANGE;

	if (!Joystick.bGamepadStyle && axissel >= JA_DIGITAL)
	{
		const INT32 jdeadzone = ((JOYAXISRANGE-1) * cv_digitaldeadzone.value) >> FRACBITS;
		if (-jdeadzone < retaxis && retaxis < jdeadzone)
			return 0;
	}

	if (flp) retaxis = -retaxis; //flip it around
	return retaxis;
}

INT32 Joy2Axis(joyaxis_e axissel)
{
	INT32 retaxis;
	INT32 axisval;
	boolean flp = false;

	//find what axis to get
	switch (axissel)
	{
		case JA_TURN:
			axisval = cv_turnaxis2.value;
			break;
		case JA_MOVE:
			axisval = cv_moveaxis2.value;
			break;
		case JA_LOOK:
			axisval = cv_lookaxis2.value;
			break;
		case JA_STRAFE:
			axisval = cv_sideaxis2.value;
			break;
		case JA_JUMP:
			axisval = cv_jumpaxis2.value;
			break;
		case JA_SPIN:
			axisval = cv_spinaxis2.value;
			break;
		case JA_FIRE:
			axisval = cv_fireaxis2.value;
			break;
		case JA_FIRENORMAL:
			axisval = cv_firenaxis2.value;
			break;
		default:
			return 0;
	}


	if (axisval < 0) //odd -axises
	{
		axisval = -axisval;
		flp = true;
	}

	if (axisval > JOYAXISSET*2 || axisval == 0) //not there in array or None
		return 0;

	if (axisval%2)
	{
		axisval /= 2;
		retaxis = joy2xmove[axisval];
	}
	else
	{
		axisval--;
		axisval /= 2;
		retaxis = joy2ymove[axisval];
	}

	if (retaxis < (-JOYAXISRANGE))
		retaxis = -JOYAXISRANGE;
	if (retaxis > (+JOYAXISRANGE))
		retaxis = +JOYAXISRANGE;

	if (!Joystick2.bGamepadStyle && axissel >= JA_DIGITAL)
	{
		const INT32 jdeadzone = ((JOYAXISRANGE-1) * cv_digitaldeadzone2.value) >> FRACBITS;
		if (-jdeadzone < retaxis && retaxis < jdeadzone)
			return 0;
	}

	if (flp) retaxis = -retaxis; //flip it around
	return retaxis;
}


#define PlayerJoyAxis(p, ax) ((p) == 1 ? JoyAxis(ax) : Joy2Axis(ax))

// Take a magnitude of two axes, and adjust it to take out the deadzone
// Will return a value between 0 and JOYAXISRANGE
static INT32 G_BasicDeadZoneCalculation(INT32 magnitude, fixed_t deadZone)
{
	const INT32 jdeadzone = (JOYAXISRANGE * deadZone) / FRACUNIT;
	INT32 deadzoneAppliedValue = 0;
	INT32 adjustedMagnitude = abs(magnitude);

	if (jdeadzone >= JOYAXISRANGE && adjustedMagnitude >= JOYAXISRANGE) // If the deadzone and magnitude are both 100%...
		return JOYAXISRANGE; // ...return 100% input directly, to avoid dividing by 0
	else if (adjustedMagnitude > jdeadzone) // Otherwise, calculate how much the magnitude exceeds the deadzone
	{
		adjustedMagnitude = min(adjustedMagnitude, JOYAXISRANGE);

		adjustedMagnitude -= jdeadzone;

		deadzoneAppliedValue = (adjustedMagnitude * JOYAXISRANGE) / (JOYAXISRANGE - jdeadzone);
	}

	return deadzoneAppliedValue;
}

// Get the actual sensible radial value for a joystick axis when accounting for a deadzone
static void G_HandleAxisDeadZone(UINT8 splitnum, joystickvector2_t *joystickvector)
{
	INT32 gamepadStyle = Joystick.bGamepadStyle;
	fixed_t deadZone = cv_deadzone.value;

	if (splitnum == 1)
	{
		gamepadStyle = Joystick2.bGamepadStyle;
		deadZone = cv_deadzone2.value;
	}

	// When gamepadstyle is "true" the values are just -1, 0, or 1. This is done in the interface code.
	if (!gamepadStyle)
	{
		// Get the total magnitude of the 2 axes
		INT32 magnitude = (joystickvector->xaxis * joystickvector->xaxis) + (joystickvector->yaxis * joystickvector->yaxis);
		INT32 normalisedXAxis;
		INT32 normalisedYAxis;
		INT32 normalisedMagnitude;
		double dMagnitude = sqrt((double)magnitude);
		magnitude = (INT32)dMagnitude;

		// Get the normalised xy values from the magnitude
		normalisedXAxis = (joystickvector->xaxis * magnitude) / JOYAXISRANGE;
		normalisedYAxis = (joystickvector->yaxis * magnitude) / JOYAXISRANGE;

		// Apply the deadzone to the magnitude to give a correct value between 0 and JOYAXISRANGE
		normalisedMagnitude = G_BasicDeadZoneCalculation(magnitude, deadZone);

		// Apply the deadzone to the xy axes
		joystickvector->xaxis = (normalisedXAxis * normalisedMagnitude) / JOYAXISRANGE;
		joystickvector->yaxis = (normalisedYAxis * normalisedMagnitude) / JOYAXISRANGE;

		// Cap the values so they don't go above the correct maximum
		joystickvector->xaxis = min(joystickvector->xaxis, JOYAXISRANGE);
		joystickvector->xaxis = max(joystickvector->xaxis, -JOYAXISRANGE);
		joystickvector->yaxis = min(joystickvector->yaxis, JOYAXISRANGE);
		joystickvector->yaxis = max(joystickvector->yaxis, -JOYAXISRANGE);
	}
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
// set secondaryplayer true to build player 2's ticcmd in splitscreen mode
//
INT32 localaiming, localaiming2;
angle_t localangle, localangle2;

static fixed_t forwardmove[2] = {25<<FRACBITS>>16, 50<<FRACBITS>>16};
static fixed_t sidemove[2] = {25<<FRACBITS>>16, 50<<FRACBITS>>16}; // faster!
static fixed_t angleturn[3] = {640, 1280, 320}; // + slow turn

INT16 ticcmd_oldangleturn[2];
boolean ticcmd_centerviewdown[2]; // For simple controls, lock the camera behind the player
mobj_t *ticcmd_ztargetfocus[2]; // Locking onto an object?
void G_BuildTiccmd(ticcmd_t *cmd, INT32 realtics, UINT8 ssplayer)
{
	boolean forcestrafe = false;
	boolean forcefullinput = false;
	INT32 tspeed, forward, side, axis, strafeaxis, moveaxis, turnaxis, lookaxis, i;

	joystickvector2_t movejoystickvector, lookjoystickvector;

	const INT32 speed = 1;
	// these ones used for multiple conditions
	boolean turnleft, turnright, strafelkey, straferkey, movefkey, movebkey, mouseaiming, analogjoystickmove, gamepadjoystickmove, thisjoyaiming;
	boolean strafeisturn; // Simple controls only
	player_t *player = &players[ssplayer == 2 ? secondarydisplayplayer : consoleplayer];
	camera_t *thiscam = ((ssplayer == 1 || player->bot == BOT_2PHUMAN) ? &camera : &camera2);
	angle_t *myangle = (ssplayer == 1 ? &localangle : &localangle2);
	INT32 *myaiming = (ssplayer == 1 ? &localaiming : &localaiming2);

	angle_t drawangleoffset = (player->powers[pw_carry] == CR_ROLLOUT) ? ANGLE_180 : 0;
	INT32 chasecam, chasefreelook, alwaysfreelook, usejoystick, invertmouse, turnmultiplier, mousemove;
	controlstyle_e controlstyle = G_ControlStyle(ssplayer);
	INT32 mdx, mdy, mldy;

	static INT32 turnheld[2]; // for accelerative turning
	static boolean keyboard_look[2]; // true if lookup/down using keyboard
	static boolean resetdown[2]; // don't cam reset every frame
	static boolean joyaiming[2]; // check the last frame's value if we need to reset the camera

	// simple mode vars
	static boolean zchange[2]; // only switch z targets once per press
	static fixed_t tta_factor[2] = {FRACUNIT, FRACUNIT}; // disables turn-to-angle when manually turning camera until movement happens
	boolean centerviewdown = false;

	UINT8 forplayer = ssplayer-1;

	if (ssplayer == 1)
	{
		chasecam = cv_chasecam.value;
		chasefreelook = cv_chasefreelook.value;
		alwaysfreelook = cv_alwaysfreelook.value;
		usejoystick = cv_usejoystick.value;
		invertmouse = cv_invertmouse.value;
		turnmultiplier = cv_cam_turnmultiplier.value;
		mousemove = cv_mousemove.value;
		mdx = mouse.dx;
		mdy = -mouse.dy;
		mldy = -mouse.mlookdy;
		G_CopyTiccmd(cmd, I_BaseTiccmd(), 1); // empty, or external driver
	}
	else
	{
		chasecam = cv_chasecam2.value;
		chasefreelook = cv_chasefreelook2.value;
		alwaysfreelook = cv_alwaysfreelook2.value;
		usejoystick = cv_usejoystick2.value;
		invertmouse = cv_invertmouse2.value;
		turnmultiplier = cv_cam2_turnmultiplier.value;
		mousemove = cv_mousemove2.value;
		mdx = mouse2.dx;
		mdy = -mouse2.dy;
		mldy = -mouse2.mlookdy;
		G_CopyTiccmd(cmd, I_BaseTiccmd2(), 1); // empty, or external driver
	}

	if (menuactive || CON_Ready() || chat_on)
		mdx = mdy = mldy = 0;

	strafeisturn = controlstyle == CS_SIMPLE && ticcmd_centerviewdown[forplayer] &&
		((cv_cam_lockedinput[forplayer].value && !ticcmd_ztargetfocus[forplayer]) || (player->pflags & PF_STARTDASH)) &&
		!player->climbing && player->powers[pw_carry] != CR_MINECART;

	// why build a ticcmd if we're paused?
	// Or, for that matter, if we're being reborn.
	// ...OR if we're blindfolded. No looking into the floor.
	if (ignoregameinputs || paused || P_AutoPause() || (gamestate == GS_LEVEL && (player->playerstate == PST_REBORN || ((gametyperules & GTR_TAG)
	&& (leveltime < hidetime * TICRATE) && (player->pflags & PF_TAGIT)))))
	{//@TODO splitscreen player
		cmd->angleturn = ticcmd_oldangleturn[forplayer];
		cmd->aiming = G_ClipAimingPitch(myaiming);
		return;
	}

	turnright = PLAYERINPUTDOWN(ssplayer, GC_TURNRIGHT);
	turnleft = PLAYERINPUTDOWN(ssplayer, GC_TURNLEFT);

	straferkey = PLAYERINPUTDOWN(ssplayer, GC_STRAFERIGHT);
	strafelkey = PLAYERINPUTDOWN(ssplayer, GC_STRAFELEFT);
	movefkey = PLAYERINPUTDOWN(ssplayer, GC_FORWARD);
	movebkey = PLAYERINPUTDOWN(ssplayer, GC_BACKWARD);

	if (strafeisturn)
	{
		turnright |= straferkey;
		turnleft |= strafelkey;
		straferkey = strafelkey = false;
	}

	mouseaiming = (PLAYERINPUTDOWN(ssplayer, GC_MOUSEAIMING)) ^
		((chasecam && !player->spectator) ? chasefreelook : alwaysfreelook);
	analogjoystickmove = usejoystick && !Joystick.bGamepadStyle;
	gamepadjoystickmove = usejoystick && Joystick.bGamepadStyle;

	thisjoyaiming = (chasecam && !player->spectator) ? chasefreelook : alwaysfreelook;

	// Reset the vertical look if we're no longer joyaiming
	if (!thisjoyaiming && joyaiming[forplayer])
		*myaiming = 0;
	joyaiming[forplayer] = thisjoyaiming;

	turnaxis = PlayerJoyAxis(ssplayer, JA_TURN);
	if (strafeisturn)
		turnaxis += PlayerJoyAxis(ssplayer, JA_STRAFE);
	lookaxis = PlayerJoyAxis(ssplayer, JA_LOOK);
	lookjoystickvector.xaxis = turnaxis;
	lookjoystickvector.yaxis = lookaxis;
	G_HandleAxisDeadZone(forplayer, &lookjoystickvector);

	if (gamepadjoystickmove && lookjoystickvector.xaxis != 0)
	{
		turnright = turnright || (lookjoystickvector.xaxis > 0);
		turnleft = turnleft || (lookjoystickvector.xaxis < 0);
	}
	forward = side = 0;

	// use two stage accelerative turning
	// on the keyboard and joystick
	if (turnleft || turnright)
		turnheld[forplayer] += realtics;
	else
		turnheld[forplayer] = 0;

	if (turnheld[forplayer] < SLOWTURNTICS)
		tspeed = 2; // slow turn
	else
		tspeed = speed;

	// let movement keys cancel each other out
	if (controlstyle == CS_LMAOGALOG) // Analog
	{
		if (turnright)
			cmd->angleturn = (INT16)(cmd->angleturn - angleturn[tspeed]);
		if (turnleft)
			cmd->angleturn = (INT16)(cmd->angleturn + angleturn[tspeed]);
	}
	if (twodlevel
		|| (player->mo && (player->mo->flags2 & MF2_TWOD))
		|| (!demoplayback && (player->pflags & PF_SLIDING)))
			forcefullinput = true;
	if (twodlevel
		|| (player->mo && (player->mo->flags2 & MF2_TWOD))
		|| (!demoplayback && ((player->powers[pw_carry] == CR_NIGHTSMODE)
		|| (player->pflags & (PF_SLIDING|PF_FORCESTRAFE))))) // Analog
			forcestrafe = true;
	if (forcestrafe)
	{
		if (turnright)
			side += sidemove[speed];
		if (turnleft)
			side -= sidemove[speed];

		if (analogjoystickmove && lookjoystickvector.xaxis != 0)
		{
			// JOYAXISRANGE is supposed to be 1023 (divide by 1024)
			side += ((lookjoystickvector.xaxis * sidemove[1]) >> 10);
		}
	}
	else if (controlstyle == CS_LMAOGALOG) // Analog
	{
		if (turnright)
			cmd->buttons |= BT_CAMRIGHT;
		if (turnleft)
			cmd->buttons |= BT_CAMLEFT;
	}
	else
	{
		if (turnright && turnleft);
		else if (turnright)
			cmd->angleturn = (INT16)(cmd->angleturn - ((angleturn[tspeed] * turnmultiplier)>>FRACBITS));
		else if (turnleft)
			cmd->angleturn = (INT16)(cmd->angleturn + ((angleturn[tspeed] * turnmultiplier)>>FRACBITS));

		if (analogjoystickmove && lookjoystickvector.xaxis != 0)
		{
			// JOYAXISRANGE should be 1023 (divide by 1024)
			cmd->angleturn = (INT16)(cmd->angleturn - ((((lookjoystickvector.xaxis * angleturn[1]) >> 10) * turnmultiplier)>>FRACBITS)); // ANALOG!
		}

		if (turnright || turnleft || abs(cmd->angleturn) > angleturn[2])
			tta_factor[forplayer] = 0; // suspend turn to angle
	}

	strafeaxis = strafeisturn ? 0 : PlayerJoyAxis(ssplayer, JA_STRAFE);
	moveaxis = PlayerJoyAxis(ssplayer, JA_MOVE);
	movejoystickvector.xaxis = strafeaxis;
	movejoystickvector.yaxis = moveaxis;
	G_HandleAxisDeadZone(forplayer, &movejoystickvector);

	if (gamepadjoystickmove && movejoystickvector.xaxis != 0)
	{
		if (movejoystickvector.xaxis > 0)
			side += sidemove[speed];
		else if (movejoystickvector.xaxis < 0)
			side -= sidemove[speed];
	}
	else if (analogjoystickmove && movejoystickvector.xaxis != 0)
	{
		// JOYAXISRANGE is supposed to be 1023 (divide by 1024)
		side += ((movejoystickvector.xaxis * sidemove[1]) >> 10);
	}

	// forward with key or button
	if (movefkey || (gamepadjoystickmove && movejoystickvector.yaxis < 0)
		|| ((player->powers[pw_carry] == CR_NIGHTSMODE)
			&& (PLAYERINPUTDOWN(ssplayer, GC_LOOKUP) || (gamepadjoystickmove && lookjoystickvector.yaxis > 0))))
		forward = forwardmove[speed];
	if (movebkey || (gamepadjoystickmove && movejoystickvector.yaxis > 0)
		|| ((player->powers[pw_carry] == CR_NIGHTSMODE)
			&& (PLAYERINPUTDOWN(ssplayer, GC_LOOKDOWN) || (gamepadjoystickmove && lookjoystickvector.yaxis < 0))))
		forward -= forwardmove[speed];

	if (analogjoystickmove && movejoystickvector.yaxis != 0)
		forward -= ((movejoystickvector.yaxis * forwardmove[1]) >> 10); // ANALOG!

	// some people strafe left & right with mouse buttons
	// those people are weird
	if (straferkey)
		side += sidemove[speed];
	if (strafelkey)
		side -= sidemove[speed];

	if (PLAYERINPUTDOWN(ssplayer, GC_WEAPONNEXT))
		cmd->buttons |= BT_WEAPONNEXT; // Next Weapon
	if (PLAYERINPUTDOWN(ssplayer, GC_WEAPONPREV))
		cmd->buttons |= BT_WEAPONPREV; // Previous Weapon

#if NUM_WEAPONS > 10
"Add extra inputs to g_input.h/gamecontrols_e"
#endif
	//use the four avaliable bits to determine the weapon.
	cmd->buttons &= ~BT_WEAPONMASK;
	for (i = 0; i < NUM_WEAPONS; ++i)
		if (PLAYERINPUTDOWN(ssplayer, GC_WEPSLOT1 + i))
		{
			cmd->buttons |= (UINT16)(i + 1);
			break;
		}

	// fire with any button/key
	axis = PlayerJoyAxis(ssplayer, JA_FIRE);
	if (PLAYERINPUTDOWN(ssplayer, GC_FIRE) || (usejoystick && axis > 0))
		cmd->buttons |= BT_ATTACK;

	// fire normal with any button/key
	axis = PlayerJoyAxis(ssplayer, JA_FIRENORMAL);
	if (PLAYERINPUTDOWN(ssplayer, GC_FIRENORMAL) || (usejoystick && axis > 0))
		cmd->buttons |= BT_FIRENORMAL;

	if (PLAYERINPUTDOWN(ssplayer, GC_TOSSFLAG))
		cmd->buttons |= BT_TOSSFLAG;

	// Lua scriptable buttons
	if (PLAYERINPUTDOWN(ssplayer, GC_CUSTOM1))
		cmd->buttons |= BT_CUSTOM1;
	if (PLAYERINPUTDOWN(ssplayer, GC_CUSTOM2))
		cmd->buttons |= BT_CUSTOM2;
	if (PLAYERINPUTDOWN(ssplayer, GC_CUSTOM3))
		cmd->buttons |= BT_CUSTOM3;

	// use with any button/key
	axis = PlayerJoyAxis(ssplayer, JA_SPIN);
	if (PLAYERINPUTDOWN(ssplayer, GC_SPIN) || (usejoystick && axis > 0))
		cmd->buttons |= BT_SPIN;

	if (gamestate == GS_INTRO) // prevent crash in intro
	{
		cmd->angleturn = ticcmd_oldangleturn[forplayer];
		cmd->aiming = G_ClipAimingPitch(myaiming);
		return;
	}

	// Centerview can be a toggle in simple mode!
	{
		static boolean last_centerviewdown[2], centerviewhold[2]; // detect taps for toggle behavior
		boolean down = PLAYERINPUTDOWN(ssplayer, GC_CENTERVIEW);

		if (!(controlstyle == CS_SIMPLE && cv_cam_centertoggle[forplayer].value))
			centerviewdown = down;
		else
		{
			if (down && !last_centerviewdown[forplayer])
				centerviewhold[forplayer] = !centerviewhold[forplayer];
			last_centerviewdown[forplayer] = down;

			if (cv_cam_centertoggle[forplayer].value == 2 && !down && !ticcmd_ztargetfocus[forplayer])
				centerviewhold[forplayer] = false;

			centerviewdown = centerviewhold[forplayer];
		}
	}

	if (centerviewdown)
	{
		if (controlstyle == CS_SIMPLE && !ticcmd_centerviewdown[forplayer] && !G_RingSlingerGametype())
		{
			CV_SetValue(&cv_directionchar[forplayer], 2);
			cmd->angleturn = (INT16)((player->mo->angle - *myangle) >> 16);
			*myaiming = 0;

			if (cv_cam_lockonboss[forplayer].value)
				P_SetTarget(&ticcmd_ztargetfocus[forplayer], P_LookForFocusTarget(player, NULL, 0, cv_cam_lockonboss[forplayer].value));
		}

		ticcmd_centerviewdown[forplayer] = true;
	}
	else if (ticcmd_centerviewdown[forplayer] || (leveltime < 5))
	{
		if (controlstyle == CS_SIMPLE)
		{
			P_SetTarget(&ticcmd_ztargetfocus[forplayer], NULL);
			CV_SetValue(&cv_directionchar[forplayer], 1);
		}

		ticcmd_centerviewdown[forplayer] = false;
	}

	if (ticcmd_ztargetfocus[forplayer])
	{
		if (
			P_MobjWasRemoved(ticcmd_ztargetfocus[forplayer]) ||
			(leveltime < 5) ||
			(player->playerstate != PST_LIVE) ||
			player->exiting ||
			!ticcmd_ztargetfocus[forplayer]->health ||
			(ticcmd_ztargetfocus[forplayer]->type == MT_EGGMOBILE3 && !ticcmd_ztargetfocus[forplayer]->movecount) // Sea Egg is moving around underground and shouldn't be tracked
		)
			P_SetTarget(&ticcmd_ztargetfocus[forplayer], NULL);
		else
		{
			mobj_t *newtarget = NULL;
			if (zchange[forplayer])
			{
				if (!turnleft && !turnright && abs(cmd->angleturn) < angleturn[0])
					zchange[forplayer] = false;
			}
			else if (turnleft || cmd->angleturn > angleturn[0])
			{
				zchange[forplayer] = true;
				newtarget = P_LookForFocusTarget(player, ticcmd_ztargetfocus[forplayer], 1, cv_cam_lockonboss[forplayer].value);
			}
			else if (turnright || cmd->angleturn < -angleturn[0])
			{
				zchange[forplayer] = true;
				newtarget = P_LookForFocusTarget(player, ticcmd_ztargetfocus[forplayer], -1, cv_cam_lockonboss[forplayer].value);
			}

			if (newtarget)
				P_SetTarget(&ticcmd_ztargetfocus[forplayer], newtarget);

			// I assume this is netgame-safe because gunslinger spawns this for only the local player...... *sweats intensely*
			newtarget = P_SpawnMobj(ticcmd_ztargetfocus[forplayer]->x, ticcmd_ztargetfocus[forplayer]->y, ticcmd_ztargetfocus[forplayer]->z, MT_LOCKON); // positioning, flip handled in P_SceneryThinker
			P_SetTarget(&newtarget->target, ticcmd_ztargetfocus[forplayer]);
			newtarget->drawonlyforplayer = player; // Hide it from the other player in splitscreen, and yourself when spectating

			if (player->mo && R_PointToDist2(0, 0,
				player->mo->x - ticcmd_ztargetfocus[forplayer]->x,
				player->mo->y - ticcmd_ztargetfocus[forplayer]->y
			) > 50*player->mo->scale)
			{
				INT32 anglediff = R_PointToAngle2(player->mo->x, player->mo->y, ticcmd_ztargetfocus[forplayer]->x, ticcmd_ztargetfocus[forplayer]->y) - *myangle;
				const INT32 maxturn = ANG10/2;
				anglediff /= 4;

				if (anglediff > maxturn)
					anglediff = maxturn;
				else if (anglediff < -maxturn)
					anglediff = -maxturn;

				cmd->angleturn = (INT16)(cmd->angleturn + (anglediff >> 16));
			}
		}
	}

	if (ticcmd_centerviewdown[forplayer] && controlstyle == CS_SIMPLE)
		controlstyle = CS_LEGACY;

	if (PLAYERINPUTDOWN(ssplayer, GC_CAMRESET))
	{
		if (thiscam->chase && !resetdown[forplayer])
			P_ResetCamera(&players[ssplayer == 1 ? displayplayer : secondarydisplayplayer], thiscam);

		resetdown[forplayer] = true;
	}
	else
		resetdown[forplayer] = false;


	// jump button
	axis = PlayerJoyAxis(ssplayer, JA_JUMP);
	if (PLAYERINPUTDOWN(ssplayer, GC_JUMP) || (usejoystick && axis > 0))
		cmd->buttons |= BT_JUMP;

	// player aiming shit, ahhhh...
	{
		INT32 player_invert = invertmouse ? -1 : 1;
		INT32 screen_invert =
			(player->mo && (player->mo->eflags & MFE_VERTICALFLIP)
			 && (!thiscam->chase || player->pflags & PF_FLIPCAM)) //because chasecam's not inverted
			 ? -1 : 1; // set to -1 or 1 to multiply
		 INT32 configlookaxis = ssplayer == 1 ? cv_lookaxis.value : cv_lookaxis2.value;

		// mouse look stuff (mouse look is not the same as mouse aim)
		if (mouseaiming)
		{
			keyboard_look[forplayer] = false;

			// looking up/down
			*myaiming += (mldy<<19)*player_invert*screen_invert;
		}

		if (analogjoystickmove && joyaiming[forplayer] && lookjoystickvector.yaxis != 0 && configlookaxis != 0)
			*myaiming += (lookjoystickvector.yaxis<<16) * screen_invert;

		// spring back if not using keyboard neither mouselookin'
		if (!keyboard_look[forplayer] && configlookaxis == 0 && !joyaiming[forplayer] && !mouseaiming)
			*myaiming = 0;

		if (!(player->powers[pw_carry] == CR_NIGHTSMODE))
		{
			if (PLAYERINPUTDOWN(ssplayer, GC_LOOKUP) || (gamepadjoystickmove && lookjoystickvector.yaxis < 0))
			{
				*myaiming += KB_LOOKSPEED * screen_invert;
				keyboard_look[forplayer] = true;
			}
			else if (PLAYERINPUTDOWN(ssplayer, GC_LOOKDOWN) || (gamepadjoystickmove && lookjoystickvector.yaxis > 0))
			{
				*myaiming -= KB_LOOKSPEED * screen_invert;
				keyboard_look[forplayer] = true;
			}
			else if (ticcmd_centerviewdown[forplayer])
				*myaiming = 0;
		}

		// accept no mlook for network games
		if (!cv_allowmlook.value)
			*myaiming = 0;

		cmd->aiming = G_ClipAimingPitch(myaiming);
	}

	if (!mouseaiming && mousemove)
		forward += mdy;

	if ((!demoplayback && (player->pflags & PF_SLIDING))) // Analog for mouse
		side += mdx*2;
	else if (controlstyle == CS_LMAOGALOG)
	{
		if (mdx)
		{
			if (mdx > 0)
				cmd->buttons |= BT_CAMRIGHT;
			else
				cmd->buttons |= BT_CAMLEFT;
		}
	}
	else
		cmd->angleturn = (INT16)(cmd->angleturn - (mdx*8));

	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	// No additional acceleration when moving forward/backward and strafing simultaneously.
	// do this AFTER we cap to MAXPLMOVE so people can't find ways to cheese around this.
	if (!forcefullinput && forward && side)
	{
		angle_t angle = R_PointToAngle2(0, 0, side << FRACBITS, forward << FRACBITS);
		INT32 maxforward = abs(P_ReturnThrustY(NULL, angle, MAXPLMOVE));
		INT32 maxside = abs(P_ReturnThrustX(NULL, angle, MAXPLMOVE));
		forward = max(min(forward, maxforward), -maxforward);
		side = max(min(side, maxside), -maxside);
	}

	//Silly hack to make 2d mode *somewhat* playable with no chasecam.
	if ((twodlevel || (player->mo && player->mo->flags2 & MF2_TWOD)) && !thiscam->chase)
	{
		INT32 temp = forward;
		forward = side;
		side = temp;
	}

	cmd->forwardmove = (SINT8)(cmd->forwardmove + forward);
	cmd->sidemove = (SINT8)(cmd->sidemove + side);

	// Note: Majority of botstuffs are handled in G_Ticker now.
	if (player->bot == BOT_2PAI
		&& !player->powers[pw_tailsfly]
		&& (cmd->forwardmove || cmd->sidemove || cmd->buttons))
	{
		player->bot = BOT_2PHUMAN; // A player-controlled bot. Returns to AI when it respawns.
		CV_SetValue(&cv_analog[1], true);
	}

	if (player->bot == BOT_2PHUMAN)
		cmd->angleturn = (INT16)((localangle - *myangle) >> 16);

	*myangle += (cmd->angleturn<<16);

	if (controlstyle == CS_LMAOGALOG) {
		angle_t angle;

		if (player->awayviewtics)
			angle = player->awayviewmobj->angle;
		else
			angle = thiscam->angle;

		cmd->angleturn = (INT16)((angle - (ticcmd_oldangleturn[forplayer] << 16)) >> 16);
	}
	else
	{
		// Adjust camera angle by player input
		if (controlstyle == CS_SIMPLE && !forcestrafe && thiscam->chase && !turnheld[forplayer] && !ticcmd_centerviewdown[forplayer] && !player->climbing && player->powers[pw_carry] != CR_MINECART)
		{
			fixed_t camadjustfactor = cv_cam_turnfacinginput[forplayer].value;

			if (camadjustfactor)
			{
				fixed_t sine = FINESINE((R_PointToAngle2(0, 0, player->rmomx, player->rmomy) - localangle)>>ANGLETOFINESHIFT);
				fixed_t factor;
				INT16 camadjust;

				if ((sine > 0) == (cmd->sidemove > 0))
					sine = 0; // Prevent jerking right when braking from going left, or vice versa

				factor = min(40, FixedMul(player->speed, abs(sine))*2 / FRACUNIT);

				camadjust = (cmd->sidemove * factor * camadjustfactor) >> 16;

				*myangle -= camadjust << 16;
				cmd->angleturn = (INT16)(cmd->angleturn - camadjust);
			}

			if (ticcmd_centerviewdown[forplayer] && (cv_cam_lockedinput[forplayer].value || (player->pflags & PF_STARTDASH)))
				cmd->sidemove = 0;
		}

		// Adjust camera angle to face player direction, depending on circumstances
		// Nothing happens if cam left/right are held, so you can hold both to lock the camera in one direction
		if (controlstyle == CS_SIMPLE && !forcestrafe && thiscam->chase && !turnheld[forplayer] && !ticcmd_centerviewdown[forplayer] && player->powers[pw_carry] != CR_MINECART)
		{
			fixed_t camadjustfactor;
			boolean alt = false; // Reduce intensity on diagonals and prevent backwards movement from turning the camera

			if (player->pflags & PF_GLIDING)
				camadjustfactor = cv_cam_turnfacingability[forplayer].value/4;
			else if (player->pflags & PF_STARTDASH)
				camadjustfactor = cv_cam_turnfacingspindash[forplayer].value/4;
			else
			{
				alt = true;
				camadjustfactor = cv_cam_turnfacing[forplayer].value/8;
			}

			camadjustfactor = FixedMul(camadjustfactor, max(FRACUNIT - player->speed, min(player->speed/18, FRACUNIT)));

			camadjustfactor = FixedMul(camadjustfactor, tta_factor[forplayer]);

			if (tta_factor[forplayer] < FRACUNIT && (cmd->forwardmove || cmd->sidemove || tta_factor[forplayer] >= FRACUNIT/3))
				tta_factor[forplayer] += FRACUNIT>>5;
			else if (tta_factor[forplayer] && tta_factor[forplayer] < FRACUNIT/3)
				tta_factor[forplayer] -= FRACUNIT>>5;

			if (camadjustfactor)
			{
				angle_t controlangle;
				INT32 anglediff;
				INT16 camadjust;

				if ((cmd->forwardmove || cmd->sidemove) && !(player->pflags & PF_SPINNING))
					controlangle = *myangle + R_PointToAngle2(0, 0, cmd->forwardmove << FRACBITS, -cmd->sidemove << FRACBITS);
				else
					controlangle = player->drawangle + drawangleoffset;

				anglediff = controlangle - *myangle;

				if (alt)
				{
					fixed_t sine = FINESINE((angle_t) (anglediff)>>ANGLETOFINESHIFT);
					sine = abs(sine);

					if (abs(anglediff) > ANGLE_90)
						sine = max(0, sine*3 - 2*FRACUNIT); // At about 135 degrees, this will stop turning

					anglediff = FixedMul(anglediff, sine);
				}

				camadjust = FixedMul(anglediff, camadjustfactor) >> 16;

				*myangle += camadjust << 16;
				cmd->angleturn = (INT16)(cmd->angleturn + camadjust);
			}
		}
	}

	// At this point, cmd doesn't contain the final angle yet,
	// So we need to temporarily transform it so Lua scripters
	// don't need to handle it differently than in other hooks.
	if (addedtogame && gamestate == GS_LEVEL)
	{
		INT16 extra = ticcmd_oldangleturn[forplayer] - player->oldrelangleturn;
		INT16 origangle = cmd->angleturn;
		INT16 orighookangle = (INT16)(origangle + player->angleturn + extra);
		INT16 origaiming = cmd->aiming;

		cmd->angleturn = orighookangle;

		// Accel bug (for the funny)
		/*
		if (cmd->forwardmove != 0 && cmd->sidemove != 0) {
			if (cmd->forwardmove > 0) {
				cmd->forwardmove = 50;
			} else {
				cmd->forwardmove = -50;
			}

			if (cmd->sidemove > 0) {
				cmd->sidemove = 50;
			} else {
				cmd->sidemove = -50;
			}
		}
		*/
		LUA_HookTiccmd(player, cmd, HOOK(PlayerCmd));

		extra = cmd->angleturn - orighookangle;
		cmd->angleturn = origangle + extra;
		*myangle += extra << 16;
		*myaiming += (cmd->aiming - origaiming) << 16;

		// Send leveltime when this tic was generated to the server for control lag calculations.
		// Only do this when in a level. Also do this after the hook, so that it can't overwrite this.
		cmd->latency = (leveltime & 0xFF);
	}

	//Reset away view if a command is given.
	if (ssplayer == 1 && (cmd->forwardmove || cmd->sidemove || cmd->buttons)
		&& displayplayer != consoleplayer)
	{
		// Call ViewpointSwitch hooks here.
		// The viewpoint was forcibly changed.
		LUA_HookViewpointSwitch(player, &players[consoleplayer], true);
		displayplayer = consoleplayer;
	}

	cmd->angleturn = (INT16)(cmd->angleturn + ticcmd_oldangleturn[forplayer]);
	ticcmd_oldangleturn[forplayer] = cmd->angleturn;
}

ticcmd_t *G_CopyTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n)
{
	return M_Memcpy(dest, src, n*sizeof(*src));
}

ticcmd_t *G_MoveTiccmd(ticcmd_t* dest, const ticcmd_t* src, const size_t n)
{
	size_t i;
	for (i = 0; i < n; i++)
	{
		dest[i].forwardmove = src[i].forwardmove;
		dest[i].sidemove = src[i].sidemove;
		dest[i].angleturn = SHORT(src[i].angleturn);
		dest[i].aiming = (INT16)SHORT(src[i].aiming);
		dest[i].buttons = (UINT16)SHORT(src[i].buttons);
		dest[i].latency = src[i].latency;
	}
	return dest;
}

// User has designated that they want
// analog ON, so tell the game to stop
// fudging with it.
static void UserAnalog_OnChange(void)
{
	if (cv_useranalog[0].value)
		CV_SetValue(&cv_analog[0], 1);
	else
		CV_SetValue(&cv_analog[0], 0);
}

static void UserAnalog2_OnChange(void)
{
	if (botingame)
		return;
	if (cv_useranalog[1].value)
		CV_SetValue(&cv_analog[1], 1);
	else
		CV_SetValue(&cv_analog[1], 0);
}

static void Analog_OnChange(void)
{
	if (!cv_cam_dist.string)
		return;

	// cameras are not initialized at this point

	if (!cv_chasecam.value && cv_analog[0].value) {
		CV_SetValue(&cv_analog[0], 0);
		return;
	}

	SendWeaponPref();
}

static void Analog2_OnChange(void)
{
	if (!(splitscreen || botingame) || !cv_cam2_dist.string)
		return;

	// cameras are not initialized at this point

	if (!cv_chasecam2.value && cv_analog[1].value) {
		CV_SetValue(&cv_analog[1], 0);
		return;
	}

	SendWeaponPref2();
}

static void DirectionChar_OnChange(void)
{
	SendWeaponPref();
}

static void DirectionChar2_OnChange(void)
{
	SendWeaponPref2();
}

static void AutoBrake_OnChange(void)
{
	SendWeaponPref();
}

static void AutoBrake2_OnChange(void)
{
	SendWeaponPref2();
}

//
// G_DoLoadLevel
//
void G_DoLoadLevel(boolean resetplayer)
{
	INT32 i;

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();
    freezelevelthinkers = false;
	demosynced = true;

	levelstarttic = gametic; // for time calculation

	if (wipegamestate == GS_LEVEL)
		wipegamestate = -1; // force a wipe

	if (gamestate == GS_INTERMISSION)
		Y_EndIntermission();

	// cleanup
	if (titlemapinaction == TITLEMAP_LOADING)
	{
		if (W_CheckNumForName(G_BuildMapName(gamemap)) == LUMPERROR)
		{
			titlemap = 0; // let's not infinite recursion ok
			Command_ExitGame_f();
			return;
		}

		titlemapinaction = TITLEMAP_RUNNING;
	}
	else
		titlemapinaction = TITLEMAP_OFF;

	G_SetGamestate(GS_LEVEL);
	I_UpdateMouseGrab();

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (resetplayer || (playeringame[i] && players[i].playerstate == PST_DEAD))
			players[i].playerstate = PST_REBORN;
	}

	// Setup the level.
	if (!P_LoadLevel(false, false)) // this never returns false?
	{
		// fail so reset game stuff
		Command_ExitGame_f();
		return;
	}

	P_FindEmerald();

	displayplayer = consoleplayer; // view the guy you are playing
	if (!splitscreen && !botingame)
		secondarydisplayplayer = consoleplayer;

	gameaction = ga_nothing;
#ifdef PARANOIA
	Z_CheckHeap(-2);
#endif

	if (camera.chase)
		P_ResetCamera(&players[displayplayer], &camera);
	if (camera2.chase && splitscreen)
		P_ResetCamera(&players[secondarydisplayplayer], &camera2);

	// clear cmd building stuff
	memset(gamekeydown, 0, sizeof (gamekeydown));
	for (i = 0;i < JOYAXISSET; i++)
	{
		joyxmove[i] = joyymove[i] = 0;
		joy2xmove[i] = joy2ymove[i] = 0;
	}
	G_SetMouseDeltas(0, 0, 1);
	G_SetMouseDeltas(0, 0, 2);

	// clear hud messages remains (usually from game startup)
	CON_ClearHUD();
}

//
// Start the title card.
//
void G_StartTitleCard(void)
{
	ST_stopTitleCard();

	// The title card has been disabled for this map.
	// Oh well.
	if (!G_IsTitleCardAvailable())
	{
		WipeStageTitle = false;
		return;
	}

	// clear the hud
	CON_ClearHUD();

	// prepare status bar
	ST_startTitleCard();

	// start the title card
	WipeStageTitle = (!titlemapinaction);
}

//
// Run the title card before fading in to the level.
//
void G_PreLevelTitleCard(void)
{
#ifndef NOWIPE
	tic_t starttime = I_GetTime();
	tic_t endtime = starttime + (PRELEVELTIME*NEWTICRATERATIO);
	tic_t nowtime = starttime;
	tic_t lasttime = starttime;
	while (nowtime < endtime)
	{
		// draw loop
		while (!((nowtime = I_GetTime()) - lasttime))
		{
			I_Sleep(cv_sleep.value);
			I_UpdateTime(cv_timescale.value);
		}
		lasttime = nowtime;

		ST_runTitleCard();
		ST_preLevelTitleCardDrawer();
		I_FinishUpdate(); // page flip or blit buffer
		NetKeepAlive(); // Prevent timeouts

		if (moviemode)
			M_SaveFrame();
		if (takescreenshot) // Only take screenshots after drawing.
			M_DoScreenShot();
	}
	if (!cv_showhud.value)
		wipestyleflags = WSF_CROSSFADE;
#endif
}

static boolean titlecardforreload = false;

//
// Returns true if the current level has a title card.
//
boolean G_IsTitleCardAvailable(void)
{
	// The current level header explicitly disabled the title card.
	UINT16 titleflag = LF_NOTITLECARDFIRST;

	if (modeattacking != ATTACKING_NONE)
		titleflag = LF_NOTITLECARDRECORDATTACK;
	else if (titlecardforreload)
		titleflag = LF_NOTITLECARDRESPAWN;

	if (mapheaderinfo[gamemap-1]->levelflags & titleflag)
		return false;

	// The current gametype doesn't have a title card.
	if (gametyperules & GTR_NOTITLECARD)
		return false;

	// The current level has no name.
	if (!mapheaderinfo[gamemap-1]->lvlttl[0])
		return false;

	// The title card is available.
	return true;
}

INT32 pausedelay = 0;
boolean pausebreakkey = false;
static INT32 camtoggledelay, camtoggledelay2 = 0;

static boolean ViewpointSwitchResponder(event_t *ev)
{
	// ViewpointSwitch Lua hook.
	UINT8 canSwitchView = 0;

	INT32 direction = 0;
	if (ev->key == KEY_F12 || ev->key == gamecontrol[GC_VIEWPOINTNEXT][0] || ev->key == gamecontrol[GC_VIEWPOINTNEXT][1])
		direction = 1;
	if (ev->key == gamecontrol[GC_VIEWPOINTPREV][0] || ev->key == gamecontrol[GC_VIEWPOINTPREV][1])
		direction = -1;
	// This enabled reverse-iterating with shift+F12, sadly I had to
	// disable this in case your shift key is bound to a control =((
	//if (shiftdown)
	//	direction = -direction;

	// allow spy mode changes even during the demo
	if (!(gamestate == GS_LEVEL && ev->type == ev_keydown && direction != 0))
		return false;

	if (splitscreen || !netgame)
	{
		displayplayer = consoleplayer;
		return false;
	}

	// spy mode
	do
	{
		// Wrap in both directions
		displayplayer += direction;
		displayplayer = (displayplayer + MAXPLAYERS) % MAXPLAYERS;

		if (!playeringame[displayplayer])
			continue;

		// Call ViewpointSwitch hooks here.
		canSwitchView = LUA_HookViewpointSwitch(&players[consoleplayer], &players[displayplayer], false);
		if (canSwitchView == 1) // Set viewpoint to this player
			break;
		else if (canSwitchView == 2) // Skip this player
			continue;

		if (players[displayplayer].spectator)
			continue;

		if (G_GametypeHasTeams())
		{
			if (players[consoleplayer].ctfteam
				&& players[displayplayer].ctfteam != players[consoleplayer].ctfteam)
				continue;
		}
		else if (gametyperules & GTR_HIDEFROZEN)
		{
			if (players[consoleplayer].pflags & PF_TAGIT)
				continue;
		}
		// Other Tag-based gametypes?
		else if (G_TagGametype())
		{
			if (!players[consoleplayer].spectator
				&& (players[consoleplayer].pflags & PF_TAGIT) != (players[displayplayer].pflags & PF_TAGIT))
				continue;
		}
		else if (G_GametypeHasSpectators() && G_RingSlingerGametype())
		{
			if (!players[consoleplayer].spectator)
				continue;
		}

		break;
	} while (displayplayer != consoleplayer);

	// change statusbar also if playing back demo
	if (singledemo)
		ST_changeDemoView();

	return true;
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
boolean G_Responder(event_t *ev)
{
	// any other key pops up menu if in demos
	if (gameaction == ga_nothing && !singledemo &&
		((demoplayback && !modeattacking && !titledemo) || gamestate == GS_TITLESCREEN))
	{
		if (ev->type == ev_keydown && ev->key != 301 && !(gamestate == GS_TITLESCREEN && finalecount < (cv_tutorialprompt.value ? TICRATE : 0)))
		{
			M_StartControlPanel();
			return true;
		}
		return false;
	}
	else if (demoplayback && titledemo)
	{
		// Title demo uses intro responder
		if (F_IntroResponder(ev))
		{
			// stop the title demo
			G_CheckDemoStatus();
			return true;
		}
		return false;
	}

	if (gamestate == GS_LEVEL)
	{
		if (HU_Responder(ev))
			return true; // chat ate the event
		if (AM_Responder(ev))
			return true; // automap ate it
		// map the event (key/mouse/joy) to a gamecontrol
	}
	// Intro
	else if (gamestate == GS_INTRO)
	{
		if (F_IntroResponder(ev))
		{
			D_StartTitle();
			return true;
		}
	}
	else if (gamestate == GS_CUTSCENE)
	{
		if (HU_Responder(ev))
			return true; // chat ate the event

		if (F_CutsceneResponder(ev))
		{
			D_StartTitle();
			return true;
		}
	}
	else if (gamestate == GS_CREDITS || gamestate == GS_ENDING) // todo: keep ending here?
	{
		if (HU_Responder(ev))
			return true; // chat ate the event

		if (F_CreditResponder(ev))
		{
			// Skip credits for everyone
			if (! netgame)
				F_StartGameEvaluation();
			else if (server || IsPlayerAdmin(consoleplayer))
				SendNetXCmd(XD_EXITLEVEL, NULL, 0);
			return true;
		}
	}
	else if (gamestate == GS_CONTINUING)
	{
		if (F_ContinueResponder(ev))
			return true;
	}
	// Demo End
	else if (gamestate == GS_GAMEEND)
		return true;
	else if (gamestate == GS_INTERMISSION || gamestate == GS_EVALUATION)
		if (HU_Responder(ev))
			return true; // chat ate the event

	if (ViewpointSwitchResponder(ev))
		return true;

	// update keys current state
	G_MapEventsToControls(ev);

	switch (ev->type)
	{
		case ev_keydown:
			if (ev->key == gamecontrol[GC_PAUSE][0]
				|| ev->key == gamecontrol[GC_PAUSE][1]
				|| ev->key == KEY_PAUSE)
			{
				if (modeattacking && !demoplayback && (gamestate == GS_LEVEL))
				{
					pausebreakkey = (ev->key == KEY_PAUSE);
					if (menuactive || pausedelay < 0 || leveltime < 2)
						return true;

					if (!cv_instantretry.value && pausedelay < 1+(NEWTICRATE/2))
						pausedelay = 1+(NEWTICRATE/2);
					else if (cv_instantretry.value || ++pausedelay > 1+(NEWTICRATE/2)+(NEWTICRATE/3))
					{
						G_SetModeAttackRetryFlag();
						return true;
					}
					pausedelay++; // counteract subsequent subtraction this frame
				}
				else
				{
					INT32 oldpausedelay = pausedelay;
					pausedelay = (NEWTICRATE/7);
					if (!oldpausedelay)
					{
						// command will handle all the checks for us
						COM_ImmedExecute("pause");
						return true;
					}
				}
			}
			if (ev->key == gamecontrol[GC_CAMTOGGLE][0]
				|| ev->key == gamecontrol[GC_CAMTOGGLE][1])
			{
				if (!camtoggledelay)
				{
					camtoggledelay = NEWTICRATE / 7;
					CV_SetValue(&cv_chasecam, cv_chasecam.value ? 0 : 1);
				}
			}
			if (ev->key == gamecontrolbis[GC_CAMTOGGLE][0]
				|| ev->key == gamecontrolbis[GC_CAMTOGGLE][1])
			{
				if (!camtoggledelay2)
				{
					camtoggledelay2 = NEWTICRATE / 7;
					CV_SetValue(&cv_chasecam2, cv_chasecam2.value ? 0 : 1);
				}
			}
			return true;

		case ev_keyup:
			return false; // always let key up events filter down

		case ev_mouse:
			return true; // eat events

		case ev_joystick:
			return true; // eat events

		case ev_joystick2:
			return true; // eat events


		default:
			break;
	}

	return false;
}

//
// G_LuaResponder
// Let Lua handle key events.
//
boolean G_LuaResponder(event_t *ev)
{
	boolean cancelled = false;

	if (ev->type == ev_keydown)
	{
		cancelled = LUA_HookKey(ev, HOOK(KeyDown));
		LUA_InvalidateUserdata(ev);
	}
	else if (ev->type == ev_keyup)
	{
		cancelled = LUA_HookKey(ev, HOOK(KeyUp));
		LUA_InvalidateUserdata(ev);
	}

	return cancelled;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker(boolean run)
{
	UINT32 i;
	INT32 buf;

	// Bot players queued for removal
	for (i = MAXPLAYERS-1; i != UINT32_MAX; i--)
	{
		if (playeringame[i] && players[i].removing)
		{
			CL_RemovePlayer(i, i);
			if (netgame)
			{
				char kickmsg[256];

				strcpy(kickmsg, M_GetText("\x82*Bot %s has been removed"));
				strcpy(kickmsg, va(kickmsg, player_names[i], i));
				HU_AddChatText(kickmsg, false);
			}
		}
	}

	// see also SCR_DisplayMarathonInfo
	if ((marathonmode & (MA_INIT|MA_INGAME)) == MA_INGAME && gamestate == GS_LEVEL)
		marathontime++;

	P_MapStart();
	// do player reborns if needed
	if (gamestate == GS_LEVEL)
	{
		// Or, alternatively, retry.
		if (!(netgame || multiplayer) && G_GetRetryFlag())
		{
			G_ClearRetryFlag();

			if (modeattacking)
			{
				pausedelay = INT32_MIN;
				M_ModeAttackRetry(0);
			}
			else
			{
				// Costs a life to retry ... unless the player in question is dead already, or you haven't even touched the first starpost in marathon run.
				if (marathonmode && gamemap == spmarathon_start && !players[consoleplayer].starposttime)
				{
					player_t *p = &players[consoleplayer];
					marathonmode |= MA_INIT;
					marathontime = 0;

					numgameovers = tokenlist = token = 0;
					countdown = countdown2 = exitfadestarted = 0;

					p->playerstate = PST_REBORN;
					p->starpostx = p->starposty = p->starpostz = 0;

					p->lives = startinglivesbalance[0];
					p->continues = 1;

					p->score = p->recordscore = 0;

					// The latter two should clear by themselves, but just in case
					p->pflags &= ~(PF_TAGIT|PF_GAMETYPEOVER|PF_FULLSTASIS);

					// Clear cheatcodes too, just in case.
					p->pflags &= ~(PF_GODMODE|PF_NOCLIP|PF_INVIS);

					p->xtralife = 0;

					// Reset unlockable triggers
					unlocktriggers = 0;

					emeralds = 0;

					memset(&luabanks, 0, sizeof(luabanks));
				}
				else if (G_GametypeUsesLives() && players[consoleplayer].playerstate == PST_LIVE && players[consoleplayer].lives != INFLIVES)
					players[consoleplayer].lives -= 1;

				G_DoReborn(consoleplayer);
			}
		}

		for (i = 0; i < MAXPLAYERS; i++)
			if (playeringame[i] && players[i].playerstate == PST_REBORN)
				G_DoReborn(i);
	}
	P_MapEnd();

	// do things to change the game state
	while (gameaction != ga_nothing)
		switch (gameaction)
		{
			case ga_completed: G_DoCompleted(); break;
			case ga_startcont: G_DoStartContinue(); break;
			case ga_continued: G_DoContinued(); break;
			case ga_worlddone: G_DoWorldDone(); break;
			case ga_nothing: break;
			default: I_Error("gameaction = %d\n", gameaction);
		}

	buf = gametic % BACKUPTICS;

	// Generate ticcmds for bots FIRST, then copy received ticcmds for players.
	// This emulates pre-2.2.10 behaviour where the bot referenced their leader's last copied ticcmd,
	// which is desirable because P_PlayerThink can override inputs (e.g. while PF_STASIS is applied or in a waterslide),
	// and the bot AI needs to respect that.
#define ISHUMAN (players[i].bot == BOT_NONE || players[i].bot == BOT_2PHUMAN)
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !ISHUMAN) // Less work is required if we're building a bot ticcmd.
		{
			players[i].lastbuttons = players[i].cmd.buttons; // Save last frame's button readings
			B_BuildTiccmd(&players[i], &players[i].cmd);

			// Since bot TicCmd is pre-determined for both the client and server, the latency and packet checks are simplified.
			players[i].cmd.latency = 0;
			P_SetPlayerAngle(&players[i], players[i].cmd.angleturn << 16);
		}
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && ISHUMAN)
		{
			players[i].lastbuttons = players[i].cmd.buttons; // Save last frame's button readings
			G_CopyTiccmd(&players[i].cmd, &netcmds[buf][i], 1);

			// Use the leveltime sent in the player's ticcmd to determine control lag
			players[i].cmd.latency = min(((leveltime & 0xFF) - players[i].cmd.latency) & 0xFF, MAXPREDICTTICS-1);

			// Do angle adjustments.
			players[i].angleturn += players[i].cmd.angleturn - players[i].oldrelangleturn;
			players[i].oldrelangleturn = players[i].cmd.angleturn;
			if (P_ControlStyle(&players[i]) == CS_LMAOGALOG)
				P_ForceLocalAngle(&players[i], players[i].angleturn << 16);
			else
				players[i].cmd.angleturn = (players[i].angleturn & ~TICCMD_RECEIVED) | (players[i].cmd.angleturn & TICCMD_RECEIVED);
		}
	}
#undef ISHUMAN

	// do main actions
	switch (gamestate)
	{
		case GS_LEVEL:
			if (titledemo)
				F_TitleDemoTicker();
			P_Ticker(run); // tic the game
			ST_Ticker(run);
			F_TextPromptTicker();
			AM_Ticker();
			HU_Ticker();

			break;

		case GS_INTERMISSION:
			if (run)
				Y_Ticker();
			HU_Ticker();
			break;

		case GS_TIMEATTACK:
			if (run)
				F_MenuPresTicker();
			break;

		case GS_INTRO:
			if (run)
				F_IntroTicker();
			break;

		case GS_ENDING:
			if (run)
				F_EndingTicker();
			HU_Ticker();
			break;

		case GS_CUTSCENE:
			if (run)
				F_CutsceneTicker();
			HU_Ticker();
			break;

		case GS_GAMEEND:
			if (run)
				F_GameEndTicker();
			break;

		case GS_EVALUATION:
			if (run)
				F_GameEvaluationTicker();
			HU_Ticker();
			break;

		case GS_CONTINUING:
			if (run)
				F_ContinueTicker();
			break;

		case GS_CREDITS:
			if (run)
				F_CreditTicker();
			HU_Ticker();
			break;

		case GS_TITLESCREEN:
			if (titlemapinaction)
				P_Ticker(run);
			if (run)
				F_MenuPresTicker();
			F_TitleScreenTicker(run);
			break;

		case GS_WAITINGPLAYERS:
			if (netgame)
				F_WaitingPlayersTicker();
			HU_Ticker();
			break;

		case GS_DEDICATEDSERVER:
		case GS_NULL:
			break; // do nothing
	}

	if (run)
	{
		if (pausedelay && pausedelay != INT32_MIN)
		{
			if (pausedelay > 0)
				pausedelay--;
			else
				pausedelay++;
		}

		if (camtoggledelay)
			camtoggledelay--;

		if (camtoggledelay2)
			camtoggledelay2--;

		if (gametic % NAMECHANGERATE == 0)
		{
			memset(player_name_changes, 0, sizeof player_name_changes);
		}
	}
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
static inline void G_PlayerFinishLevel(INT32 player)
{
	player_t *p;

	p = &players[player];

	memset(p->powers, 0, sizeof (p->powers));
	p->ringweapons = 0;

	p->mo->flags2 &= ~MF2_SHADOW; // cancel invisibility
	P_FlashPal(p, 0, 0); // Resets
	p->starpostscale = 0;
	p->starpostangle = 0;
	p->starposttime = 0;
	p->starpostx = 0;
	p->starposty = 0;
	p->starpostz = 0;
	p->starpostnum = 0;

	if (rendermode == render_soft)
		V_SetPaletteLump(GetPalette()); // Reset the palette
}

//
// G_PlayerReborn
// Called after a player dies. Almost everything is cleared and initialized.
//
void G_PlayerReborn(INT32 player, boolean betweenmaps)
{
	player_t *p;
	INT32 score;
	INT32 lives;
	INT32 continues;
	fixed_t camerascale;
	fixed_t shieldscale;
	UINT8 charability;
	UINT8 charability2;
	fixed_t normalspeed;
	fixed_t runspeed;
	UINT8 thrustfactor;
	UINT8 accelstart;
	UINT8 acceleration;
	INT32 charflags;
	INT32 pflags;
	UINT32 thokitem;
	UINT32 spinitem;
	UINT32 revitem;
	UINT32 followitem;
	fixed_t actionspd;
	fixed_t mindash;
	fixed_t maxdash;
	INT32 ctfteam;
	INT32 starposttime;
	INT16 starpostx;
	INT16 starposty;
	INT16 starpostz;
	INT32 starpostnum;
	INT32 starpostangle;
	fixed_t starpostscale;
	fixed_t jumpfactor;
	fixed_t height;
	fixed_t spinheight;
	INT32 exiting;
	tic_t dashmode;
	INT16 numboxes;
	INT16 totalring;
	UINT8 laps;
	UINT8 mare;
	UINT16 skincolor;
	UINT8 skin;
	UINT32 availabilities;
	tic_t jointime;
	tic_t quittime;
	boolean spectator;
	boolean outofcoop;
	boolean removing;
	boolean muted;
	INT16 bot;
	SINT8 pity;
	INT16 rings;
	INT16 spheres;
	INT16 playerangleturn;
	INT16 oldrelangleturn;

	score = players[player].score;
	lives = players[player].lives;
	continues = players[player].continues;
	ctfteam = players[player].ctfteam;
	exiting = players[player].exiting;
	jointime = players[player].jointime;
	quittime = players[player].quittime;
	spectator = players[player].spectator;
	outofcoop = players[player].outofcoop;
	removing = players[player].removing;
	muted = players[player].muted;
	pflags = (players[player].pflags & (PF_FLIPCAM|PF_ANALOGMODE|PF_DIRECTIONCHAR|PF_AUTOBRAKE|PF_TAGIT|PF_GAMETYPEOVER));
	playerangleturn = players[player].angleturn;
	oldrelangleturn = players[player].oldrelangleturn;

	if (!betweenmaps)
		pflags |= (players[player].pflags & PF_FINISHED);

	// As long as we're not in multiplayer, carry over cheatcodes from map to map
	if (!(netgame || multiplayer))
		pflags |= (players[player].pflags & (PF_GODMODE|PF_NOCLIP|PF_INVIS));

	dashmode = players[player].dashmode;

	numboxes = players[player].numboxes;
	laps = players[player].laps;
	totalring = players[player].totalring;

	skincolor = players[player].skincolor;
	skin = players[player].skin;
	availabilities = players[player].availabilities;
	camerascale = players[player].camerascale;
	shieldscale = players[player].shieldscale;
	charability = players[player].charability;
	charability2 = players[player].charability2;
	normalspeed = players[player].normalspeed;
	runspeed = players[player].runspeed;
	thrustfactor = players[player].thrustfactor;
	accelstart = players[player].accelstart;
	acceleration = players[player].acceleration;
	charflags = players[player].charflags;

	starposttime = players[player].starposttime;
	starpostx = players[player].starpostx;
	starposty = players[player].starposty;
	starpostz = players[player].starpostz;
	starpostnum = players[player].starpostnum;
	starpostangle = players[player].starpostangle;
	starpostscale = players[player].starpostscale;
	jumpfactor = players[player].jumpfactor;
	height = players[player].height;
	spinheight = players[player].spinheight;
	thokitem = players[player].thokitem;
	spinitem = players[player].spinitem;
	revitem = players[player].revitem;
	followitem = players[player].followitem;
	actionspd = players[player].actionspd;
	mindash = players[player].mindash;
	maxdash = players[player].maxdash;

	mare = players[player].mare;
	bot = players[player].bot;
	pity = players[player].pity;

	if (betweenmaps || !G_IsSpecialStage(gamemap))
	{
		rings = (ultimatemode ? 0 : mapheaderinfo[gamemap-1]->startrings);
		spheres = 0;
	}
	else
	{
		rings = players[player].rings;
		spheres = players[player].spheres;
	}

	p = &players[player];
	memset(p, 0, sizeof (*p));

	p->score = score;
	p->lives = lives;
	p->continues = continues;
	p->pflags = pflags;
	p->ctfteam = ctfteam;
	p->jointime = jointime;
	p->quittime = quittime;
	p->spectator = spectator;
	p->outofcoop = outofcoop;
	p->removing = removing;
	p->muted = muted;
	p->angleturn = playerangleturn;
	p->oldrelangleturn = oldrelangleturn;

	// save player config truth reborn
	p->skincolor = skincolor;
	p->skin = skin;
	p->availabilities = availabilities;
	p->camerascale = camerascale;
	p->shieldscale = shieldscale;
	p->charability = charability;
	p->charability2 = charability2;
	p->normalspeed = normalspeed;
	p->runspeed = runspeed;
	p->thrustfactor = thrustfactor;
	p->accelstart = accelstart;
	p->acceleration = acceleration;
	p->charflags = charflags;
	p->thokitem = thokitem;
	p->spinitem = spinitem;
	p->revitem = revitem;
	p->followitem = followitem;
	p->actionspd = actionspd;
	p->mindash = mindash;
	p->maxdash = maxdash;

	p->starposttime = starposttime;
	p->starpostx = starpostx;
	p->starposty = starposty;
	p->starpostz = starpostz;
	p->starpostnum = starpostnum;
	p->starpostangle = starpostangle;
	p->starpostscale = starpostscale;
	p->jumpfactor = jumpfactor;
	p->height = height;
	p->spinheight = spinheight;
	p->exiting = exiting;

	p->dashmode = dashmode;

	p->numboxes = numboxes;
	p->laps = laps;
	p->totalring = totalring;

	p->mare = mare;
	if (bot == BOT_2PHUMAN)
		p->bot = BOT_2PAI; // reset to AI-controlled
	else
		p->bot = bot;
	p->pity = pity;
	p->rings = rings;
	p->spheres = spheres;

	// Don't do anything immediately
	p->pflags |= PF_SPINDOWN;
	p->pflags |= PF_ATTACKDOWN;
	p->pflags |= PF_JUMPDOWN;

	p->playerstate = PST_LIVE;
	p->panim = PA_IDLE; // standing animation

	//if ((netgame || multiplayer) && !p->spectator) -- moved into P_SpawnPlayer to account for forced changes there
		//p->powers[pw_flashing] = flashingtics-1; // Babysitting deterrent

	if (betweenmaps)
		return;

	if (p-players == consoleplayer)
	{
		if (mapmusflags & MUSIC_RELOADRESET)
		{
			strncpy(mapmusname, mapheaderinfo[gamemap-1]->musname, 7);
			mapmusname[6] = 0;
			mapmusflags = (mapheaderinfo[gamemap-1]->mustrack & MUSIC_TRACKMASK);
			mapmusposition = mapheaderinfo[gamemap-1]->muspos;
		}

		// This is in S_Start, but this was not here previously.
		// if (RESETMUSIC)
		// 	S_StopMusic();
		S_ChangeMusicEx(mapmusname, mapmusflags, true, mapmusposition, 0, 0);
	}

	if (gametyperules & GTR_EMERALDHUNT)
		P_FindEmerald(); // scan for emeralds to hunt for

	// If NiGHTS, find lowest mare to start with.
	p->mare = P_FindLowestMare();

	CONS_Debug(DBG_NIGHTS, M_GetText("Current mare is %d\n"), p->mare);

	if (p->mare == 255)
		p->mare = 0;
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
static boolean G_CheckSpot(INT32 playernum, mapthing_t *mthing)
{
	fixed_t x;
	fixed_t y;
	INT32 i;

	// maybe there is no player start
	if (!mthing)
		return false;

	if (!players[playernum].mo)
	{
		// first spawn of level
		for (i = 0; i < playernum; i++)
			if (playeringame[i] && players[i].mo
				&& players[i].mo->x == mthing->x << FRACBITS
				&& players[i].mo->y == mthing->y << FRACBITS)
			{
				return false;
			}
		return true;
	}

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;

	if (!P_CheckPosition(players[playernum].mo, x, y))
		return false;

	return true;
}

//
// G_SpawnPlayer
// Spawn a player in a spot appropriate for the gametype --
// or a not-so-appropriate spot, if it initially fails
// due to a lack of starts open or something.
//
void G_SpawnPlayer(INT32 playernum)
{
	if (!playeringame[playernum])
		return;

	P_SpawnPlayer(playernum);
	G_MovePlayerToSpawnOrStarpost(playernum);
	LUA_HookPlayer(&players[playernum], HOOK(PlayerSpawn)); // Lua hook for player spawning :)
}

void G_MovePlayerToSpawnOrStarpost(INT32 playernum)
{
	if (players[playernum].starposttime)
		P_MovePlayerToStarpost(playernum);
	else
		P_MovePlayerToSpawn(playernum, G_FindMapStart(playernum));

	R_ResetMobjInterpolationState(players[playernum].mo);

	if (players[playernum].bot) // don't reset the camera for bots
		return;

	if (playernum == consoleplayer)
		P_ResetCamera(&players[playernum], &camera);
	else if (playernum == secondarydisplayplayer)
		P_ResetCamera(&players[playernum], &camera2);
}

mapthing_t *G_FindCTFStart(INT32 playernum)
{
	INT32 i,j;

	if (!numredctfstarts && !numbluectfstarts) //why even bother, eh?
	{
		if ((gametyperules & GTR_TEAMFLAGS) && (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer)))
			CONS_Alert(CONS_WARNING, M_GetText("No CTF starts in this map!\n"));
		return NULL;
	}

	if ((!players[playernum].ctfteam && numredctfstarts && (!numbluectfstarts || P_RandomChance(FRACUNIT/2))) || players[playernum].ctfteam == 1) //red
	{
		if (!numredctfstarts)
		{
			if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
				CONS_Alert(CONS_WARNING, M_GetText("No Red Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(numredctfstarts);
			if (G_CheckSpot(playernum, redctfstarts[i]))
				return redctfstarts[i];
		}

		if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Red Team starts!\n"));
		return NULL;
	}
	else if (!players[playernum].ctfteam || players[playernum].ctfteam == 2) //blue
	{
		if (!numbluectfstarts)
		{
			if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
				CONS_Alert(CONS_WARNING, M_GetText("No Blue Team starts in this map!\n"));
			return NULL;
		}

		for (j = 0; j < 32; j++)
		{
			i = P_RandomKey(numbluectfstarts);
			if (G_CheckSpot(playernum, bluectfstarts[i]))
				return bluectfstarts[i];
		}
		if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Blue Team starts!\n"));
		return NULL;
	}
	//should never be reached but it gets stuff to shut up
	return NULL;
}

mapthing_t *G_FindMatchStart(INT32 playernum)
{
	INT32 i, j;

	if (numdmstarts)
	{
		for (j = 0; j < 64; j++)
		{
			i = P_RandomKey(numdmstarts);
			if (G_CheckSpot(playernum, deathmatchstarts[i]))
				return deathmatchstarts[i];
		}
		if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
			CONS_Alert(CONS_WARNING, M_GetText("Could not spawn at any Deathmatch starts!\n"));
		return NULL;
	}

	if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
		CONS_Alert(CONS_WARNING, M_GetText("No Deathmatch starts in this map!\n"));
	return NULL;
}

mapthing_t *G_FindCoopStart(INT32 playernum)
{
	if (numcoopstarts)
	{
		//if there's 6 players in a map with 3 player starts, this spawns them 1/2/3/1/2/3.
		if (G_CheckSpot(playernum, playerstarts[playernum % numcoopstarts]))
			return playerstarts[playernum % numcoopstarts];

		//Don't bother checking to see if the player 1 start is open.
		//Just spawn there.
		return playerstarts[0];
	}

	if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
		CONS_Alert(CONS_WARNING, M_GetText("No Co-op starts in this map!\n"));
	return NULL;
}

// Find a Co-op start, or fallback into other types of starts.
static inline mapthing_t *G_FindCoopStartOrFallback(INT32 playernum)
{
	mapthing_t *spawnpoint = NULL;
	if (!(spawnpoint = G_FindCoopStart(playernum)) // find a Co-op start
	&& !(spawnpoint = G_FindMatchStart(playernum))) // find a DM start
		spawnpoint = G_FindCTFStart(playernum); // fallback
	return spawnpoint;
}

// Find a Match start, or fallback into other types of starts.
static inline mapthing_t *G_FindMatchStartOrFallback(INT32 playernum)
{
	mapthing_t *spawnpoint = NULL;
	if (!(spawnpoint = G_FindMatchStart(playernum)) // find a DM start
	&& !(spawnpoint = G_FindCTFStart(playernum))) // find a CTF start
		spawnpoint = G_FindCoopStart(playernum); // fallback
	return spawnpoint;
}

mapthing_t *G_FindMapStart(INT32 playernum)
{
	mapthing_t *spawnpoint;

	if (!playeringame[playernum])
		return NULL;

	// -- Spectators --
	// Order in platform gametypes: Coop->DM->CTF
	// And, with deathmatch starts: DM->CTF->Coop
	if (players[playernum].spectator)
	{
		// In platform gametypes, spawn in Co-op starts first
		// Overriden by GTR_DEATHMATCHSTARTS.
		if (G_PlatformGametype() && !(gametyperules & GTR_DEATHMATCHSTARTS))
			spawnpoint = G_FindCoopStartOrFallback(playernum);
		else
			spawnpoint = G_FindMatchStartOrFallback(playernum);
	}

	// -- CTF --
	// Order: CTF->DM->Coop
	else if ((gametyperules & (GTR_TEAMFLAGS|GTR_TEAMS)) && players[playernum].ctfteam)
	{
		if (!(spawnpoint = G_FindCTFStart(playernum)) // find a CTF start
		&& !(spawnpoint = G_FindMatchStart(playernum))) // find a DM start
			spawnpoint = G_FindCoopStart(playernum); // fallback
	}

	// -- DM/Tag/CTF-spectator/etc --
	// Order: DM->CTF->Coop
	else if (G_TagGametype() ? (!(players[playernum].pflags & PF_TAGIT)) : (gametyperules & GTR_DEATHMATCHSTARTS))
		spawnpoint = G_FindMatchStartOrFallback(playernum);

	// -- Other game modes --
	// Order: Coop->DM->CTF
	else
		spawnpoint = G_FindCoopStartOrFallback(playernum);

	//No spawns found. ANYWHERE.
	if (!spawnpoint)
	{
		if (nummapthings)
		{
			if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the first mapthing!\n"));
			spawnpoint = &mapthings[0];
		}
		else
		{
			if (playernum == consoleplayer || (splitscreen && playernum == secondarydisplayplayer))
				CONS_Alert(CONS_ERROR, M_GetText("No player spawns found, spawning at the origin!\n"));
		}
	}

	return spawnpoint;
}

// Go back through all the projectiles and remove all references to the old
// player mobj, replacing them with the new one.
void G_ChangePlayerReferences(mobj_t *oldmo, mobj_t *newmo)
{
	thinker_t *th;
	mobj_t *mo2;

	I_Assert((oldmo != NULL) && (newmo != NULL));

	// scan all thinkers
	for (th = thlist[THINK_MOBJ].next; th != &thlist[THINK_MOBJ]; th = th->next)
	{
		if (th->removing)
			continue;

		mo2 = (mobj_t *)th;

		if (!(mo2->flags & MF_MISSILE))
			continue;

		if (mo2->target == oldmo)
		{
			P_SetTarget(&mo2->target, newmo);
			mo2->flags2 |= MF2_BEYONDTHEGRAVE; // this mobj belongs to a player who has reborn
		}
	}
}

//
// G_DoReborn
//
void G_DoReborn(INT32 playernum)
{
	player_t *player = &players[playernum];
	boolean resetlevel = false;
	INT32 i;

	if (modeattacking)
	{
		M_EndModeAttackRun();
		return;
	}

	// Make sure objectplace is OFF when you first start the level!
	OP_ResetObjectplace();
    freezelevelthinkers = false;

	// Tailsbot
	if (player->bot == BOT_2PAI || player->bot == BOT_2PHUMAN)
	{ // Bots respawn next to their master.
		mobj_t *oldmo = NULL;

		// first dissasociate the corpse
		if (player->mo)
		{
			oldmo = player->mo;
			// Don't leave your carcass stuck 10-billion feet in the ground!
			P_RemoveMobj(player->mo);
		}

		B_RespawnBot(playernum);
		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);

		return;
	}

	// Additional players (e.g. independent bots) in Single Player
	if (playernum != consoleplayer && !(netgame || multiplayer))
	{
		mobj_t *oldmo = NULL;
		// Do nothing if out of lives
		if (player->lives <= 0)
			return;

		// Otherwise do respawn, starting by removing the player object
		if (player->mo)
		{
			oldmo = player->mo;
			P_RemoveMobj(player->mo);
		}
		// Do spawning
		G_SpawnPlayer(playernum);
		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);

		return; //Exit function to avoid proccing other SP related mechanics
	}

	if (countdowntimeup || (!(netgame || multiplayer) && (gametyperules & GTR_CAMPAIGN)))
		resetlevel = true;
	else if ((G_GametypeUsesCoopLives() || G_GametypeUsesCoopStarposts()) && (netgame || multiplayer) && !G_IsSpecialStage(gamemap))
	{
		boolean notgameover = true;

		if (G_GametypeUsesCoopLives() && (cv_cooplives.value != 0 && player->lives <= 0)) // consider game over first
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;
				if (players[i].exiting || players[i].lives > 0)
					break;
			}

			if (i == MAXPLAYERS)
			{
				notgameover = false;
				if (!countdown2)
				{
					// They're dead, Jim.
					//nextmapoverride = spstage_start;
					nextmapoverride = gamemap;
					countdown2 = TICRATE;
					skipstats = 2;

					for (i = 0; i < MAXPLAYERS; i++)
					{
						if (playeringame[i])
							players[i].score = 0;
					}

					//emeralds = 0;
					tokenbits = 0;
					tokenlist = 0;
					token = 0;
				}
			}
		}

		if (G_GametypeUsesCoopStarposts() && (notgameover && cv_coopstarposts.value == 2))
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;

				if (players[i].playerstate != PST_DEAD && !players[i].spectator && players[i].mo && players[i].mo->health)
					break;
			}
			if (i == MAXPLAYERS)
				resetlevel = true;
		}
	}

	if (resetlevel)
	{
		// Don't give completion emblems for reloading the level...
		stagefailed = true;

		// reload the level from scratch
		if (countdowntimeup)
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;
				players[i].recordscore = 0;
				players[i].starpostscale = 0;
				players[i].starpostangle = 0;
				players[i].starposttime = 0;
				players[i].starpostx = 0;
				players[i].starposty = 0;
				players[i].starpostz = 0;
				players[i].starpostnum = 0;
			}
		}
		if (!countdowntimeup && (mapheaderinfo[gamemap-1]->levelflags & LF_NORELOAD) && !(marathonmode & MA_INIT))
		{
			P_RespawnThings();

			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;
				players[i].playerstate = PST_REBORN;
				P_ClearStarPost(players[i].starpostnum);
			}

			// Do a wipe
			wipegamestate = -1;
			wipestyleflags = WSF_CROSSFADE;

			if (camera.chase)
				P_ResetCamera(&players[displayplayer], &camera);
			if (camera2.chase && splitscreen)
				P_ResetCamera(&players[secondarydisplayplayer], &camera2);

			// clear cmd building stuff
			memset(gamekeydown, 0, sizeof (gamekeydown));
			for (i = 0; i < JOYAXISSET; i++)
			{
				joyxmove[i] = joyymove[i] = 0;
				joy2xmove[i] = joy2ymove[i] = 0;
			}
			G_SetMouseDeltas(0, 0, 1);
			G_SetMouseDeltas(0, 0, 2);

			// clear hud messages remains (usually from game startup)
			CON_ClearHUD();

			// Starpost support
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;
				G_SpawnPlayer(i);
			}

			// restore time in netgame (see also p_setup.c)
			if ((netgame || multiplayer) && G_GametypeUsesCoopStarposts() && cv_coopstarposts.value == 2)
			{
				// is this a hack? maybe
				tic_t maxstarposttime = 0;
				for (i = 0; i < MAXPLAYERS; i++)
				{
					if (playeringame[i] && players[i].starposttime > maxstarposttime)
						maxstarposttime = players[i].starposttime;
				}
				leveltime = maxstarposttime;
			}
		}
		else
		{
			LUA_HookInt(gamemap, HOOK(MapChange));
			titlecardforreload = true;
			G_DoLoadLevel(true);
			titlecardforreload = false;
			if (metalrecording)
				G_BeginMetal();
			return;
		}
	}
	else
	{
		// respawn at the start
		mobj_t *oldmo = NULL;

		// Not resetting map, so return to level music
		if (!countdown2
		&& player->lives <= 0
		&& cv_cooplives.value == 1) // not allowed for life steal because no way to come back from zero group lives without addons, which should call this anyways
			P_RestoreMultiMusic(player);

		// first dissasociate the corpse
		if (player->mo)
		{
			oldmo = player->mo;
			// Don't leave your carcass stuck 10-billion feet in the ground!
			P_RemoveMobj(player->mo);
		}

		G_SpawnPlayer(playernum);
		if (oldmo)
			G_ChangePlayerReferences(oldmo, players[playernum].mo);
	}
}

void G_AddPlayer(INT32 playernum)
{
	INT32 countplayers = 0, notexiting = 0;

	player_t *p = &players[playernum];

	// Go through the current players and make sure you have the latest starpost set
	if (G_PlatformGametype() && (netgame || multiplayer))
	{
		INT32 i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
				continue;

			if (players[i].bot == BOT_2PAI || players[i].bot == BOT_2PHUMAN) // ignore dumb, stupid tails
				continue;

			countplayers++;

			if (!players[i].exiting)
				notexiting++;

			if (!(cv_coopstarposts.value && G_GametypeUsesCoopStarposts() && (p->starpostnum < players[i].starpostnum)))
				continue;

			p->starpostscale = players[i].starpostscale;
			p->starposttime = players[i].starposttime;
			p->starpostx = players[i].starpostx;
			p->starposty = players[i].starposty;
			p->starpostz = players[i].starpostz;
			p->starpostangle = players[i].starpostangle;
			p->starpostnum = players[i].starpostnum;
		}
	}

	p->playerstate = PST_REBORN;

	p->height = skins[p->skin]->height;

	if (G_GametypeUsesLives() || ((netgame || multiplayer) && (gametyperules & GTR_FRIENDLY)))
		p->lives = cv_startinglives.value;

	if ((countplayers && !notexiting) || G_IsSpecialStage(gamemap))
		P_DoPlayerExit(p, false);
}

boolean G_EnoughPlayersFinished(void)
{
	UINT8 numneeded = (G_IsSpecialStage(gamemap) ? 4 : cv_playersforexit.value);
	INT32 total = 0;
	INT32 exiting = 0;
	INT32 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || players[i].bot)
			continue;
		if (players[i].quittime > 30 * TICRATE)
			continue;
		if (players[i].lives <= 0)
			continue;

		total++;
		if ((players[i].pflags & PF_FINISHED) || players[i].exiting)
			exiting++;
	}

	if (exiting)
		return exiting * 4 / total >= numneeded;
	else
		return false;
}

void G_ExitLevel(void)
{
	if (gamestate == GS_LEVEL)
	{
		gameaction = ga_completed;
		lastdraw = true;

		// If you want your teams scrambled on map change, start the process now.
		// The teams will scramble at the start of the next round.
		if (cv_scrambleonchange.value && G_GametypeHasTeams())
		{
			if (server)
				CV_SetValue(&cv_teamscramble, cv_scrambleonchange.value);
		}

		if (!(gametyperules & (GTR_FRIENDLY|GTR_CAMPAIGN)))
			CONS_Printf(M_GetText("The round has ended.\n"));

		// Remove CEcho text on round end.
		HU_ClearCEcho();
	}
	else if (gamestate == GS_ENDING)
	{
		F_StartCredits();
	}
	else if (gamestate == GS_CREDITS)
	{
		F_StartGameEvaluation();
	}
}

// See also the enum GameType in doomstat.h
const char *Gametype_Names[NUMGAMETYPES] =
{
	"Co-op", // GT_COOP
	"Competition", // GT_COMPETITION
	"Race", // GT_RACE

	"Match", // GT_MATCH
	"Team Match", // GT_TEAMMATCH

	"Tag", // GT_TAG
	"Hide & Seek", // GT_HIDEANDSEEK

	"CTF" // GT_CTF
};

// For dehacked
const char *Gametype_ConstantNames[NUMGAMETYPES] =
{
	"GT_COOP", // GT_COOP
	"GT_COMPETITION", // GT_COMPETITION
	"GT_RACE", // GT_RACE

	"GT_MATCH", // GT_MATCH
	"GT_TEAMMATCH", // GT_TEAMMATCH

	"GT_TAG", // GT_TAG
	"GT_HIDEANDSEEK", // GT_HIDEANDSEEK

	"GT_CTF" // GT_CTF
};

// Gametype rules
UINT32 gametypedefaultrules[NUMGAMETYPES] =
{
	// Co-op
	GTR_CAMPAIGN|GTR_LIVES|GTR_FRIENDLY|GTR_SPAWNENEMIES|GTR_ALLOWEXIT|GTR_EMERALDHUNT|GTR_EMERALDTOKENS|GTR_SPECIALSTAGES|GTR_CUTSCENES,
	// Competition
	GTR_RACE|GTR_LIVES|GTR_SPAWNENEMIES|GTR_EMERALDTOKENS|GTR_SPAWNINVUL|GTR_ALLOWEXIT,
	// Race
	GTR_RACE|GTR_SPAWNENEMIES|GTR_SPAWNINVUL|GTR_ALLOWEXIT,

	// Match
	GTR_RINGSLINGER|GTR_FIRSTPERSON|GTR_SPECTATORS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_POWERSTONES|GTR_DEATHMATCHSTARTS|GTR_SPAWNINVUL|GTR_RESPAWNDELAY|GTR_PITYSHIELD|GTR_DEATHPENALTY,
	// Team Match
	GTR_RINGSLINGER|GTR_FIRSTPERSON|GTR_SPECTATORS|GTR_TEAMS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_DEATHMATCHSTARTS|GTR_SPAWNINVUL|GTR_RESPAWNDELAY|GTR_PITYSHIELD,

	// Tag
	GTR_RINGSLINGER|GTR_FIRSTPERSON|GTR_TAG|GTR_SPECTATORS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_STARTCOUNTDOWN|GTR_BLINDFOLDED|GTR_DEATHMATCHSTARTS|GTR_SPAWNINVUL|GTR_RESPAWNDELAY,
	// Hide and Seek
	GTR_RINGSLINGER|GTR_FIRSTPERSON|GTR_TAG|GTR_SPECTATORS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_STARTCOUNTDOWN|GTR_HIDEFROZEN|GTR_BLINDFOLDED|GTR_DEATHMATCHSTARTS|GTR_SPAWNINVUL|GTR_RESPAWNDELAY,

	// CTF
	GTR_RINGSLINGER|GTR_FIRSTPERSON|GTR_SPECTATORS|GTR_TEAMS|GTR_TEAMFLAGS|GTR_POINTLIMIT|GTR_TIMELIMIT|GTR_OVERTIME|GTR_POWERSTONES|GTR_DEATHMATCHSTARTS|GTR_SPAWNINVUL|GTR_RESPAWNDELAY|GTR_PITYSHIELD,
};

//
// Sets a new gametype.
//
void G_SetGametype(INT16 gtype)
{
	gametype = gtype;
	gametyperules = gametypedefaultrules[gametype];
}

//
// G_AddGametype
//
// Add a gametype. Returns the new gametype number.
//
INT16 G_AddGametype(UINT32 rules)
{
	INT16 newgtype = gametypecount;
	gametypecount++;

	// Set gametype rules.
	gametypedefaultrules[newgtype] = rules;
	Gametype_Names[newgtype] = "???";

	// Update gametype_cons_t accordingly.
	G_UpdateGametypeSelections();

	return newgtype;
}

//
// G_AddGametypeConstant
//
// Self-explanatory. Filters out "bad" characters.
//
void G_AddGametypeConstant(INT16 gtype, const char *newgtconst)
{
	size_t r = 0; // read
	size_t w = 0; // write
	char *gtconst = Z_Calloc(strlen(newgtconst) + 4, PU_STATIC, NULL);
	char *tmpconst = Z_Calloc(strlen(newgtconst) + 1, PU_STATIC, NULL);

	// Copy the gametype name.
	strcpy(tmpconst, newgtconst);

	// Make uppercase.
	strupr(tmpconst);

	// Prepare to write the new constant string now.
	strcpy(gtconst, "GT_");

	// Remove characters that will not be allowed in the constant string.
	for (; r < strlen(tmpconst); r++)
	{
		boolean writechar = true;
		char rc = tmpconst[r];
		switch (rc)
		{
			// Space, at sign and question mark
			case ' ':
			case '@':
			case '?':
			// Used for operations
			case '+':
			case '-':
			case '*':
			case '/':
			case '%':
			case '^':
			case '&':
			case '!':
			// Part of Lua's syntax
			case '#':
			case '=':
			case '~':
			case '<':
			case '>':
			case '(':
			case ')':
			case '{':
			case '}':
			case '[':
			case ']':
			case ':':
			case ';':
			case ',':
			case '.':
				writechar = false;
				break;
		}
		if (writechar)
		{
			gtconst[3 + w] = rc;
			w++;
		}
	}

	// Free the temporary string.
	Z_Free(tmpconst);

	// Finally, set the constant string.
	Gametype_ConstantNames[gtype] = gtconst;
}

//
// G_UpdateGametypeSelections
//
// Updates gametype_cons_t.
//
void G_UpdateGametypeSelections(void)
{
	INT32 i;
	for (i = 0; i < gametypecount; i++)
	{
		gametype_cons_t[i].value = i;
		gametype_cons_t[i].strvalue = Gametype_Names[i];
	}
	gametype_cons_t[NUMGAMETYPES].value = 0;
	gametype_cons_t[NUMGAMETYPES].strvalue = NULL;
}

//
// G_SetGametypeDescription
//
// Set a description for the specified gametype.
// (Level platter)
//
void G_SetGametypeDescription(INT16 gtype, char *descriptiontext, UINT8 leftcolor, UINT8 rightcolor)
{
	if (descriptiontext != NULL)
		strncpy(gametypedesc[gtype].notes, descriptiontext, 441);
	gametypedesc[gtype].col[0] = leftcolor;
	gametypedesc[gtype].col[1] = rightcolor;
}

// Gametype rankings
INT16 gametyperankings[NUMGAMETYPES] =
{
	GT_COOP,
	GT_COMPETITION,
	GT_RACE,

	GT_MATCH,
	GT_TEAMMATCH,

	GT_TAG,
	GT_HIDEANDSEEK,

	GT_CTF,
};

// Gametype to TOL (Type Of Level)
UINT32 gametypetol[NUMGAMETYPES] =
{
	TOL_COOP, // Co-op
	TOL_COMPETITION, // Competition
	TOL_RACE, // Race

	TOL_MATCH, // Match
	TOL_MATCH, // Team Match

	TOL_TAG, // Tag
	TOL_TAG, // Hide and Seek

	TOL_CTF, // CTF
};

tolinfo_t TYPEOFLEVEL[NUMTOLNAMES] = {
	{"SOLO",TOL_SP},
	{"SP",TOL_SP},
	{"SINGLEPLAYER",TOL_SP},
	{"SINGLE",TOL_SP},

	{"COOP",TOL_COOP},
	{"CO-OP",TOL_COOP},

	{"COMPETITION",TOL_COMPETITION},
	{"RACE",TOL_RACE},

	{"MATCH",TOL_MATCH},
	{"TAG",TOL_TAG},
	{"CTF",TOL_CTF},

	{"2D",TOL_2D},
	{"MARIO",TOL_MARIO},
	{"NIGHTS",TOL_NIGHTS},
	{"OLDBRAK",TOL_ERZ3},
	{"ERZ3",TOL_ERZ3},

	{"XMAS",TOL_XMAS},
	{"CHRISTMAS",TOL_XMAS},
	{"WINTER",TOL_XMAS},

	{NULL, 0}
};

UINT32 lastcustomtol = (TOL_XMAS<<1);

//
// G_AddTOL
//
// Adds a type of level.
//
void G_AddTOL(UINT32 newtol, const char *tolname)
{
	INT32 i;
	for (i = 0; TYPEOFLEVEL[i].name; i++)
		;

	TYPEOFLEVEL[i].name = Z_StrDup(tolname);
	TYPEOFLEVEL[i].flag = newtol;
}

//
// G_AddGametypeTOL
//
// Assigns a type of level to a gametype.
//
void G_AddGametypeTOL(INT16 gtype, UINT32 newtol)
{
	gametypetol[gtype] = newtol;
}

//
// G_GetGametypeByName
//
// Returns the number for the given gametype name string, or -1 if not valid.
//
INT32 G_GetGametypeByName(const char *gametypestr)
{
	INT32 i;

	for (i = 0; i < gametypecount; i++)
		if (!stricmp(gametypestr, Gametype_Names[i]))
			return i;

	return -1; // unknown gametype
}

//
// G_IsSpecialStage
//
// Returns TRUE if
// the given map is a special stage.
//
boolean G_IsSpecialStage(INT32 mapnum)
{
	if (modeattacking == ATTACKING_RECORD)
		return false;
	if (mapnum >= sstage_start && mapnum <= sstage_end)
		return true;
	if (mapnum >= smpstage_start && mapnum <= smpstage_end)
		return true;

	return false;
}

//
// G_GametypeUsesLives
//
// Returns true if the current gametype uses
// the lives system.  False otherwise.
//
boolean G_GametypeUsesLives(void)
{
	// Coop, Competitive
	if ((gametyperules & GTR_LIVES)
	 && !(modeattacking || metalrecording) // No lives in Time Attack
	 && !G_IsSpecialStage(gamemap)
	 && !(maptol & TOL_NIGHTS)) // No lives in NiGHTS
		return true;
	return false;
}

//
// G_GametypeUsesCoopLives
//
// Returns true if the current gametype uses
// the cooplives CVAR.  False otherwise.
//
boolean G_GametypeUsesCoopLives(void)
{
	return (gametyperules & (GTR_LIVES|GTR_FRIENDLY)) == (GTR_LIVES|GTR_FRIENDLY);
}

//
// G_GametypeUsesCoopStarposts
//
// Returns true if the current gametype uses
// the coopstarposts CVAR.  False otherwise.
//
boolean G_GametypeUsesCoopStarposts(void)
{
	return (gametyperules & GTR_FRIENDLY);
}

//
// G_GametypeHasTeams
//
// Returns true if the current gametype uses
// Red/Blue teams.  False otherwise.
//
boolean G_GametypeHasTeams(void)
{
	return (gametyperules & GTR_TEAMS);
}

//
// G_GametypeHasSpectators
//
// Returns true if the current gametype supports
// spectators.  False otherwise.
//
boolean G_GametypeHasSpectators(void)
{
	return (gametyperules & GTR_SPECTATORS);
}

//
// G_RingSlingerGametype
//
// Returns true if the current gametype supports firing rings.
// ANY gametype can be a ringslinger gametype, just flick a switch.
//
boolean G_RingSlingerGametype(void)
{
	return ((gametyperules & GTR_RINGSLINGER) || (cv_ringslinger.value));
}

//
// G_PlatformGametype
//
// Returns true if a gametype is a more traditional platforming-type.
//
boolean G_PlatformGametype(void)
{
	return (!(gametyperules & GTR_RINGSLINGER));
}

//
// G_CoopGametype
//
// Returns true if a gametype is a Co-op gametype.
//
boolean G_CoopGametype(void)
{
	return ((gametyperules & (GTR_FRIENDLY|GTR_CAMPAIGN)) == (GTR_FRIENDLY|GTR_CAMPAIGN));
}

//
// G_TagGametype
//
// For Jazz's Tag/HnS modes that have a lot of special cases..
//
boolean G_TagGametype(void)
{
	return (gametyperules & GTR_TAG);
}

//
// G_CompetitionGametype
//
// For gametypes that are race gametypes, and have lives.
//
boolean G_CompetitionGametype(void)
{
	return ((gametyperules & GTR_RACE) && (gametyperules & GTR_LIVES));
}

/** Get the typeoflevel flag needed to indicate support of a gametype.
  * In single-player, this always returns TOL_SP.
  * \param gametype The gametype for which support is desired.
  * \return The typeoflevel flag to check for that gametype.
  * \author Graue <graue@oceanbase.org>
  */
UINT32 G_TOLFlag(INT32 pgametype)
{
	if (!multiplayer)
		return TOL_SP;
	return gametypetol[pgametype];
}

/** Select a random map with the given typeoflevel flags.
  * If no map has those flags, this arbitrarily gives you map 1.
  * \param tolflags The typeoflevel flags to insist on. Other bits may
  *                 be on too, but all of these must be on.
  * \return A random map with those flags, 1-based, or 1 if no map
  *         has those flags.
  * \author Graue <graue@oceanbase.org>
  */
static INT16 RandMap(UINT32 tolflags, INT16 pprevmap)
{
	INT16 *okmaps = Z_Malloc(NUMMAPS * sizeof(INT16), PU_STATIC, NULL);
	INT32 numokmaps = 0;
	INT16 ix;

	// Find all the maps that are ok and and put them in an array.
	for (ix = 0; ix < NUMMAPS; ix++)
		if (mapheaderinfo[ix] && (mapheaderinfo[ix]->typeoflevel & tolflags) == tolflags
		 && ix != pprevmap // Don't pick the same map.
		 && (!M_MapLocked(ix+1, serverGamedata)) // Don't pick locked maps.
		)
			okmaps[numokmaps++] = ix;

	if (numokmaps == 0)
		ix = 0; // Sorry, none match. You get MAP01.
	else
		ix = okmaps[M_RandomKey(numokmaps)];

	Z_Free(okmaps);

	return ix;
}

//
// G_UpdateVisited
//
static void G_UpdateVisited(gamedata_t *data, player_t *player, boolean global)
{
	// Update visitation flags?
	if (!demoplayback
		&& G_CoopGametype() // Campaign mode
		&& !stagefailed // Did not fail the stage
		&& (global || player->pflags & PF_FINISHED)) // Actually beat the stage
	{
		UINT8 earnedEmblems;
		UINT16 totalrings = 0;
		INT32 i;

		// Update visitation flags
		data->mapvisited[gamemap-1] |= MV_BEATEN;

		// eh, what the hell
		if (ultimatemode)
			data->mapvisited[gamemap-1] |= MV_ULTIMATE;

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
			{
				continue;
			}

			totalrings += players[i].rings;
		}

		// may seem incorrect but IS possible in what the main game uses as mp special stages, and nummaprings will be -1 in NiGHTS
		if (nummaprings > 0 && totalrings >= nummaprings)
		{
			data->mapvisited[gamemap-1] |= MV_PERFECT;
			if (modeattacking)
				data->mapvisited[gamemap-1] |= MV_PERFECTRA;
		}

		if (!G_IsSpecialStage(gamemap))
		{
			// not available to special stages because they can only really be done in one order in an unmodified game, so impossible for first six and trivial for seventh
			if (ALL7EMERALDS(emeralds))
				data->mapvisited[gamemap-1] |= MV_ALLEMERALDS;
		}

		if ((earnedEmblems = M_CompletionEmblems(data)) && !global)
		{
			CONS_Printf(M_GetText("\x82" "Earned %hu emblem%s for level completion.\n"), (UINT16)earnedEmblems, earnedEmblems > 1 ? "s" : "");
		}

		if (global)
		{
			M_CheckLevelEmblems(data);
		}
		else
		{
			if (mapheaderinfo[gamemap-1]->menuflags & LF2_RECORDATTACK)
				G_SetMainRecords(data, player);
			else if (mapheaderinfo[gamemap-1]->menuflags & LF2_NIGHTSATTACK)
				G_SetNightsRecords(data, player);
		}
	}
}

static void G_UpdateAllVisited(void)
{
	// Update server
	G_UpdateVisited(serverGamedata, &players[serverplayer], true);

	// Update client
	G_UpdateVisited(clientGamedata, &players[consoleplayer], false);

	if (splitscreen)
	{
		// Allow P2 to get emblems too, why not :)
		G_UpdateVisited(clientGamedata, &players[secondarydisplayplayer], false);
	}
}

static boolean CanSaveLevel(INT32 mapnum)
{
	// You can never save in a special stage.
	if (G_IsSpecialStage(mapnum))
		return false;

	// If the game is complete for this save slot, then any level can save!
	if (gamecomplete)
		return true;

	// Be kind with Marathon Mode live event backups.
	if (marathonmode)
		return true;

	// Any levels that have the savegame flag can save normally.
	return (mapheaderinfo[mapnum-1] && (mapheaderinfo[mapnum-1]->levelflags & LF_SAVEGAME));
}

static void G_HandleSaveLevel(void)
{
	// Update records & emblems
	G_UpdateAllVisited();

	// do this before running the intermission or custom cutscene, mostly for the sake of marathon mode but it also massively reduces redundant file save events in f_finale.c
	if (nextmap >= 1100-1)
	{
		if (!gamecomplete)
			gamecomplete = 2; // special temporary mode to prevent using SP level select in pause menu until the intermission is over without restricting it in every intermission
		if (cursaveslot > 0)
		{
			if (marathonmode)
			{
				// don't keep a backup around when the run is done!
				if (FIL_FileExists(liveeventbackup))
					remove(liveeventbackup);
				cursaveslot = 0;
			}
			else if (!usedCheats && !(netgame || multiplayer || ultimatemode || demorecording || metalrecording || modeattacking))
			{
				G_SaveGame((UINT32)cursaveslot, spstage_start);
			}
		}
	}
	// and doing THIS here means you don't lose your progress if you close the game mid-intermission
	else if (!(ultimatemode || netgame || multiplayer || demoplayback || demorecording || metalrecording || modeattacking)
		&& !usedCheats && cursaveslot > 0 && CanSaveLevel(lastmap+1))
	{
		G_SaveGame((UINT32)cursaveslot, lastmap+1); // not nextmap+1 to route around special stages
	}
}

//
// G_GetNextMap
//
INT16 G_GetNextMap(boolean ignoretokens, boolean silent)
{
	INT32 i;
	INT16 newmapnum;
	boolean spec = G_IsSpecialStage(gamemap);

	// go to next level
	// newmapnum is 0-based, unlike gamemap
	if (nextmapoverride != 0)
		newmapnum = (INT16)(nextmapoverride-1);
	else if (marathonmode && mapheaderinfo[gamemap-1]->marathonnext)
		newmapnum = (INT16)(mapheaderinfo[gamemap-1]->marathonnext-1);
	else
	{
		newmapnum = (INT16)(mapheaderinfo[gamemap-1]->nextlevel-1);
		if (marathonmode && newmapnum == spmarathon_start-1)
			newmapnum = 1100-1; // No infinite loop for you
	}

	INT16 gametype_to_use;

	if (nextgametype >= 0 && nextgametype < gametypecount)
		gametype_to_use = nextgametype;
	else
		gametype_to_use = gametype;

	// If newmapnum is actually going to get used, make sure it points to
	// a map of the proper gametype -- skip levels that don't support
	// the current gametype. (Helps avoid playing boss levels in Race,
	// for instance).
	if (!spec || nextmapoverride)
	{
		if (newmapnum >= 0 && newmapnum < NUMMAPS)
		{
			INT16 cm = newmapnum;
			UINT32 tolflag = G_TOLFlag(gametype_to_use);
			UINT8 visitedmap[(NUMMAPS+7)/8];

			memset(visitedmap, 0, sizeof (visitedmap));

			while (!mapheaderinfo[cm] || !(mapheaderinfo[cm]->typeoflevel & tolflag))
			{
				visitedmap[cm/8] |= (1<<(cm&7));
				if (!mapheaderinfo[cm])
					cm = -1; // guarantee error execution
				else if (marathonmode && mapheaderinfo[cm]->marathonnext)
					cm = (INT16)(mapheaderinfo[cm]->marathonnext-1);
				else
					cm = (INT16)(mapheaderinfo[cm]->nextlevel-1);

				if (cm >= NUMMAPS || cm < 0) // out of range (either 1100ish or error)
				{
					cm = newmapnum; //Start the loop again so that the error checking below is executed.

					//Make sure the map actually exists before you try to go to it!
					if ((W_CheckNumForName(G_BuildMapName(cm + 1)) == LUMPERROR))
					{
						if (!silent)
							CONS_Alert(CONS_ERROR, M_GetText("Next map given (MAP %d) doesn't exist! Reverting to MAP01.\n"), cm+1);
						cm = 0;
						break;
					}
				}

				if (visitedmap[cm/8] & (1<<(cm&7))) // smells familiar
				{
					// We got stuck in a loop, came back to the map we started on
					// without finding one supporting the current gametype.
					// Thus, print a warning, and just use this map anyways.
					if (!silent)
						CONS_Alert(CONS_WARNING, M_GetText("Can't find a compatible map after map %d; using map %d anyway\n"), prevmap+1, cm+1);
					break;
				}
			}
			newmapnum = cm;
		}

		// wrap around in race
		if (newmapnum >= 1100-1 && newmapnum <= 1102-1 && !(gametyperules & GTR_CAMPAIGN))
			newmapnum = (INT16)(spstage_start-1);

		if (newmapnum < 0 || (newmapnum >= NUMMAPS && newmapnum < 1100-1) || newmapnum > 1103-1)
			I_Error("Followed map %d to invalid map %d\n", prevmap + 1, newmapnum + 1);

		if (!spec)
			lastmap = newmapnum; // Remember last map for when you come out of the special stage.
	}

	if (!ignoretokens && (gottoken = ((gametyperules & GTR_SPECIALSTAGES) && token)))
	{
		token--;

//		if (!nextmapoverride) // Having a token should pull the player into the special stage before going to the overridden map (Issue #933)
			for (i = 0; i < 7; i++)
				if (!(emeralds & (1<<i)))
				{
					newmapnum = ((netgame || multiplayer) ? smpstage_start : sstage_start) + i - 1; // to special stage!
					break;
				}

		if (i == 7)
		{
			gottoken = false;
			token = 0;
		}
	}

	if (spec && (!gottoken || ignoretokens) && !nextmapoverride)
		newmapnum = lastmap; // Exiting from a special stage? Go back to the game. Tails 08-11-2001

	if (!(gametyperules & GTR_CAMPAIGN))
	{
		if (cv_advancemap.value == 0) // Stay on same map.
			newmapnum = prevmap;
		else if (cv_advancemap.value == 2) // Go to random map.
			newmapnum = RandMap(G_TOLFlag(gametype_to_use), prevmap);
	}

	return newmapnum;
}

//
// G_DoCompleted
//
static void G_DoCompleted(void)
{
	INT32 i;

	tokenlist = 0; // Reset the list

	if (modeattacking && pausedelay)
		pausedelay = 0;

	gameaction = ga_nothing;

	if (metalplayback)
		G_StopMetalDemo();
	if (metalrecording)
		G_StopMetalRecording(false);

	G_SetGamestate(GS_NULL);
	wipegamestate = GS_NULL;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			G_PlayerFinishLevel(i); // take away cards and stuff

	if (automapactive)
		AM_Stop();

	S_StopSounds();

	//Get and set prevmap/nextmap
	prevmap = (INT16)(gamemap-1);
	nextmap = G_GetNextMap(false, false);

	automapactive = false;

	// We are committed to this map now.
	// We may as well allocate its header if it doesn't exist
	// (That is, if it's a real map)
	if (nextmap < NUMMAPS && !mapheaderinfo[nextmap])
		P_AllocMapHeader(nextmap);

	Y_DetermineIntermissionType();

	if ((skipstats && !modeattacking) || (modeattacking && stagefailed) || (intertype == int_none))
	{
		G_HandleSaveLevel();
		G_AfterIntermission();
	}
	else
	{
		G_SetGamestate(GS_INTERMISSION);
		Y_StartIntermission();
		Y_LoadIntermissionData();
		G_HandleSaveLevel();
	}
}

// See also F_EndCutscene, the only other place which handles intra-map/ending transitions
void G_AfterIntermission(void)
{
	Y_CleanupScreenBuffer();

	if (modeattacking)
	{
		M_EndModeAttackRun();
		return;
	}

	if (gamecomplete == 2) // special temporary mode to prevent using SP level select in pause menu until the intermission is over without restricting it in every intermission
		gamecomplete = 1;

	HU_ClearCEcho();

	if ((gametyperules & GTR_CUTSCENES) && mapheaderinfo[gamemap-1]->cutscenenum
		&& !modeattacking
		&& skipstats <= 1
		&& (gamecomplete || !(marathonmode & MA_NOCUTSCENES))
		&& stagefailed == false)
	{
		// Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[gamemap-1]->cutscenenum-1, false, false, false);
	}
	else
	{
		if (nextmap < 1100-1)
			G_NextLevel();
		else
			G_EndGame();
	}
}

//
// G_NextLevel (WorldDone)
//
// init next level or go to the final scene
// called by end of intermission screen (y_inter)
//
void G_NextLevel(void)
{
	gameaction = ga_worlddone;
}

static void G_DoWorldDone(void)
{
	if (server)
	{
		INT16 gametype_to_use;

		if (nextgametype >= 0 && nextgametype < gametypecount)
			gametype_to_use = nextgametype;
		else
			gametype_to_use = gametype;

		if (gametyperules & GTR_CAMPAIGN)
			// don't reset player between maps
			D_MapChange(nextmap+1, gametype_to_use, ultimatemode, false, 0, false, false);
		else
			// resetplayer in match/chaos/tag/CTF/race for more equality
			D_MapChange(nextmap+1, gametype_to_use, ultimatemode, true, 0, false, false);

		nextgametype = -1;
	}

	gameaction = ga_nothing;
}

//
// G_UseContinue
//
void G_UseContinue(void)
{
	if (gamestate == GS_LEVEL && !netgame && !multiplayer)
	{
		gameaction = ga_startcont;
		lastdraw = true;
	}
}

static void G_DoStartContinue(void)
{
	I_Assert(!netgame && !multiplayer);

	G_PlayerFinishLevel(consoleplayer); // take away cards and stuff

	F_StartContinue();
	gameaction = ga_nothing;
}

//
// G_Continue
//
// re-init level, used by continue and possibly countdowntimeup
//
void G_Continue(void)
{
	if (!netgame && !multiplayer)
		gameaction = ga_continued;
}

static void G_DoContinued(void)
{
	player_t *pl = &players[consoleplayer];
	I_Assert(!netgame && !multiplayer);
	//I_Assert(pl->continues > 0);

	if (pl->continues)
		pl->continues--;

	// Reset score
	pl->score = 0;

	// Allow tokens to come back
	tokenlist = 0;
	token = 0;

	if (!(netgame || multiplayer || demoplayback || demorecording || metalrecording || modeattacking) && !usedCheats && cursaveslot > 0)
	{
		G_SaveGameOver((UINT32)cursaveslot, true);
	}

	// Reset # of lives
	pl->lives = (ultimatemode) ? 1 : startinglivesbalance[numgameovers];

	D_MapChange(gamemap, gametype, ultimatemode, false, 0, false, false);

	gameaction = ga_nothing;
}

//
// G_EndGame (formerly Y_EndGame)
// Frankly this function fits better in g_game.c than it does in y_inter.c
//
// ...Gee, (why) end the game?
// Because G_AfterIntermission and F_EndCutscene would
// both do this exact same thing *in different ways* otherwise,
// which made it so that you could only unlock Ultimate mode
// if you had a cutscene after the final level and crap like that.
// This function simplifies it so only one place has to be updated
// when something new is added.
void G_EndGame(void)
{
	// Only do evaluation and credits in coop games.
	if (gametyperules & GTR_CUTSCENES)
	{
		if (nextmap == 1103-1) // end game with ending
		{
			F_StartEnding();
			return;
		}
		if (nextmap == 1102-1) // end game with credits
		{
			F_StartCredits();
			return;
		}
		if (nextmap == 1101-1) // end game with evaluation
		{
			F_StartGameEvaluation();
			return;
		}
	}

	// 1100 or competitive multiplayer, so go back to title screen.
	D_StartTitle();
}

//
// G_LoadGameSettings
//
// Sets a tad of default info we need.
void G_LoadGameSettings(void)
{
	// defaults
	spstage_start = spmarathon_start = 1;
	sstage_start = 50;
	sstage_end = 56; // 7 special stages in vanilla SRB2
	sstage_end++; // plus one weirdo
	smpstage_start = 60;
	smpstage_end = 66; // 7 multiplayer special stages too

	// initialize free sfx slots for skin sounds
	S_InitRuntimeSounds();
}

#define GAMEDATA_ID 0x86E4A27C // Change every major version, as usual
#define COMPAT_GAMEDATA_ID 0xFCAFE211 // TODO: 2.3: Delete

// G_LoadGameData
// Loads the main data file, which stores information such as emblems found, etc.
void G_LoadGameData(gamedata_t *data)
{
	save_t savebuffer;
	INT32 i, j;

	UINT32 versionID;
	UINT8 rtemp;

	//For records
	UINT32 recscore;
	tic_t  rectime;
	UINT16 recrings;

	UINT8 recmares;
	INT32 curmare;

	// Stop saving, until we successfully load it again.
	data->loaded = false;

	// Backwards compat stuff
	INT32 max_emblems = MAXEMBLEMS;
	INT32 max_extraemblems = MAXEXTRAEMBLEMS;
	INT32 max_unlockables = MAXUNLOCKABLES;
	INT32 max_conditionsets = MAXCONDITIONSETS;

	// Clear things so previously read gamedata doesn't transfer
	// to new gamedata
	G_ClearRecords(data); // main and nights records
	M_ClearSecrets(data); // emblems, unlocks, maps visited, etc
	data->totalplaytime = 0; // total play time (separate from all)

	if (M_CheckParm("-nodata"))
	{
		// Don't load at all.
		return;
	}

	if (M_CheckParm("-resetdata"))
	{
		// Don't load, but do save. (essentially, reset)
		data->loaded = true;
		return;
	}

	savebuffer.size = FIL_ReadFile(va(pandf, srb2home, gamedatafilename), &savebuffer.buf);
	if (!savebuffer.size)
	{
		// No gamedata. We can save a new one.
		data->loaded = true;
		return;
	}

	savebuffer.pos = 0;

	// Version check
	versionID = P_ReadUINT32(&savebuffer);
	if (versionID != GAMEDATA_ID
#ifdef COMPAT_GAMEDATA_ID // backwards compat behavior
		&& versionID != COMPAT_GAMEDATA_ID
#endif
		)
	{
		const char *gdfolder = "the SRB2 folder";
		if (strcmp(srb2home,"."))
			gdfolder = srb2home;

		Z_Free(savebuffer.buf);
		I_Error("Game data is from another version of SRB2.\nDelete %s(maybe in %s) and try again.", gamedatafilename, gdfolder);
	}

#ifdef COMPAT_GAMEDATA_ID // Account for lower MAXUNLOCKABLES and MAXEXTRAEMBLEMS from older versions
	if (versionID == COMPAT_GAMEDATA_ID)
	{
		max_extraemblems = 16;
		max_unlockables = 32;
	}
#endif

	data->totalplaytime = P_ReadUINT32(&savebuffer);

#ifdef COMPAT_GAMEDATA_ID
	if (versionID == COMPAT_GAMEDATA_ID)
	{
		// We'll temporarily use the old condition when loading an older file.
		// The proper mod-specific hash will get saved in afterwards.
		boolean modded = P_ReadUINT8(&savebuffer);

		if (modded && !savemoddata)
		{
			goto datacorrupt;
		}
		else if (modded != true && modded != false)
		{
			goto datacorrupt;
		}

		// make a backup of the old data
		char currentfilename[64];
		char backupfilename[69];
		char bak[5];

		strcpy(bak, ".bak");
		strcpy(currentfilename, gamedatafilename);
		STRBUFCPY(backupfilename, strcat(currentfilename, bak));

		FIL_WriteFile(va(pandf, srb2home, backupfilename), &savebuffer.buf, savebuffer.size);
	}
	else
#endif
	{
		// Quick & dirty hash for what mod this save file is for.
		UINT32 modID = P_ReadUINT32(&savebuffer);
		UINT32 expectedID = quickncasehash(timeattackfolder, sizeof timeattackfolder);

		if (modID != expectedID)
		{
			// Aha! Someone's been screwing with the save file!
			goto datacorrupt;
		}
	}

	// TODO put another cipher on these things? meh, I don't care...
	for (i = 0; i < NUMMAPS; i++)
		if ((data->mapvisited[i] = P_ReadUINT8(&savebuffer)) > MV_MAX)
			goto datacorrupt;

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < max_emblems;)
	{
		rtemp = P_ReadUINT8(&savebuffer);
		for (j = 0; j < 8 && j+i < max_emblems; ++j)
			data->collected[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < max_extraemblems;)
	{
		rtemp = P_ReadUINT8(&savebuffer);
		for (j = 0; j < 8 && j+i < max_extraemblems; ++j)
			data->extraCollected[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < max_unlockables;)
	{
		rtemp = P_ReadUINT8(&savebuffer);
		for (j = 0; j < 8 && j+i < max_unlockables; ++j)
			data->unlocked[j+i] = ((rtemp >> j) & 1);
		i += j;
	}
	for (i = 0; i < max_conditionsets;)
	{
		rtemp = P_ReadUINT8(&savebuffer);
		for (j = 0; j < 8 && j+i < max_conditionsets; ++j)
			data->achieved[j+i] = ((rtemp >> j) & 1);
		i += j;
	}

	data->timesBeaten = P_ReadUINT32(&savebuffer);
	data->timesBeatenWithEmeralds = P_ReadUINT32(&savebuffer);
	data->timesBeatenUltimate = P_ReadUINT32(&savebuffer);

	// Main records
	for (i = 0; i < NUMMAPS; ++i)
	{
		recscore = P_ReadUINT32(&savebuffer);
		rectime  = (tic_t)P_ReadUINT32(&savebuffer);
		recrings = P_ReadUINT16(&savebuffer);
		P_ReadUINT8(&savebuffer); // compat

		if (recrings > 10000 || recscore > MAXSCORE)
			goto datacorrupt;

		if (recscore || rectime || recrings)
		{
			G_AllocMainRecordData((INT16)i, data);
			data->mainrecords[i]->score = recscore;
			data->mainrecords[i]->time = rectime;
			data->mainrecords[i]->rings = recrings;
		}
	}

	// Nights records
	for (i = 0; i < NUMMAPS; ++i)
	{
		if ((recmares = P_ReadUINT8(&savebuffer)) == 0)
			continue;

		G_AllocNightsRecordData((INT16)i, data);

		for (curmare = 0; curmare < (recmares+1); ++curmare)
		{
			data->nightsrecords[i]->score[curmare] = P_ReadUINT32(&savebuffer);
			data->nightsrecords[i]->grade[curmare] = P_ReadUINT8(&savebuffer);
			data->nightsrecords[i]->time[curmare] = (tic_t)P_ReadUINT32(&savebuffer);

			if (data->nightsrecords[i]->grade[curmare] > GRADE_S)
			{
				goto datacorrupt;
			}
		}

		data->nightsrecords[i]->nummares = recmares;
	}

	// done
	Z_Free(savebuffer.buf);

	// Don't consider loaded until it's a success!
	// It used to do this much earlier, but this would cause the gamedata to
	// save over itself when it I_Errors from the corruption landing point below,
	// which can accidentally delete players' legitimate data if the code ever has any tiny mistakes!
	data->loaded = true;

	// Silent update unlockables in case they're out of sync with conditions
	M_SilentUpdateUnlockablesAndEmblems(data);
	M_SilentUpdateSkinAvailabilites();

	return;

	// Landing point for corrupt gamedata
	datacorrupt:
	{
		const char *gdfolder = "the SRB2 folder";
		if (strcmp(srb2home,"."))
			gdfolder = srb2home;

		Z_Free(savebuffer.buf);

		I_Error("Corrupt game data file.\nDelete %s(maybe in %s) and try again.", gamedatafilename, gdfolder);
	}
}

// G_SaveGameData
// Saves the main data file, which stores information such as emblems found, etc.
void G_SaveGameData(gamedata_t *data)
{
	save_t savebuffer;

	INT32 i, j;
	UINT8 btemp;

	INT32 curmare;

	if (!data)
		return; // data struct not valid

	if (!data->loaded)
		return; // If never loaded (-nodata), don't save

	savebuffer.size = GAMEDATASIZE;
	savebuffer.buf = (UINT8 *)malloc(savebuffer.size);
	if (!savebuffer.buf)
	{
		CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
		return;
	}
	savebuffer.pos = 0;

	if (usedCheats)
	{
		free(savebuffer.buf);
		return;
	}

	// Version test
	P_WriteUINT32(&savebuffer, GAMEDATA_ID);

	P_WriteUINT32(&savebuffer, data->totalplaytime);

	P_WriteUINT32(&savebuffer, quickncasehash(timeattackfolder, sizeof timeattackfolder));

	// TODO put another cipher on these things? meh, I don't care...
	for (i = 0; i < NUMMAPS; i++)
		P_WriteUINT8(&savebuffer, (data->mapvisited[i] & MV_MAX));

	// To save space, use one bit per collected/achieved/unlocked flag
	for (i = 0; i < MAXEMBLEMS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXEMBLEMS; ++j)
			btemp |= (data->collected[j+i] << j);
		P_WriteUINT8(&savebuffer, btemp);
		i += j;
	}
	for (i = 0; i < MAXEXTRAEMBLEMS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXEXTRAEMBLEMS; ++j)
			btemp |= (data->extraCollected[j+i] << j);
		P_WriteUINT8(&savebuffer, btemp);
		i += j;
	}
	for (i = 0; i < MAXUNLOCKABLES;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXUNLOCKABLES; ++j)
			btemp |= (data->unlocked[j+i] << j);
		P_WriteUINT8(&savebuffer, btemp);
		i += j;
	}
	for (i = 0; i < MAXCONDITIONSETS;)
	{
		btemp = 0;
		for (j = 0; j < 8 && j+i < MAXCONDITIONSETS; ++j)
			btemp |= (data->achieved[j+i] << j);
		P_WriteUINT8(&savebuffer, btemp);
		i += j;
	}

	P_WriteUINT32(&savebuffer, data->timesBeaten);
	P_WriteUINT32(&savebuffer, data->timesBeatenWithEmeralds);
	P_WriteUINT32(&savebuffer, data->timesBeatenUltimate);

	// Main records
	for (i = 0; i < NUMMAPS; i++)
	{
		if (data->mainrecords[i])
		{
			P_WriteUINT32(&savebuffer, data->mainrecords[i]->score);
			P_WriteUINT32(&savebuffer, data->mainrecords[i]->time);
			P_WriteUINT16(&savebuffer, data->mainrecords[i]->rings);
		}
		else
		{
			P_WriteUINT32(&savebuffer, 0);
			P_WriteUINT32(&savebuffer, 0);
			P_WriteUINT16(&savebuffer, 0);
		}
		P_WriteUINT8(&savebuffer, 0); // compat
	}

	// NiGHTS records
	for (i = 0; i < NUMMAPS; i++)
	{
		if (!data->nightsrecords[i] || !data->nightsrecords[i]->nummares)
		{
			P_WriteUINT8(&savebuffer, 0);
			continue;
		}

		P_WriteUINT8(&savebuffer, data->nightsrecords[i]->nummares);

		for (curmare = 0; curmare < (data->nightsrecords[i]->nummares + 1); ++curmare)
		{
			P_WriteUINT32(&savebuffer, data->nightsrecords[i]->score[curmare]);
			P_WriteUINT8(&savebuffer, data->nightsrecords[i]->grade[curmare]);
			P_WriteUINT32(&savebuffer, data->nightsrecords[i]->time[curmare]);
		}
	}

	FIL_WriteFile(va(pandf, srb2home, gamedatafilename), savebuffer.buf, savebuffer.pos);
	free(savebuffer.buf);
}

#define VERSIONSIZE 16

//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame(UINT32 slot, INT16 mapoverride)
{
	save_t savebuffer;
	char vcheck[VERSIONSIZE];
	char savename[255];

	// memset savedata to all 0, fixes calling perfectly valid saves corrupt because of bots
	memset(&savedata, 0, sizeof(savedata));

#ifdef SAVEGAME_OTHERVERSIONS
	//Oh christ.  The force load response needs access to mapoverride too...
	startonmapnum = mapoverride;
#endif

	if (marathonmode)
		strcpy(savename, liveeventbackup);
	else
		sprintf(savename, savegamename, slot);

	savebuffer.size = FIL_ReadFile(savename, &savebuffer.buf);
	if (!savebuffer.size)
	{
		CONS_Printf(M_GetText("Couldn't read file %s\n"), savename);
		return;
	}

	savebuffer.pos = 0;

	memset(vcheck, 0, sizeof (vcheck));
	sprintf(vcheck, (marathonmode ? "back-up %d" : "version %d"), VERSION);
	if (strcmp((const char *)&savebuffer.buf[savebuffer.pos], (const char *)vcheck))
	{
#ifdef SAVEGAME_OTHERVERSIONS
		M_StartMessage(M_GetText("Save game from different version.\nYou can load this savegame, but\nsaving afterwards will be disabled.\n\nDo you want to continue anyway?\n\n(Press 'Y' to confirm)\n"),
		               M_ForceLoadGameResponse, MM_YESNO);
		//Freeing done by the callback function of the above message
#else
		M_ClearMenus(true); // so ESC backs out to title
		M_StartMessage(M_GetText("Save game from different version\n\nPress ESC\n"), NULL, MM_NOTHING);
		Command_ExitGame_f();
		Z_Free(savebuffer.buf);

		// no cheating!
		memset(&savedata, 0, sizeof(savedata));
#endif
		return; // bad version
	}
	savebuffer.pos += VERSIONSIZE;

//	if (demoplayback) // reset game engine
//		G_StopDemo();

//	paused = false;
//	automapactive = false;

	// dearchive all the modifications
	if (!P_LoadGame(&savebuffer, mapoverride))
	{
		M_ClearMenus(true); // so ESC backs out to title
		M_StartMessage(M_GetText("Savegame file corrupted\n\nPress ESC\n"), NULL, MM_NOTHING);
		Command_ExitGame_f();
		Z_Free(savebuffer.buf);

		// no cheating!
		memset(&savedata, 0, sizeof(savedata));
		return;
	}
	if (marathonmode)
	{
		marathontime = P_ReadUINT32(&savebuffer);
		marathonmode |= P_ReadUINT8(&savebuffer);
	}

	// done
	Z_Free(savebuffer.buf);

	displayplayer = consoleplayer;
	multiplayer = splitscreen = false;

	if (setsizeneeded)
		R_ExecuteSetViewSize();

	M_ClearMenus(true);
	CON_ToggleOff();
}

//
// G_SaveGame
// Saves your game.
//
void G_SaveGame(UINT32 slot, INT16 mapnum)
{
	save_t savebuffer;
	boolean saved;
	char savename[256] = "";
	const char *backup;

	if (marathonmode)
		strcpy(savename, liveeventbackup);
	else
		sprintf(savename, savegamename, slot);
	backup = va("%s",savename);

	gameaction = ga_nothing;
	{
		char name[VERSIONSIZE];

		savebuffer.size = SAVEGAMESIZE;
		savebuffer.buf = (UINT8 *)malloc(savebuffer.size);
		if (!savebuffer.buf)
		{
			CONS_Alert(CONS_ERROR, M_GetText("No more free memory for saving game data\n"));
			return;
		}
		savebuffer.pos = 0;

		memset(name, 0, sizeof (name));
		sprintf(name, (marathonmode ? "back-up %d" : "version %d"), VERSION);
		P_WriteMem(&savebuffer, name, VERSIONSIZE);

		P_SaveGame(&savebuffer, mapnum);
		if (marathonmode)
		{
			UINT32 writetime = marathontime;
			if (!(marathonmode & MA_INGAME))
				writetime += TICRATE*5; // live event backup penalty because we don't know how long it takes to get to the next map
			P_WriteUINT32(&savebuffer, writetime);
			P_WriteUINT8(&savebuffer, (marathonmode & ~MA_INIT));
		}

		saved = FIL_WriteFile(backup, savebuffer.buf, savebuffer.pos);
		free(savebuffer.buf);
	}

	gameaction = ga_nothing;

	if (cv_debug && saved)
		CONS_Printf(M_GetText("Game saved.\n"));
	else if (!saved)
		CONS_Alert(CONS_ERROR, M_GetText("Error while writing to %s for save slot %u, base: %s\n"), backup, slot, (marathonmode ? liveeventbackup : savegamename));
}

#define BADSAVE goto cleanup;
void G_SaveGameOver(UINT32 slot, boolean modifylives)
{
	save_t savebuffer;
	boolean saved = false;
	char vcheck[VERSIONSIZE];
	char savename[255];
	const char *backup;

	if (marathonmode)
		strcpy(savename, liveeventbackup);
	else
		sprintf(savename, savegamename, slot);
	backup = va("%s",savename);

	savebuffer.size = FIL_ReadFile(savename, &savebuffer.buf);
	if (!savebuffer.size)
	{
		CONS_Printf(M_GetText("Couldn't read file %s\n"), savename);
		return;
	}

	savebuffer.pos = 0;

	{
		char temp[sizeof(timeattackfolder)];
		UINT8 *lives_p;
		SINT8 pllives;
#ifdef NEWSKINSAVES
		INT16 backwardsCompat = 0;
#endif

		// Version check
		memset(vcheck, 0, sizeof (vcheck));
		sprintf(vcheck, (marathonmode ? "back-up %d" : "version %d"), VERSION);
		if (strcmp((const char *)&savebuffer.buf[savebuffer.pos], (const char *)vcheck)) BADSAVE
		savebuffer.pos += VERSIONSIZE;

		// P_UnArchiveMisc()
		(void)P_ReadINT16(&savebuffer);
		(void)P_ReadUINT16(&savebuffer); // emeralds
		P_ReadStringN(&savebuffer, temp, sizeof(temp)); // mod it belongs to
		if (strcmp(temp, timeattackfolder)) BADSAVE

		// P_UnArchivePlayer()
#ifdef NEWSKINSAVES
		backwardsCompat = P_ReadUINT16(&savebuffer);

		if (backwardsCompat == NEWSKINSAVES) // New save, read skin names
#endif
		{
			char ourSkinName[SKINNAMESIZE+1];
			char botSkinName[SKINNAMESIZE+1];

			P_ReadStringN(&savebuffer, ourSkinName, SKINNAMESIZE);

			P_ReadStringN(&savebuffer, botSkinName, SKINNAMESIZE);
		}

		P_WriteUINT8(&savebuffer, numgameovers);

		lives_p = &savebuffer.buf[savebuffer.pos];
		pllives = P_ReadSINT8(&savebuffer); // lives
		if (modifylives && pllives < startinglivesbalance[numgameovers])
		{
			*lives_p = startinglivesbalance[numgameovers];
		}

		(void)P_ReadINT32(&savebuffer); // Score
		(void)P_ReadINT32(&savebuffer); // continues

		// File end marker check
		switch (P_ReadUINT8(&savebuffer))
		{
			case 0xb7:
				{
					UINT8 i, banksinuse;
					banksinuse = P_ReadUINT8(&savebuffer);
					if (banksinuse > NUM_LUABANKS)
						BADSAVE
					for (i = 0; i < banksinuse; i++)
					{
						(void)P_ReadINT32(&savebuffer);
					}
					if (P_ReadUINT8(&savebuffer) != 0x1d)
						BADSAVE
				}
			case 0x1d:
				break;
			default:
				BADSAVE
		}

		// done
		saved = FIL_WriteFile(backup, savebuffer.buf, savebuffer.size);
	}

cleanup:
	if (cv_debug && saved)
		CONS_Printf(M_GetText("Game saved.\n"));
	else if (!saved)
		CONS_Alert(CONS_ERROR, M_GetText("Error while writing to %s for save slot %u, base: %s\n"), backup, slot, (marathonmode ? liveeventbackup : savegamename));
	Z_Free(savebuffer.buf);

}
#undef BADSAVE

//
// G_DeferedInitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//
void G_DeferedInitNew(boolean pultmode, const char *mapname, INT32 character, boolean SSSG, boolean FLS)
{
	pickedchar = character;
	paused = false;

	if (demoplayback)
		COM_BufAddText("stopdemo\n");
	G_FreeGhosts(); // TODO: do we actually need to do this?

	// this leave the actual game if needed
	SV_StartSinglePlayerServer();

	if (savedata.lives > 0)
	{
		if ((botingame = ((botskin = savedata.botskin) != 0)))
			botcolor = skins[botskin-1]->prefcolor;
	}
	else if (splitscreen != SSSG)
	{
		splitscreen = SSSG;
		SplitScreen_OnChange();
	}

	SetPlayerSkinByNum(consoleplayer, character);

	if (mapname)
		D_MapChange(M_MapNumber(mapname[3], mapname[4]), gametype, pultmode, true, 1, false, FLS);
}

//
// This is the map command interpretation something like Command_Map_f
//
// called at: map cmd execution, doloadgame, doplaydemo
void G_InitNew(UINT8 pultmode, const char *mapname, boolean resetplayer, boolean skipprecutscene, boolean FLS)
{
	INT32 i;

	Y_CleanupScreenBuffer();

	if (paused)
	{
		paused = false;
		S_ResumeAudio();
	}

	if (netgame || multiplayer) // Nice try, haxor.
		pultmode = false;

	if (!demoplayback && !netgame) // Netgame sets random seed elsewhere, demo playback sets seed just before us!
		P_SetRandSeed(M_RandomizedSeed()); // Use a more "Random" random seed

	if (resetplayer)
	{
		// Clear a bunch of variables
		numgameovers = tokenlist = token = sstimer = redscore = bluescore = lastmap = 0;
		countdown = countdown2 = exitfadestarted = 0;

		if (!FLS)
		{
			emeralds = 0;
			memset(&luabanks, 0, sizeof(luabanks));
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			players[i].playerstate = PST_REBORN;
			players[i].starpostscale = players[i].starpostangle = players[i].starpostnum = players[i].starposttime = 0;
			players[i].starpostx = players[i].starposty = players[i].starpostz = 0;
			players[i].recordscore = 0;

			// default lives, continues and score
			if (netgame || multiplayer)
			{
				if (!FLS || (players[i].lives < 1))
					players[i].lives = cv_startinglives.value;
				players[i].continues = 0;
			}
			else
			{
				players[i].lives = (pultmode) ? 1 : startinglivesbalance[0];
				players[i].continues = (pultmode) ? 0 : 1;
			}

			if (!((netgame || multiplayer) && (FLS)))
				players[i].score = 0;

			// The latter two should clear by themselves, but just in case
			players[i].pflags &= ~(PF_TAGIT|PF_GAMETYPEOVER|PF_FULLSTASIS);

			// Clear cheatcodes too, just in case.
			players[i].pflags &= ~(PF_GODMODE|PF_NOCLIP|PF_INVIS);

			players[i].xtralife = 0;
		}

		// Reset unlockable triggers
		unlocktriggers = 0;

		// clear itemfinder, just in case
		if (!dedicated)	// except in dedicated servers, where it is not registered and can actually I_Error debug builds
			CV_StealthSetValue(&cv_itemfinder, 0);
	}

	// Restore each player's skin if it was previously forced to be a specific one
	// (Looks a bit silly, but it works.)
	boolean reset_skin = netgame && mapheaderinfo[gamemap-1] && mapheaderinfo[gamemap-1]->forcecharacter[0] != '\0';

	// internal game map
	// well this check is useless because it is done before (d_netcmd.c::command_map_f)
	// but in case of for demos....
	if (W_CheckNumForName(mapname) == LUMPERROR)
	{
		I_Error("Internal game map '%s' not found\n", mapname);
		Command_ExitGame_f();
		return;
	}

	gamemap = (INT16)M_MapNumber(mapname[3], mapname[4]); // get xx out of MAPxx

	// gamemap changed; we assume that its map header is always valid,
	// so make it so
	if(!mapheaderinfo[gamemap-1])
		P_AllocMapHeader(gamemap-1);

	maptol = mapheaderinfo[gamemap-1]->typeoflevel;
	globalweather = mapheaderinfo[gamemap-1]->weather;

	// Don't carry over custom music change to another map.
	mapmusflags |= MUSIC_RELOADRESET;

	ultimatemode = pultmode;
	automapactive = false;
	imcontinuing = false;

	if (reset_skin)
		D_SendPlayerConfig();

	// fetch saved data if available
	if (savedata.lives > 0)
	{
		numgameovers = savedata.numgameovers;
		players[consoleplayer].continues = savedata.continues;
		players[consoleplayer].lives = savedata.lives;
		players[consoleplayer].score = savedata.score;
		if ((botingame = ((botskin = savedata.botskin) != 0)))
			botcolor = skins[botskin-1]->prefcolor;
		emeralds = savedata.emeralds;
		savedata.lives = 0;
	}

	if ((gametyperules & GTR_CUTSCENES) && !skipprecutscene && mapheaderinfo[gamemap-1]->precutscenenum && !modeattacking && !(marathonmode & MA_NOCUTSCENES)) // Start a custom cutscene.
		F_StartCustomCutscene(mapheaderinfo[gamemap-1]->precutscenenum-1, true, resetplayer, FLS);
	else
		G_DoLoadLevel(resetplayer);

	if (netgame)
	{
		char *title = G_BuildMapTitle(gamemap);

		CONS_Printf(M_GetText("Map is now \"%s"), G_BuildMapName(gamemap));
		if (title)
		{
			CONS_Printf(": %s", title);
			Z_Free(title);
		}
		CONS_Printf("\"\n");
	}
}


char *G_BuildMapTitle(INT32 mapnum)
{
	char *title = NULL;

	if (!mapheaderinfo[mapnum-1])
		P_AllocMapHeader(mapnum-1);

	if (strcmp(mapheaderinfo[mapnum-1]->lvlttl, ""))
	{
		size_t len = 1;
		const char *zonetext = NULL;
		const UINT8 actnum = mapheaderinfo[mapnum-1]->actnum;

		len += strlen(mapheaderinfo[mapnum-1]->lvlttl);
		if (!(mapheaderinfo[mapnum-1]->levelflags & LF_NOZONE))
		{
			zonetext = M_GetText("Zone");
			len += strlen(zonetext) + 1;	// ' ' + zonetext
		}
		if (actnum > 0)
			len += 1 + 11;			// ' ' + INT32

		title = Z_Malloc(len, PU_STATIC, NULL);

		sprintf(title, "%s", mapheaderinfo[mapnum-1]->lvlttl);
		if (zonetext) sprintf(title + strlen(title), " %s", zonetext);
		if (actnum > 0) sprintf(title + strlen(title), " %d", actnum);
	}

	return title;
}

static void measurekeywords(mapsearchfreq_t *fr,
		struct searchdim **dimp, UINT8 *cuntp,
		const char *s, const char *q, boolean wanttable)
{
	char *qp;
	char *sp;
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), 255 * sizeof (struct searchdim),
				PU_STATIC, NULL);
	for (qp = strtok(va("%s", q), " ");
			qp && fr->total < 255;
			qp = strtok(0, " "))
	{
		if (( sp = strcasestr(s, qp) ))
		{
			if (wanttable)
			{
				(*dimp)[(*cuntp)].pos = sp - s;
				(*dimp)[(*cuntp)].siz = strlen(qp);
			}
			(*cuntp)++;
			fr->total++;
		}
	}
	if (wanttable)
		(*dimp) = Z_Realloc((*dimp), (*cuntp) * sizeof (struct searchdim),
				PU_STATIC, NULL);
}

static void writesimplefreq(mapsearchfreq_t *fr, INT32 *frc,
		INT32 mapnum, UINT8 pos, UINT8 siz)
{
	fr[(*frc)].mapnum = mapnum;
	fr[(*frc)].matchd = ZZ_Alloc(sizeof (struct searchdim));
	fr[(*frc)].matchd[0].pos = pos;
	fr[(*frc)].matchd[0].siz = siz;
	fr[(*frc)].matchc = 1;
	fr[(*frc)].total = 1;
	(*frc)++;
}

INT32 G_FindMap(const char *mapname, char **foundmapnamep,
		mapsearchfreq_t **freqp, INT32 *freqcp)
{
	INT32 newmapnum = 0;
	INT32 mapnum;
	INT32 apromapnum = 0;

	size_t      mapnamelen;
	char   *realmapname = NULL;
	char   *newmapname = NULL;
	char   *apromapname = NULL;
	char   *aprop = NULL;

	mapsearchfreq_t *freq;
	boolean wanttable;
	INT32 freqc;
	UINT8 frequ;

	INT32 i;

	mapnamelen = strlen(mapname);

	/* Count available maps; how ugly. */
	for (i = 0, freqc = 0; i < NUMMAPS; ++i)
	{
		if (mapheaderinfo[i])
			freqc++;
	}

	freq = ZZ_Calloc(freqc * sizeof (mapsearchfreq_t));

	wanttable = !!( freqp );

	freqc = 0;
	for (i = 0, mapnum = 1; i < NUMMAPS; ++i, ++mapnum)
		if (mapheaderinfo[i])
	{
		if (!( realmapname = G_BuildMapTitle(mapnum) ))
			continue;

		aprop = realmapname;

		/* Now that we found a perfect match no need to fucking guess. */
		if (strnicmp(realmapname, mapname, mapnamelen) == 0)
		{
			if (wanttable)
			{
				writesimplefreq(freq, &freqc, mapnum, 0, mapnamelen);
			}
			if (newmapnum == 0)
			{
				newmapnum = mapnum;
				newmapname = realmapname;
				realmapname = 0;
				Z_Free(apromapname);
				if (!wanttable)
					break;
			}
		}
		else
		if (apromapnum == 0 || wanttable)
		{
			/* LEVEL 1--match keywords verbatim */
			if (( aprop = strcasestr(realmapname, mapname) ))
			{
				if (wanttable)
				{
					writesimplefreq(freq, &freqc,
							mapnum, aprop - realmapname, mapnamelen);
				}
				if (apromapnum == 0)
				{
					apromapnum = mapnum;
					apromapname = realmapname;
					realmapname = 0;
				}
			}
			else/* ...match individual keywords */
			{
				freq[freqc].mapnum = mapnum;
				measurekeywords(&freq[freqc],
						&freq[freqc].matchd, &freq[freqc].matchc,
						realmapname, mapname, wanttable);
				measurekeywords(&freq[freqc],
						&freq[freqc].keywhd, &freq[freqc].keywhc,
						mapheaderinfo[i]->keywords, mapname, wanttable);
				if (freq[freqc].total)
					freqc++;
			}
		}

		Z_Free(realmapname);/* leftover old name */
	}

	if (newmapnum == 0)/* no perfect match--try a substring */
	{
		newmapnum = apromapnum;
		newmapname = apromapname;
	}

	if (newmapnum == 0)/* calculate most queries met! */
	{
		frequ = 0;
		for (i = 0; i < freqc; ++i)
		{
			if (freq[i].total > frequ)
			{
				frequ = freq[i].total;
				newmapnum = freq[i].mapnum;
			}
		}
		if (newmapnum)
		{
			newmapname = G_BuildMapTitle(newmapnum);
		}
	}

	if (freqp)
		(*freqp) = freq;
	else
		Z_Free(freq);

	if (freqcp)
		(*freqcp) = freqc;

	if (foundmapnamep)
		(*foundmapnamep) = newmapname;
	else
		Z_Free(newmapname);

	return newmapnum;
}

void G_FreeMapSearch(mapsearchfreq_t *freq, INT32 freqc)
{
	INT32 i;
	for (i = 0; i < freqc; ++i)
	{
		Z_Free(freq[i].matchd);
	}
	Z_Free(freq);
}

INT32 G_FindMapByNameOrCode(const char *mapname, char **realmapnamep)
{
	boolean usemapcode = false;
	INT32 newmapnum = -1;
	size_t mapnamelen = strlen(mapname);
	char *p;

	if (mapnamelen == 1)
	{
		if (mapname[0] == '*') // current map
		{
			usemapcode = true;
			newmapnum = gamemap;
		}
		else if (mapname[0] == '+' && mapheaderinfo[gamemap-1]) // next map
		{
			usemapcode = true;
			newmapnum = mapheaderinfo[gamemap-1]->nextlevel;
			if (newmapnum < 1 || newmapnum > NUMMAPS)
			{
				CONS_Alert(CONS_ERROR, M_GetText("NextLevel (%d) is not a valid map.\n"), newmapnum);
				return 0;
			}
		}
	}
	else if (mapnamelen == 2)/* maybe two digit code */
	{
		if (( newmapnum = M_MapNumber(mapname[0], mapname[1]) ))
			usemapcode = true;
	}
	else if (mapnamelen == 5 && strnicmp(mapname, "MAP", 3) == 0)
	{
		if (( newmapnum = M_MapNumber(mapname[3], mapname[4]) ))
			usemapcode = true;
	}

	if (!usemapcode)
	{
		/* Now detect map number in base 10, which no one asked for. */
		newmapnum = strtol(mapname, &p, 10);
		if (*p == '\0')/* we got it */
		{
			if (newmapnum < 1 || newmapnum > NUMMAPS)
			{
				CONS_Alert(CONS_ERROR, M_GetText("Invalid map number %d.\n"), newmapnum);
				return 0;
			}
			usemapcode = true;
		}
		else
		{
			newmapnum = G_FindMap(mapname, realmapnamep, NULL, NULL);
		}
	}

	if (usemapcode)
	{
		/* we can't check mapheaderinfo for this hahahaha */
		if (W_CheckNumForName(G_BuildMapName(newmapnum)) == LUMPERROR)
			return 0;

		if (realmapnamep)
			(*realmapnamep) = G_BuildMapTitle(newmapnum);
	}

	return newmapnum;
}

//
// G_SetGamestate
//
// Use this to set the gamestate, please.
//
void G_SetGamestate(gamestate_t newstate)
{
	gamestate = newstate;
}

/* These functions handle the exitgame flag. Before, when the user
   chose to end a game, it happened immediately, which could cause
   crashes if the game was in the middle of something. Now, a flag
   is set, and the game can then be stopped when it's safe to do
   so.
*/

// Used as a callback function.
void G_SetExitGameFlag(void)
{
	exitgame = true;
}

void G_ClearExitGameFlag(void)
{
	exitgame = false;
}

boolean G_GetExitGameFlag(void)
{
	return exitgame;
}

// Same deal with retrying.
void G_SetRetryFlag(void)
{
	retrying = true;
}

void G_ClearRetryFlag(void)
{
	retrying = false;
}

boolean G_GetRetryFlag(void)
{
	return retrying;
}

void G_SetModeAttackRetryFlag(void)
{
	retryingmodeattack = true;
	G_SetRetryFlag();
}

void G_ClearModeAttackRetryFlag(void)
{
	retryingmodeattack = false;
}

boolean G_GetModeAttackRetryFlag(void)
{
	return retryingmodeattack;
}

// Time utility functions
INT32 G_TicsToHours(tic_t tics)
{
	return tics/(3600*TICRATE);
}

INT32 G_TicsToMinutes(tic_t tics, boolean full)
{
	if (full)
		return tics/(60*TICRATE);
	else
		return tics/(60*TICRATE)%60;
}

INT32 G_TicsToSeconds(tic_t tics)
{
	return (tics/TICRATE)%60;
}

INT32 G_TicsToCentiseconds(tic_t tics)
{
	return (INT32)((tics%TICRATE) * (100.00f/TICRATE));
}

INT32 G_TicsToMilliseconds(tic_t tics)
{
	return (INT32)((tics%TICRATE) * (1000.00f/TICRATE));
}
