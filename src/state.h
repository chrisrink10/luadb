/*****************************************************************************
 * LuaDB :: state.h
 *
 * LuaDB state creation functions
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_STATE_H
#define LUADB_STATE_H

/**
 * @brief Create a new @c lua_State object instantiated with
 * the correct LuaDB libraries in the namespace.
 */
lua_State *LuaDB_NewState();

/**
 * @brief Add the given path to the Lua `package.path` global exactly
 * as it is given.
 */
void LuaDB_PathAddAbsolute(lua_State *L, const char *path);

/**
 * @brief Add the relative path of the file specified (so that files
 * in that directory are included in the package path).
 *
 * Note that as a consequence of using `dirname` for the path selection,
 * this function will select directories such as "/var/www" from even
 * fully qualified directory paths (e.g. "/var/www/luadb/").
 */
void LuaDB_PathAddRelative(lua_State *L, const char *path);

#endif //LUADB_STATE_H
