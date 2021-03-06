/*****************************************************************************
 * LuaDB :: query.c
 *
 * HTTP query string iterator.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <string.h>

#include "query.h"

#define QUERY_STRING_KEY_SEPARATOR '='
#define QUERY_STRING_FIELD_SEPARATOR '&'
#define QUERY_STRING_FIELD_SEPARATOR_BACKUP ';'

static size_t RemainderLength(LuaDB_QueryIter *iter, const char *field);
static char HexToAscii(char *hex);
static int HexDigitToDecimal(char hex);

/*
 * PUBLIC FUNCTIONS
 */

void LuaDB_QueryIterInit(LuaDB_QueryIter *iter, const char *qs, size_t *len) {
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

bool LuaDB_QueryIterNext(LuaDB_QueryIter *iter) {
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
        iter->keylen = (onlykey) ?
                       (size_t)(fsep - iter->key) :
                       RemainderLength(iter, iter->key);
        iter->val = NULL;   // No value found
        iter->vallen = 0;   // Value length is 0
        return true;
    }

    // Set a value pointer and compute a key length
    iter->val = kvsep + 1;
    iter->keylen = (size_t)(kvsep - iter->key);

    // No next field was found; this is fine
    if (!fsep) {
        iter->cur = NULL;       // Set current null to indicate this iterator is done
        iter->vallen = RemainderLength(iter, iter->val);
    } else {
        iter->cur = fsep + 1;
        iter->vallen = (size_t)(fsep - iter->val);
    }

    return true;
}

char *LuaDB_QueryIterKey(LuaDB_QueryIter *iter, size_t *len) {
    return LuaDB_QueryStrDecode(iter->key, iter->keylen, len);
}

char *LuaDB_QueryIterVal(LuaDB_QueryIter *iter, size_t *len) {
    return LuaDB_QueryStrDecode(iter->val, iter->vallen, len);
}

char *LuaDB_QueryStrDecode(const char *in, size_t inlen, size_t *outlen) {
    assert(outlen);
    *outlen = 0;
    if (!in) { return NULL; }

    char *out = malloc(inlen);
    if (!out) {
        return NULL;
    }

    for (size_t i = 0; i < inlen; ) {
        bool enough_chars;
        switch (in[i]) {
            case '%':
                enough_chars = (i+2 < inlen) ? true : false;
                if (enough_chars) {
                    char hex[2];
                    hex[0] = in[i+1];
                    hex[1] = in[i+2];
                    i += 3;
                    out[*outlen] = HexToAscii(hex);
                    (*outlen)++;
                }
                break;
            case '+':
                out[*outlen] = ' ';
                i++;
                (*outlen)++;
                break;
            default:
                out[*outlen] = in[i];
                i++;
                (*outlen)++;
                break;
        }
    }

    return out;
}

/*
 * PRIVATE FUNCTIONS
 */

// Return the length remaining from the given field in a query string to the end
static size_t RemainderLength(LuaDB_QueryIter *iter, const char *field) {
    return iter->qslen - (size_t)(field - iter->qs);
}

// Decode a two digit hex code into a decimal value.
static char HexToAscii(char *hex) {
    int val = ((HexDigitToDecimal(hex[0]) * 16) + (HexDigitToDecimal(hex[1])));
    return (char)val;
}

// Convert a single hex digit into decimal.
static int HexDigitToDecimal(char hex) {
    int v = -1;
    switch(hex) {
        case '0': v = 0; break;
        case '1': v = 1; break;
        case '2': v = 2; break;
        case '3': v = 3; break;
        case '4': v = 4; break;
        case '5': v = 5; break;
        case '6': v = 6; break;
        case '7': v = 7; break;
        case '8': v = 8; break;
        case '9': v = 9; break;
        case 'A': // Fall through
        case 'a': v = 10; break;
        case 'B': // Fall through
        case 'b': v = 11; break;
        case 'C': // Fall through
        case 'c': v = 12; break;
        case 'D': // Fall through
        case 'd': v = 13; break;
        case 'E': // Fall through
        case 'e': v = 14; break;
        case 'F': // Fall through
        case 'f': v = 15; break;
        default:
            break;
    }
    return v;
}
