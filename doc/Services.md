# Web Services

User application code can quickly create Web Services using Lua scripts with
minimal setup.

## Configuration
Configuring a web service is fairly straightforward. Users can find their
environment configuration file in `/etc/luadb/config.lua` for -nix operating
systems and `%installdir%\config.lua` on Windows.

For most use cases, the default settings are probably appropriate. The
default settings tell LuaDB where it can find the routing engine for your
service. The settings also name the web root for Lua scripts for your
application.

## Request Routing
A simple routing engine (`reqhandler.lua`) is provided by default. Users
looking to quickly set up a router may want to expand on this existing
router. However, more advanced users may want to completely replace the
default routing option. 

The only requirement for a routing engine is that it returns a function with
a single parameter (typically called `request`) which contains the HTTP 
request. The routing engine function should return a table containing the 
response (specifically `status` as a number,  `headers` table, and `body`).

A routing engine can call out to any arbitrary Lua code to generate the
response, so long as it ultimately returns the values described above. The
default LuaDB libraries are also available to code called via the router.

### HTTP Request
The request table passed to the routing engine contains the following fields
that may be examined and manipulated by the routing engine:

* `request` - HTTP request table
    * `request.vars` - Table storing variables passed on from the
      webserver during the FastCGI request.
    * `request.headers` - Table storing HTTP headers mapped to their
      values.
    * `request.body` - The entire body of the HTTP request as a string.