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
lua_State *luadb_new_state();

/**
 * @brief Add the relative path of the file specified (so that files
 * in that directory are included in the package path).
 */
void luadb_set_relative_path(lua_State *L, const char *path);

#endif //LUADB_STATE_H
