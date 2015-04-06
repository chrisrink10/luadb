/*****************************************************************************
 * LuaDB :: main.c
 *
 * Main entry point for LuaDB from the command line.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#ifdef LUADB_USE_GETOPT
#include <unistd.h>
#endif
#ifdef LUADB_USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"

#include "luadb.h"
#include "state.h"

/* Handle line history and line editing for the REPL */
#ifdef LUADB_USE_READLINE
#define REPL_BUF_ALLOC() NULL
#define REPL_READLINE(out, in, buf) (buf = readline("> "))
#define REPL_ADD_HISTORY(buf) (buf && *buf) ? (add_history(buf)) : (void)0
#define REPL_BUF_FREE(buf) free(buf)
#else
static const int REPL_BUFFER_SIZE = 512;
#define REPL_BUF_ALLOC() malloc(REPL_BUFFER_SIZE)
#define REPL_READLINE(out, in, buf) ((fprintf(out, "> ") >= 0) && \
                                     (fgets(buf, REPL_BUFFER_SIZE, in)))
#define REPL_ADD_HISTORY(buf) (void)0
#define REPL_BUF_FREE(buf) (void)0
#endif

/* Handle command line options */
#ifdef _WIN32
#define LUADB_ARG_SYMBOL '/'
#else  //_WIN32
#define LUADB_ARG_SYMBOL '-'
#endif //_WIN32


// Print the short usage line
static void print_usage(FILE *dest, const char *cmd) {
    fprintf(dest, "usage: %s [-h] [-n hostname:port] [file]\n", cmd);
}

// Prints the name and destination of the file
static void print_name_and_version(FILE *dest) {
    fprintf(dest, "%s v%d.%d.%d %s\n", LUADB_NAME,
            LUADB_MAJOR_VERSION, LUADB_MINOR_VERSION,
            LUADB_PATCH_VERSION, LUADB_PATCH_STATUS);
}

// Print the LuaDB help to the given destination file
static void print_help(FILE *dest, const char *cmd) {
    print_usage(dest, cmd);
    fprintf(dest, "\n");
    print_name_and_version(dest);
    fprintf(dest, "\n");
    fprintf(dest, "Options and arguments:\n");
    fprintf(dest, "-n : start a FastCGI worker\n");
    fprintf(dest, "-h : print out this help text\n");
}

// Start the Lua REPL.
static int start_repl(FILE *outdev, FILE *indev, FILE *errdev) {
    int exit_code = EXIT_SUCCESS;
    char *buf = REPL_BUF_ALLOC();

    // Introduce ourselves...
    print_name_and_version(outdev);

    // Open the Lua state
    lua_State *L = luadb_new_state();
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
static int run_script(const char *fname, FILE *outdev, FILE *indev, FILE *errdev) {
    int exit_code = EXIT_SUCCESS;
    lua_State *L = luadb_new_state();
    if (!L) {
        fprintf(errdev, "%s: could not create Lua state\n", LUADB_EXEC);
        return EXIT_FAILURE;
    }
    luadb_set_relative_path(L, fname);

    int err = luaL_dofile(L, fname);
    if (err) {
        fprintf(errdev, "%s: %s\n", LUADB_EXEC, lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
        exit_code = EXIT_FAILURE;
    }

    lua_close(L);
    return exit_code;
}

// Parse command line arguments
static int parse_arguments(int argc, char *const argv[]) {
    int exit_code = EXIT_SUCCESS;
    char *fname = NULL;

#ifdef LUADB_USE_GETOPT
    int c;

    // Parse available arguments
    while ((c = getopt (argc, argv, "hn:")) != -1) {
        switch (c) {
            case 'h':
                print_help(stdout, argv[0]);
                goto exit_main;
            case 'n':
                fprintf(stderr, "%s: not implemented yet (arg: '%s')\n",
                        LUADB_EXEC, optarg);
                goto exit_main;
            default:
                fprintf(stderr, "%s: invalid option '%c' selected\n",
                        LUADB_EXEC, c);
                print_usage(stdout, argv[0]);
                exit_code = EXIT_FAILURE;
                goto exit_main;
        }
    }

    // File name
    if (optind < argc) {
        fname = argv[optind];
    }
#else  // LUADB_USE_GETOPT
    for (int i = 1; i < argc; i++) {
        size_t len = strlen(argv[i]);
        switch(argv[i][0]) {
            case LUADB_ARG_SYMBOL:
                if (len < 2) {
                    fprintf(stderr, "%s: option must be specified after '-'\n",
                            LUADB_EXEC);
                    print_usage(stdout, argv[0]);
                    exit_code = EXIT_FAILURE;
                    goto exit_main;
                }
                switch(argv[i][1]) {
                    case 'h':
                        print_help(stdout, argv[0]);
                        goto exit_main;
                    case 'n':
                        fprintf(stderr, "%s: not working yet :)\n",
                                LUADB_EXEC);
                        goto exit_main;
                    default:
                        fprintf(stderr, "%s: invalid option '%s' selected\n",
                                LUADB_EXEC, argv[i]);
                        print_usage(stdout, argv[0]);
                        exit_code = EXIT_FAILURE;
                        goto exit_main;
                }
                break;
            default:
                fname = (char*) argv[i];
        }
    }
#endif // LUADB_USE_GETOPT

    // With no filename given, we can start the REPL, otherwise
    // run that script
    if (!fname) {
        exit_code = start_repl(stdout, stdin, stderr);
    } else {
        exit_code = run_script(fname, stdout, stdin, stderr);
    }

exit_main:
    return exit_code;
}

int main(int argc, char *const argv[]) {
    return parse_arguments(argc, argv);
}
