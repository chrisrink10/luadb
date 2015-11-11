/*****************************************************************************
 * LuaDB :: lmdb.c
 *
 * LMDB to Lua library
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/lmdb/lmdb.h"

#include "lmdb.h"
#include "uuid.h"

static const char *const LMDB_ENV_REGISTRY_NAME = "lmdb.Env";
static const char *const LMDB_TX_REGISTRY_NAME = "lmdb.Tx";
static const char *const LMDB_CURSOR_REGISTRY_NAME = "lmdb.Cursor";
static const unsigned int LMDB_DEFAULT_FLAGS = 0;
static const unsigned int LMDB_DEFAULT_MAX_READERS = 126;
static const size_t LMDB_DEFAULT_MAP_SIZE = 10485760;
static const int LMDB_DEFAULT_MODE = 0644;          // -rw-r--r--
static const int LMDB_DEFAULT_TXN_COUNT = 10;
static const int LMDB_DEFAULT_CURSOR_COUNT = 10;
static const int LMDB_MAX_KEY_SEGMENTS = 32;
static const int LMDB_MAX_KEY_SEG_LENGTH = UCHAR_MAX;

#define LMDB_BOOLEAN_CHAR 'b'
#define LMDB_INTEGER_CHAR 'i'
#define LMDB_NUMERIC_CHAR 'n'
#define LMDB_STRING_CHAR 's'
static const char LMDB_BOOLEAN_TYPE = LMDB_BOOLEAN_CHAR;
static const char LMDB_INTEGER_TYPE = LMDB_INTEGER_CHAR;
static const char LMDB_NUMERIC_TYPE = LMDB_NUMERIC_CHAR;
static const char LMDB_STRING_TYPE = LMDB_STRING_CHAR;

/*
 * FORWARD DECLARATIONS
 */

// LMDB Transaction type
typedef struct LuaDB_LmdbTx {
    MDB_txn *txn;
    MDB_dbi dbi;
} LuaDB_LmdbTx;

// LMDB Cursor type
typedef struct LuaDB_LmdbCursor {
    MDB_cursor *cur;
    char *prefix;
} LuaDB_LmdbCursor;

// LMDB Order type cursor
typedef struct LuaDB_LmdbOrder {
    MDB_cursor *cur;
    char *prefix;
    size_t pfxlen;
    MDB_cursor_op op;
} LuaDB_LmdbOrder;

static int LmdbEnv_ToString(lua_State *L);
static int LmdbEnv_BeginTx(lua_State *L);
static int LmdbEnv_Close(lua_State *L);
static int LmdbEnv_Copy(lua_State *L);
static int LmdbEnv_Flags(lua_State *L);
static int LmdbEnv_Info(lua_State *L);
static int LmdbEnv_MaxKeySize(lua_State *L);
static int LmdbEnv_MaxReaders(lua_State *L);
static int LmdbEnv_Path(lua_State *L);
static int LmdbEnv_Readers(lua_State *L);
static int LmdbEnv_ReaderCheck(lua_State *L);
static int LmdbEnv_Stat(lua_State *L);
static int LmdbEnv_Sync(lua_State *L);
static int LmdbEnv__Uuid(lua_State *L);

static int LmdbTx_ToString(lua_State *L);
static int LmdbTx_Close(lua_State *L);
static int LmdbTx_Commit(lua_State *L);
static int LmdbTx__Dbi(lua_State *L);
static int LmdbTx_Delete(lua_State *L);
static int LmdbTx__Dump(lua_State *L);
static int LmdbTx_Get(lua_State *L);
static int LmdbTx_Put(lua_State *L);
static int LmdbTx_Order(lua_State *L);

static int Lmdb_OrderClose(lua_State *L);

static MDB_env *OpenLmdbEnv(const char *path, unsigned int flags, unsigned int max_readers, size_t map_size, int *err);
static void ReadLmdbEnvParamsFromLua(lua_State *L, unsigned int *flags, unsigned int *max_readers, size_t *map_size);
static inline MDB_env *CheckLmdbEnvParam(lua_State *L, int idx);
static inline LuaDB_LmdbTx *CheckLmdbTxParam(lua_State *L, int idx);
static void CleanLmdbEnvRefTable(lua_State *L, char *uuid);
static int LmdbEnvReaderTableCreate(const char *msg, lua_State *L);
static void AddTxToLmdbEnvRefTable(lua_State *L, MDB_env *env, MDB_txn *txn, int idx);
static void RemoveTxFromLmdbEnvRefTable(lua_State *L, MDB_env *env, MDB_txn *txn, int idx);
static char *CreateLmdbEnvRefTable(lua_State *L);
static char *GetLmdbKeyFromLua(lua_State *L, size_t *len, int idx, int last);
static char *GenerateLuaDbKey(int *err, size_t *len, const char **keys, size_t *lens, char *types, int elems);
static const char *NextKeySegment(const char *key, size_t keylen, size_t pfxlen, size_t *len, int *type);
static bool PushValueByType(lua_State *L, const char *val, size_t len, int type);
static char *CreateKeyDumpString(MDB_val *key);
static int LuaDbOrderTxClosure(lua_State *L);
static bool CreateLmdbEnvMetatable(lua_State *L);
static bool CreateLmdbTxMetatable(lua_State *L);
static bool CreateLmdbCursorMetatable(lua_State *L);

// Library functions
static luaL_Reg lmdb_lib_funcs[] = {
        { "open", LuaDB_LmdbOpenEnv},
        { "version", LuaDB_LmdbVersion},
        { NULL, NULL },
};

// LMDB Environment methods
static luaL_Reg lmdb_env_methods[] = {
        { "__gc",  LmdbEnv_Close},
        { "__tostring", LmdbEnv_ToString},
        { "begin", LmdbEnv_BeginTx},
        { "close", LmdbEnv_Close},
        { "copy", LmdbEnv_Copy},
        { "flags", LmdbEnv_Flags},
        { "info", LmdbEnv_Info},
        { "max_key_size", LmdbEnv_MaxKeySize},
        { "max_readers", LmdbEnv_MaxReaders},
        { "path", LmdbEnv_Path},
        { "readers", LmdbEnv_Readers},
        { "reader_check", LmdbEnv_ReaderCheck},
        { "stat", LmdbEnv_Stat},
        { "sync", LmdbEnv_Sync},
        { "_uuid", LmdbEnv__Uuid},
        { NULL, NULL },
};

// LMDB Transaction methods
static luaL_Reg lmdb_tx_methods[] = {
        { "__gc",     LmdbTx_Close},
        { "__tostring", LmdbTx_ToString},
        { "close",    LmdbTx_Close},
        { "_dbi", LmdbTx__Dbi},
        { "commit", LmdbTx_Commit},
        { "delete", LmdbTx_Delete},
        { "_dump", LmdbTx__Dump},
        { "get", LmdbTx_Get},
        { "put", LmdbTx_Put},
        { "order", LmdbTx_Order},
        { "rollback", LmdbTx_Close},
        { NULL, NULL },
};

// LMDB Order Cursor methods
static luaL_Reg lmdb_cursor_methods[] = {
        { "__gc", Lmdb_OrderClose},
        { NULL, NULL },
};

// LMDB Environment flags
typedef struct luadb_env_flag {
    char *name;
    unsigned int val;
} luadb_env_flag;
static luadb_env_flag lmdb_env_opts[] = {
        { "fixedmap", MDB_FIXEDMAP },
        { "nosubdir", MDB_NOSUBDIR },
        { "nosync", MDB_NOSYNC },
        { "rdonly", MDB_RDONLY },
        { "nometasync", MDB_NOMETASYNC },
        { "writemap", MDB_WRITEMAP },
        { "mapasync", MDB_MAPASYNC },
        { "notls", MDB_NOTLS },
        { "nolock", MDB_NOLOCK },
        { "nordahead", MDB_NORDAHEAD },
        { "nomeminit", MDB_NOMEMINIT },
        { NULL, 0 },
};

/*
 * PUBLIC FUNCTIONS
 */

void LuaDB_LmdbAddLib(lua_State *L) {
    // Create the lmdb.Environment metatable
    CreateLmdbEnvMetatable(L);
    CreateLmdbTxMetatable(L);
    CreateLmdbCursorMetatable(L);

    // Register library level functions
    luaL_newlib(L, lmdb_lib_funcs);
    lua_setglobal(L, "lmdb");
}

int LuaDB_LmdbOpenEnv(lua_State *L) {
    const char *path = luaL_checklstring(L, 1, NULL);

    // Get any flags or options passed in as parameters
    unsigned int flags, max_readers;
    size_t map_size;
    ReadLmdbEnvParamsFromLua(L, &flags, &max_readers, &map_size);

    // Open the environment
    int err;
    MDB_env *env = OpenLmdbEnv(path, flags, max_readers, map_size, &err);
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
    char *uuid = CreateLmdbEnvRefTable(L);
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

int LuaDB_LmdbVersion(lua_State *L) {
    char *version = mdb_version(NULL, NULL, NULL);
    lua_pushstring(L, version);
    return 1;
}

/*
 * PRIVATE LUADB ENV CFUNCTIONS
 */

static int LmdbEnv_ToString(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    const char *path;
    int err = mdb_env_get_path(env, &path);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_pushfstring(L, "lmdb.Env('%s')", path);
    return 1;
}

static int LmdbEnv_BeginTx(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    MDB_txn *txn = NULL;
    unsigned int flags = 0;

    // Set the transaction as read only if requested
    if (lua_gettop(L) > 1) {
        flags = (lua_toboolean(L, 2) == 1) ? (MDB_RDONLY) : 0;
    }

    // Open the new transaction
    int err = mdb_txn_begin(env, NULL, flags, &txn);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Allocate space for the LMDB environment as a full userdata
    // Store the pointer to the environment there
    LuaDB_LmdbTx *loc = lua_newuserdata(L, sizeof(LuaDB_LmdbTx));
    loc->txn = txn;

    // Set the Env metatable
    luaL_getmetatable(L, LMDB_TX_REGISTRY_NAME);
    lua_setmetatable(L, -2);

    // Add our weak Txn reference
    int idx = lua_gettop(L);
    AddTxToLmdbEnvRefTable(L, env, txn, idx);

    // Get a DBI handle for the database
    err = mdb_dbi_open(txn, NULL, 0, &loc->dbi);
    if (err != 0) {
        RemoveTxFromLmdbEnvRefTable(L, NULL, txn, idx);
        mdb_txn_abort(txn);
        luaL_error(L, "could not create a database handle");
        return 0;
    }
    return 1;
}

static int LmdbEnv_Close(lua_State *L) {
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
        CleanLmdbEnvRefTable(L, uuid);
        free(uuid);
    }

    mdb_env_close(env);
    *loc = NULL;
    return 0;
}

static int LmdbEnv_Copy(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    const char *path = luaL_checkstring(L, 2);

    unsigned int flags = 0;
    if (lua_gettop(L) > 2) {
        int compact = lua_toboolean(L, 3);
        flags = (compact) ? (MDB_CP_COMPACT) : 0;
    }

    int err = mdb_env_copy2(env, path, flags);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    return 0;
}

static int LmdbEnv_Flags(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    unsigned int flags;

    int err = mdb_env_get_flags(env, &flags);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    luaL_checkstack(L, 3, "out of memory");
    lua_newtable(L);

    for (luadb_env_flag *f = &lmdb_env_opts[0]; f->name != NULL; f++) {
        lua_pushstring(L, f->name);
        lua_pushboolean(L, flags & f->val);
        lua_settable(L, -3);
    }

    return 1;
}

static int LmdbEnv_Info(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
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

static int LmdbEnv_MaxKeySize(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    int max = mdb_env_get_maxkeysize(env);
    lua_pushnumber(L, max);
    return 1;
}

static int LmdbEnv_MaxReaders(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    unsigned int max;
    int err = mdb_env_get_maxreaders(env, &max);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushnumber(L, max);
    return 1;
}

static int LmdbEnv_Path(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    const char *path;
    int err = mdb_env_get_path(env, &path);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushstring(L, path);
    return 1;
}

static int LmdbEnv_Readers(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    luaL_checkstack(L, 4, "out of memory");
    lua_newtable(L);
    lua_pushnumber(L, 1);   // Push an index onto the stack for the callback

    int err = mdb_reader_list(env, (MDB_msg_func *) LmdbEnvReaderTableCreate, L);
    if (err < 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    lua_pop(L, 1);          // Pop the integer index off the stack
    return 1;
}

static int LmdbEnv_ReaderCheck(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    int dead;
    int err = mdb_reader_check(env, &dead);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    lua_pushnumber(L, dead);
    return 1;
}

static int LmdbEnv_Stat(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
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

static int LmdbEnv_Sync(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);
    int force = 0;

    if (!lua_isnone(L, 2)) {
        force = lua_toboolean(L, 2);
    }

    mdb_env_sync(env, force);
    return 1;
}

static int LmdbEnv__Uuid(lua_State *L) {
    MDB_env *env = CheckLmdbEnvParam(L, 1);

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

static int LmdbTx_ToString(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);
    lua_pushfstring(L, "lmdb.Tx(%I)", loc->dbi);
    return 1;
}

static int LmdbTx_Close(lua_State *L) {
    LuaDB_LmdbTx *loc = luaL_checkudata(L, 1, LMDB_TX_REGISTRY_NAME);

    if (!loc) {
        luaL_error(L, "LMDB transaction not found");
        return 0;
    }

    // Get the associated environment
    MDB_env *env = mdb_txn_env(loc->txn);
    if (!env) {
        mdb_txn_abort(loc->txn);
        loc->txn = NULL;
        luaL_error(L, "LMDB transaction has no associated environment");
        return 0;
    }

    // Clean this Txn reference from the table
    RemoveTxFromLmdbEnvRefTable(L, env, loc->txn, lua_gettop(L));

    mdb_txn_abort(loc->txn);
    loc->txn = NULL;
    return 0;
}

static int LmdbTx_Commit(lua_State *L) {
    LuaDB_LmdbTx *loc = luaL_checkudata(L, 1, LMDB_TX_REGISTRY_NAME);

    if (!loc) {
        luaL_error(L, "LMDB transaction not found");
        return 0;
    }

    // Get the associated environment
    MDB_env *env = mdb_txn_env(loc->txn);
    if (!env) {
        mdb_txn_abort(loc->txn);
        loc->txn = NULL;
        luaL_error(L, "LMDB transaction has no associated environment");
        return 0;
    }

    // Clean this Txn reference from the table
    RemoveTxFromLmdbEnvRefTable(L, env, loc->txn, lua_gettop(L));

    int err = mdb_txn_commit(loc->txn);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    loc->txn = NULL;
    return 1;
}

static int LmdbTx_Delete(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);
    MDB_val key;

    // Create a LMDB key from multiple input parameters
    char *tkey = GetLmdbKeyFromLua(L, &key.mv_size, 2, lua_gettop(L));
    key.mv_data = tkey;

    // Delete the value in the database
    int err = mdb_del(loc->txn, loc->dbi, &key, NULL);
    free(tkey);
    if (err == MDB_NOTFOUND) {
        lua_pushboolean(L, 0);
        return 1;
    } else if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Push a true to indicate the value was deleted
    lua_pushboolean(L, 1);
    return 1;
}

static int LmdbTx__Dump(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);

    // Open a new cursor
    MDB_cursor *cur;
    MDB_cursor_op op = MDB_NEXT;
    MDB_val key;
    int err = mdb_cursor_open(loc->txn, loc->dbi, &cur);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Generate the prefix if there is one
    size_t pfxlen;
    char *prefix = GetLmdbKeyFromLua(L, &pfxlen, 2, lua_gettop(L));
    if (pfxlen > 0) {
        op = MDB_SET_RANGE;
        key.mv_size = pfxlen;
        key.mv_data = prefix;
    }

    // Get the key and value from the db
    MDB_val val;
    while ((mdb_cursor_get(cur, &key, &val, op)) != MDB_NOTFOUND) {
        // If given a prefix, make sure we stop once we loop past it
        if ((pfxlen > 0) &&
            (strncmp(prefix, (char *)key.mv_data, pfxlen) != 0)) {
            break;
        }

        // Create the key and print the k/v pair
        char *keystr = CreateKeyDumpString(&key);
        printf("%s = %*s\n", keystr, (int)val.mv_size, (char*)val.mv_data);
        free(keystr);
        op = MDB_NEXT;
    }

    mdb_cursor_close(cur);
    return 0;
}

static int LmdbTx_Get(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);
    MDB_val key;
    MDB_val val;

    // Create a LMDB key from multiple input parameters
    char *tkey = GetLmdbKeyFromLua(L, &key.mv_size, 2, lua_gettop(L));
    key.mv_data = tkey;

    // Get the value in the database
    int err = mdb_get(loc->txn, loc->dbi, &key, &val);
    free(tkey);
    if (err == MDB_NOTFOUND) {
        lua_pushnil(L);
        return 1;
    } else if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Push its string value onto the stack
    const char *vstr = val.mv_data;
    lua_pushlstring(L, vstr, val.mv_size);
    return 1;
}

static int LmdbTx_Put(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);
    MDB_val key;
    MDB_val val;
    unsigned int flags = 0;

    // Get the data and size for the value
    const char *tval = lua_tolstring(L, 2, &val.mv_size);
    val.mv_data = (void*)tval;

    // Create a LMDB key from multiple input parameters
    char *tkey = GetLmdbKeyFromLua(L, &key.mv_size, 3, lua_gettop(L));
    key.mv_data = tkey;

    // Put the values into the database
    int err = mdb_put(loc->txn, loc->dbi, &key, &val, flags);
    free(tkey);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }
    return 0;
}

static int LmdbTx_Order(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);

    MDB_cursor *cur;
    int err = mdb_cursor_open(loc->txn, loc->dbi, &cur);
    if (err != 0) {
        luaL_error(L, "%s", mdb_strerror(err));
        return 0;
    }

    // Generate the given prefix
    size_t len;
    char *pfx = GetLmdbKeyFromLua(L, &len, 2, lua_gettop(L));

    // Get a full userdatum
    LuaDB_LmdbOrder *curloc = lua_newuserdata(L, sizeof(LuaDB_LmdbOrder));
    curloc->cur = cur;
    curloc->prefix = pfx;
    curloc->pfxlen = len;
    curloc->op = MDB_SET_RANGE;

    // Set the Cursor metatable
    luaL_getmetatable(L, LMDB_CURSOR_REGISTRY_NAME);
    lua_setmetatable(L, -2);

    // Push the private LuaDbOrderTxClosure function on the stack as a closure
    // containing the previously declared cursor as it's upvalue
    lua_pushcclosure(L, &LuaDbOrderTxClosure, 1);
    return 1;
}

static int LmdbTx__Dbi(lua_State *L) {
    LuaDB_LmdbTx *loc = CheckLmdbTxParam(L, 1);
    lua_pushinteger(L, loc->dbi);
    return 1;
}

/*
 * PRIVATE LUADB CURSOR CFUNCTIONS
 */

static int Lmdb_OrderClose(lua_State *L) {
    LuaDB_LmdbOrder *cur = luaL_checkudata(L, lua_upvalueindex(1),
                                            LMDB_CURSOR_REGISTRY_NAME);

    if (!cur) {
        luaL_error(L, "LMDB order cursor not found");
        return 0;
    }

    mdb_cursor_close(cur->cur);
    free(cur->prefix);
    cur->cur = NULL;
    cur->prefix = NULL;
    return 0;
}

/*
 * PRIVATE UTILITY FUNCTIONS
 */

// Create a new MDB_env with the given options.
static MDB_env *OpenLmdbEnv(const char *path, unsigned int flags, unsigned int max_readers, size_t map_size, int *err) {
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
static void ReadLmdbEnvParamsFromLua(lua_State *L, unsigned int *flags, unsigned int *max_readers, size_t *map_size) {
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
    for (luadb_env_flag *f = &lmdb_env_opts[0]; f->name != NULL; f++) {
        // Try to get the field with flag name
        lua_pushstring(L, f->name);
        ftype = lua_gettable(L, -2);

        // If it wasn't given, just skip it (all options are optional)
        if (ftype == LUA_TNONE) {
            continue;
        }

        // Convert to boolean and add the flag if true
        val = lua_toboolean(L, -1);
        if (val) {
            *flags = *flags | f->val;
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
static inline MDB_env *CheckLmdbEnvParam(lua_State *L, int idx) {
    assert(L);

    MDB_env **loc = luaL_checkudata(L, idx, LMDB_ENV_REGISTRY_NAME);

    if (!(*loc)) {
        luaL_error(L, "LMDB environment not found");
        return NULL;
    }

    return *loc;
}

// Check for a LuaDB_LmdbTx as a function parameter and dererence it.
//
// This function issues a Lua error if the transaction variable isn't
// found (i.e. is NULL), so it would technically be safe to avoid a
// NULL check on the return value here.
static inline LuaDB_LmdbTx *CheckLmdbTxParam(lua_State *L, int idx) {
    assert(L);

    LuaDB_LmdbTx *loc = luaL_checkudata(L, idx, LMDB_TX_REGISTRY_NAME);

    if (!loc) {
        luaL_error(L, "LMDB transaction not found");
        return NULL;
    }

    return loc;
}

// Clean up any lingering cursors and transactions before closing the
// entire environment.
static void CleanLmdbEnvRefTable(lua_State *L, char *uuid) {
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
// Assumes it is being called by LmdbEnv_Readers and, thus,
// that there is a table at the (1) index on the stack and
// an index integer right above that.
static int LmdbEnvReaderTableCreate(const char *msg, lua_State *L) {
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
static char *CreateLmdbEnvRefTable(lua_State *L) {
    assert(L);

    char *uuid = LuaDB_CreateGuid();
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
static void AddTxToLmdbEnvRefTable(lua_State *L, MDB_env *env, MDB_txn *txn, int idx) {
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
static void RemoveTxFromLmdbEnvRefTable(lua_State *L, MDB_env *env, MDB_txn *txn, int idx) {
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

// Concatenate all of the variadic Lua parameters into a single key and
// return that (and length). Note that all parameters between `idx` and
// `last` will be considered as string keys.
static char *GetLmdbKeyFromLua(lua_State *L, size_t *len, int idx, int last) {
    assert(L);
    int elems = last - idx + 1;
    const char *keys[elems];
    size_t lens[elems];
    char types[elems];
    *len = 0;

    // Check that we don't have too many key segments
    if (elems > LMDB_MAX_KEY_SEGMENTS) {
        luaL_error(L, "max number of key segments is %d", LMDB_MAX_KEY_SEGMENTS);
        return NULL;
    }

    // Compute string lengths and cache the strings
    for (int i = 0; i < elems; i++) {
        int type = lua_type(L, i+idx);
        switch(type) {
            case LUA_TNUMBER:
                keys[i] = lua_tolstring(L, i+idx, &lens[i]);
                types[i] = (lua_isinteger(L, i+idx)) ? LMDB_INTEGER_TYPE : LMDB_NUMERIC_TYPE;
                break;
            case LUA_TSTRING:
                keys[i] = lua_tolstring(L, i+idx, &lens[i]);
                types[i] = LMDB_STRING_TYPE;
                break;
            case LUA_TBOOLEAN:
                keys[i] = (lua_toboolean(L, i+idx)) ? "1" : "0";
                lens[i] = 1;
                types[i] = LMDB_BOOLEAN_TYPE;
                break;
            default:
                luaL_error(L, "type '%s' not permitted in keys", lua_typename(L, type));
                return NULL;
        }

        if (lens[i] > LMDB_MAX_KEY_SEG_LENGTH) {
            luaL_error(L, "length of individual key piece exceeds %d %d %s", LMDB_MAX_KEY_SEG_LENGTH);
            return NULL;
        }

        *len += lens[i];
    }

    int err;
    char *tkey = GenerateLuaDbKey(&err, len, keys, lens, types, elems);
    if (err) {
        luaL_error(L, "could not allocate memory for key");
        return NULL;
    }
    return tkey;
}

// Create an LuaDB key given a series of character arrays of given sizes.
//
// The array is a character array of this form:
// key = {
//     {
//         [0] = length of segment, `n`, as unsigned char,
//         [1] = type as unsigned char,
//         [2-n] = segment value (all values except boolean are stored as strings)
//     },
//     { ... },
//     { ... }
// }
static char *GenerateLuaDbKey(int *err, size_t *len, const char **keys, size_t *lens, char *types, int elems) {
    char *tkey = NULL;
    *err = 0;

    // Allocate memory for the new key
    *len = *len + (elems * 2);
    tkey = malloc(*len);
    if (!tkey) {
        *err = 1;
        return NULL;
    }

    // Generate the new key
    int keyidx = 0;
    for (int i = 0; i < elems; i++) {
        tkey[keyidx] = (unsigned char)lens[i];
        tkey[keyidx+1] = (unsigned char)types[i];
        memcpy(&tkey[keyidx+2], keys[i], lens[i]);
        keyidx += lens[i] + 2;
    }

    return tkey;
}

// Given a key and a prefix, return the next single key segment.
//
// The return value is merely the pointer offset within the given
// key value, so the caller should NOT free this value.
static const char *NextKeySegment(const char *key, size_t keylen, size_t pfxlen, size_t *len, int *type) {
    if (pfxlen >= keylen) { return NULL; }
    *len = (size_t)key[pfxlen];
    *type = (int)key[pfxlen+1];
    return &key[pfxlen+2];
}

// Accept a generic type of data, scan it into a value, and push
// that value onto the stack. Note that the type specified by the
// `type` parameter refers to the LMDB type, not the LUA_T* values.
static bool PushValueByType(lua_State *L, const char *val, size_t len, int type) {
    // Determine which data type to push back onto the stack
    switch(type) {
        case LMDB_STRING_CHAR:
            lua_pushlstring(L, val, len);
            return true;
        case LMDB_NUMERIC_CHAR: {
            lua_Number f;
#ifdef _WIN32
            __mingw_sscanf(val, "%lf", &f);
#else
            sscanf(val, "%lf", &f);
#endif
            lua_pushnumber(L, f);
            return true;
        }
        case LMDB_INTEGER_CHAR: {
            lua_Integer i;
#ifdef _WIN32
            __mingw_sscanf(val, "%lld", &i);
#else
            sscanf(val, "%lld", &i);
#endif
            lua_pushinteger(L, i);
            return true;
        }
        case LMDB_BOOLEAN_CHAR: {
            int b;
#ifdef _WIN32
            __mingw_sscanf(val, "%d", &b);
#else
            sscanf(val, "%d", &b);
#endif
            lua_pushboolean(L, b);
            return true;
        }
        default:
            return false;
    }
}

// Produce a key dump string of a Lua DB key for debugging purposes
static char *CreateKeyDumpString(MDB_val *key) {
    assert(key);

    size_t len = key->mv_size * 2;
    size_t remaining = len;
    char *str = malloc(len);
    if (!str) {
        return NULL;
    }

    // Create the beginning of the string
    str[0] = '[';
    char *cur = &str[1];

    // Iterate on each individual key segment
    size_t seglen;
    for (size_t i = 0; i < key->mv_size; i += (seglen + 2)) {
        // Read metadata from the key segment
        unsigned char *data = &key->mv_data[i];
        seglen = (size_t)(data[0]);
        unsigned char type = data[1];

        // Determine format string and length
        const char *fmt;
        size_t estimate;
        switch (type) {
            case LMDB_BOOLEAN_CHAR:
                fmt = ((int) data[2]) ? "true, " : "false, ";
                estimate = ((int) data[2]) ? 6 : 7;
                break;
            case LMDB_NUMERIC_CHAR:     // Fall through
            case LMDB_INTEGER_CHAR:
                fmt = "%.*s, ";
                estimate = seglen + 2;
                break;
            case LMDB_STRING_CHAR:
                fmt = "\"%.*s\", ";
                estimate = seglen + 4;
                break;
            default:
                continue;
        }

        // Reallocate the buffer if we run out of space
        if (remaining >= estimate) {
            size_t offset = (cur - str);    // Cache the cur offset
            size_t newlen = (len * 2);
            str = realloc(str, newlen);
            if (!str) { return NULL; }
            cur = (str + offset);           // Reset cur to the previous offset
            remaining = (newlen - offset);  // Determine the new remainder
        }

        // Print into the string buffer
        int diff = sprintf(cur, fmt, seglen, &data[2]);
        cur += diff;
        remaining -= diff;
    }

    // NUL-terminate the string before returning
    cur = cur - 2;
    cur[0] = ']';
    cur[1] = '\0';
    return str;
}

// Emulate the $ORDER function from MUMPS. Iterate over the keys in
// key order, using prefix comparison to verify that we're only ever
// considering "child" nodes.
static int LuaDbOrderTxClosure(lua_State *L) {
    LuaDB_LmdbOrder *cur = luaL_checkudata(L, lua_upvalueindex(1),
                                            LMDB_CURSOR_REGISTRY_NAME);

    if (!cur) {
        luaL_error(L, "LMDB order cursor not found");
        return 0;
    }

    MDB_val key;
    MDB_val val;
    int err;

    // Set our range to the given prefix
    if (cur->op == MDB_SET_RANGE) {
        key.mv_size = cur->pfxlen;
        key.mv_data = cur->prefix;
    }

    // Get the value
    err = mdb_cursor_get(cur->cur, &key, &val, cur->op);

    // Check if LDMB is reporting that it is not found
    if (err == MDB_NOTFOUND) {
        return 0;
    }

    // Verify that we're on the right prefix and we didn't skip
    if (strncmp(cur->prefix, (char *)key.mv_data, cur->pfxlen) != 0) {
        return 0;
    }

    // Get the next value and push it onto the Lua stack
    size_t len;
    int type;
    const char *seg = NextKeySegment((char *) key.mv_data, key.mv_size,
                                     cur->pfxlen, &len, &type);
    if (!seg) { return 0; }

    // Push the generic value onto the stack
    if (!PushValueByType(L, seg, len, type)) { return 0; }

    // Move to the next element continuously now
    cur->op = MDB_NEXT;
    return 1;
}

// Create the metatable for LMDB Environment objects.
static bool CreateLmdbEnvMetatable(lua_State *L) {
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
    return true;
}

// Create the metatable for LMDB Tx objects
static bool CreateLmdbTxMetatable(lua_State *L) {
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
    return true;
}

// Create the metatable for LMDB Cursor objects
static bool CreateLmdbCursorMetatable(lua_State *L) {
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
    return true;
}
