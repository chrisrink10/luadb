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
void LuaDB_UuidAddLib(lua_State *L);

/**
 * @brief Create a new UUID value.
 */
int LuaDB_UuidUuid(lua_State *L);

/**
 * @brief Create a new UUID value. Callers are responsible for freeing the
 * returned value.
 */
char *LuaDB_CreateGuid(void);

#endif //LUADB_UUID_H
