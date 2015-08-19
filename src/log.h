/*****************************************************************************
 * LuaDB :: log.h
 *
 * Implement a logging interface for LuaDB (or use syslog).
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#ifndef LUADB_LOG_H
#define LUADB_LOG_H

#if !DEBUG
#define debuglog(fmt) syslog(LOG_DEBUG, fmt)
#define debuglogf(fmt, ...) syslog(LOG_DEBUG, fmt, __VA_ARGS__)
#else
#define debuglog(fmt)
#define debuglogf(fmt, ...)
#endif

#if LUADB_USE_SYSLOG
#include <syslog.h>
#else
#define LOG_EMERG 8
#define LOG_ALERT 7
#define LOG_CRIT 6
#define LOG_ERR 5
#define LOG_WARNING 4
#define LOG_NOTICE 3
#define LOG_INFO 2
#define LOG_DEBUG 1

#define LOG_PID 0
#define LOG_PERROR 0
#define LOG_NDELAY 0
#define LOG_CONS 0

#define LOG_USER 0

extern FILE *__luadb_logfile;
extern char *__luadb_pfx;

void openlog(const char *pfx, int logopt, int facility);
void vsyslog(int priority, const char *fmt, va_list args);
void syslog(int priority, const char *fmt, ...);
void closelog(void);

#endif
#endif //LUADB_LOG_H
