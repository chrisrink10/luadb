/*****************************************************************************
 * LuaDB :: fcgi.c
 *
 * Push HTTP request information into the Lua state and start a FastCGI worker.
 *
 * Author:  Chris Rink <chrisrink10@gmail.com>
 *
 * License: MIT (see LICENSE document at source tree root)
 *****************************************************************************/

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "deps/lua/lua.h"
#include "deps/lua/lauxlib.h"
#include "deps/fcgi/fcgiapp.h"

#include "config.h"
#include "state.h"
#include "fcgi.h"
#include "log.h"
#include "util.h"

static const int FASTCGI_DEFAULT_BACKLOG = 10;
static const int FASTCGI_DEFAULT_NUM_VARS = 20;
static const int FASTCGI_DEFAULT_NUM_HEADERS = 10;  /* Approx num headers/req */
static const char FASTCGI_KEY_VALUE_SEP = '=';

/*
 * FORWARD DECLARATIONS
 */

typedef enum LuaDB_FcgxResult {
    LUADB_FCGX_SUCCESS,
    LUADB_FCGX_ERROR,
    LUADB_FCGX_FATAL,
} LuaDB_FcgxResult;

static int InitFcgxRequest(FCGX_Request *req, const char *path);
static LuaDB_FcgxResult ProcessFcgxRequest(FCGX_Request *req, LuaDB_EnvConfig *config);
static bool RouteHttpRequest(lua_State *L);
static void SendHttpResponse(lua_State *L, FCGX_Request *req);
static void SendHttpResponseStatus(lua_State *L, FCGX_Request *req);
static void SendHttpResponseHeaders(lua_State *L, FCGX_Request *req);
static void SendHttpResponseBody(lua_State *L, FCGX_Request *req);
static bool ReadHttpRequest(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config);
static bool ReadHttpRequestHeaders(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config);
static bool ReadHttpRequestVars(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config);
static bool ReadHttpRequestBody(lua_State *L, FCGX_Request *req);
static bool ReadHttpRequestQueryString(lua_State *L, const char *qs, size_t *len);
static inline void PushQueryValueString(lua_State *L, const char *key, size_t keylen, const char *val, size_t vallen);
static char *ConvertEnvVarToHeader(LuaDB_Setting *pfx, const char *in, size_t len, size_t *outlen);
static char *ConvertEnvVarToLower(const char *in, size_t len);

/*
 * PUBLIC FUNCTIONS
 */

int LuaDB_FcgiStartWorker(const char *path) {
    FCGX_Request req;
    LuaDB_EnvConfig config;
    openlog("luadb", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Starting FastCGI worker on %s", path);

    // Read the default environment configuration
    if (!LuaDB_ReadEnvironmentConfig(&config)) {
        syslog(LOG_ERR, "Failed to read environment configuration.");
        return EXIT_FAILURE;
    }

    // Initialize the FCGI app request
    if (!InitFcgxRequest(&req, path)) {
        syslog(LOG_ERR, "Failed to intialize FastCGI request.");
        LuaDB_CleanEnvironmentConfig(&config);
        return EXIT_FAILURE;
    }

    // Accept FastCGI requests on the given device
    while (FCGX_Accept_r(&req) >= 0) {
        LuaDB_FcgxResult result = ProcessFcgxRequest(&req, &config);
        if (result == LUADB_FCGX_FATAL) {
            syslog(LOG_CRIT, "Failed reading the current request. Exiting.");
            LuaDB_CleanEnvironmentConfig(&config);
            return EXIT_FAILURE;
        }
    }

    LuaDB_CleanEnvironmentConfig(&config);
    syslog(LOG_INFO, "Stopping FastCGI worker on %s", path);
    return EXIT_SUCCESS;
};

/*
 * PRIVATE FUNCTIONS
 */

// Initialize the FCGX request structure.
static int InitFcgxRequest(FCGX_Request *req, const char *path) {
    if (FCGX_Init() != 0) {
        syslog(LOG_ERR, "Failed to intialize FastCGI library.");
        return 0;
    }
    int sock = FCGX_OpenSocket(path, FASTCGI_DEFAULT_BACKLOG);
    if (sock == -1) {
        syslog(LOG_ERR, "Could not open FastCGI socket '%s'", path);
        return 0;
    }
    if (FCGX_InitRequest(req, sock, 0) != 0) {
        syslog(LOG_ERR, "Failed to initialize FastCGI request object.");
        return 0;
    }

    return 1;
}

// Process a single FastCGI request:
// 1. Create a new LuaDB state.
// 2. Load in the routing engine specified in user configuration.
// 3. Call the routing engine with the request table.
// 4. Process the response from the routing engine and return it to
//    the web server.
static LuaDB_FcgxResult ProcessFcgxRequest(FCGX_Request *req, LuaDB_EnvConfig *config) {
    // Create a new Lua state
    lua_State *L = LuaDB_NewState();
    if (!L) {
        syslog(LOG_ERR, "Could not create new lua_State object.");
        return LUADB_FCGX_ERROR;
    }

    // Start the routing engine, which will push a function
    // onto the stack accepting one parameter (the HTTP request)
    LuaDB_PathAddAbsolute(L, config->root.val);
    int err = luaL_dofile(L, config->router.val);
    if (err) {
        const char *errmsg = lua_tostring(L, -1);
        syslog(LOG_ERR, "Error occurred intializing routing engine: %s", errmsg);
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Read the HTTP request
    if (!ReadHttpRequest(L, req, config)) {
        syslog(LOG_ERR, "Error occurred reading HTTP request.");
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Route the HTTP request using the routing engine from the
    // previous step
    int success = RouteHttpRequest(L);
    if (!success) {
        const char *lua_error = lua_tostring(L, -1);
        syslog(LOG_ERR, "Error occurred routing HTTP request: %s", lua_error);
        lua_close(L);
        return LUADB_FCGX_ERROR;
    }

    // Send the response
    SendHttpResponse(L, req);
    lua_close(L);
    return LUADB_FCGX_SUCCESS;
}

// Route an HTTP request through the defined routing engine. The routing
// engine is a function call which is assumed to be sitting on the stack.
// It should accept one parameter (`request`), which includes all of the
// request details, and return a single table consisting of the response
// code, response headers, and response body.
static bool RouteHttpRequest(lua_State *L) {
    assert(L);

    // Call the routing engine function
    int success = lua_pcall(L, 1, 1, 0);
    return (success != LUA_OK) ? false : true;
}

// Read the HTTP response from the Lua State and send it to the web server.
//
// We expect that the HTTP response table value should be pushed on the
// stack from the function call return. It should contain the following
// values:
// - `status` : Lua string or number (e.g. "200 OK" or 200)
// - `headers` : Lua table of HTTP headers
// - `body` : Lua string or table
static void SendHttpResponse(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    SendHttpResponseStatus(L, req);
    SendHttpResponseHeaders(L, req);
    SendHttpResponseBody(L, req);
}

// Send the HTTP response status back to the web server from a Lua table
// assumed to be sitting on the stack.
static void SendHttpResponseStatus(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    // Get the status string
    luaL_checkstack(L, 3, "Could not allocate memory to send response status");
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

// Send the HTTP response headers back to the web server from a Lua table
// assumed to be sitting on the stack.
static void SendHttpResponseHeaders(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    luaL_checkstack(L, 4, "Could not allocate to send response headers");

    // Push headers onto the stack
    lua_pushlstring(L, "headers", 7);
    if (lua_gettable(L, -2) != LUA_TTABLE) { return; }

    // Iterate on the response headers
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        if ((lua_type(L, -2) != LUA_TSTRING) ||
            (lua_type(L, -1) != LUA_TSTRING))
        {
            syslog(LOG_ERR, "HTTP header fields and values must be strings.");
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

// Send the HTTP response body back to the web server from a Lua table
// assumed to be sitting on the stack.
static void SendHttpResponseBody(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    luaL_checkstack(L, 2, "Could not allocate memory to send response body");
    lua_pushlstring(L, "body", 4);
    if (lua_gettable(L, -2) != LUA_TSTRING) { return; }
    const char *body = lua_tostring(L, -1);
    FCGX_FPrintF(req->out, "%s", body);
}

// Read in the HTTP request from the FastCGI server to a Lua table value
// containing the following fields:
// - `vars` : server variables
// - `headers` : any HTTP request headers
// - `body` : the request body as a string
//
// Leaves one unnamed table on the stack to be consumed by the routing
// engine function (as a parameter).
static bool ReadHttpRequest(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config) {
    assert(L);

    // Add the request object
    lua_createtable(L, 0, 4);

    // Add the request body
    bool loaded_body = ReadHttpRequestBody(L, req);
    if (!loaded_body) { return false; }

    // Add the request headers
    bool loaded_headers = ReadHttpRequestHeaders(L, req, config);
    if (!loaded_headers) { return false; }

    // Add any web-server variables
    bool loaded_vars = ReadHttpRequestVars(L, req, config);
    if (!loaded_vars) { return false; }

    // Push the request table
    return true;
}

// Read in the FastCGI parameters from the web server. Create a new Lua
// table and push that into the `request` table which is assumed to be
// sitting on the stack.
//
// If a variable matching the FastCGI query string name (default: QUERY_STRING)
// is found among the variables, it will be parsed into a separate Lua table
// which will be pushed into the `request` table under the `query` key.
static bool ReadHttpRequestVars(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config) {
    assert(L);
    assert(req);
    assert(config);
    const char *qs = NULL;

    luaL_checkstack(L, 4, "Could not allocate memory for request variable table");

    // Create the `request.vars` table
    lua_pushlstring(L, "vars", 4);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_VARS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = NULL;
    environ = req->envp;
    for (int i = 0; environ[i] != NULL; i++) {
        // Skip any headers with the prefix HTTP_
        if (strncmp(environ[i], config->fcgi_header_prefix.val,
                    config->fcgi_header_prefix.len) == 0) {
            continue;
        }

        // Find the equals separator for the variable
        char *sep = strchr(environ[i], FASTCGI_KEY_VALUE_SEP);
        if (!sep) {
            syslog(LOG_WARNING, "No '=' separator found in request variable '%s'",
                   environ[i]);
            continue;
        }

        // Cache a reference to the query string
        if (!qs && (strncmp(environ[i], config->fcgi_query.val,
                            config->fcgi_query.len) == 0)) {
            qs = sep+1;
        }

        // Find the length of the key
        size_t keylen = (size_t)(sep - environ[i]);

        // Force the name to lowercase
        char *var = ConvertEnvVarToLower(environ[i], keylen);
        if (!var) { continue; }

        // Push the header key and value into the table
        lua_pushlstring(L, var, keylen);
        lua_pushstring(L, sep+1);
        lua_settable(L, -3);

        // Free the converted header string
        free(var);
    }

    lua_settable(L, -3);

    // Create a query table if we found a query string
    if (qs) {
        lua_pushlstring(L, "query", 5);
        if (!ReadHttpRequestQueryString(L, qs, NULL)) {
            syslog(LOG_WARNING, "Could not parse query string '%s'", qs);
            lua_pop(L, 1);  // Pop the key if we couldn't parse the query string
        } else {
            lua_settable(L, -3);
        }
    }

    return true;
}

// Read in the HTTP request headers from the FastCGI server. Create a new Lua
// table and push that into the `request` table which is assumed to be
// sitting on the stack.
//
// Header names will be converted from their environment variable style
// names to a more 'correct' HTTP header style using title-casing and
// underscores will be converted to hyphens. This might mangle certain
// header names such as "DNT", which will be rendered as "Dnt".
static bool ReadHttpRequestHeaders(lua_State *L, FCGX_Request *req, LuaDB_EnvConfig *config) {
    assert(L);
    assert(req);

    luaL_checkstack(L, 4, "Could not allocate memory for request headers table");

    // Create the `request.headers` table
    lua_pushlstring(L, "headers", 7);
    lua_createtable(L, 0, FASTCGI_DEFAULT_NUM_HEADERS);

    // Iterate on every environment variable, looking for HTTP_ prefix
    char **environ = NULL;
    environ = req->envp;
    for (int i = 0; environ[i] != NULL; i++) {
        // Skip any headers without the prefix HTTP_
        if (strncmp(environ[i], config->fcgi_header_prefix.val,
                    config->fcgi_header_prefix.len) != 0) {
            continue;
        }

        // Find the equals separator for the variable
        char *sep = strchr(environ[i], FASTCGI_KEY_VALUE_SEP);
        if (!sep) {
            syslog(LOG_WARNING, "No '=' separator found in header '%s'",
                   environ[i]);
            continue;
        }

        // Find the length of the key
        size_t keylen = (size_t)(sep - environ[i]);
        size_t outlen;

        // Convert the environment variable name to a header name
        char *header = ConvertEnvVarToHeader(&config->fcgi_header_prefix,
                                             environ[i], keylen, &outlen);
        if (!header) { continue; }

        // Push the header key and value into the table
        lua_pushlstring(L, header, outlen);
        lua_pushstring(L, sep+1);
        lua_settable(L, -3);

        // Free the converted header string
        free(header);
    }

    // Push the headers table into the `request` table
    lua_settable(L, -3);
    return true;
}

// Read in the HTTP request body from the FastCGI server. Read the string
// value into Lua as a string and push that into the `request` table which
// is assumed to be sitting on the stack.
static bool ReadHttpRequestBody(lua_State *L, FCGX_Request *req) {
    assert(L);
    assert(req);

    const char *content_length;
    int body_len;

    // Determine the length of the request body, so we can read it
    content_length = FCGX_GetParam("CONTENT_LENGTH", req->envp);
    sscanf(content_length, "%d", &body_len);

    // Allocate memory for the request body
    luaL_checkstack(L, 2, "Could not allocate memory for request body");
    char *body = malloc((size_t)body_len + 1);
    if (!body) { return false; }

    // Read in the request body
    FCGX_GetStr(body, body_len, req->in);

    // Create the `request.body` value
    lua_pushlstring(L, "body", 4);
    lua_pushlstring(L, body, (size_t)body_len);
    lua_settable(L, -3);

    // Free the allocated string and return
    free(body);
    return true;
}

// Read the Query String parameter from the FastCGI server and parse it
// into a Lua table of key-value pairs. Push that into the `request` table
// assumed to be sitting on the stack.
//
// This should be able to handle most common query string cases such as
// - Multiple query string values with the same key
// - Keys without a value
// - Percent encoded query string values (assuming UTF-8 encoded)
static bool ReadHttpRequestQueryString(lua_State *L, const char *qs, size_t *len) {
    assert(L);
    if (!qs) { return false; }

    // Verify we can handle all of these operations
    luaL_checkstack(L, 6, "out of memory");

    // Set up a query string iterator
    LuaDB_QueryIter iter;
    LuaDB_QueryIterInit(&iter, qs, len);

    // Create a table from the query string
    lua_newtable(L);

    // Iterate over each k/v pair in the query string
    while (LuaDB_QueryIterNext(&iter)) {
        // Decode the query string values
        size_t keylen;
        char *key = LuaDB_QueryStrDecode(iter.key, iter.keylen, &keylen);
        if (!key) { continue; }
        size_t vallen;
        char *val = LuaDB_QueryStrDecode(iter.val, iter.vallen, &vallen);

        // Push the key onto the stack twice
        lua_pushlstring(L, key, keylen);
        lua_pushvalue(L, -1);   // Duplicate the value, since we'll need it

        int type = lua_gettable(L, -3); // Pops the second value
        if (type == LUA_TTABLE) {       // There were previously values by this name
            // Get the next array index (#table + 1)
            lua_len(L, -1);
            lua_pushinteger(L, 1);
            lua_arith(L, LUA_OPADD);

            // Add the next query string value at that index
            PushQueryValueString(L, key, keylen, val, vallen);
            lua_settable(L, -3);
            lua_pop(L, 1);
        } else if ((type == LUA_TNIL) || (type == LUA_TNONE)) { // Not seen before
            // Add the k/v pair into the table
            if (type == LUA_TNIL) { lua_pop(L, 1); }
            PushQueryValueString(L, key, keylen, val, vallen);
            lua_settable(L, -3);
        } else {    // This value has been seen once before; need to make it an array
            lua_createtable(L, 2, 0);
            lua_rotate(L, -2, 1);       // Swap the table and original value
            lua_pushinteger(L, 1);      // Push the integer 1 (future key)
            lua_rotate(L, -2, 1);       // Swap the integer key and value
            lua_settable(L, -3);        // Set the original key with a new table
            lua_pushinteger(L, 2);      // Start pushing in the second (new) key
            PushQueryValueString(L, key, keylen, val, vallen);
            lua_settable(L, -3);        // Set the new key/value pair into table
            lua_settable(L, -3);        // Finally, set the table into the query table
        }

        free(key);
        free(val);
    }

    return true;
}

// Push an empty string if the query string value is NULL.
//
// Since the equivalent Lua value is Nil, which is also how one checks if
// a key is not in a table, we need to push an empty string so that callers
// can still check for the query string key in the table.
static inline void PushQueryValueString(lua_State *L, const char *key, size_t keylen, const char *val, size_t vallen) {
    assert(L);

    if ((keylen > 0) && (key)) {
        lua_pushlstring(L, val, vallen);
    } else {
        lua_pushstring(L, "");
    }
}

// Convert the FastCGI environment variable header names to standard
// HTTP header names (e.g. HTTP_CONTENT_LENGTH to Content-Length).
//
// This function assumes that the `in` pointer contains a 5 character
// prefix of 'HTTP_', so the length will be reduced by that many chars.
//
// This function also assumes that HTTP Header keys are ASCII encoded,
// which is currently required by RFC 2616 for HTTP Headers.
static char*ConvertEnvVarToHeader(LuaDB_Setting *pfx, const char *in, size_t len, size_t *outlen) {
    *outlen = len - pfx->len;
    char *header = malloc(*outlen + 1);
    if (!header) { return NULL; }

    bool first = true;
    for (int i = 0; i < (*outlen); i++) {
        char cur = in[i+pfx->len];

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

    header[*outlen] = '\0';
    return header;
}

// Convert the FastCGI environment variable name to lower case.
static char*ConvertEnvVarToLower(const char *in, size_t len) {
    char *header = malloc(len + 1);
    if (!header) { return NULL; }

    for (int i = 0; i < len; i++) {
        header[i] = (char)tolower(in[i]);
    }

    header[len] = '\0';
    return header;
}