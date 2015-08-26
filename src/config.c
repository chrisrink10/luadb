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

// Configuration value format function to format configuration values
// before they are saved. Accepts current environment configuration,
// given value, and value size.
typedef char *(*ConfigFormatFunc)(LuaDB_EnvConfig *, const char*, size_t);

// Configuration validation function to validate values given by users.
typedef bool (*ConfigValidateFunc)(const char*, size_t);

// Default configuration value details
typedef struct DefaultConfig {
    const char *name;
    size_t len;
    const char *val;
    int offset;
    ConfigFormatFunc fmt;
    ConfigValidateFunc validate;
} DefaultConfig;

/*
 * FORWARD DECLARATIONS
 */

static bool LoadConfigFromLua(lua_State *L, LuaDB_EnvConfig *config);
static inline void LoadConfigSetting(lua_State *L, LuaDB_EnvConfig *config, DefaultConfig *def);
static inline void ApplyDefaultConfig(LuaDB_EnvConfig *config, LuaDB_Setting *s, DefaultConfig *def);
static inline char *FormatRouter(LuaDB_EnvConfig *config, const char *val, size_t len);

// NOTE: Order is extremely important; certain format functions may require
// certain values to be defined in the LuaDB_EnvConfig struct before they
// can work properly.
static DefaultConfig default_cfg[] = {
        { "root", 4, LUADB_WEB_ROOT_L, offsetof(LuaDB_EnvConfig, root), NULL, NULL },
        { "router", 6,  "reqhandler.lua", offsetof(LuaDB_EnvConfig, router), &FormatRouter, NULL },
        { "fcgi_query", 10, "QUERY_STRING", offsetof(LuaDB_EnvConfig, fcgi_query), NULL, NULL },
        { "fcgi_header_prefix", 18, "HTTP_", offsetof(LuaDB_EnvConfig, fcgi_header_prefix), NULL, NULL },
};

/*
 * PUBLIC FUNCTIONS
 */

// Read the environment configuration file into a struct.
bool LuaDB_ReadEnvironmentConfig(LuaDB_EnvConfig *config) {
    assert(config);

    // Create the new filename
    char *cfgfile = LuaDB_PathJoin(LUADB_CONFIG_FOLDER,
                                   LUADB_DEFAULT_CONFIG_FILE);
    if (!cfgfile) {
        return false;
    }

    // Spawn a quick Lua state to read the file
    lua_State *L = LuaDB_NewState();
    if (!L) {
        free(cfgfile);
        return false;
    }

    // Read in our configuration file
    int err = luaL_dofile(L, cfgfile);
    if (err) {
        free(cfgfile);
        lua_close(L);
        return false;
    }

    // Read in the configuration options
    if (!LoadConfigFromLua(L, config)) {
        lua_close(L);
        return false;
    }

    lua_close(L);
    return true;
}

// Clean up any strings saved in the environment configuration.
void LuaDB_CleanEnvironmentConfig(LuaDB_EnvConfig *config) {
    assert(config);

    free(config->root.val);
    free(config->router.val);
    free(config->fcgi_query.val);
}

/*
 * PRIVATE FUNCTIONS
 */

// Load the configuration files from the environment
static bool LoadConfigFromLua(lua_State *L, LuaDB_EnvConfig *config) {
    assert(L);
    assert(config);

    size_t len = sizeof(default_cfg) / sizeof(default_cfg[0]);
    for (int i = 0; i < len; i++) {
        LoadConfigSetting(L, config, &default_cfg[i]);
    }

    return true;
}

// Load a generic configuration setting from the config table.
static inline void LoadConfigSetting(lua_State *L, LuaDB_EnvConfig *config, DefaultConfig *def) {
    assert(L);
    assert(config);
    assert(def);

    // Get a pointer to the setting in the configuration
    LuaDB_Setting *s = (LuaDB_Setting *)((char *)config + def->offset);

    // Load the configuration given in environment config file
    lua_pushlstring(L, def->name, def->len);

    // Environment config contained a value
    if (lua_gettable(L, -2) != LUA_TNIL) {
        size_t len;
        const char *setval = luaL_tolstring(L, -1, &len);

        // Validate the user's given value
        if (def->validate && !def->validate(setval, len)) {
            ApplyDefaultConfig(config, s, def);
            lua_pop(L, 2);
            return;
        }

        // Apply any formatting to it
        if (def->fmt) {
            s->val = def->fmt(config, setval, len);
            s->len = strlen(s->val);
        } else {
            s->val = LuaDB_StrDupLen(setval, len);
            s->len = len;
        }
        lua_pop(L, 2);
    } else {
        // No value found, use defaults
        ApplyDefaultConfig(config, s, def);
    }
}

// Apply default configuration to a setting.
static inline void ApplyDefaultConfig(LuaDB_EnvConfig *config, LuaDB_Setting *s, DefaultConfig *def) {
    s->len = strlen(def->val);
    s->val = LuaDB_StrDupLen(def->val, s->len);

    if (def->fmt) {
        char *newval = def->fmt(config, s->val, s->len);
        free(s->val);
        s->val = newval;
        s->len = strlen(s->val);
    }
}

/*
 * PRIVATE FORMAT FUNCTIONS
 */

// Format the router name to be a fully qualified path
static inline char *FormatRouter(LuaDB_EnvConfig *config, const char *val, size_t len) {
    return LuaDB_PathJoinLen(config->root.val, config->root.len, val, len);
}