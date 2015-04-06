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

#ifndef LUADB_WIN32
#include <libgen.h>
#else
static const int WIN32_DRIVE_LEN = 5;
static const int WIN32_DIR_LEN = 150;
#endif

#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"
#include "deps/lua/lauxlib.h"

#include "json.h"
#include "state.h"

/*
 * FORWARD DECLARATIONS
 */

static void luadb_add_json_lib(lua_State *L);
static char *luadb_posix_path(const char *cur_path, const char *path, size_t len);
static char *luadb_win32_path(const char *cur_path, const char *path, size_t len);

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
    #ifdef LUADB_WIN32
    char *newpath = luadb_win32_path(cur_path, path, len);
    #else
    char *newpath = luadb_posix_path(cur_path, path, len);
    #endif
    if (!newpath) {
        return;
    }

    // Replace the package path string
    lua_pop(L, 1);
    lua_pushstring(L, newpath);
    lua_setfield(L, -2, "path");
    lua_pop(L, 1);
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
    };

    luaL_newlib(L, json_lib);
    lua_setglobal(L, "json");
}

// POSIX only: update the current Lua package path with the input
// path and return the updated path.
static char *luadb_posix_path(const char *cur_path, const char *path, size_t len) {
    #ifndef LUADB_WIN32
    char *pathcpy = strdup(path);
    if (!pathcpy) {
        return NULL;
    }

    char *dir = dirname(pathcpy);
    size_t dirlen = strlen(dir);

    char *newpath = malloc(len + dirlen + 6 + 2);
    if (!newpath) {
        free(pathcpy);
        return NULL;
    }

    memcpy(newpath, cur_path, len);
    memcpy(&newpath[len], ";", 1);
    memcpy(&newpath[len+1], dir, dirlen);
    memcpy(&newpath[len+dirlen+1], "/?.lua", 6);
    memcpy(&newpath[len+dirlen+6+1], "\0", 1);
    free(pathcpy);
    return newpath;
    #else
    return NULL;
    #endif
}

// WIN32 only: update the currently Lua package path with the input
// path and return the updated path.
//
// _splitpath_s function documentation is located at:
// https://msdn.microsoft.com/en-us/library/8e46eyt7.aspx
static char *luadb_win32_path(const char *cur_path, const char *path, size_t len) {
    #ifdef LUADB_WIN32
    char drive[WIN32_DRIVE_LEN];
    char dir[WIN32_DIR_LEN];

    _splitpath_s(path, &drive, WIN32_DRIVE_LEN, &dir, WIN32_DIR_LEN, NULL, 0, NULL, 0);

    size_t drivelen = strlen(drive);
    size_t dirlen = strlen(dir);
    char *newpath = malloc(len + drivelen + dirlen + 6 + 2);
    if (!newpath) {
        return NULL;
    }

    memcpy(newpath, cur_path, len);
    memcpy(&newpath[len], ";", 1);
    memcpy(&newpath[len+1], drive, drivelen);
    memcpy(&newpath[len+drivelen], dir, dirlen);
    memcpy(&newpath[len+drivelen+dirlen+6], "/?.lua", 6);
    memcpy(&newpath[len+drivelen+dirlen+6+1], '\0', 1);
    return newpath;
    #else
    return NULL;
    #endif
}