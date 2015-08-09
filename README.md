# LuaDB

LuaDB is a web service platform written in C. User application code is
written in Lua and access is provided via a convenient HTTP API, routed
through an existing web server (such as nginx or Apache).

## Getting Started
To get started, clone this repository on your local computer:

    git clone github.com/chrisrink10/luadb

You'll need a copy of [CMake 3.1](http://www.cmake.org) to generate the
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

## Configuration
LuaDB environment configuration is specified via configuration files
written in - unsurprisingly - Lua format. For UNIX-like operating systems,
that configuration file will be located at `/etc/luadb/config.lua`. For
Windows, it will be `%installdir%\config.lua`.

The configuration file contains comments explaining the meaning of each
setting. If you make any changes while LuaDB is running, you will need 
to terminate the instance for your changes to take effect.

In addition to LuaDB configuration, you will need to specify appropriate
configuration to pass FastCGI requests on to the LuaDB application. 

## Lua APIs
LuaDB provides a number of specialized Lua APIs for application code. This
is documented in the API file in the `doc` folder in the project root.

## License
MIT License - see LICENSE file provided in the project root for more details
of the licenses of bundled code.

## Resources
 * [Lua](http://www.lua.org) - detailed documentation of C and Lua APIs
 * [Lua-Users.org](http://lua-users.org) - excellent Wiki and mailing lists
 * [Symas LMDB](http://symas.com/mdb/) - key-value store used by LuaDB
 * [CCAN](http://www.ccodearchive.net) - source of JSON module used in LuaDB
 * [FastCGI](http://www.fastcgi.com) - source of FastCGI C module
 * [Linenoise](https://github.com/antirez/linenoise) - line editing library
 * [mingw64](http://mingw-w64.yaxm.org/doku.php) - GCC for Windows
 * [CMake](http://www.cmake.org) - cross-platform make specification tool
