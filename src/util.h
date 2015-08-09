/*****************************************************************************
 * LuaDB :: util.h
 *
 * Utilities for functions that are not in the C99 standard.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_UTIL_H
#define LUADB_UTIL_H

/**
 * @brief Join two directory paths.
 */
char *luadb_path_join(const char *x, const char *y);

/**
 * @brief Join two directory paths with known lengths.
 */
char *luadb_path_njoin(const char *x, size_t xlen, const char *y, size_t ylen);

/**
 * @brief Duplicate the input string up to the first @c NUL character.
 */
char *luadb_strdup(const char *in);

/**
 * @brief Duplicate the first @c n characters of the input string.
 *
 * This function will always return a @c NUL terminated string.
 */
char *luadb_strndup(const char *in, size_t n);

#endif //LUADB_UTIL_H
