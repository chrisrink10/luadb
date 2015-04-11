--[[
luadb :: lmdb_test.lua

Test LMDB environment functions.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

local LuaTest = require("test")
local lt = LuaTest.new("lmdb")

--[[ ENVIRONMENT TESTS ]]--



--[[ ADD TEST CASES ]]--

-- Add test runners to the test suite

lt:add_case("environment", function()

end)

lt:add_case("txn", function()

end)

lt:add_case("cursor", function()

end)

return lt
