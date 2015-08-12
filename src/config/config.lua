--[[
LuaDB :: config.lua

Configure environment wide LuaDB settings.
]]--

local config = {}

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

-- FastCGI Query String Parameter
-- The name of the FastCGI parameter sent from the web server
-- which will contain the query string parameter.
-- Default: QUERY_STRING
config.fcgi_query = "QUERY_STRING"

return config