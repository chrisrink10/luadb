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
#include <libgen.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lualib.h"
#include "deps/lua/lauxlib.h"

#include "json.h"
#include "lmdb.h"
#include "luadb.h"
#include "state.h"
#include "uuid.h"

#define ENDS_WITH_SEP(str, strlen) (str[strlen-1] == LUADB_PATH_SEPARATOR[0])

/*
 * FORWARD DECLARATIONS
 */

static char *AppendLuaDbPath(const char *cur_path, size_t len, const char *path, bool truncate);
static void UpdateLuaPackagePath(lua_State *L, const char *path, bool truncate);

/*
 * FORWARD DECLARATIONS
 */

lua_State *LuaDB_NewState(void) {
    lua_State *L = luaL_newstate();
    if (!L) {
        return NULL;
    }

    luaL_openlibs(L);
    LuaDB_JsonAddLib(L);
    LuaDB_LmdbAddLib(L);
    LuaDB_UuidAddLib(L);

    return L;
}

void LuaDB_PathAddRelative(lua_State *L, const char *path) {
    UpdateLuaPackagePath(L, path, true);
}

void LuaDB_PathAddAbsolute(lua_State *L, const char *path) {
    UpdateLuaPackagePath(L, path, false);
}

/*
 * PRIVATE FUNCTIONS
 */

// Add the given path to the Lua global `package.path`.
//
// If `truncate` is specified, take a relative path from the given path.
static void UpdateLuaPackagePath(lua_State *L, const char *path, bool truncate) {
    assert(L);
    assert(path);

    // Get the package.path variable
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "path");
    size_t len;
    const char *cur_path = lua_tolstring(L, -1, &len);

    // Get the directory path
    char *newpath = AppendLuaDbPath(cur_path, len, path, truncate);
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
static char *AppendLuaDbPath(const char *cur_path, size_t len, const char *path, bool truncate) {
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
            (need_sep) ? LUADB_PATH_SEPARATOR_L : "");
    free(pathcpy);
    return newpath;
}
