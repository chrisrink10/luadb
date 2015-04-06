--[[
luadb :: json_test.lua

Test JSON encode and decode tests. Corner cases provided by
[Lua-Users.org](http://lua-users.org/wiki/JsonModules).

Note that sparse arrays as discussed on that Wiki page are *not* a
problem in LuaDB. In LuaDB, callers must specifically request a JSON
array using the metatable field "_json_array", at which point LuaDB's
JSON module will discard the table key entirely.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

local LuaTest = require("test")
local lt = LuaTest.new("json")

--[[ ENCODING TESTS ]]--

-- Test that empty arrays are properly marshaled and unmarshaled
function test_empty_arrays()
	local json_str = '{"items":[],"properties":{}}'
	local decoded = json.decode(json_str)
	local mt = getmetatable(decoded["items"])
	lt:assert_table_equal(decoded, { ["items"] = {}, ["properties"] = {} })
	lt:assert_equal(json.isarray(decoded["items"]), true)
	lt:assert_not_equal(mt, nil)
	--lt:assert_equal(mt["_json_array"], true)

	local encoded = json.encode(decoded)
	lt:assert_equal(json_str, encoded)
end

-- Test that invalid JSON keys throw errors in encoding
function test_invalid_keys()
	local tkey = {1}
	local bad = { [tkey] = 15 , ["Key"] = "Value" }
	local res = pcall(json.encode, bad)
	lt:assert_equal(res, false)

	bad = { [true] = 12 , [10] = 10 }
	res = pcall(json.encode, bad)
	lt:assert_equal(res, false)
end

-- Test that NaN and Inf values do not get crazy encodings
function test_large_nums()
	lt:assert_equal(json.encode(math.huge * 0), "null")
	lt:assert_equal(json.encode(math.huge), "null")
end

-- Test that numeric keys are encoded as strings
function test_numeric_keys()
	local xt = {[1] = 1, a = 2}
	table.sort(xt)
	local x = json.encode(xt)
	lt:assert_equal(x, '{"a":2,"1":1}')

	local yt = {[44] = 3, [67] = 5, a = 9}
	table.sort(yt)
	local y = json.encode(yt)
	lt:assert_equal(y, '{"44":3,"a":9,"67":5}')
end

-- Test that reference cycles do not result in infinite loops
function test_ref_cycles()
  --local a = {}
	--a.a = a
	--local x = json.encode(a)
	--lt:assert_equal(x == nil)
end


--[[ DECODING TESTS ]]--

-- Test that simple decoding cases work
function test_basic_decode()
	lt:assert_equal(json.decode(""), nil)
	lt:assert_equal(json.decode("null"), nil)
	lt:assert_equal(json.decode("10"), 10)
	lt:assert_equal(json.decode("15.54"), 15.54)
	lt:assert_equal(json.decode("10e2"), 1000)
	lt:assert_equal(json.decode("0"), 0)
	lt:assert_equal(json.decode('"string"'), "string")
	lt:assert_equal(json.decode('""'), "")
	lt:assert_table_equal(json.decode("[]"), {})
	lt:assert_table_equal(json.decode("{}"), {})
end

-- Test that simple arrays are decoded correctly
function test_array_decode()
	local t1 = {1, "string", true, 8}
	lt:assert_table_equal(json.decode('[1, "string", true, 8]'), t1)

	local t2 = { { 1, 8, "string" }, 8, "a", { "12", "jam"} }
  lt:assert_table_equal(json.decode('[[1, 8, "string"], 8, "a", ["12", "jam"]]'), t2)

	-- Make sure our automatic table indices are correctly reset each nested table
	local t3 = { 1, { 2, 3 }, { 4, { 5, 6 }} }
  lt:assert_table_equal(json.decode('[1, [2, 3], [4, [5, 6]]]'), t3)

	-- Make sure we get our metatable
end

-- Test that simple objects are decoded correctly
function test_object_decode()
	local t1 = { key1 = 1, ["string"] = true, ["3"] = {1, 2, 8}, twelve = "forty"}
	local d1 = '{"key1":1,"string":true,"3":[1,2,8],"twelve":"forty"}'
	lt:assert_table_equal(json.decode(d1), t1)

	local t2 = {
		id = "32",
		name = "Jon Snow",
		address = { "1 Lord Commanders Tower" , "Castle Black, The North" },
		age = 14,
		has_pet = true,
		siblings = {
			{ name = "Sansa Stark", pet = "Lady", address = { "1 Winterfell Lane", "Winterfell Castle" } },
			{ name = "Arya Stark", pet = "Nymeria", address = nil }
		}
	}
	local d2 = ('{"id":"32","name":"Jon Snow",' ..
							'"address":["1 Lord Commanders Tower","Castle Black, The North"],' ..
							'"age":14,"has_pet":true,"siblings":' ..
							'[{"name":"Sansa Stark","pet":"Lady","address":["1 Winterfell Lane","Winterfell Castle"]},' ..
							'{"name":"Arya Stark","pet":"Nymeria","address":null}]}')
	lt:assert_table_equal(json.decode(d2), t2)
end


--[[ UTILITY FUNCTION TESTS ]]--

-- Test that we can easily detect Lua tables which are JSON arrays
function test_isarray()
	lt:assert_equal(json.isarray({}), false)

	local t = {}
	setmetatable(t, {_json_array = true})
	lt:assert_equal(json.isarray(t), true)
end

-- Test that we can readily make Lua tables into JSON arrays
function test_makearray()
	local t = {}

	local mt1 = getmetatable(t)
	lt:assert_equal(mt1, nil)

	json.makearray(t)
	local mt2 = getmetatable(t)
	lt:assert_not_equal(mt2, nil)
	lt:assert_equal(mt2["_json_array"], true)
	lt:assert_equal(json.isarray(t), true)

	-- Make sure we can't make non-table values into JSON arrays
	local err = pcall(json.makearray, "string")
	lt:assert_equal(err, false)
end

--[[ ADD TEST CASES ]]--

-- Add test runners to the test suite

lt:add_case("encode", function()
	test_empty_arrays()
	test_invalid_keys()
	test_large_nums()
	test_numeric_keys()
	--test_ref_cycles()
end)

lt:add_case("decode", function()
	test_basic_decode()
	test_array_decode()
	test_object_decode()
end)

lt:add_case("isarray", function()
	test_isarray()
end)

lt:add_case("makearray", function()
	test_makearray()
end)

-- Return the test module
return lt
