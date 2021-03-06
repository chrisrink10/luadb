--[[
luadb :: main_test.lua

Run all tests.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

-- Load up all of the tests
local test_suite = {
	require("json_test"),
	require("lmdb_test"),
}

-- Introduce yourself!
print("--[[ Unit Tests for LuaDB ]]--\n")

-- Run tests
for k, mod in pairs(test_suite) do
	if mod["run"] ~= nil then
		mod:run()
		print("") -- Print a newline
	end
end
