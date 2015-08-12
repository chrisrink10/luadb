/*****************************************************************************
 * LuaDB :: util.c
 *
 * Utilities for functions that are not in the C99 standard.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

static const char QUERY_STRING_KEY_SEPARATOR = '=';
static const char QUERY_STRING_FIELD_SEPARATOR = '&';
#ifdef _WIN32
static const char *const PATH_SEPARATOR = "\\";
#else  //_WIN32
static const char *const PATH_SEPARATOR = "/";
#endif //_WIN32
#define ENDS_WITH_SEP(str, strlen) (str[strlen-1] == PATH_SEPARATOR[0])
#define REMAINDER_LEN(iter, field) (iter->qslen - (field - iter->qs))

char *luadb_path_join(const char *x, const char *y) {
    return luadb_path_njoin(x, strlen(x), y, strlen(y));
}

char *luadb_path_njoin(const char *x, size_t xlen, const char *y, size_t ylen) {
    int hassep = ENDS_WITH_SEP(x, xlen);
    size_t len = xlen + ylen + (hassep ? 1 : 2);
    char *path = malloc(len);
    if (!path) {
        return NULL;
    }

    snprintf(path, len, "%s%s%s", x, (hassep ? "" : PATH_SEPARATOR), y);
    return path;
}

void luadb_init_query_iter(luadb_query_iter *iter, const char* qs, size_t *len) {
    assert(iter);
    if (!qs) { return; }

    iter->qs = qs;
    iter->qslen = (len) ? (*len) : strlen(qs);
    iter->cur = qs;
    iter->key = NULL;
    iter->keylen = 0;
    iter->val = NULL;
    iter->vallen = 0;
}

bool luadb_next_query_field(luadb_query_iter *iter) {
    // Verify we have an iterator and we've not gone past the end
    if (!iter || !iter->cur || iter->cur[0] == '\0') {
        return false;
    }

    // Set the key pointer to our current token; find the key/value separator
    iter->key = iter->cur;
    char *kvsep = strchr(iter->cur, (int) QUERY_STRING_KEY_SEPARATOR);

    // No value was found, so we only have a key
    if (!kvsep) {
        iter->cur = NULL;   // Set current NULL to indicate this iterator is done
        iter->keylen = REMAINDER_LEN(iter, iter->key);
        iter->val = NULL;   // No value found
        iter->vallen = 0;   // Value length is 0
        return true;
    } else {
        iter->cur = kvsep + 1;
        iter->keylen = (kvsep - iter->key);
    }

    // Seek to the end of the field (i.e. end of the value)
    char *fsep = strchr(iter->cur, (int) QUERY_STRING_FIELD_SEPARATOR);
    iter->val = iter->cur;  // Set to the current starting point

    // No next field was found; this is fine
    if (!fsep) {
        iter->cur = NULL;       // Set current null to indicate this iterator is done
        iter->vallen = REMAINDER_LEN(iter, iter->val);
    } else {
        iter->cur = fsep + 1;
        iter->vallen = (fsep - iter->val);
    }

    return true;
}

char *luadb_strdup(const char *in) {
    return luadb_strndup(in, strlen(in));
}

char *luadb_strndup(const char *in, size_t n) {
    char *new = malloc(n + 1);
    if (!new) {
        return NULL;
    }

    strncpy(new, in, n);
    return new;
}