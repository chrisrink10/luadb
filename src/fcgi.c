/*****************************************************************************
 * LuaDB :: fcgi.c
 *
 * Push HTTP request information into the Lua state and start a FastCGI worker.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/fcgi/fcgiapp.h"

#include "luadb.h"
#include "state.h"
#include "fcgi.h"

static const char *const LUADB_HTTP_REQUEST_HANDLER = "/var/www/luadb/reqhandler.lua";   // Hard-coding for now
static const char *const LUADB_FCGI_LOG_FORMAT = "[LuaDB] FastCGI Error: %s";
static const int FASTCGI_DEFAULT_BACKLOG = 10;
static const int FASTCGI_DEFAULT_NUM_VARS = 20;
static const int FASTCGI_DEFAULT_NUM_HEADERS = 10;  /* Approx num headers/req */
static const char *const FASTCGI_ENVIRON_PREFIX = "HTTP_";
static const size_t FASTCGI_ENVIRON_PREFIX_LEN = 5;

/*
 * FORWARD DECLARATIONS
 */

typedef enum LuaDB_FCGX_Result {
    LUADB_FCGX_SUCCESS,
    LUADB_FCGX_ERROR,
    LUADB_FCGX_FATAL,
} LuaDB_FCGX_Result;

static int init_fcgx_request(FCGX_Request *req, const char *path);
static LuaDB_FCGX_Result process_fcgx_request(FCGX_Request *req);
static void send_http_response(lua_State *L, FCGX_Request *req);
static int create_http_global(lua_State *L, FCGX_Request *req);
static int create_http_response_table(lua_State *L);
static int read_http_request(lua_State *L, FCGX_Request *req);
static int read_http_request_headers(lua_State *L, FCGX_Request *req);
static int read_http_request_vars(lua_State *L, FCGX_Request *req);
static int read_http_request_body(lua_State *L, FCGX_Request *req);
static char* convert_env_to_header(const char *in, size_t len, size_t *outlen);
static char* convert_env_to_lower(const char *in, size_t len);

/*
 * PUBLIC FUNCTIONS
 */

int luadb_start_fcgi_worker(const char *path) {
    FCGX_Request req;

    // Initialize the FCGI app request
    if (init_fcgx_request(&req, path) != 0) {
        return EXIT_FAILURE;
    }

    // Accept FastCGI requests on the given device
    while (FCGX_Accept_r(&req) >= 0) {
        LuaDB_FCGX_Result result = process_fcgx_request(&req);
        if (result == LUADB_FCGX_FATAL) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
};

/*
 * PRIVATE FUNCTIONS
 */

// Initialize the FCGX request structure. Returns 1 on failure.
static int init_fcgx_request(FCGX_Request *req, const char *path) {
    if (FCGX_Init() != 0) {
        return 1;
    }
    int sock = FCGX_OpenSocket(path, FASTCGI_DEFAULT_BACKLOG);
    if (sock == -1) {
        return 1;
    }
    if (FCGX_InitRequest(req, sock, 0) != 0) {
        return 1;
    }

    return 0;
}

// Process an FCGX request body
static LuaDB_FCGX_Result process_fcgx_request(FCGX_Request *req) {
    // Create a new Lua state
    lua_State *L = luadb_new_state();
    if (!L) {
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Could not create new lua_State object.");
        return LUADB_FCGX_ERROR;
    }

    // Read the HTTP request
    int success = create_http_global(L, req);
    if (!success) {
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Could not create `http` global.");
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }
    luadb_add_absolute_path(L, LUADB_WEB_ROOT);

    // Start the routing engine
    int err = luaL_dofile(L, LUADB_HTTP_REQUEST_HANDLER);
    if (err) {
        const char *errmsg = lua_tostring(L, -1);
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT, errmsg);
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Send the response
    send_http_response(L, req);
    lua_close(L);
    return LUADB_FCGX_SUCCESS;
}

// Read the HTTP response from the Lua State and send it to the web server
static void send_http_response(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Push the response table onto the stack
    if (lua_getglobal(L, "http") != LUA_TTABLE) { return; }
    lua_pushlstring(L, "response", 8);
    if (lua_gettable(L, -2) != LUA_TTABLE) { return; }

    // Push headers onto the stack
    lua_pushlstring(L, "headers", 7);
    if (lua_gettable(L, -2) != LUA_TTABLE) { return; }

    // Iterate on the response headers
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if ((lua_type(L, -2) != LUA_TSTRING) ||
                (lua_type(L, -1) != LUA_TSTRING))
        {
            luaL_error(L, "HTTP header fields and their values must be strings.");
            return;
        }
        const char *key = lua_tostring(L, -2);
        const char *val = lua_tostring(L, -1);
        FCGX_FPrintF(req->out, "%s: %s\r\n", key, val);
        lua_pop(L, 1);  /* pop the value */
    }

    // Pop the headers
    lua_pop(L, 1);

    // HTTP header separator
    FCGX_FPrintF(req->out, "Content-Type: text/html\r\n");
    FCGX_FPrintF(req->out, "\r\n");

    // HTTP response body
    lua_pushlstring(L, "body", 4);
    if (lua_gettable(L, -2) != LUA_TSTRING) { return; }
    const char *body = lua_tostring(L, -1);
    FCGX_FPrintF(req->out, "%s", body);
}

// Create the `http` global in the Lua state
static int create_http_global(lua_State *L, FCGX_Request *req) {
    // Create the `http` global
    lua_createtable(L, 0, 4);

    // Read in the incoming HTTP request
    int read = read_http_request(L, req);
    if (!read) {
        return 0;
    }

    // Create the HTTP response table
    int resp = create_http_response_table(L);
    if (!resp) {
        return 0;
    }

    // Set the global table
    lua_setglobal(L, "http");
    return 1;
}

// Create the `http.response` global table in the Lua state
static int create_http_response_table(lua_State *L) {
    assert(L);

    // Create the `response` table
    lua_pushlstring(L, "response", 8);
    lua_createtable(L, 0, 3);

    // Create the `http.response.headers` table
    lua_pushlstring(L, "headers", 7);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_HEADERS);
    lua_settable(L, -3);

    // Create the `http.response.body` string
    lua_pushlstring(L, "body", 4);
    lua_pushstring(L, "");
    lua_settable(L, -3);

    lua_settable(L, -3);
    return 1;
}

// Insert the HTTP request headers and body into the Lua State.
static int read_http_request(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Add the request object
    lua_pushlstring(L, "request", 7);
    lua_createtable(L, 0, 4);

    // Add the request body
    int req_body = read_http_request_body(L, req);
    if (!req_body) {
        return 0;
    }

    // Add the request headers
    int req_headers = read_http_request_headers(L, req);
    if (!req_headers) {
        return 0;
    }

    // Add any web-server variables
    int srv_vars = read_http_request_vars(L, req);
    if (!srv_vars) {
        return 0;
    }

    // Push the request table
    lua_settable(L, -3);
    return 1;
}

// Read in the variables provided by the webserver to `http.request.vars`
static int read_http_request_vars(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Create the `http.request.vars` table
    lua_pushlstring(L, "vars", 4);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_VARS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = req->envp;
    for (int i = 0; environ[i] != NULL; i++) {
        // Skip any headers with the prefix HTTP_
        if (strncmp(environ[i], FASTCGI_ENVIRON_PREFIX,
                    FASTCGI_ENVIRON_PREFIX_LEN) == 0) {
            continue;
        }

        // Find the equals separator for the variable
        char *sep = strchr(environ[i], '=');
        if (!sep) {
            continue;
        }

        // Find the length of the key
        size_t keylen = (size_t)(sep - environ[i]);

        // Force the name to lowercase
        char *var = convert_env_to_lower(environ[i], keylen);
        if (!var) {
            continue;
        }

        // Push the header key and value into the table
        lua_pushlstring(L, var, keylen);
        lua_pushstring(L, sep+1);
        lua_settable(L, -3);

        // Free the converted header string
        free(var);
    }

    lua_settable(L, -3);
    return 1;
}

// Read in the request headers to the global `http.request.headers`
static int read_http_request_headers(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Create the `http.request.headers` table
    lua_pushlstring(L, "headers", 7);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_HEADERS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = req->envp;
    for (int i = 0; environ[i] != NULL; i++) {
        // Skip any headers without the prefix HTTP_
        if (strncmp(environ[i], FASTCGI_ENVIRON_PREFIX,
                    FASTCGI_ENVIRON_PREFIX_LEN) != 0) {
            continue;
        }

        // Find the equals separator for the variable
        char *sep = strchr(environ[i], '=');
        if (!sep) {
            continue;
        }

        // Find the length of the key
        size_t keylen = (size_t)(sep - environ[i]);
        size_t outlen;

        // Convert the environment variable name to a header name
        char *header = convert_env_to_header(environ[i], keylen, &outlen);
        if (!header) {
            continue;
        }

        // Push the header key and value into the table
        lua_pushlstring(L, header, outlen);
        lua_pushstring(L, sep+1);
        lua_settable(L, -3);

        // Free the converted header string
        free(header);
    }

    // Push the headers table into the `http.request` table
    lua_settable(L, -3);
    return 1;
}

// Read in the request body to the global `http.request.body`
static int read_http_request_body(lua_State *L, FCGX_Request *req) {
    assert(L);

    const char *content_length;
    int body_len;

    // Determine the length of the request body, so we can read it
    content_length = FCGX_GetParam("CONTENT_LENGTH", req->envp);
    sscanf(content_length, "%d", &body_len);

    // Allocate memory for the request body
    char *body = malloc((size_t)body_len + 1);
    if (!body) {
        return 0;
    }

    // Read in the request body
    FCGX_GetStr(body, body_len, req->in);

    // Create the `http.request.body` value
    lua_pushlstring(L, "body", 4);
    lua_pushlstring(L, body, (size_t)body_len);
    lua_settable(L, -3);

    // Free the allocated string and return
    free(body);
    return 1;
}

// Convert the FastCGI environment variable header names to standard
// HTTP header names (e.g. HTTP_CONTENT_LENGTH to Content-Length).
//
// This function assumes that the `in` pointer contains a 5 character
// prefix of 'HTTP_', so the length will be reduced by that many chars.
//
// This function also assumes that HTTP Header keys are ASCII encoded,
// which is currently required by RFC 2616 for HTTP Headers.
static char* convert_env_to_header(const char *in, size_t len, size_t *outlen) {
    *outlen = len - FASTCGI_ENVIRON_PREFIX_LEN;
    char *header = malloc(*outlen + 1);
    if (!header) {
        return NULL;
    }

    bool first = true;
    for (int i = 0; i < (*outlen); i++) {
        char cur = in[i+FASTCGI_ENVIRON_PREFIX_LEN];

        // Convert all underscores to hyphens
        if (cur == '_') {
            cur = '-';
            first = true;
        } else {
            // Convert the first letter of any word to uppercase;
            // the subsequent letters are all lowercase
            if (first) {
                cur = (char) toupper(cur);
                first = false;
            } else {
                cur = (char) tolower(cur);
            }
        }

        header[i] = cur;
    }

    header[*outlen+1] = '\0';
    return header;
}

// Convert the FastCGI environment variable name to lower case.
static char* convert_env_to_lower(const char *in, size_t len) {
    char *header = malloc(len + 1);
    if (!header) {
        return NULL;
    }

    for (int i = 0; i < len; i++) {
        header[i] = (char)tolower(in[i]);
    }

    header[len+1] = '\0';
    return header;
}