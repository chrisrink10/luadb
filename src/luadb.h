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
static const char *const LUADB_CONFIG_FOLDER = "C:\\luadb\\config\\";
static const char *const LUADB_WEB_ROOT = "C:\\luadb\\web\\";
static const char *const LUADB_LOG_FOLDER = "C:\\luadb\\log\\";
#else
static const char *const LUADB_CONFIG_FOLDER = "/etc/luadb/";
static const char *const LUADB_WEB_ROOT = "/var/www/luadb/";
static const char *const LUADB_LOG_FOLDER = "/var/log/luadb/";
#endif

#endif //LUADB_LUADB_H
