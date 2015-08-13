/*****************************************************************************
 * LuaDB :: config.c
 *
 * Read and expose environment configuration.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"

#include "config.h"
#include "luadb.h"
#include "state.h"
#include "util.h"

static const char *const LUADB_CONFIG_DEFAULT_ROUTER = "reqhandler.lua";
static const char *const LUADB_CONFIG_DEFAULT_FCGI_QUERY_STRING = "QUERY_STRING";

/*
 * FORWARD DECLARATIONS
 */

static int load_config(lua_State *L, LuaDB_Env_Config *config);

/*
 * PUBLIC FUNCTIONS
 */

// Read the environment configuration file into a struct.
int luadb_read_environment_config(LuaDB_Env_Config *config) {
    assert(config);

    // Create the new filename
    char *cfgfile = luadb_path_join(LUADB_CONFIG_FOLDER, LUADB_DEFAULT_CONFIG_FILE);
    if (!cfgfile) {
        return 0;
    }

    // Spawn a quick Lua state to read the file
    lua_State *L = luadb_new_state();
    if (!L) {
        free(cfgfile);
        return 0;
    }

    // Read in our configuration file
    int err = luaL_dofile(L, cfgfile);
    if (err) {
        free(cfgfile);
        lua_close(L);
        return 0;
    }

    // Read in the configuration options
    if (!load_config(L, config)) {
        lua_close(L);
        return 0;
    }

    lua_close(L);
    return 1;
}

// Clean up any strings saved in the environment configuration.
void luadb_clean_environment_config(LuaDB_Env_Config *config) {
    assert(config);

    free(config->root.val);
    free(config->router.val);
    free(config->fcgi_query.val);
}

/*
 * PRIVATE FUNCTIONS
 */

// Load the configuration files from the environment
static int load_config(lua_State *L, LuaDB_Env_Config *config) {
    assert(L);
    assert(config);

    // TODO: check for and ignore failed configuration
    lua_pushlstring(L, "root", 4);
    if (lua_gettable(L, -2) != LUA_TNIL) {
        size_t rootlen;
        const char *root = luaL_tolstring(L, -1, &rootlen);
        config->root.val = luadb_strndup(root, rootlen);
        config->root.len = rootlen;
        lua_pop(L, 2);
    } else {
        config->root.val = luadb_strdup(LUADB_WEB_ROOT);
        config->root.len = strlen(LUADB_WEB_ROOT);
    }

    lua_pushlstring(L, "router", 6);
    if (lua_gettable(L, -2) != LUA_TNIL) {
        size_t routerlen;
        const char *router = luaL_tolstring(L, -1, &routerlen);
        config->router.val = luadb_path_njoin(config->root.val, config->root.len,
                                              router, routerlen);
        config->router.len = routerlen;
        lua_pop(L, 2);
    } else {
        config->router.val = luadb_path_njoin(config->root.val, config->root.len,
                                              LUADB_CONFIG_DEFAULT_ROUTER,
                                              strlen(LUADB_CONFIG_DEFAULT_ROUTER));
        config->router.len = strlen(config->router.val);
    }

    lua_pushlstring(L, "fcgi_query", 10);
    if (lua_gettable(L, -2) != LUA_TNIL) {
        size_t paramlen;
        const char *fcgi_query = luaL_tolstring(L, -1, &paramlen);
        config->fcgi_query.val = luadb_strndup(fcgi_query, paramlen);
        config->fcgi_query.len = paramlen;
        lua_pop(L, 2);
    } else {
        config->fcgi_query.val = luadb_strdup(LUADB_CONFIG_DEFAULT_FCGI_QUERY_STRING);
        config->fcgi_query.len = strlen(LUADB_CONFIG_DEFAULT_FCGI_QUERY_STRING);
    }

    return 1;
}