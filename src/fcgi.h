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
 * @brief Start a LuaDB FastCGI worker listening on the specified device.
 *
 * @param device the file/device to listen for FastCGI connections
 * @returns a system exit code indicating failure or success
 */
int LuaDB_FcgiStartWorker(const char *device);

/**
 * @brief Start a LuaDB FastCGI worker listening on the specified device
 * with the given Lua include paths already set in the environment.
 *
 * @param device the file/device to listen for FastCGI connections
 * @param paths an array of C strings with additional Lua include paths
 * @param npaths the number of Lua include paths in @c paths
 * @returns a system exit code indicating failure or success
 */
int LuaDB_FcgiStartWorkerWithPaths(const char *device, const char **paths, size_t npaths);

#endif //LUADB_FCGI_H
