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

return config