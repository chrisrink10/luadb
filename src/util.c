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

#define QUERY_STRING_KEY_SEPARATOR '='
#define QUERY_STRING_FIELD_SEPARATOR '&'
#define QUERY_STRING_FIELD_SEPARATOR_BACKUP ';'
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

    const char *cur = iter->cur;    // Current pointer for loop
    const char *kvsep = NULL;       // Location of k/v separator '='
    const char *fsep = NULL;        // Location of field separator '&' or ';'
    bool done = false;              // Sentinel if we've found a full k/v pair
    bool onlykey = false;           // Indicate if the current value is key only
    iter->key = iter->cur;          // Note the start position for the key

    for (; cur; cur++) {
        switch(cur[0]) {
            case QUERY_STRING_KEY_SEPARATOR:
                kvsep = &cur[0];
                break;
            case QUERY_STRING_FIELD_SEPARATOR:              // Fall through
            case QUERY_STRING_FIELD_SEPARATOR_BACKUP:
                if (!kvsep) { onlykey = true; }
                fsep = &cur[0];
                done = true;
                break;
            case '\0':
                done = true;
                break;
            default:
                break;
        }
        if (done) { break; }
    }

    // We have found a field which only contains a key and either:
    // - there may be more fields => onlykey
    // - we have reached the end => !kvsep
    if (onlykey || !kvsep) {
        iter->cur = (onlykey) ? (fsep + 1) : NULL;
        iter->keylen = (onlykey) ? (fsep - iter->key) : REMAINDER_LEN(iter, iter->key);
        iter->val = NULL;   // No value found
        iter->vallen = 0;   // Value length is 0
        return true;
    }

    // Set a value pointer and compute a key length
    iter->val = kvsep + 1;
    iter->keylen = (kvsep - iter->key);

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

    memcpy(new, in, n);
    new[n] = '\0';
    return new;
}