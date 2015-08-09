/*****************************************************************************
 * LuaDB :: util.c
 *
 * Utilities for functions that are not in the C99 standard.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

#ifdef _WIN32
static const char *const PATH_SEPARATOR = "\\";
#else  //_WIN32
static const char *const PATH_SEPARATOR = "/";
#endif //_WIN32
#define ENDS_WITH_SEP(str, strlen) (str[strlen-1] == PATH_SEPARATOR[0])

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