/*****************************************************************************
 * LuaDB :: log.h
 *
 * Implement a logging interface for LuaDB.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifdef _WIN32
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "log.h"
#include "util.h"

static const char *priority_name(int priority);

// Log file used in place of actual Syslog daemon on Windows
FILE *__luadb_logfile = NULL;

// Log prefix used for the log file
char *__luadb_pfx = NULL;

// Open the system log file
void openlog(const char *pfx, int logopt, int facility) {
    __luadb_pfx = LuaDB_StrDup(pfx);
    __luadb_logfile = fopen("luadb.log", "a+");
}

// Syslog for variadic argument lists
void vsyslog(int priority, const char *fmt, va_list args) {
    if (__luadb_logfile) {
        // Format and print the current instant
        time_t tval = time(NULL);
        struct tm *t = localtime(&tval);
        char tfmt[24];
        strftime(tfmt, 24, "%b %d %Y %H:%M:%S", t);
        fprintf(__luadb_logfile, "%s ", tfmt);

        // Format and print the rest of the message
        fprintf(__luadb_logfile, "[%s] %s: ", __luadb_pfx, priority_name(priority));
        vfprintf(__luadb_logfile, fmt, args);
        fprintf(__luadb_logfile, "\n");
        fflush(__luadb_logfile);
    }
}

// Define a basic syslog() interface for Windows
void syslog(int priority, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    vsyslog(priority, fmt, argp);
    va_end(argp);
}

// Close the syslog file
void closelog(void) {
    fclose(__luadb_logfile);
}

// Translate the priority values into string names.
static const char *priority_name(int priority) {
    static char *pristr[] = {
            "UNKNOWN", "DEBUG", "INFO", "NOTICE",
            "WARNING", "ERR", "CRIT", "ALERT", "EMERG"
    };

    switch(priority) {
        case LOG_DEBUG:   return pristr[LOG_DEBUG];
        case LOG_INFO:    return pristr[LOG_INFO];
        case LOG_NOTICE:  return pristr[LOG_NOTICE];
        case LOG_WARNING: return pristr[LOG_WARNING];
        case LOG_ERR:     return pristr[LOG_ERR];
        case LOG_CRIT:    return pristr[LOG_CRIT];
        case LOG_ALERT:   return pristr[LOG_ALERT];
        case LOG_EMERG:   return pristr[LOG_EMERG];
        default:          return pristr[0];             // UNKNOWN
    }
}

#endif