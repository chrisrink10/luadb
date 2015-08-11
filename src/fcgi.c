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
#include "util.h"

static const char *const LUADB_CONFIG_FILE = "config.lua";
static const char *const LUADB_FCGI_LOG_FORMAT = "[LuaDB] FCGI Error: %s %s";
static const int FASTCGI_DEFAULT_BACKLOG = 10;
static const int FASTCGI_DEFAULT_NUM_VARS = 20;
static const int FASTCGI_DEFAULT_NUM_HEADERS = 10;  /* Approx num headers/req */
static const char *const FASTCGI_ENVIRON_PREFIX = "HTTP_";
static const size_t FASTCGI_ENVIRON_PREFIX_LEN = 5;

/*
 * FORWARD DECLARATIONS
 */

typedef struct LuaDB_Env_Config {
    char *root;
    char *router;
} LuaDB_Env_Config;

typedef enum LuaDB_FCGX_Result {
    LUADB_FCGX_SUCCESS,
    LUADB_FCGX_ERROR,
    LUADB_FCGX_FATAL,
} LuaDB_FCGX_Result;

static int init_fcgx_request(FCGX_Request *req, const char *path);
static int read_environment_config(LuaDB_Env_Config *config);
static void clean_environment_config(LuaDB_Env_Config *config);
static LuaDB_FCGX_Result process_fcgx_request(FCGX_Request *req, LuaDB_Env_Config *config);
static int route_http_request(lua_State *L);
static void send_http_response(lua_State *L, FCGX_Request *req);
static void send_http_response_status(lua_State *L, FCGX_Request *req);
static void send_http_response_headers(lua_State *L, FCGX_Request *req);
static void send_http_response_body(lua_State *L, FCGX_Request *req);
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
    LuaDB_Env_Config config;

    // Read the default environment configuration
    if (!read_environment_config(&config)) {
        return EXIT_FAILURE;
    }

    // Initialize the FCGI app request
    if (!init_fcgx_request(&req, path)) {
        clean_environment_config(&config);
        return EXIT_FAILURE;
    }

    // Accept FastCGI requests on the given device
    while (FCGX_Accept_r(&req) >= 0) {
        LuaDB_FCGX_Result result = process_fcgx_request(&req, &config);
        if (result == LUADB_FCGX_FATAL) {
            clean_environment_config(&config);
            return EXIT_FAILURE;
        }
    }

    clean_environment_config(&config);
    return EXIT_SUCCESS;
};

/*
 * PRIVATE FUNCTIONS
 */

// Initialize the FCGX request structure.
static int init_fcgx_request(FCGX_Request *req, const char *path) {
    if (FCGX_Init() != 0) {
        return 0;
    }
    int sock = FCGX_OpenSocket(path, FASTCGI_DEFAULT_BACKLOG);
    if (sock == -1) {
        return 0;
    }
    if (FCGX_InitRequest(req, sock, 0) != 0) {
        return 0;
    }

    return 1;
}

// Read the environment configuration file into a struct.
static int read_environment_config(LuaDB_Env_Config *config) {
    assert(config);

    // Create the new filename
    char *cfgfile = luadb_path_join(LUADB_CONFIG_FOLDER, LUADB_CONFIG_FILE);
    if (!cfgfile) {
        return 0;
    }

    // Spawn a quick Lua state to read the file
    lua_State *L = luadb_new_state();
    if (!L) {
        free(cfgfile);
        return 0;
    }

    // Read in our configuration file
    int err = luaL_dofile(L, cfgfile);
    if (err) {
        free(cfgfile);
        lua_close(L);
        return 0;
    }

    // Read in the configuration options
    lua_pushlstring(L, "root", 4);
    lua_gettable(L, -2);
    size_t rootlen;
    const char *root = luaL_tolstring(L, -1, &rootlen);
    config->root = luadb_strndup(root, rootlen);
    lua_pop(L, 2);

    lua_pushlstring(L, "router", 6);
    lua_gettable(L, -2);
    size_t routerlen;
    const char *router = luaL_tolstring(L, -1, &routerlen);
    config->router = luadb_path_njoin(config->root, rootlen, router, routerlen);
    lua_pop(L, 2);

    return 1;
}

// Clean up any strings saved in the environment configuration.
static void clean_environment_config(LuaDB_Env_Config *config) {
    assert(config);

    free(config->root);
    free(config->router);
}

// Process a single FastCGI request:
// 1. Create a new LuaDB state.
// 2. Load in the routing engine specified in user configuration.
// 3. Call the routing engine with the request table.
// 4. Process the response from the routing engine and return it to
//    the web server.
static LuaDB_FCGX_Result process_fcgx_request(FCGX_Request *req, LuaDB_Env_Config *config) {
    // Create a new Lua state
    lua_State *L = luadb_new_state();
    if (!L) {
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Could not create new lua_State object.");
        return LUADB_FCGX_ERROR;
    }

    // Start the routing engine, which will push a function
    // onto the stack accepting one parameter (the HTTP request)
    luadb_add_absolute_path(L, config->root);
    int err = luaL_dofile(L, config->router);
    if (err) {
        const char *errmsg = lua_tostring(L, -1);
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Error occurred intializing routing engine: ", errmsg);
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Read the HTTP request
    if (!read_http_request(L, req)) {
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Error occurred reading HTTP request.", "");
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Route the HTTP request using the routing engine from the
    // previous step
    int success = route_http_request(L);
    if (!success) {
        const char *lua_error = lua_tostring(L, -1);
        FCGX_FPrintF(req->err, LUADB_FCGI_LOG_FORMAT,
                     "Error occurred routing HTTP request: ", lua_error);
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Send the response
    send_http_response(L, req);
    lua_close(L);
    return LUADB_FCGX_SUCCESS;
}

// Route an HTTP request through the defined routing engine.
static int route_http_request(lua_State *L) {
    assert(L);

    // Call the routing engine function
    int success = lua_pcall(L, 1, 1, 0);
    if (success != LUA_OK) {
        return 0;
    }

    return 1;
}

// Read the HTTP response from the Lua State and send it to the web server.
//
// We expect that the HTTP response table value should be pushed on the
// stack from the function call return. It should contain the following
// values:
// - "status" : Lua string or number (e.g. "200 OK" or 200)
// - "headers" : Lua table of HTTP headers
// - "body" : Lua string or table
static void send_http_response(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    send_http_response_status(L, req);
    send_http_response_headers(L, req);
    send_http_response_body(L, req);
}

// Send out the HTTP response status.
static void send_http_response_status(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    // Get the status string
    lua_pushlstring(L, "status", 7);
    size_t statlen;
    int type = lua_gettable(L, -2);
    if (type == LUA_TSTRING || type == LUA_TNUMBER) {
        const char *status = luaL_tolstring(L, -1, &statlen);
        FCGX_FPrintF(req->out, "%s\r\n", status);
    }

    // Pop the status from the stack
    if (type != LUA_TNONE) { lua_pop(L, 1); }
}

// Send out the HTTP response headers.
static void send_http_response_headers(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

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
    FCGX_FPrintF(req->out, "\r\n");
}

// Send out the HTTP response body.
static void send_http_response_body(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    lua_pushlstring(L, "body", 4);
    if (lua_gettable(L, -2) != LUA_TSTRING) { return; }
    const char *body = lua_tostring(L, -1);
    FCGX_FPrintF(req->out, "%s", body);
}

// Read in the HTTP request from the FastCGI server to a Lua
// table value containing server variables ('vars'), request
// headers ('headers'), and the request body ('body').
//
// Leaves one unnamed table on the stack.
static int read_http_request(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Add the request object
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
    return 1;
}

// Read in the FastCGI parameters from the web server.
static int read_http_request_vars(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Create the `request.vars` table
    lua_pushlstring(L, "vars", 4);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_VARS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = NULL;
    environ = req->envp;
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

// Read in the HTTP request headers from the FastCGI server.
static int read_http_request_headers(lua_State *L, FCGX_Request *req) {
    assert(L);

    // Create the `request.headers` table
    lua_pushlstring(L, "headers", 7);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_HEADERS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = NULL;
    environ = req->envp;
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

    // Push the headers table into the `request` table
    lua_settable(L, -3);
    return 1;
}

// Read in the HTTP request body from the FastCGI server.
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

    // Create the `request.body` value
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