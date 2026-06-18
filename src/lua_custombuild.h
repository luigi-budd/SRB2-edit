// SONIC ROBO BLAST 2; TSOURDT3RD
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by Star "Guy Who Names Scripts After Him" ManiaKG.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lua_custombuild.h
/// \brief Custom Build BLua stuff

#ifndef __EDIT_LUA__
#define __EDIT_LUA__

#include "lua_script.h"

extern boolean edit_complexlocaladdons;

// now that srb2-edit is officially called... srb2-edit,
// all E functions will be related to srb2-Edit
INT32 E_PushGlobals(lua_State *L, const char *word);

#endif // __EDIT_LUA__