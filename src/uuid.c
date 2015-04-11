/*****************************************************************************
 * LuaDB :: uuid.c
 *
 * Generate Universally Unique Identifiers in Lua (and in C).
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef LUADB_USE_UUID
#include <uuid/uuid.h>
#else  //LUADB_USE_UUID
#ifdef _WIN32
#include <objbase.h>
#else  //_WIN32
#include <math.h>
#endif //_WIN32
#endif //LUADB_USE_UUID

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"

#include "uuid.h"

/*
 * FORWARD DECLARATIONS
 */

static char *create_uuid_uuid();
static char *create_uuid_win32();
static char *create_uuid_rand();

// Library functions
static luaL_Reg uuid_lib_funcs[] = {
        { "uuid", luadb_uuid_uuid },
        { NULL, NULL },
};

/*
 * PUBLIC FUNCTIONS
 */

void luadb_add_uuid_lib(lua_State *L) {
    assert(L);

    luaL_newlib(L, uuid_lib_funcs);
    lua_setglobal(L, "uuid");
}

int luadb_uuid_uuid(lua_State *L) {
    char *uuid = luadb_create_guid();
    if (!uuid) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, uuid);
    free(uuid);
    return 1;
}

char *luadb_create_guid() {
#ifdef LUADB_USE_UUID
    return create_uuid_uuid();
#else  //LUADB_USE_UUID
    #ifdef _WIN32
    return create_uuid_win32();
    #else  //_WIN32
    return create_uuid_rand();
    #endif //_WIN32
#endif //LUADB_USE_UUID
}

/*
 * PRIVATE FUNCTIONS
 */

// Create a UUID using uuid.h defined on most *NIX systems.
static char *create_uuid_uuid() {
#ifdef LUADB_USE_UUID
    char *out = malloc(sizeof(uuid_string_t) + 1);
    if (!out) {
        return NULL;
    }

    uuid_t uuid;
    uuid_string_t uuid_str;
    uuid_generate_random(uuid);
    uuid_unparse(uuid, uuid_str);

    sprintf(out, "%s", uuid_str);
    return out;
#else  //LUADB_USE_UUID
    return NULL;
#endif
}

// Create a UUID using the Windows API.
static char *create_uuid_win32() {
#ifdef _WIN32
    char *out = malloc((sizeof(GUID) * 2) + 1);
    if (!out) {
        return NULL;
    }

    GUID guid;
    HRESULT res = CoCreateGuid(&guid);
    if (res != S_OK) {
        free(out);
        return NULL;
    }

    __mingw_sprintf(out, "%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
            guid.Data1, guid.Data2, guid.Data3,
            guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
            guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return out;
#else  //_WIN32
    return NULL;
#endif
}

// Create a "UUID" using the rand() function from the C math library.
static char *create_uuid_rand() {
    char *out = malloc(RAND_MAX);
    if (!out) {
        return NULL;
    }

    int uuid = rand();

    sprintf(out, "%d", uuid);
    return out;
}