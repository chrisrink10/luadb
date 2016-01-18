/*****************************************************************************
 * LuaDB :: query.h
 *
 * HTTP query string iterator.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_QUERY_H
#define LUADB_QUERY_H

#include <stdlib.h>
#include <stdbool.h>

/**
 * @brief Iterator structure for parsing out a query string.
 */
typedef struct LuaDB_QueryIter {
    const char *qs;     /** the query string */
    size_t qslen;       /** length of the query string; if NULL will be set to the length */
    const char *cur;    /** integer index to the current position */
    const char *key;    /** the current key; NULL if there are no further keys */
    size_t keylen;      /** length of the key */
    const char *val;    /** the current value; NULL if there are no further values */
    size_t vallen;      /** length of the value */
} LuaDB_QueryIter;

/**
 * @brief Initialize a query string iterator.
 * @param iter a pointer to the iterator structure
 * @param qs a pointer to the query string
 * @param len the length of the query string; pass NULL to calculate this
 */
void LuaDB_QueryIterInit(LuaDB_QueryIter *iter, const char *qs, size_t *len);

/**
 * @brief Iterate on a query string to reveal each field one key/value pair
 * at a time. Callers should call @c LuaDB_QueryIterInit first. Callers
 * should also check that the value has a non-zero length, since it may
 * be that the last item was only a key.
 * @param iter a query iterator
 * @returns true if the current k/v pair is valid
 */
bool LuaDB_QueryIterNext(LuaDB_QueryIter *iter);

/**
 * @brief Get the current key from the LuaDB_QueryIter object.
 *
 * @param iter a pointer to the query string iterator
 * @param len [out] the length of the key
 * @returns a copy of the current query string key
 */
char *LuaDB_QueryIterKey(LuaDB_QueryIter *iter, size_t *len);

/**
 * @brief Get the current value from the LuaDB_QueryIter object.
 *
 * @param iter a pointer to the query string iterator
 * @param len [out] the length of the value
 * @returns a copy of the current query string value
 */
char *LuaDB_QueryIterVal(LuaDB_QueryIter *iter, size_t *len);

/**
 * @brief Provided a query string, duplicate that string byte-by-byte,
 * decoding any percent encoding values used in URI syntax and other
 * URI-safe characters (such as '+' for ' ').
 */
char *LuaDB_QueryStrDecode(const char *in, size_t inlen, size_t *outlen);

#endif //LUADB_QUERY_H
