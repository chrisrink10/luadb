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
#include <stdbool.h>

#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"
#include "deps/lua/lauxlib.h"

#include "json.h"
#include "lmdb.h"
#include "state.h"
#include "uuid.h"

#ifdef _WIN32
static const char *const PATH_SEPARATOR = "\\";
#else  //_WIN32
static const char *const PATH_SEPARATOR = "/";
#endif //_WIN32
#define ENDS_WITH_SEP(str, strlen) (str[strlen-1] == PATH_SEPARATOR[0])

/*
 * FORWARD DECLARATIONS
 */

static char *append_luadb_path(const char *cur_path, size_t len,
                               const char *path, bool truncate);
static void update_lua_package_path(lua_State *L, const char *path,
                                    bool truncate);

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
    luadb_add_uuid_lib(L);

    return L;
}

void luadb_add_relative_path(lua_State *L, const char *path) {
    update_lua_package_path(L, path, true);
}

void luadb_add_absolute_path(lua_State *L, const char *path) {
    update_lua_package_path(L, path, false);
}

/*
 * PRIVATE FUNCTIONS
 */

// Add the given path to the Lua global `package.path`.
//
// If `truncate` is specified, take a relative path from the given path.
static void update_lua_package_path(lua_State *L, const char *path, bool truncate) {
    assert(L);
    assert(path);

    // Get the package.path variable
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    size_t len;
    const char *cur_path = lua_tolstring(L, -1, &len);

    // Get the directory path
    char *newpath = append_luadb_path(cur_path, len, path, truncate);
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

// Update the current Lua package path with the input
// path and return the updated path.
//
// If truncate is specified, use `dirname` to truncate the
// given `path` down one level.
static char *append_luadb_path(const char *cur_path, size_t len,
                               const char *path, bool truncate) {
    // Create a copy of path, since dirname may modify the output
    size_t pathlen = strlen(path);
    char *pathcpy = malloc(pathlen + 1);
    if (!pathcpy) {
        return NULL;
    }
    memcpy(pathcpy, path, pathlen);
    pathcpy[pathlen] = '\0';

    // Create space for the new path
    char *dir = (truncate) ? dirname(pathcpy) : pathcpy;
    char *newpath = malloc(len + pathlen + 6 + 2);
    if (!newpath) {
        free(pathcpy);
        return NULL;
    }

    // Determine if a separator should be added
    bool need_sep = !((truncate) ?
                    ENDS_WITH_SEP(dir, strlen(dir)) :
                    ENDS_WITH_SEP(path, pathlen));

    // Generate the new path
    sprintf(newpath, "%s;%s%s?.lua", cur_path, dir,
            (need_sep) ? PATH_SEPARATOR : "");
    free(pathcpy);
    return newpath;
}