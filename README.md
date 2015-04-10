# LuaDB

## Getting Started
To get started, clone this repository on your local computer:

    git clone github.com/chrisrink10/luadb
    
You'll need a copy of [CMake](http://www.cmake.org) to generate the 
appropriate makefiles for your system. 

Follow the steps laid out below for your System.

### OS X and Other UNIX-like Systems
Follow these steps to compile LuaDB:

* Open a command prompt (such as `Terminal` for OS X) to your project 
  build location.
* Enter the command `cmake .`.
* Enter the command `make`.

### Windows
Windows will require [mingw64](http://mingw-w64.yaxm.org/doku.php) in 
order to compile for Windows. Note that you may need to add the MinGW `bin`
directory to your environment variable `%path%`. 

Once you have installed MinGW, follow the steps below:

* Generate MinGW Makefiles with CMake GUI.
* Open `cmd` to the project build location specified in CMake.
* Type `mingw32-make`.
* There may be a few warnings, but LuaDB should compile.

## Lua APIs
LuaDB provides a number of specialized Lua APIs for reading and responding
to HTTP and JSON requests and accessing your application's persistent data.

The modules listed in sub-headers below are the global library modules
which are always defined in a LuaDB environment.

### `db` global
The `db` global object is a shortcut for the default LMDB `Environment` 
object. This global has the same API as objects of that type as listed below.

Lua scripts running in the environment are encouraged not to modify the
value of the `db` global. Rather, system administrators should set the
default environment in the LuaDB configuration file.

### `http` module
The `http` module is a global available in the state for instances of LuaDB
serving FastCGI requests. It provides clients with required APIs to read
an HTTP request and respond appropriately.

* `http.request` - HTTP request object
    * `http.request.method` - Stores the HTTP request method (e.g. `POST`, 
      `GET`, etc.)
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

### `json` module
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

### `lmdb` module
The `lmdb` module provides access to LMDB databases on the environment. Note
that scripts are suggested against long-running transactions.

* `lmdb.open(env, [opts])` - Open a new LMDB `Env` (single database file). 
  The available options are thus:
    * TODO
* `lmdb.version()` - Return the LMDB version that this build of LuaDB was
  built against.
* `lmdb.Env` - LMDB `Env`(ironments) represent a single database file on
  the host file system. 
    * `lmdb.Env.begin()`
    * `lmdb.Env.close()` 
    * `lmdb.Env.copy()`
    * `lmdb.Env.info()`
    * `lmdb.Env.flags()`
    * `lmdb.Env.max_key_size()`
    * `lmdb.Env.max_readers()`
    * `lmdb.Env.readers()`
    * `lmdb.Env.stat()`
    * `lmdb.Env.sync([force])` - Flush data buffers to disk. 
* `lmdb.Transaction` - A single LMDB transaction.
    * `lmdb.Transaction.commit()` - Commit any pending changes in the
      transaction.
    * `lmdb.Transaction.cursor([...])` - Provide a new cursor to use on this
      transaction. Callers can specify zero or more keys to use as a range
      prefix for the cursor.
    * `lmdb.Transaction.delete(...)` - Delete the value located at the 
      given key.
    * `lmdb.Transaction.get(...)` - Get the value located at the given key.
    * `lmdb.Transaction.put(val, ...)` - Put the given value at the given key.
    * `lmdb.Transaction.rollback()` - Roll back any changes made in the 
      transaction.
* `lmdb.Cursor` - Cursor used to iterate over keys in a `Transaction`. Note
  that if a cursor is called with parameters, these keys will be used as a
  range prefix.
    * `lmdb.Cursor.close()` - Close this cursor.
    * `lmdb.Cursor.delete()` - Delete the current key/value pair.
    * `lmdb.Cursor.first()` - Seek to the first key in the cursor.
    * `lmdb.Cursor.item()` - Return a table with `key` and `value` elements.
    * `lmdb.Cursor.key()` - Return the current key as an array.
    * `lmdb.Cursor.last()` - Seek to the last key in the cursor.
    * `lmdb.Cursor.next()` - Move the cursor forward one element.
    * `lmdb.Cursor.prev()` - Move the cursor forward one element.
    * `lmdb.Cursor.value()` - Return the current Lua value.

## License
MIT License - see LICENSE file provided in the project root for more details
of the licenses of bundled code.

## Resources
 * [Lua](http://www.lua.org) - detailed documentation of C and Lua APIs
 * [Lua-Users.org](http://lua-users.org) - excellent Wiki and mailing lists
 * [Symas LMDB](http://symas.com/mdb/) - key-value store used by LuaDB
 * [CCAN](http://www.ccodearchive.net) - source of JSON module used in LuaDB
 * [FastCGI](http://www.fastcgi.com) - source of FastCGI C module
 * [mingw64](http://mingw-w64.yaxm.org/doku.php) - GCC for Windows
 * [CMake](http://www.cmake.org) - cross-platform make specification tool