/*****************************************************************************
 * LuaDB :: uuid.h
 *
 * Generate Universally Unique Identifiers in Lua (and in C).
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_UUID_H
#define LUADB_UUID_H

/**
 * @brief Add the UUID library to the global Lua state.
 */
void luadb_add_uuid_lib(lua_State *L);

/**
 * @brief Create a new UUID value.
 */
int luadb_uuid_uuid(lua_State *L);

/**
 * @brief Create a new UUID value. Callers are responsible for freeing the
 * returned value.
 */
char *luadb_create_guid();

#endif //LUADB_UUID_H
