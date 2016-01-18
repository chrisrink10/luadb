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
char *LuaDB_PathJoin(const char *x, const char *y);

/**
 * @brief Join two directory paths with known lengths.
 */
char *LuaDB_PathJoinLen(const char *x, size_t xlen, const char *y, size_t ylen);

/**
 * @brief Duplicate the input string up to the first @c NUL character.
 */
char *LuaDB_StrDup(const char *in);

/**
 * @brief Duplicate the first @c n characters of the input string.
 *
 * This function will always return a @c NUL terminated string.
 */
char *LuaDB_StrDupLen(const char *in, size_t n);

/**
 * @brief Verify if a Lua numeric value is an integer.
 *
 * @param n a numeric value from Lua
 * @param v a pointer to the Lua integer version of @c n, if
 *          this function returns true
 * @returns true if value in @c n was integral; false otherwise
 */
bool LuaDB_NumberIsInt(double n, long long *v);

#endif //LUADB_UTIL_H
