/*****************************************************************************
 * LuaDB :: json.c
 *
 * JSON to Lua table (and vice versa) functions.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/json/json.h"

#include "json.h"
#include "util.h"

static const char *const JSON_ARRAY_METAFIELD = "_json_array";
#ifndef _WIN32
static const int JSON_STRING_KEY_DIGITS = 15;
#else  //_WIN32
#define JSON_STRING_KEY_DIGITS 15
#endif //_WIN32

/*
 * FORWARD DECLARATIONS
 */

static inline void JsonToLuaValue(lua_State *L, JsonNode *json);
static JsonNode *LuaValueToJson(lua_State *L);
static void JsonToLuaValuePrivate(lua_State *L, JsonNode *json, int i);
static inline void SetTableAsJsonArray(lua_State *L, int idx);
static JsonNode *LuaTableToJsonPrivate(lua_State *L, int idx);
static bool LuaNumberIsInt(lua_Number n, int *v);
static inline bool LuaTableIsJsonArray(lua_State *L, int idx);

// Library functions
static luaL_Reg json_lib_funcs[] = {
        { "decode", LuaDB_JsonDecode},
        { "encode", LuaDB_JsonEncode},
        { "isarray", LuaDB_JsonIsArray},
        { "makearray", LuaDB_JsonMakeArray},
        { NULL, NULL },
};

/*
 * PUBLIC FUNCTIONS
 */

void LuaDB_JsonAddLib(lua_State *L) {
    assert(L);

    luaL_newlib(L, json_lib_funcs);
    lua_setglobal(L, "json");
}

int LuaDB_JsonDecode(lua_State *L) {
    size_t len;
    const char *str = luaL_checklstring(L, 1, &len);

    JsonNode *json = json_decode(str);
    if (!json) {
        lua_pushnil(L);
        return 1;
    }

    JsonToLuaValue(L, json);
    json_delete(json);
    return 1;
}

int LuaDB_JsonEncode(lua_State *L) {
    JsonNode *json = LuaValueToJson(L);
    char *str = json_encode(json);
    if (!str) {
        free(json);
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, str);
    json_delete(json);
    free(str);
    return 1;
}

int LuaDB_JsonIsArray(lua_State *L) {
    int istable = lua_istable(L, 1);
    if (!istable) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int isarray = (int) LuaTableIsJsonArray(L, 1);
    lua_pushboolean(L, isarray);
    return 1;
}

int LuaDB_JsonMakeArray(lua_State *L) {
    int istable = lua_istable(L, 1);
    if (!istable) {
        luaL_error(L, "can only make tables into JSON arrays");
        return 0;
    }

    SetTableAsJsonArray(L, 1);
    return 0;
}

/*
 * PRIVATE FUNCTIONS
 */

// Convert a JsonNode structure into a Lua value.
static inline void JsonToLuaValue(lua_State *L, JsonNode *json) {
    assert(L);
    assert(json);

    JsonToLuaValuePrivate(L, json, 1);
}

// Convert a Lua value into a JsonNode structure.
static JsonNode *LuaValueToJson(lua_State *L) {
    assert(L);

    JsonNode *json = NULL;
    int top = lua_gettop(L);
    int type = lua_type(L, top);

    switch (type) {
        case LUA_TTABLE:
            json = LuaTableToJsonPrivate(L, top);
            break;
        case LUA_TSTRING:
            json = json_mkstring(luaL_tolstring(L, top, NULL));
            lua_pop(L, 1);
            break;
        case LUA_TNUMBER: {
            int isnum;
            lua_Number num = lua_tonumberx(L, top, &isnum);
            if (isnum) {
                json = json_mknumber(num);
            }
            break;
        }
        case LUA_TBOOLEAN:
            json = json_mkbool((bool)lua_toboolean(L, top));
            break;
        case LUA_TNIL:
            json = json_mknull();
            break;
        default:
            break;
    }

    return json;
}

// Private function to convert a JsonNode into a Lua table which
// can be called recursively for JSON arrays and objects and
// will handle array indices (1-indexed, as in Lua).
static void JsonToLuaValuePrivate(lua_State *L, JsonNode *json, int i) {
    assert(L);
    assert(json);

    bool need_key = ((json->parent) && (lua_istable(L, -1)));
    JsonNode *child;

    // Verify that our stack is large enough for the operation
    luaL_checkstack(L, 3, "out of memory");

    // Push the key onto the stack if needed
    if (need_key) {
        switch (json->parent->tag) {
            case JSON_ARRAY:
                lua_pushinteger(L, i);
                i = 1;  // Reset the automatic table indices if this is a nested table
                break;
            case JSON_OBJECT:
                lua_pushstring(L, json->key);
                break;
            default:
                break;
        }
    }

    // Push the value onto the stack
    switch (json->tag) {
        case JSON_OBJECT:
            lua_newtable(L);
            json_foreach(child, json) {
                JsonToLuaValuePrivate(L, child, 1);
            }
            break;
        case JSON_ARRAY:
            lua_newtable(L);
            SetTableAsJsonArray(L, lua_gettop(L));
            json_foreach(child, json) {
                JsonToLuaValuePrivate(L, child, i++);
            }
            break;
        case JSON_STRING:
            lua_pushstring(L, json->string_);
            break;
        case JSON_NUMBER:
            lua_pushnumber(L, json->number_);
            break;
        case JSON_BOOL:
            lua_pushboolean(L, json->bool_);
            break;
        case JSON_NULL:
            lua_pushnil(L);
            break;
    }

    // Push the entire field into the table
    if (need_key) {
        lua_settable(L, -3);
    }
}

// Set the value at the top of the stack as a JSON array
// by way of marking it with the meta-field _json_array
static inline void SetTableAsJsonArray(lua_State *L, int idx) {
    luaL_checkstack(L, 3, "out of memory");
    if (!lua_getmetatable(L, idx)) {
        lua_createtable(L, 0, 1);
    }
    lua_pushstring(L, JSON_ARRAY_METAFIELD);
    lua_pushboolean(L, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, (idx < 0) ? (idx-1) : (idx));
}

// Convert a Lua table (ONLY) to JsonNode structure.
static JsonNode *LuaTableToJsonPrivate(lua_State *L, int idx) {
    assert(L);

    JsonNode *json;
    const char *orig = NULL;
    char *key = NULL;
    char numstr[JSON_STRING_KEY_DIGITS];
    lua_Number num;
    int inum = 0;
    size_t len;

    luaL_checkstack(L, 2, "out of memory");

    // Decide whether or not we're creating an array
    bool as_array = LuaTableIsJsonArray(L, idx);
    json = (as_array) ? json_mkarray() : json_mkobject();

    // Traverse the Lua table
    lua_pushnil(L);
    while (lua_next(L, idx) != 0) {
        bool free_key = false;
        // Key (index -2), Value (index -1)
        int ktype = lua_type(L, idx+1);

        // Make sure we got a valid key (string or number)
        switch (ktype) {
            case LUA_TNUMBER:
                num = lua_tonumber(L, idx+1);
                if (LuaNumberIsInt(num, &inum)) {
                    sprintf(numstr, "%d", inum);
                } else {
                    sprintf(numstr, "%f", num);
                }
                key = &numstr[0];
                break;
            case LUA_TSTRING:
                orig = luaL_tolstring(L, idx+1, &len);
                key = LuaDB_StrDupLen(orig, len);
                free_key = true;
                lua_pop(L, 1);
                break;
            default:
                luaL_error(L, "table keys may only be strings or numbers");
                /* code unreachable since luaL_error does not return */
                return NULL;
        }

        // Generate the child node
        JsonNode *child = LuaValueToJson(L);

        // Pop the value off the stack
        lua_pop(L, 1);

        // Build the JSON value
        if (as_array) {
            json_append_element(json, child);
        } else {
            json_append_member(json, key, child);
            if (free_key) { free(key); }
        }
    }

    return json;
}

// Check if a Lua number is integral and return that value in v if so.
static bool LuaNumberIsInt(lua_Number n, int *v) {
    *v = (int)n;
    return ((n - (double)*v) == 0.0);
}

// Return true if the value at the given index is considered a JSON array.
static inline bool LuaTableIsJsonArray(lua_State *L, int idx) {
    bool is_array = (luaL_getmetafield(L, idx, JSON_ARRAY_METAFIELD) != LUA_TNIL);
    if (is_array) { lua_pop(L, 1); }
    return is_array;
}
