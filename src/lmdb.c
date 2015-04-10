/*****************************************************************************
 * LuaDB :: lmdb.c
 *
 * LMDB to Lua library
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdlib.h>
#include <assert.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/lmdb/lmdb.h"

#include "lmdb.h"

static const char *const LMDB_ENV_REGISTRY_NAME = "lmdb.env";
static const int LMDB_DEFAULT_MODE = 0644;   // -rw-r--r--

/*
 * FORWARD DECLARATIONS
 */

static int lmdb_env_tostring(lua_State *L);
static int lmdb_env_close(lua_State *L);

static MDB_env *open_mdb_env(const char *path, unsigned int flags, int *err);
static int make_lmdb_env_metatable(lua_State *L);

// Library functions
static luaL_Reg lmdb_lib_funcs[] = {
        { "open", luadb_lmdb_open_env },
        { "version", luadb_lmdb_version },
        { NULL, NULL },
};

// LMDB Environment methods
static luaL_Reg lmdb_env_methods[] = {
        { "__gc", lmdb_env_close },
        { "__tostring", lmdb_env_tostring },
        { "close", lmdb_env_close },
        { NULL, NULL },
};

/*
 * PUBLIC FUNCTIONS
 */

void luadb_add_lmdb_lib(lua_State *L) {
    // Create the lmdb.Environment metatable
    make_lmdb_env_metatable(L);

    // Register library level functions
    luaL_newlib(L, lmdb_lib_funcs);
    lua_setglobal(L, "lmdb");
}

int luadb_lmdb_open_env(lua_State *L) {
    const char *path = luaL_checklstring(L, 1, NULL);

    int err;
    MDB_env *env = open_mdb_env(path, 0, &err);
    if (!env) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Allocate space for the LMDB environment as a full userdata
    // Store the pointer to the environment there
    MDB_env **loc = lua_newuserdata(L, sizeof(MDB_env *));
    *loc = env;

    // Set the Env metatable
    luaL_getmetatable(L, LMDB_ENV_REGISTRY_NAME);
    lua_setmetatable(L, -2);
    return 1;
}

int luadb_lmdb_version(lua_State *L) {
    char *version = mdb_version(NULL, NULL, NULL);
    lua_pushstring(L, version);
    return 1;
}

/*
 * PRIVATE LUADB ENV CFUNCTIONS
 */

static int lmdb_env_tostring(lua_State *L) {
    MDB_env **loc = luaL_checkudata(L, 1, LMDB_ENV_REGISTRY_NAME);
    MDB_env *env = *loc;

    if (!env) {
        luaL_error(L, "LMDB environment not found");
        return 0;
    }

    lua_pushfstring(L, "lmdb.Env()");
    return 1;
}

static int lmdb_env_close(lua_State *L) {
    MDB_env **loc = luaL_checkudata(L, 1, LMDB_ENV_REGISTRY_NAME);
    MDB_env *env = *loc;

    if (!env) {
        luaL_error(L, "LMDB environment not found");
        return 0;
    }

    mdb_env_close(env);
    *loc = NULL;
    return 0;
}

/*
 * PRIVATE UTILITY FUNCTIONS
 */

// Create a new MDB_env.
static MDB_env *open_mdb_env(const char *path, unsigned int flags, int *err) {
    MDB_env *env = NULL;
    mdb_mode_t mode = LMDB_DEFAULT_MODE;

    if ((*err = mdb_env_create(&env)) != 0) {
        return NULL;
    }

    if ((*err = mdb_env_open(env, path, flags, mode)) != 0) {
        mdb_env_close(env);
        return NULL;
    }

    return env;
}

// Create the metatable for LMDB Environment objects.
static int make_lmdb_env_metatable(lua_State *L) {
    assert(L);

    // Create the Lua table
    luaL_newmetatable(L, LMDB_ENV_REGISTRY_NAME);

    // Set the metatable as it's own index
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    // Attach the methods to this table
    luaL_setfuncs(L, lmdb_env_methods, 0);
    return 1;
}