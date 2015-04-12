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
#include "uuid.h"

static const char *const LMDB_ENV_REGISTRY_NAME = "lmdb.env";
static const char *const LMDB_TX_REGISTRY_NAME = "lmdb.tx";
static const char *const LMDB_CURSOR_REGISTRY_NAME = "lmdb.cursor";
static const unsigned int LMDB_DEFAULT_FLAGS = 0;
static const unsigned int LMDB_DEFAULT_MAX_READERS = 126;
static const size_t LMDB_DEFAULT_MAP_SIZE = 10485760;
static const int LMDB_DEFAULT_MODE = 0644;          // -rw-r--r--
static const int LMDB_DEFAULT_TXN_COUNT = 10;
static const int LMDB_DEFAULT_CURSOR_COUNT = 10;

/*
 * FORWARD DECLARATIONS
 */

static int lmdb_env_tostring(lua_State *L);
static int lmdb_env_begin_tx(lua_State *L);
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
static int lmdb_env__uuid(lua_State *L);

static int lmdb_tx_tostring(lua_State *L);
static int lmdb_tx_close(lua_State *L);

static MDB_env *open_mdb_env(const char *path, unsigned int flags, unsigned int max_readers, size_t map_size, int *err);
static void get_mdb_env_flags(lua_State *L, unsigned int *flags, unsigned int *max_readers, size_t *map_size);
static inline MDB_env *check_mdb_env(lua_State *L, int idx);
static inline MDB_txn *check_mdb_txn(lua_State *L, int idx);
static void clean_lmdb_env_reference_table(lua_State *L, char *uuid);
static int make_lmdb_env_reader_table(const char *msg, lua_State *L);
static void add_tx_to_reference_table(lua_State *L, MDB_env *env, MDB_txn *txn, int idx);
static void remove_tx_from_reference_table(lua_State *L, MDB_env *env, MDB_txn *txn, int idx);
static char *make_lmdb_env_reference_table(lua_State *L);
static int make_lmdb_env_metatable(lua_State *L);

static int make_lmdb_tx_metatable(lua_State *L);

static int make_lmdb_cursor_metatable(lua_State *L);

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
        { "begin", lmdb_env_begin_tx },
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
        { "_uuid", lmdb_env__uuid },
        { NULL, NULL },
};

// LMDB Transaction methods
static luaL_Reg lmdb_tx_methods[] = {
        { "__gc", lmdb_tx_close },
        { "__tostring", lmdb_tx_tostring },
        { "close", lmdb_tx_close },
        { NULL, NULL },
};

// LMDB Cursor methods
static luaL_Reg lmdb_cursor_methods[] = {
        { NULL, NULL },
};

// LMDB Environment flags
typedef struct luadb_env_flag {
    char *name;
    int val;
} luadb_env_flag;
static luadb_env_flag lmdb_env_opts[] = {
        { "fixedmap", MDB_FIXEDMAP },
        { "nosubdir", MDB_NOSUBDIR },
        { "nosync", MDB_NOSYNC },
        { "rdonly", MDB_RDONLY },
        { "nometasync", MDB_NOMETASYNC },
        { "writemap", MDB_WRITEMAP },
        { "notls", MDB_NOTLS },
        { "nolock", MDB_NOLOCK },
        { "nordahead", MDB_NORDAHEAD },
        { "nomeminit", MDB_NOMEMINIT },
};
static const int LMDB_NUM_FLAGS = 10;

/*
 * PUBLIC FUNCTIONS
 */

void luadb_add_lmdb_lib(lua_State *L) {
    // Create the lmdb.Environment metatable
    make_lmdb_env_metatable(L);
    make_lmdb_tx_metatable(L);
    make_lmdb_cursor_metatable(L);

    // Register library level functions
    luaL_newlib(L, lmdb_lib_funcs);
    lua_setglobal(L, "lmdb");
}

int luadb_lmdb_open_env(lua_State *L) {
    const char *path = luaL_checklstring(L, 1, NULL);

    // Get any flags or options passed in as parameters
    unsigned int flags, max_readers;
    size_t map_size;
    get_mdb_env_flags(L, &flags, &max_readers, &map_size);

    // Open the environment
    int err;
    MDB_env *env = open_mdb_env(path, flags, max_readers, map_size, &err);
    if (!env) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Allocate space for the LMDB environment as a full userdata
    // Store the pointer to the environment there
    MDB_env **loc = lua_newuserdata(L, sizeof(MDB_env *));
    if (!loc) {
        mdb_env_close(env);
        luaL_error(L, "could not allocate memory for LMDB environment");
        return 0;
    }
    *loc = env;

    // Get the UUID for this table, which is used as user_ctx
    char *uuid = make_lmdb_env_reference_table(L);
    if (!uuid) {
        mdb_env_close(env);
        luaL_error(L, "could not allocate memory for LMDB environment");
        return 0;
    }

    // Set the user context
    err = mdb_env_set_userctx(env, uuid);
    if (err != 0) {
        mdb_env_close(env);
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

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
    MDB_env *env = check_mdb_env(L, 1);
    const char *path;
    int err = mdb_env_get_path(env, &path);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_pushfstring(L, "lmdb.Env('%s')", path);
    return 1;
}

static int lmdb_env_begin_tx(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);
    MDB_txn *txn = NULL;

    int err = mdb_txn_begin(env, NULL, 0, &txn);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Allocate space for the LMDB environment as a full userdata
    // Store the pointer to the environment there
    MDB_txn **loc = lua_newuserdata(L, sizeof(MDB_txn *));
    *loc = txn;

    // Set the Env metatable
    luaL_getmetatable(L, LMDB_TX_REGISTRY_NAME);
    lua_setmetatable(L, -2);

    // Add our weak Txn reference
    add_tx_to_reference_table(L, env, txn, lua_gettop(L));
    return 1;
}

static int lmdb_env_close(lua_State *L) {
    MDB_env **loc = luaL_checkudata(L, 1, LMDB_ENV_REGISTRY_NAME);
    MDB_env *env = *loc;

    if (!env) {
        luaL_error(L, "LMDB environment not found");
        return 0;
    }

    // Load the UUID associated with this Environment and
    // load that table onto the stack. Clean up any open
    // cursors and transactions before closing the environment.
    char *uuid = mdb_env_get_userctx(env);
    if (uuid) {
        clean_lmdb_env_reference_table(L, uuid);
        free(uuid);
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

    luaL_checkstack(L, 3, "out of memory");
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

    lua_pushstring(L, "notls");
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

    luaL_checkstack(L, 3, "out of memory");
    lua_newtable(L);

    lua_pushstring(L, "last_pgno");
    lua_pushnumber(L, info.me_last_pgno);
    lua_settable(L, -3);

    lua_pushstring(L, "last_txnid");
    lua_pushnumber(L, info.me_last_txnid);
    lua_settable(L, -3);

    lua_pushstring(L, "mapaddr");
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
    luaL_checkstack(L, 4, "out of memory");
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

    luaL_checkstack(L, 3, "out of memory");
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

static int lmdb_env__uuid(lua_State *L) {
    MDB_env *env = check_mdb_env(L, 1);

    char *uuid = mdb_env_get_userctx(env);
    if (!uuid) {
        luaL_error(L, "no UUID found for this Environment");
        return 0;
    }

    lua_pushstring(L, uuid);
    return 1;
}

/*
 * PRIVATE LUADB TX CFUNCTIONS
 */

static int lmdb_tx_tostring(lua_State *L) {
    /*MDB_env *env = */check_mdb_txn(L, 1);
    lua_pushstring(L, "lmdb.Tx()");
    return 1;
}

static int lmdb_tx_close(lua_State *L) {
    MDB_txn **loc = luaL_checkudata(L, 1, LMDB_TX_REGISTRY_NAME);
    MDB_txn *txn = *loc;

    if (!txn) {
        luaL_error(L, "LMDB transaction not found");
        return 0;
    }

    // Get the associated environment
    MDB_env *env = mdb_txn_env(txn);
    if (!env) {
        mdb_txn_abort(txn);
        *loc = NULL;
        luaL_error(L, "LMDB transaction has no associated environment");
        return 0;
    }

    // Clean this Txn reference from the table
    remove_tx_from_reference_table(L, env, txn, lua_gettop(L));

    mdb_txn_abort(txn);
    *loc = NULL;
    return 0;
}

/*
 * PRIVATE LUADB CURSOR CFUNCTIONS
 */

/*
 * PRIVATE UTILITY FUNCTIONS
 */

// Create a new MDB_env with the given options.
static MDB_env *open_mdb_env(const char *path, unsigned int flags, unsigned int max_readers, size_t map_size, int *err) {
    MDB_env *env = NULL;
    mdb_mode_t mode = LMDB_DEFAULT_MODE;

    if ((*err = mdb_env_create(&env)) != 0) {
        return NULL;
    }

    *err = mdb_env_set_maxreaders(env, max_readers);
    if (*err != 0) {
        mdb_env_close(env);
        return NULL;
    }

    *err = mdb_env_set_mapsize(env, map_size);
    if (*err != 0) {
        mdb_env_close(env);
        return NULL;
    }

    if ((*err = mdb_env_open(env, path, flags, mode)) != 0) {
        mdb_env_close(env);
        return NULL;
    }

    return env;
}

// Load the MDB environment options from the user's open parameters.
static void get_mdb_env_flags(lua_State *L, unsigned int *flags, unsigned int *max_readers, size_t *map_size) {
    int type = lua_type(L, 2);

    // Set some defaults for each of the settings
    *flags = LMDB_DEFAULT_FLAGS;
    *max_readers = LMDB_DEFAULT_MAX_READERS;
    *map_size = LMDB_DEFAULT_MAP_SIZE;

    // Decide how to proceed based on parameters given
    switch(type) {
        case LUA_TTABLE:
            break;
        case LUA_TNIL:
        case LUA_TNONE:
            return;
        default:
            luaL_error(L, "expected a table, nil, or none, not %s", lua_typename(L, type));
            return;
    }

    // Add up the flag values
    int ftype, val;
    for (int i = 0; i < LMDB_NUM_FLAGS; i++) {
        luadb_env_flag *flag = &lmdb_env_opts[i];

        // Try to get the field with flag name
        lua_pushstring(L, flag->name);
        ftype = lua_gettable(L, -2);

        // If it wasn't given, just skip it (all options are optional)
        if (ftype == LUA_TNONE) {
            continue;
        }

        // Convert to boolean and add the flag if true
        val = lua_toboolean(L, -1);
        if (val) {
            flags += flag->val;
        }
        lua_pop(L, 1);
    }

    // Finally, get the non-flag settings
    lua_pushstring(L, "maxreaders");
    ftype = lua_gettable(L, -2);
    if (ftype != LUA_TNONE) {
        *max_readers = (unsigned int)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pushstring(L, "mapsize");
    ftype = lua_gettable(L, -2);
    if (ftype != LUA_TNONE) {
        *map_size = (size_t)luaL_checknumber(L, -1);
        lua_pop(L, 1);
    }
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

// Check for a MDB_txn as a function parameter and dererence it.
//
// This function issues a Lua error if the transaction variable isn't
// found (i.e. is NULL), so it would technically be safe to avoid a
// NULL check on the return value here.
static inline MDB_txn *check_mdb_txn(lua_State *L, int idx) {
    assert(L);

    MDB_txn **loc = luaL_checkudata(L, idx, LMDB_TX_REGISTRY_NAME);

    if (!(*loc)) {
        luaL_error(L, "LMDB transaction not found");
        return NULL;
    }

    return *loc;
}

// Clean up any lingering cursors and transactions before closing the
// entire environment.
static void clean_lmdb_env_reference_table(lua_State *L, char *uuid) {
    assert(L);
    assert(uuid);

    luaL_checkstack(L, 10, "out of memory");

    // Get the table from the registry
    lua_pushstring(L, uuid);
    lua_gettable(L, LUA_REGISTRYINDEX);
    int type = lua_type(L, -1);
    if (type != LUA_TTABLE) {
        return;
    }

    // Iterate on each transaction
    int txidx = lua_gettop(L);
    lua_pushnil(L);
    while(lua_next(L, txidx) != 0) {
        // Get the transaction and table
        MDB_txn **loc = luaL_checkudata(L, -2, LMDB_TX_REGISTRY_NAME);
        MDB_txn *txn = *loc;
        type = lua_type(L, -1);     // Value type

        // If there is no table, just continue
        if (type != LUA_TTABLE) {
            lua_pop(L, 1);
            continue;
        }

        // Clean up the references in that table
        int cursoridx = lua_gettop(L);
        lua_pushnil(L);
        while(lua_next(L, cursoridx) != 0) {
            MDB_cursor **cloc = luaL_checkudata(L, -1, LMDB_CURSOR_REGISTRY_NAME);
            MDB_cursor *cursor = *cloc;

            mdb_cursor_close(cursor);
            cursor = NULL;
            *cloc = NULL;

            lua_pop(L, 1);
        }

        // Abort the transaction and set all of the pointers null
        mdb_txn_abort(txn);
        txn = NULL;
        *loc = NULL;

        // Pop the value from the stack
        lua_pop(L, 1);
    }
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

// Create a new weak table in the Lua registry under a UUID which
// holds transaction and cursor references to permit ordered
// garbage collection by LMDB.
//
// The return value is the UUID which can be saved into the MDB_env
// as the user_ctx value.
//
// The table looks like this:
// registry = {
//     ...,
//     uuid = {
//         tx1 = { cursor1, cursor2, ... },
//         tx2 = { cursor1, cursor2, ... },
//     },
//     ...
// }
static char *make_lmdb_env_reference_table(lua_State *L) {
    assert(L);

    char *uuid = luadb_create_guid();
    if (!uuid) {
        return NULL;
    }

    // Verify we'll have enough memory
    luaL_checkstack(L, 6, "out of memory");

    // Create a table stored which will be stored in the registry
    lua_pushstring(L, uuid);        // Registry key = `uuid`
    lua_createtable(L, 0, LMDB_DEFAULT_TXN_COUNT);

    // Make the table weak by setting a metatable with `__mode` = `kv`
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__mode");
    lua_pushstring(L, "kv");
    lua_settable(L, -3);
    lua_setmetatable(L, -2);

    // Set the table into the registry at key `uuid`
    lua_settable(L, LUA_REGISTRYINDEX);
    return uuid;
}

// Add the given Transaction to the Weak Reference table for the
// given environment. Use the idx to indicate where the Txn is on
// the Lua stack.
static void add_tx_to_reference_table(lua_State *L, MDB_env *env, MDB_txn *txn, int idx) {
    assert(L);
    assert(txn);

    if (!env) {
        env = mdb_txn_env(txn);
    }

    char *uuid = mdb_env_get_userctx(env);
    if (!uuid) {
        luaL_error(L, "no reference table found for environment");
    }

    // Check for enough stack space
    luaL_checkstack(L, 5, "out of memory");

    // Get the Env reference table from the registry
    lua_pushstring(L, uuid);  // 1->2
    lua_gettable(L, LUA_REGISTRYINDEX); // 2->2

    // Push the userdata back onto the stack and get its table value
    lua_pushvalue(L, idx); // 2->3
    lua_pushvalue(L, idx); // 3->4
    lua_gettable(L, -2); // 4->4

    // If this value isn't already in the table, add it
    int type = lua_type(L, -1);
    switch (type) {
        case LUA_TTABLE:
            return;             // Table already exists
        default:                // Fall through
        case LUA_TNIL:
            lua_pop(L, 1);      // Pop the nil/other off the stack
        case LUA_TNONE:         // Fall through
            break;
    }

    // Create the Txn reference table
    lua_createtable(L, 0, LMDB_DEFAULT_CURSOR_COUNT);
    lua_settable(L, -3);
    lua_settop(L, idx);
}

// Remove the given Transaction from the Weak Reference table for
// the given environment. Use the idx to indicate where the Txn userdata
// is on the Lua stack.
static void remove_tx_from_reference_table(lua_State *L, MDB_env *env, MDB_txn *txn, int idx) {
    assert(L);
    assert(txn);

    if (!env) {
        env = mdb_txn_env(txn);
    }

    char *uuid = mdb_env_get_userctx(env);
    if (!uuid) {
        luaL_error(L, "no reference table found for environment");
    }

    // Check for enough stack space
    luaL_checkstack(L, 5, "out of memory");

    // Get the Env reference table from the registry
    lua_pushstring(L, uuid);
    lua_gettable(L, LUA_REGISTRYINDEX);

    // Push the userdata back onto the stack and set it nil
    lua_pushvalue(L, idx);
    lua_pushnil(L);
    lua_settable(L, -3);
}

// Create the metatable for LMDB Environment objects.
static int make_lmdb_env_metatable(lua_State *L) {
    assert(L);

    // Verify we'll have enough memory
    luaL_checkstack(L, 3, "out of memory");

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

// Create the metatable for LMDB Tx objects
static int make_lmdb_tx_metatable(lua_State *L) {
    assert(L);

    // Verify we'll have enough memory
    luaL_checkstack(L, 3, "out of memory");

    // Create the Lua table
    luaL_newmetatable(L, LMDB_TX_REGISTRY_NAME);

    // Set the metatable as it's own index
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    // Attach the methods to this table
    luaL_setfuncs(L, lmdb_tx_methods, 0);
    return 1;
}

// Create the metatable for LMDB Cursor objects
static int make_lmdb_cursor_metatable(lua_State *L) {
    assert(L);

    // Verify we'll have enough memory
    luaL_checkstack(L, 3, "out of memory");

    // Create the Lua table
    luaL_newmetatable(L, LMDB_CURSOR_REGISTRY_NAME);

    // Set the metatable as it's own index
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);

    // Attach the methods to this table
    luaL_setfuncs(L, lmdb_cursor_methods, 0);
    return 1;
}
