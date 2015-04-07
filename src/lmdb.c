/*****************************************************************************
 * LuaDB :: lmdb.c
 *
 * LMDB to Lua library
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/lmdb/lmdb.h"

#include "lmdb.h"

int luadb_lmdb_open_env(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

int luadb_lmdb_version(lua_State *L) {
    char *version = mdb_version(NULL, NULL, NULL);
    lua_pushstring(L, version);
    return 1;
}