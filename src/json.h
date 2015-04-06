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
 * @brief C function for the Lua "json.decode(...)".
 *
 * Decode a JSON value from a string into a valid Lua value. Objects
 * are decoded as tables. Arrays are decoded as 1-indexed tables.
 * All other values are converted to their natural Lua equivalents.
 */
int luadb_json_decode(lua_State *L);

/**
 * @brief C function for the Lua "json.encode(...)".
 *
 * Encode the Lua input value as valid JSON. Note that since JSON
 * cannot encode every Lua value, there are certain types of
 * values which will produce a Lua error.
 *
 * Table keys may only be strings and numeric values. Tables are
 * only created as JSON arrays if they are endowed with the
 * Lua metafield "_json_array", which the `json` module function
 * `makearray` can give. Callers can test if a table will be
 * considered as a JSON array by using the function `isarray`.
 *
 * Attempting to encode a value which would produce a reference
 * cycle will produce a Lua error.
 */
int luadb_json_encode(lua_State *L);

/**
 * @brief C function for the Lua "json.isarray(...)"
 *
 * Returns `true` if the input argument is a table and that
 * table possesses the metafield "_json_array". Returns
 * `false` otherwise.
 */
int luadb_json_isarray(lua_State *L);

/**
 * @brief C function for the Lua "json.makearray(...)"
 *
 * Endows the input argument with the meta-field "_json_array"
 * if it is a table. Produces an error if the argument is not
 * a table.
 */
int luadb_json_makearray(lua_State *L);

#endif //LUADB_JSON_H
