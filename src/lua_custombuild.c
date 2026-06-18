// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2014-2016 by John "JTE" Muniz.
// Copyright (C) 2014-2024 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_custombuild.c
/// \brief currently only used for lua scripts to detect this build

#include "doomdef.h"
#include "fastcmp.h"
#include "dehacked.h"
#include "deh_lua.h"
#include "lua_script.h"
#include "lua_libs.h"
#include "lua_custombuild.h"

/*
	For those making SRB2-edit forks:
		All "edit_" variables should remain unchanged, as
		this ensures consistant addon behavior across
		edit branches.

		You are free to remove the deprecated "takis_"
		variable fallbacks, just don't forget the
		deprecation message in lua_script.c!

		You may also create your own variables prefixed
		with your fork's name or whatever.

		Of course, I can't really force you to comply with
		these, but these guidelines should be used as a good
		rule of thumb.
*/

// if TRUE, the user loaded a local addon that has more than just music lumps
boolean edit_complexlocaladdons = false;

INT32 E_PushGlobals(lua_State *L, const char *word)
{
    if (
		fastcmp(word,"edit_custombuild") ||
		fastcmp(word,"takis_custombuild") // DEPRECATED
	) {
		lua_pushboolean(L, true);
		return 1;
	
    } else if (
		fastcmp(word, "edit_complexlocaladdons") ||
		fastcmp(word, "takis_complexlocaladdons") // DEPRECATED
	) {
		lua_pushboolean(L, edit_complexlocaladdons);
		return 1;
	
	} else if (
		fastcmp(word, "edit_locallyloading") || 
		fastcmp(word, "takis_locallyloading") // DEPRECATED
	) {
        if (lua_locallyloading)
		    lua_pushboolean(L, true);
        else
            lua_pushboolean(L, false);
		return 1;
	
	// should this even be here...?
    } else if (
		fastcmp(word, "edit_lumpname") ||
		fastcmp(word, "takis_lumpname") // DEPRECATED
	) {
		if (!lua_lumploading)
			lua_pushnil(L);
		else if (lua_lumpname[0])
			lua_pushstring(L, lua_lumpname);
		return 1;
	}
    return 0;
}
