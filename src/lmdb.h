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
 * @brief Create a new LMDB transaction.
 */
int luadb_lmdb_open_env(lua_State *L);

/**
 * @brief Return the version number of LMDB that LuaDB was compiled with.
 */
int luadb_lmdb_version(lua_State *L);

#endif //LUADB_LMDB_H
