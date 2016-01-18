/*****************************************************************************
 * LuaDB :: util.c
 *
 * Utilities for functions that are not in the C99 standard.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "luadb.h"
#include "util.h"

static bool EndsWithPathSeparator(const char *str, size_t len);

/*
 * PUBLIC FUNCTIONS
 */

char *LuaDB_PathJoin(const char *x, const char *y) {
    return LuaDB_PathJoinLen(x, strlen(x), y, strlen(y));
}

char *LuaDB_PathJoinLen(const char *x, size_t xlen, const char *y, size_t ylen) {
    bool hassep = EndsWithPathSeparator(x, xlen);
    size_t len = xlen + ylen + (hassep ? 1 : 2);
    char *path = malloc(len);
    if (!path) {
        return NULL;
    }

    snprintf(path, len, "%s%s%s", x, (hassep ? "" : LUADB_PATH_SEPARATOR_L), y);
    return path;
}

char *LuaDB_StrDup(const char *in) {
    return LuaDB_StrDupLen(in, strlen(in));
}

char *LuaDB_StrDupLen(const char *in, size_t n) {
    char *new = malloc(n + 1);
    if (!new) {
        return NULL;
    }

    memcpy(new, in, n);
    new[n] = '\0';
    return new;
}

bool LuaDB_NumberIsInt(double n, long long *v) {
    *v = (long long)n;
    return ((n - (double)*v) == 0.0);
}

/*
 * PRIVATE FUNCTIONS
 */

// Return true if the string of given length ends with a path separator
static bool EndsWithPathSeparator(const char *str, size_t len) {
    return (str[len-1] == LUADB_PATH_SEPARATOR[0]);
}
