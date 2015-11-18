/*****************************************************************************
 * LuaDB :: main.c
 *
 * Main entry point for LuaDB from the command line.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "deps/linenoise/linenoise.h"
#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"

#include "fcgi.h"
#include "luadb.h"
#include "state.h"

/* Handle line history and line editing for the REPL */
#ifndef _WIN32
#define REPL_BUF_ALLOC() NULL
#define REPL_READLINE(out, in, buf) (buf = linenoise("> "))
#define REPL_ADD_HISTORY(buf) (buf && *buf) ? (linenoiseHistoryAdd(buf)) : (void)0
#define REPL_BUF_FREE(buf) free(buf)
#else //_WIN32
static const int REPL_BUFFER_SIZE = 512;
#define REPL_BUF_ALLOC() malloc(REPL_BUFFER_SIZE)
#define REPL_READLINE(out, in, buf) ((fprintf(out, "> ") >= 0) && \
                                     (fgets(buf, REPL_BUFFER_SIZE, in)))
#define REPL_ADD_HISTORY(buf) (void)0
#define REPL_BUF_FREE(buf) (void)0
#endif

// Print the short usage line
static void PrintProgramUsage(FILE *dest, const char *cmd) {
#ifndef _WIN32
    fprintf(dest, "usage: %s [-h] [-f] [-p port|device] [-i path] [file]\n", cmd);
#else
    fprintf(dest, "usage: %s [-h] [-p port|device] [-i path] [file]\n", cmd);
#endif
}

// Prints the name and destination of the file
static void PrintNameAndVersion(FILE *dest) {
    fprintf(dest, "%s v%d.%d.%d %s\n", LUADB_NAME,
            LUADB_MAJOR_VERSION, LUADB_MINOR_VERSION,
            LUADB_PATCH_VERSION, LUADB_PATCH_STATUS);
}

// Print the LuaDB help to the given destination file
static void PrintLuaDbHelp(FILE *dest, const char *cmd) {
    PrintProgramUsage(dest, cmd);
    fprintf(dest, "\n");
    PrintNameAndVersion(dest);
    fprintf(dest, "\n");
    fprintf(dest, "Options:\n");
#ifndef _WIN32
    fprintf(dest, "  -p <port>, -p <dev>  start a FastCGI worker\n");
    fprintf(dest, "  -f                   do not fork this FastCGI process\n");
    fprintf(dest, "  -i path              additional include path for Lua scripts\n");
#else
    fprintf(dest, "  -p <port>, -p <dev>  start as a FastCGI worker\n");
#endif
    fprintf(dest, "  -h                   print out this help text\n");
}

// Start the Lua REPL.
static int StartLuaRepl(FILE *outdev, FILE *indev, FILE *errdev, char **paths, size_t npaths) {
    int exit_code = EXIT_SUCCESS;
    char *buf = REPL_BUF_ALLOC();

    // Introduce ourselves...
    PrintNameAndVersion(outdev);

    // Open the Lua state
    lua_State *L = LuaDB_NewStateWithPaths((const char **)paths, npaths);
    free(paths);
    if (!L) {
        fprintf(errdev, "%s: could not create Lua state\n", LUADB_EXEC);
        goto cleanup_repl;
    }

    // Initiate the REPL
    while(true) {
        if (!REPL_READLINE(outdev, indev, buf)) {
            break;
        }

        REPL_ADD_HISTORY(buf);

        int err = luaL_dostring(L, buf);
        if (err) {
            fprintf(errdev, "%s: %s\n", LUADB_EXEC, lua_tostring(L, -1));
            lua_pop(L, 1);  /* pop error message from the stack */
        }

        REPL_BUF_FREE(buf);
    }

cleanup_repl:
    lua_close(L);
    free(buf);
    return exit_code;
}

// Run a Lua script specified from the command line.
static int RunLuaScript(const char *fname, FILE *outdev, FILE *indev, FILE *errdev, char **paths, size_t npaths) {
    int exit_code = EXIT_SUCCESS;
    lua_State *L = LuaDB_NewStateWithPaths((const char **)paths, npaths);
    free(paths);
    if (!L) {
        fprintf(errdev, "%s: could not create Lua state\n", LUADB_EXEC);
        return EXIT_FAILURE;
    }
    LuaDB_PathAddRelative(L, fname);

    int err = luaL_dofile(L, fname);
    if (err) {
        fprintf(errdev, "%s: %s\n", LUADB_EXEC, lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
        exit_code = EXIT_FAILURE;
    }

    lua_close(L);
    return exit_code;
}

// Start a FastCGI worker process, unless the user requests no fork.
static int StartFcgiWorker(FILE *outdev, char *fcgi_dev, bool should_fork, char **paths, size_t npaths) {
#ifdef _WIN32
    return LuaDB_FcgiStartWorker(fcgi_dev);
#else //_WIN32
    // Do not fork the process, as requested by the caller
    if (!should_fork) {
        return LuaDB_FcgiStartWorkerWithPaths(fcgi_dev, (const char **) paths, npaths);
    }

    // Fork the process
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(outdev, "%s: failed to spawn FastCGI worker process\n",
                LUADB_EXEC);
        return EXIT_FAILURE;
    } else if (pid == 0) {
        int exit_code = LuaDB_FcgiStartWorkerWithPaths(fcgi_dev, (const char **) paths, npaths);
        free(paths);
        _exit(exit_code);
    } else {
        return EXIT_SUCCESS;
    }
#endif
}

// Parse command line arguments
static int ParseCommandLineArguments(int argc, char *const *const argv) {
    int exit_code = EXIT_SUCCESS;
    bool is_fcgi = false;
    bool should_fork = true;
    char *fname = NULL;
    char *fcgi_dev = NULL;
    char **paths = malloc(sizeof(char*) * 5);
    if (!paths) { exit_code = EXIT_FAILURE; goto exit_main; }
    size_t maxpaths = 5;
    size_t npaths = 0;
    size_t pathlen = 0;
    int c;

    // Parse available arguments
    while ((c = getopt (argc, argv, "fhi:p::")) != -1) {
        switch (c) {
            case 'h':
                PrintLuaDbHelp(stdout, argv[0]);
                goto exit_main;
            case 'p':
                fcgi_dev = (optarg) ? (optarg) : ":8000";
                is_fcgi = true;
                break;
            case 'f':
                should_fork = false;
                break;
            case 'i':
                if (!optarg) { break; }
                if (npaths >= maxpaths) {
                    char **tmp = realloc(paths, (size_t)npaths * 2);
                    if (tmp) {
                        paths = tmp;
                        maxpaths = maxpaths * 2;
                    }
                }
                pathlen = strlen(optarg);
                paths[npaths] = malloc(pathlen + 1);
                if (paths[npaths]) {
                    strncpy(paths[npaths], optarg, pathlen);
                    npaths++;
                }
                break;
            default:
                fprintf(stderr, "%s: invalid option '%c' selected\n",
                        LUADB_EXEC, c);
                PrintProgramUsage(stdout, argv[0]);
                exit_code = EXIT_FAILURE;
                goto exit_main;
        }
    }

    // Start the FastCGI (maybe) daemon
    if (is_fcgi) {
        exit_code = StartFcgiWorker(stdout, fcgi_dev, should_fork, paths, npaths);
        goto exit_main;
    }

    // File name
    if (optind < argc) {
        fname = argv[optind];
    }

    // With no filename given, we can start the REPL, otherwise
    // run that script
    if (!fname) {
        exit_code = StartLuaRepl(stdout, stdin, stderr, paths, npaths);
    } else {
        exit_code = RunLuaScript(fname, stdout, stdin, stderr, paths, npaths);
    }

exit_main:
    return exit_code;
}

int main(int argc, char *const argv[]) {
    return ParseCommandLineArguments(argc, argv);
}
