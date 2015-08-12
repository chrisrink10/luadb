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
 * @brief Iterator structure for parsing out a query string.
 */
typedef struct luadb_query_iter {
    const char *qs;     /** the query string */
    size_t qslen;       /** length of the query string; if NULL will be set to the length */
    const char *cur;    /** integer index to the current position */
    const char *key;    /** the current key; NULL if there are no further keys */
    size_t keylen;      /** length of the key */
    const char *val;    /** the current value; NULL if there are no further values */
    size_t vallen;      /** length of the value */
} luadb_query_iter;

/**
 * @brief Join two directory paths.
 */
char *luadb_path_join(const char *x, const char *y);

/**
 * @brief Join two directory paths with known lengths.
 */
char *luadb_path_njoin(const char *x, size_t xlen, const char *y, size_t ylen);

/**
 * @brief Initialize a query string iterator.
 * @param iter a pointer to the iterator structure
 * @param qs a pointer to the query string
 * @param len the length of the query string; pass NULL to calculate this
 */
void luadb_init_query_iter(luadb_query_iter *iter, const char* qs, size_t *len);

/**
 * @brief Iterate on a query string to reveal each field one key/value pair
 * at a time. Callers should call @c luadb_init_query_iter first. Callers
 * should also check that the value has a non-zero length, since it may
 * be that the last item was only a key.
 * @param iter a query iterator
 * @returns true if the current k/v pair is valid
 */
bool luadb_next_query_field(luadb_query_iter *iter);

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
