/*****************************************************************************
 * LuaDB :: state.c
 *
 * LuaDB state creation functions
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"
#include "deps/lua/lauxlib.h"

#include "json.h"
#include "lmdb.h"
#include "state.h"

#ifdef _WIN32
static const char *const PATH_SEPARATOR = "\\";
#else  //_WIN32
static const char *const PATH_SEPARATOR = "/";
#endif //_WIN32

/*
 * FORWARD DECLARATIONS
 */

static void luadb_add_json_lib(lua_State *L);
static void luadb_add_lmdb_lib(lua_State *L);
static char *luadb_path(const char *cur_path, size_t len, const char *path);

/*
 * FORWARD DECLARATIONS
 */

lua_State *luadb_new_state() {
    lua_State *L = luaL_newstate();
    if (!L) {
        return NULL;
    }

    luaL_openlibs(L);
    luadb_add_json_lib(L);
    luadb_add_lmdb_lib(L);

    return L;
}

void luadb_set_relative_path(lua_State *L, const char *path) {
    assert(L);
    assert(path);

    // Get the package.path variable
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    size_t len;
    const char *cur_path = lua_tolstring(L, -1, &len);

    // Get the directory path
    char *newpath = luadb_path(cur_path, len, path);
    if (!newpath) {
        return;
    }

    // Replace the package path string
    lua_pop(L, 1);
    lua_pushstring(L, newpath);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
    free(newpath);
    return;
}

/*
 * PRIVATE FUNCTIONS
 */

// Add the JSON library functions to the Lua state.
static void luadb_add_json_lib(lua_State *L) {
    assert(L);

    static luaL_Reg json_lib[] = {
            { "decode", luadb_json_decode },
            { "encode", luadb_json_encode },
            { "isarray", luadb_json_isarray },
            { "makearray", luadb_json_makearray },
    };

    luaL_newlib(L, json_lib);
    lua_setglobal(L, "json");
}

// Add the LMDB library functions to the Lua state.
static void luadb_add_lmdb_lib(lua_State *L) {
    assert(L);

    static luaL_Reg lmdb_lib[] = {
            { "open", luadb_lmdb_open_env },
            { "version", luadb_lmdb_version },
    };

    luaL_newlib(L, lmdb_lib);
    lua_setglobal(L, "lmdb");
}

// Update the current Lua package path with the input
// path and return the updated path.
static char *luadb_path(const char *cur_path, size_t len, const char *path) {
    // Create a copy of path, since dirname may modify the output
    size_t pathlen = strlen(path);
    char *pathcpy = malloc(pathlen + 1);
    if (!pathcpy) {
        return NULL;
    }
    memcpy(pathcpy, path, pathlen);
    pathcpy[pathlen] = '\0';

    // Create space for the new path
    char *dir = dirname(pathcpy);
    char *newpath = malloc(len + pathlen + 6 + 2);
    if (!newpath) {
        return NULL;
    }

    // Generate the new path
    sprintf(newpath, "%s;%s%s?.lua", cur_path, dir, PATH_SEPARATOR);
    free(pathcpy);
    return newpath;
}