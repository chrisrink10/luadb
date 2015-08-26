/*****************************************************************************
 * LuaDB :: luadb.h
 *
 * Public header file for LuaDB.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_LUADB_H
#define LUADB_LUADB_H

static const char *const LUADB_NAME = "LuaDB";
static const char *const LUADB_EXEC = "luadb";
static const int LUADB_MAJOR_VERSION = 0;
static const int LUADB_MINOR_VERSION = 1;
static const int LUADB_PATCH_VERSION = 0;
static const char *const LUADB_PATCH_STATUS = "alpha";

#ifdef _WIN32
#define LUADB_CONFIG_FOLDER_L "C:\\luadb\\config\\"
#define LUADB_WEB_ROOT_L "C:\\luadb\\web\\"
#define LUADB_LOG_FOLDER_L "C:\\luadb\\log\\"
#define LUADB_PATH_SEPARATOR_L "\\"
#else
#define LUADB_CONFIG_FOLDER_L "/etc/luadb/"
#define LUADB_WEB_ROOT_L "/var/www/luadb/"
#define LUADB_LOG_FOLDER_L "/var/log/luadb/"
#define LUADB_PATH_SEPARATOR_L "/"
#endif
static const char *const LUADB_CONFIG_FOLDER = LUADB_CONFIG_FOLDER_L;
static const char *const LUADB_WEB_ROOT = LUADB_WEB_ROOT_L;
static const char *const LUADB_LOG_FOLDER = LUADB_LOG_FOLDER_L;
static const char *const LUADB_PATH_SEPARATOR = LUADB_PATH_SEPARATOR_L;

#endif //LUADB_LUADB_H
