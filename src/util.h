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
