/*****************************************************************************
 * LuaDB :: lmdb.h
 *
 * LMDB to Lua library
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_LMDB_H
#define LUADB_LMDB_H

/**
 * @brief Add the LMDB library to the global Lua state.
 */
void LuaDB_LmdbAddLib(lua_State *L);

/**
 * @brief Create a new LMDB transaction.
 */
int LuaDB_LmdbOpenEnv(lua_State *L);

/**
 * @brief Return the version number of LMDB that LuaDB was compiled with.
 */
int LuaDB_LmdbVersion(lua_State *L);

#endif //LUADB_LMDB_H
