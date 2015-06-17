# Lua APIs

User application code can use the following libraries to produce web
responses from LuaDB applications.

## `http` module
The `http` module is a global available in the state for instances of LuaDB
serving FastCGI requests. It provides clients with required APIs to read
an HTTP request and respond appropriately.

* `http.request` - HTTP request object
    * `http.request.vars` - Table storing variables passed on from the
      webserver during the FastCGI request.
    * `http.request.query` - Table storing query parameters mapped to their
      values.
    * `http.request.headers` - Table storing HTTP headers mapped to their
      values.
    * `http.request.body` - The entire body of the HTTP request as a string.
* `http.response` - Response object which will be read from the environment
  once the Lua script has finished serving the request.
    * `http.response.headers` - Table storing HTTP headers sent back to the
      client
    * `http.response.body` - Response body as a string
    * `http.response.status` - Response status code as a string (default: 200)
* `http.redirect(loc)` - Issue a 301 Response to redirect the client to
  the URI given as the `loc` parameter.
* `http.abort(stat)` - Issue a default (empty) response of the code given
  as the `stat` parameter

## `json` module
The `json` library provided by LuaDB allows callers to decode JSON from
a string and encode Lua objects as JSON.

* `json.decode(str)` - Decode a JSON value from a string into a valid Lua
  value. Objects are decoded as tables. Arrays are decoded as 1-indexed
  tables. All other values are converted to their natural Lua equivalents.
* `json.encode(val)` - Encode the Lua input value as valid JSON. Note that
  since JSON cannot encode every Lua value, there are certain types of
  values which will produce a Lua error.  
  Table keys may only be strings and numeric values. Tables are only created
  as JSON arrays if they are endowed with the Lua metafield `_json_array`,
  which the `json` module function `makearray` can give. Callers can test
  if a table will be considered as a JSON array by using the function
  `isarray`.  
  Attempting to encode a value which would produce a reference cycle will
  produce a Lua error.
* `json.makearray(table)` - Endow a Lua table with the `_json_array`
  metafield, to force the `json` module to consider it as a JSON array.
* `json.isarray(obj)` - Returns `true` if the input argument is a table and
  that table possesses the metafield `_json_array`. Returns `false` otherwise.

## `db` global
The `db` global object is a shortcut for the default LMDB `Environment`
object. This global has the same API as objects of that type as listed below.

Lua scripts running in the environment are encouraged not to modify the
value of the `db` global. Rather, system administrators should set the
default environment in the LuaDB configuration file.

## `lmdb` module
The `lmdb` module provides access to LMDB databases on the environment. Note
that scripts are suggested against long-running transactions.

* `lmdb.open(env, [opts])` - Open a new LMDB `Env` (single database file).
  The available options are thus:
    * TODO
* `lmdb.version()` - Return the LMDB version that this build of LuaDB was
  built against.
* `lmdb.Env` - LMDB `Env`(ironments) represent a single database file on
  the host file system.
    * `lmdb.Env:begin([readonly])` - Begin a transaction.
    * `lmdb.Env:close()` - Close the environment out. Once this function
      has been called, any additional calls to `Env` methods will produce
      a Lua error.
    * `lmdb.Env:copy(path[, compact])` - Copy the MDB environment. Note
      that this occurs in a read-only transaction, so file-size can grow
      dramatically while this is occurring due to the fact that pages
      cannot be recycled.
    * `lmdb.Env:info()` - Return some internal data about the environment.
    * `lmdb.Env:flags()` - Return flags used to create the environment.
    * `lmdb.Env:max_key_size()` - Return the maximum key size in bytes.
    * `lmdb.Env:max_readers()` - Return the maximum number of readers.
    * `lmdb.Env:path()` - Return the path used to open the environment.
    * `lmdb.Env:readers()` - Return an array of strings describing active
      readers.
    * `lmdb.Env:reader_check()` - Check for stale entries in the reader
      lock table. Return the number of those entries.
    * `lmdb.Env:stat()` - Return a table of statistics about the environment.
    * `lmdb.Env:sync([force])` - Flush data buffers to disk.
* `lmdb.Transaction` - A single LMDB transaction.
    * `lmdb.Transaction:commit()` - Commit any pending changes in the
      transaction.
    * `lmdb.Transaction:cursor([...])` - Provide a new cursor to use on this
      transaction. Callers can specify zero or more keys to use as a range
      prefix for the cursor.
    * `lmdb.Transaction:delete(...)` - Delete the value located at the
      given key.
    * `lmdb.Transaction:get(...)` - Get the value located at the given key.
    * `lmdb.Transaction:put(val, ...)` - Put the given value at the given key.
    * `lmdb.Transaction:order(...)` - Order on keys with the given node or
      nodes as a prefix. Returns the immediate next node value.
    * `lmdb.Transaction:rollback()` - Roll back any changes made in the
      transaction.
* `lmdb.Cursor` - Cursor used to iterate over keys in a `Transaction`. Note
  that if a cursor is called with parameters, these keys will be used as a
  range prefix.
    * `lmdb.Cursor:close()` - Close this cursor.
    * `lmdb.Cursor:delete()` - Delete the current key/value pair.
    * `lmdb.Cursor:first()` - Seek to the first key in the cursor.
    * `lmdb.Cursor:item()` - Return a table with `key` and `value` elements.
    * `lmdb.Cursor:key()` - Return the current key as an array.
    * `lmdb.Cursor:last()` - Seek to the last key in the cursor.
    * `lmdb.Cursor:next()` - Move the cursor forward one element.
    * `lmdb.Cursor:prev()` - Move the cursor forward one element.
    * `lmdb.Cursor:value()` - Return the current Lua value.

## `uuid` module
The `uuid` module provides an easy way to produce Universally Unique
Identifiers in your LuaDB environment.

* `uuid.uuid()` - Generate a UUID string. Returns `nil` if there was an error.