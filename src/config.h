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

typedef struct LuaDB_Env_Config {
    LuaDB_Setting root;
    LuaDB_Setting router;
    LuaDB_Setting fcgi_query;
} LuaDB_Env_Config;

/**
 * @brief Read in any environment settings into the given structure.
 */
int luadb_read_environment_config(LuaDB_Env_Config *config);

/**
 * @brief
 */
void luadb_clean_environment_config(LuaDB_Env_Config *config);

#endif //LUADB_CONFIG_H
