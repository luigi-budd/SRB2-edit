# Sonic Robo Blast 2
[![latest release](https://badgen.net/github/release/STJr/SRB2/stable)](https://github.com/STJr/SRB2/releases/latest)

[![Build status](https://ci.appveyor.com/api/projects/status/399d4hcw9yy7hg2y?svg=true)](https://ci.appveyor.com/project/STJr/srb2)
[![Build status](https://travis-ci.org/STJr/SRB2.svg?branch=master)](https://travis-ci.org/STJr/SRB2)
[![CircleCI](https://circleci.com/gh/STJr/SRB2/tree/master.svg?style=svg)](https://circleci.com/gh/STJr/SRB2/tree/master)

[Sonic Robo Blast 2](https://srb2.org/) is a 3D Sonic the Hedgehog fangame based on a modified version of [Doom Legacy](http://doomlegacy.sourceforge.net/).

## Dependencies
- SDL2 (Linux/OS X only)
- SDL2-Mixer (Linux/OS X only)
- libupnp (Linux/OS X only)
- libgme (Linux/OS X only)
- libopenmpt (Linux/OS X only)

## Disclaimer
Sonic Team Junior is in no way affiliated with SEGA or Sonic Team. We do not claim ownership of any of SEGA's intellectual property used in SRB2.

----------------------

<p align="center">
  <img width="500" height="150" alt="SRB2-edit" src="https://raw.githubusercontent.com/luigi-budd/SRB2-edit/refs/heads/master/srb2banner.png" />
</p>

# About SRB2-edit

SRB2-edit is a source mod of Sonic Robo Blast 2 aimed at adding more development/modding tools, and general gameplay and QOL improvements, without getting in the way of netplay and performance.

## Compiling

See [SRB2 Wiki/Source code compiling](http://wiki.srb2.org/wiki/Source_code_compiling)

If you get compilation errors referring to booleans and/or pointers, try reverting [this commit](https://github.com/luigi-budd/SRB2-edit/commit/8b70f986a65a735030e611c0bcf36161b4cdd505) and/or [this commit](https://github.com/luigi-budd/SRB2-edit/commit/2160051f055eed0fa1cdf0f4034534f60dfe2c0a) and [this commit](https://github.com/luigi-budd/SRB2-edit/commit/0cb43b90763d58386bf97ab6fcf732636cb5d48e) (or [this one](https://github.com/luigi-budd/SRB2-edit/commit/6acca940af796845b64ec6a3db74451735c9c023))

## Installation:

You can compile the source code normally (see "Compiling") and put the binary in your SRB2 directory. Don't forget to install the Discord RPC libraries as well.

If you're downloading a release from the Actions tab, make sure to download the correct Discord RPC libraries from the source code and put them in your SRB2 directory. The game will not start otherwise.

Please refer to `libs/DLL-README.txt` if you need help locating any libraries.

** You will need a GitHub account if you want to download from the Actions tab. **

# Settings

All settings SRB2-edit adds can be found in game inside the "SRB2-edit Options..." menu.

Binds for "Pause GIF Recording" and "Local-addon Toggle" can be found in the Player 1 Controls, under the "Meta" category.

## UI & Menus
* **Show Server Viewer:** (`showserverinfo`)
  
  Adds a menu detailing a server's info before you join. Lists addons, players, gametype, level, gamemode, and various other useful details.
  
  (Code originally from [SRB2Classic](https://codeberg.org/srb2classic/srb2classic))

* **Uppercase Menus:** (`menucaps`)

  Adds a toggle for the all-uppercase menus, similar to SRB2Kart Saturn!

* **Menu Highlight Color & Menu Color:** (`menuhighlight` & `menucolor`)

  Adjustable colors for most of the game's menus. Time Attack, Marathon, and NiGHTS Attack remain unchanged.

You can load local addons through the Addons menu by pressing R-ALT.

## HUD & Visual
* **Show TPS:** (`showtps`)

  Adds a TPS counter alongside your FPS counter.

  (Code from [SRB2Classic](https://codeberg.org/srb2classic/srb2classic) and TSoURDt3rd)

* **Compact FPS/TPS/Ping Info:** (`compactinfo`)

  Compacts all 3 displays into 1 line.

* **Ping Measurement:** (`pingmeasurement`)

  Measures latency in milliseconds or frames, just like SRB2Kart.

* **Screen Flashes, Screenshake, Screenwipes:** (`flashes`, `earthquake`, `wipes`)

  Togglable settings for anything that might be intrusive. Screenshake can also be set to look similar to Ring Racer's screenshake.

  (Screenshake toggle originally from SRB2Classic, ported to Edit by @archiNiko)

* **Caption Interpolation:** (`mischudinterpolation`)

  Self explanatory. Only works with closed captioning.

* **Inverted Crosshair (P1/P2):** (`crosshair(2)_invert`)

  Crosshairs can be set to invert the colors behind them, similar to Minecraft. Can help with visibility.

## Chat
The chat window's position can be freely adjusted. Have fun!

## Movie Mode
* **Movie Info:** (`moviemodeinfo`)

  Draws runtime and file size while recording gifs/aPNGs. Movies can also be paused with F2.

* **Max Movie Size (mb):** (`gif_maxsize`)

  Automatically stops recording when reaching this cap. Setting the cap to 0 disables it.

* **Continous Recording:** (`gif_rolling`)

  Only works when Max Movie Size is enabled. When the size limit is reached, recording will stop and immediately being recording a new movie.

## Gameplay
* **Analog Snapping (P1/P2):** (`joy(2)_snapping`)

  Snaps gamepad movement to 8 directions, similar to Super Mario 3D World.

* **Minimum Latency:** (`mindelay`)

  When on, your controls will be delayed by this many frames.

## Pitch & Roll Rotation (Slopes)
* **3D Rotation:** (`pitchroll-tation`)

  Toggles 3D rotations for sprites and models. Most visible when walking on slopes.

* **Rotation Decay** (`pitchroll-easing`)

  When airborne, you'll steadily orient yourself rightside-up. Most visible when going up half-pipes or when jumping off slopes.

## Debugging / Diagnostics
* **Force Automap:** (`forceautomap`)

  Allows the automap to be used in netgames and in singleplayer, without the need for devmode.

* **Show C-Says/C-Echos:** (`showcsays`)

  Prints any CSays/CEchos to the console.

* **CVar Info:** (`cvarinfo`)

  Toggles certain information from being printed when inputting a cvar's name into the console.

## OpenGL
* **Model Translations:** (`gr_modeltranslations`)

  Models can be affected by translations when on. Note that this may cause lag for models with high resolution textures.

* **Render Distance:** (`gr_renderdistance`)

  Applies render distance cap in OpenGL.

  (Code from [SRB2Classic](https://git.srb2.org/Hanicef/SRB2Classic/-/merge_requests/4), @GLideKS)

## Discord Integration
SRB2-edit provides Discord Rich Presence support. This feature can be toggled off with the `discordrp` cvar. Rich Presence also allows other users to join your netgames through Discord (`discordasks`), and for you to send out invites to netgames in Discord channels.

<p align="center">
  <img width="296" height="190" alt="SRB2-edit" src="https://raw.githubusercontent.com/luigi-budd/SRB2-edit/refs/heads/master/discordrp.png" />
</p>

# Changes
Several QOL changes and bug fixes have been added to SRB2-edit.

## UI & Menus
- Joining netgames now show progress bars when "checking files" and downloading files (Code from [Lugent's PR](https://git.do.srb2.org/STJr/SRB2/-/merge_requests/2446), [Lugent's PR](https://git.do.srb2.org/STJr/SRB2/-/merge_requests/2556))
- Easily rejoin servers you've played before! ("`connect last`", Multiplayer -> Rejoin Previous Servers...)
- Snake download game background fixed!
- Gamepads can now navigate menus with the D-Pad.
- Several menus now have backgrounds for better readability.
- The volume for the countdown beeps in race/tag gamemodes has been fixed.

## Visual
- Better "Fake Contrast"! (https://git.do.srb2.org/STJr/SRB2/-/merge_requests/2680, @GLideKS)
- View rollangle is interpolated!
- FOV changes are interpolated! (use `"fovchange"` to see this in action!)
- OpenGL Lack-of-Perspective can now be active only in 2D-Mode.

## Gameplay / Netplay
- Skin change at any time
- Addfilelocal from SRB2K Saturn! (use "`addfilelocal`" command or press R-ALT in the addons menu)
- Addfolderlocal!
- Improved startup times! (Code from [SRB2Classic](https://codeberg.org/srb2classic/srb2classic))
- "`cam_centertoggle`" and "`cam2_centertoggle`" are no longer exclusive to Automatic!
- See private messages as host! (Code from [SRB2Classic](https://codeberg.org/srb2classic/srb2classic))
- "Invisicam!" ("`cam_invisicam`", makes the player more transparent the closer they are to the camera, to help with visibility)
- Centering your camera with a gamepad will now snap your camera angle to the direction you are moving in, similar to Splatoon.

## Modding and Debugging
- "`renderhitbox`" in multiplayer
- Lua HUD interpolation from SRB2K Saturn
- "`freezelevel`" debug command (Will cause desynchs for clients!)
- HUD camera struct updates position in first person! (credits [Jiskster](https://git.do.srb2.org/STJr/SRB2/-/merge_requests/2629) & [Hanicef](https://git.do.srb2.org/Hanicef/SRB2Classic/-/commit/681bd160f5be3925a97d798d00e67b32a8c1df71))
- `v.cachePatch` accepts a second parameter for rotation! (https://git.do.srb2.org/STJr/SRB2/-/merge_requests/2662)
- Added "`TR`" as an alias to "`TICRATE`" in Lua
- "`getlogfile`" command (Prints the absolute path of the current log, useful when latest-log.txt is sym-linked to a different log)

## Console & Misc. Commands
- `help` now lists commands and variables by origin. Parameters are as follows:
  | Param      | Description      |
  | ------------- | ------------- |
  | `-v` | Only show variables and/or commands from vanilla SRB2 only.  |
  | `-c` | Only show variables and/or commands that are in SRB2-edit, and not vanilla. |
  | `-a` | Only show variables and/or commands created by addons. |

- Console variables can no longer be used as an argument for `help`, they now print their info instead of just their current and default value when entered into the console alone. "`cvarinfo`" lets you hide the flags and origin sections ("Show All" by default).
- "`cycle`" command (`cycle <cvar> [values]`): Inaccessible by Lua. Cycles given values on the cvar if the current value is found in the list (also loops around). Fails if the current value is not found, unless `-b` is specified (starts at the first arg if so).

# Lua Additions

## Global variables
- "`edit_custombuild`" (Read only) (boolean) :  Global to detect if the client is using this build.
- "`edit_complexlocaladdons`" (Read only) (boolean) : Global to detect if the client has loaded local addons with lua in them.
- "`edit_locallyloading`" (Read only) (boolean) : Only set during script loading, detects whether the script is being loaded locally.

  Example:
  ```lua
  if (edit_locallyloading) then
    --do local stuff here
    return
  end
  --normal, gameplay editing code
  ```
  Please note that this variable will always return false during runtime, so you will need to store this variable in a different one to preserve it.
  
  Example:
  ```lua
  local addon_is_local = edit_locallyloading
  addHook("ThinkFrame",do
      if (addon_is_local) then
        print("This addon is local!")
        M_RandomRange(0,10)
      else
        print("This is a regular addon.")
        P_RandomRange(0,10)
      end
  end)
  ```
- "`demoplayback`" (Read only) (boolean) : True if viewing a demo.

*Note: `takis_*` variables are still recognized by the game, however, they have been deprecated and will be removed soon. Use their `edit_*` counterparts instead.*

## Functions
- `P_GetLocalAiming(player_t player)` : Returns the angle_t `aiming` of `player` if they are a local player. Returns 0 otherwise.
- `P_GetLocalAngle(player_t player)` : Returns the angle_t `angle` of `player` if they are a local player. Returns 0 otherwise.

- `R_CreateTranslation(string name, string translations...)` : Adds a custom translation, using the same parser as TRNSLATE.
- `R_RemoveTranslation(string name)` : Removes a custom translation. Can only remove translations made by Lua.
- `R_TranslationExists(string name)` : Returns true if a custom translation with a given name exists, false if not.

- `io.openlump(string filename, [string mode])` : Similar to `io.openlocal`, but reads a lump inside any addon loaded. Two new options are supported: `f` to scan addons forward from start to end, and `m` to only search in game-modifying addons.

  Example:
  ```lua
  local file = io.openlump("lua/main.lua","r")
  
  if file
  	local dat = file:read("*a")
  	print("Length: "..dat:len())
  	file:close()
  else
  	print("Could not read lump")
  end
  ```

- `v.interpolate/v.interpLatch(boolean/int)` : See [SRB2K Saturn's documentation](https://github.com/Indev450/SRB2Kart-Saturn/blob/Saturn/LUASTUFF.md)
- `v.drawFixedFill` : Same as `v.drawFill`, but x, y, width, and height arguments are all in fixed point scale.

- `M_Random`* : Same as `v.Random*` functions, except also client-sided and not limited to HUD hooks. Use these if you ever need to have randomness in a local addon outside of HUD hooks.

## mobj_t
- `mobj.pitch/roll` : Now rotates mobjs in 3D space, including models
- `mobj.resetinterp` : Resets ALL interpolation values. (`P_SetOrigin` only resets positional interpolation values)


Example that tilts your character in their 3D direction:
```lua
addHook("PlayerThink",function(p)
    local me = p.mo
    if not (me and me.valid) then return end

    local angle = R_PointToAngle2(0,0, me.momx,me.momy)
    local mang = R_PointToAngle2(0,0, FixedHypot(me.momx, me.momy), me.momz)
    mang = InvAngle($)

    me.roll = FixedMul(mang, sin(angle))
    me.pitch = FixedMul(mang, cos(angle))
end)
```

## player_t
- `player.ipaddress` (string) (read only): For use in moderation addons, this only returns a string for the server of the players IP address. Clients _cannot_ see other clients' IP addresses. The only way for other clients to know is if the server sends a command with them or something :p
- `player.muted` (boolean) (read + write): Returns whether or not the player is muted. (though changes may not be reflected in servers not running SRB2-edit)


## renderflags_t
- `RF_ALWAYSONTOP` : The sprite always draws on top of level geometry and other sprites. Not supported for models, and culled sprites wont be rendered. Note that in OpenGL, anything transparent will render on any `RF_ALWAYSONTOP` sprites
- `RF_HIDEINSKYBOX` : The sprite will be hidden in the skybox.
- `RF_NOMODEL` : The sprite will be forced to not use a model in OpenGL.


## eflags_t
- `MFE_NOPITCHROLLEASING` : When "pitchroll-easing" is toggled, adding this eflag will not ease the pitch/roll axis this tic. Removed at the end of MobjThinker.
