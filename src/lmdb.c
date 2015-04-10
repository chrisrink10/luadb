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
#include <stdbool.h>

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
static int lmdb_env_flags(lua_State *L);
static int lmdb_env_info(lua_State *L);
static int lmdb_env_max_key_size(lua_State *L);
static int lmdb_env_max_readers(lua_State *L);
static int lmdb_env_path(lua_State *L);
static int lmdb_env_readers(lua_State *L);
static int lmdb_env_reader_check(lua_State *L);
static int lmdb_env_stat(lua_State *L);
static int lmdb_env_sync(lua_State *L);

static MDB_env *open_mdb_env(const char *path, unsigned int flags, int *err);
static inline MDB_env *check_mdb_env(lua_State *L, int idx);
static int make_lmdb_env_reader_table(const char *msg, lua_State *L);
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
        { "flags", lmdb_env_flags },
        { "info", lmdb_env_info },
        { "max_key_size", lmdb_env_max_key_size },
        { "max_readers", lmdb_env_max_readers },
        { "path", lmdb_env_path },
        { "readers", lmdb_env_readers },
        { "reader_check", lmdb_env_reader_check },
        { "stat", lmdb_env_stat },
        { "sync", lmdb_env_sync },
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
    /*MDB_env *env = */check_mdb_env(L, 1);

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

static int lmdb_env_flags(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    unsigned int flags;

    int err = mdb_env_get_flags(env, &flags);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_newtable(L);

    lua_pushstring(L, "fixedmap");
    lua_pushboolean(L, flags & MDB_FIXEDMAP);
    lua_settable(L, -3);

    lua_pushstring(L, "nosubdir");
    lua_pushboolean(L, flags & MDB_NOSUBDIR);
    lua_settable(L, -3);

    lua_pushstring(L, "nosync");
    lua_pushboolean(L, flags & MDB_NOSYNC);
    lua_settable(L, -3);

    lua_pushstring(L, "rdonly");
    lua_pushboolean(L, flags & MDB_RDONLY);
    lua_settable(L, -3);

    lua_pushstring(L, "nometasync");
    lua_pushboolean(L, flags & MDB_NOMETASYNC);
    lua_settable(L, -3);

    lua_pushstring(L, "writemap");
    lua_pushboolean(L, flags & MDB_WRITEMAP);
    lua_settable(L, -3);

    lua_pushstring(L, "mapasync");
    lua_pushboolean(L, flags & MDB_MAPASYNC);
    lua_settable(L, -3);

    lua_pushstring(L, "notols");
    lua_pushboolean(L, flags & MDB_NOTLS);
    lua_settable(L, -3);

    lua_pushstring(L, "nolock");
    lua_pushboolean(L, flags & MDB_NOLOCK);
    lua_settable(L, -3);

    lua_pushstring(L, "nordahead");
    lua_pushboolean(L, flags & MDB_NORDAHEAD);
    lua_settable(L, -3);

    lua_pushstring(L, "nomeminit");
    lua_pushboolean(L, flags & MDB_NOMEMINIT);
    lua_settable(L, -3);

    return 1;
}

static int lmdb_env_info(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    MDB_envinfo info;

    int err = mdb_env_info(env, &info);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_newtable(L);

    lua_pushstring(L, "last_pgno");
    lua_pushnumber(L, info.me_last_pgno);
    lua_settable(L, -3);

    lua_pushstring(L, "last_txnid");
    lua_pushnumber(L, info.me_last_txnid);
    lua_settable(L, -3);

    lua_pushstring(L, "mappaddr");
    lua_pushfstring(L, "%p", info.me_mapaddr);
    lua_settable(L, -3);

    lua_pushstring(L, "mapsize");
    lua_pushnumber(L, info.me_mapsize);
    lua_settable(L, -3);

    lua_pushstring(L, "maxreaders");
    lua_pushnumber(L, info.me_maxreaders);
    lua_settable(L, -3);

    lua_pushstring(L, "num_readers");
    lua_pushnumber(L, info.me_numreaders);
    lua_settable(L, -3);

    return 1;
}

static int lmdb_env_max_key_size(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    int max = mdb_env_get_maxkeysize(env);
    lua_pushnumber(L, max);
    return 1;
}

static int lmdb_env_max_readers(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    unsigned int max;
    int err = mdb_env_get_maxreaders(env, &max);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushnumber(L, max);
    return 1;
}

static int lmdb_env_path(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    const char *path;
    int err = mdb_env_get_path(env, &path);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushstring(L, path);
    return 1;
}

static int lmdb_env_readers(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    lua_newtable(L);
    lua_pushnumber(L, 1);   // Push an index onto the stack for the callback

    int err = mdb_reader_list(env, (MDB_msg_func *) make_lmdb_env_reader_table, L);
    if (err < 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_pop(L, 1);          // Pop the integer index off the stack
    return 1;
}

static int lmdb_env_reader_check(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    int dead;
    int err = mdb_reader_check(env, &dead);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushnumber(L, dead);
    return 1;
}

static int lmdb_env_stat(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    MDB_stat stat;

    int err = mdb_env_stat(env, &stat);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_newtable(L);

    lua_pushstring(L, "branch_pages");
    lua_pushnumber(L, stat.ms_branch_pages);
    lua_settable(L, -3);

    lua_pushstring(L, "depth");
    lua_pushnumber(L, stat.ms_depth);
    lua_settable(L, -3);

    lua_pushstring(L, "entries");
    lua_pushnumber(L, stat.ms_entries);
    lua_settable(L, -3);

    lua_pushstring(L, "leaf_pages");
    lua_pushnumber(L, stat.ms_leaf_pages);
    lua_settable(L, -3);

    lua_pushstring(L, "overflow_pages");
    lua_pushnumber(L, stat.ms_overflow_pages);
    lua_settable(L, -3);

    lua_pushstring(L, "page_size");
    lua_pushnumber(L, stat.ms_psize);
    lua_settable(L, -3);

    return 1;
}

static int lmdb_env_sync(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    int force = 0;

    if (!lua_isnone(L, 2)) {
        force = lua_toboolean(L, 2);
    }

    mdb_env_sync(env, force);
    return 1;
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

// Check for a MDB_env as a function parameter and dereference it
//
// This function issues a Lua error if the environment variable isn't
// found (i.e. is NULL), so it would technically be safe to avoid a
// NULL check on the return here.
static inline MDB_env *check_mdb_env(lua_State *L, int idx) {
    assert(L);

    MDB_env **loc = luaL_checkudata(L, idx, LMDB_ENV_REGISTRY_NAME);

    if (!(*loc)) {
        luaL_error(L, "LMDB environment not found");
        return NULL;
    }

    return *loc;
}

// Create the reader lock table as a MDB_msg_func callback.
//
// Assumes it is being called by lmdb_env_readers and, thus,
// that there is a table at the (1) index on the stack and
// an index integer right above that.
static int make_lmdb_env_reader_table(const char *msg, lua_State *L) {
    int idx = (int)luaL_checknumber(L, -1);
    lua_pushstring(L, msg);
    lua_settable(L, -3);
    lua_pushnumber(L, idx+1);       // Increment the index
    return 0;
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