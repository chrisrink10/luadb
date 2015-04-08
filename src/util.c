/*****************************************************************************
 * LuaDB :: util.c
 *
 * Utilities for functions that are not in the C99 standard.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "util.h"

char *luadb_strdup(const char *in) {
    char *new = malloc(strlen(in) + 1);
    if (!new) {
        return NULL;
    }

    strcpy(new, in);
    return new;
}

char *luadb_strndup(const char *in, size_t n) {
    char *new = malloc(n + 1);
    if (!new) {
        return NULL;
    }

    strncpy(new, in, n);
    return new;
}