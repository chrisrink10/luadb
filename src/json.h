/*****************************************************************************
 * LuaDB :: json.h
 *
 * JSON to Lua table (and vice versa) functions.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_JSON_H
#define LUADB_JSON_H

/**
 * @brief C function for the lua "json.decode(...)".
 */
int luadb_json_decode(lua_State *L);

/**
 * @brief C function for the lua "json.encode(...)".
 */
int luadb_json_encode(lua_State *L);

#endif //LUADB_JSON_H
