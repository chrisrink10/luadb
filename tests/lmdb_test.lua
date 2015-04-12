--[[
luadb :: lmdb_test.lua

Test LMDB environment functions.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

local LuaTest = require("test")
local lt = LuaTest.new("lmdb")

--[[ MODULE PRIVATE VARIABLES ]]--
local testpath = "bin/Debug/test"
local testdb = nil
local testtx = nil
local testcur = nil
local dbopts = {
  fixedmap = false,     -- Use fixed mmap
  nosubdir = false,     -- Do not use subdirectory
  nosync = false,       -- Do not fsync after commit
  rdonly = true,        -- Read only
  nometasync = false,   -- Do not fsync metapage after commit
  writemap = false,     -- Use writeable mmap
  notls = false,        -- Tie locktable slots to Tx
  nolock = true,        -- Let callers handles
  nordahead = false,    -- Don't use readahead
  nomeminit = false,    -- Do not initialize malloc'ed memory
  maxreaders = 150,     -- Maximum simultaneous readers
  mapsize = 499712,     -- Map size (multiple of OS page size)
}
local maxkeysize = 511  -- Max key size (compile-time constant)

--[[ ENVIRONMENT TESTS ]]--

-- Test that we get a table back with environment flags
function test_env_flags()
  local flags = testdb:flags()

  lt:assert_contains(flags, "fixedmap")
  lt:assert_contains(flags, "nosubdir")
  lt:assert_contains(flags, "nosync")
  lt:assert_contains(flags, "rdonly")
  lt:assert_contains(flags, "nometasync")
  lt:assert_contains(flags, "writemap")
  lt:assert_contains(flags, "mapasync")
  lt:assert_contains(flags, "notls")
  lt:assert_contains(flags, "nolock")
  lt:assert_contains(flags, "nordahead")
  lt:assert_contains(flags, "nomeminit")

  for k, v in pairs(flags) do
    lt:assert_equal(dbopts[k], flags[k])
  end
end

-- Test that we get a table back with environment info
function test_env_info()
  local info = testdb:info()

  lt:assert_contains(info, "last_pgno")
  lt:assert_contains(info, "last_txnid")
  lt:assert_contains(info, "mapaddr")
  lt:assert_contains(info, "mapsize")
  lt:assert_contains(info, "maxreaders")
  lt:assert_contains(info, "num_readers")

  lt:assert_equal(dbopts.maxreaders, info.maxreaders)
  lt:assert_equal(dbopts.mapsize, info.mapsize)
end

-- Test that we get the max key size
function test_env_max_key_size()
  local max = testdb:max_key_size()
  lt:assert_equal(type(max), "number")
  lt:assert_equal(maxkeysize, max)
end

-- Test that we get the max number of readers
function test_env_max_readers()
  local max = testdb:max_readers()
  lt:assert_equal(type(max), "number")
  lt:assert_equal(dbopts.maxreaders, max)
end

-- Test that we get the path back
function test_env_path()
  local path = testdb:path()
  lt:assert_equal(path, testpath)
end

-- Test that we get an array of readers
function test_env_readers()
  local readers = testdb:readers()
  -- TODO: do something with this
end

-- Test that we get the number of "dead" readers
function test_env_reader_check()
  local dead = testdb:reader_check()
  -- TODO: do something with this
end

-- Test that we get a table back with environment statistics
function test_env_stat()
  local stat = testdb:stat()

  lt:assert_contains(stat, "branch_pages")
  lt:assert_contains(stat, "depth")
  lt:assert_contains(stat, "entries")
  lt:assert_contains(stat, "leaf_pages")
  lt:assert_contains(stat, "overflow_pages")
  lt:assert_contains(stat, "page_size")
end

-- Test that we get the number of "dead" readers
function test_env_sync()
  -- TODO: do something with this
end


--[[ ADD TEST CASES ]]--

-- Add setup and teardown code

lt:add_setup(function()
  testdb = lmdb.open(testpath, dbopts)
end)

lt:add_teardown(function()
  testdb:close()
  testdb = nil
end)

-- Add test runners to the test suite

lt:add_case("environment", function()
  test_env_flags()
  test_env_info()
  test_env_max_key_size()
  test_env_max_readers()
  test_env_path()
  test_env_readers()
  test_env_reader_check()
  test_env_stat()
  test_env_sync()
end)

lt:add_case("txn", function()

end)

lt:add_case("cursor", function()

end)

return lt
