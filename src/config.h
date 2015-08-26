/*****************************************************************************
 * LuaDB :: config.h
 *
 * Read and expose environment configuration.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_CONFIG_H
#define LUADB_CONFIG_H

#include <stddef.h>

/** Default configuration file name */
static const char *const LUADB_DEFAULT_CONFIG_FILE = "config.lua";

typedef struct LuaDB_Setting {
    char *val;
    size_t len;
} LuaDB_Setting;

typedef struct LuaDB_EnvConfig {
    LuaDB_Setting root;
    LuaDB_Setting router;
    LuaDB_Setting fcgi_query;
    LuaDB_Setting fcgi_header_prefix;
} LuaDB_EnvConfig;

/**
 * @brief Read in any environment settings into the given structure.
 */
bool LuaDB_ReadEnvironmentConfig(LuaDB_EnvConfig *config);

/**
 * @brief
 */
void LuaDB_CleanEnvironmentConfig(LuaDB_EnvConfig *config);

#endif //LUADB_CONFIG_H
