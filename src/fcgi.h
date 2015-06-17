/*****************************************************************************
 * LuaDB :: fcgi.h
 *
 * Push HTTP request information into the Lua state and start a FastCGI worker.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_FCGI_H
#define LUADB_FCGI_H

/**
 * @brief Start a LuaDB FastCGI worker.
 */
int luadb_start_fcgi_worker(const char *path);

#endif //LUADB_FCGI_H
