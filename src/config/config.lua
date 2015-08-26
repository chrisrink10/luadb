--[[
LuaDB :: config.lua

Configure environment wide LuaDB settings.
]]--

local config = {}

--[[ LuaDB General Configuration ]]--
-- General settings that you may want to change to modify how
-- LuaDB operates.

-- Web Root
-- This is the folder that LuaDB will use to resolve
-- template values and Lua scripts for endpoints.
-- Default: /var/www/luadb
config.root = "/var/www/luadb"

-- Routing Script   
-- Lua script which dispatches any incoming HTTP requests.
-- Location should be specified relative to the above root.
-- Default: reqhandler
config.router = "reqhandler.lua"

--[[ FastCGI Configuration ]]--
-- Generally speaking, the configuration settings below should
-- not need to be modified to get LuaDB working on your system.
-- They are merely provided as an alternative to hard-coding
-- certain parameters into the binary.

-- FastCGI Query String Parameter
-- The name of the FastCGI parameter sent from the web server
-- which will contain the query string parameter.
-- Default: QUERY_STRING
config.fcgi_query = "QUERY_STRING"

-- FastCGI Header Prefix
-- The prefix applied to FastCGI environment variables which
-- store HTTP Request headers.
-- Default: HTTP_
config.fcgi_header_prefix = "HTTP_"

return config