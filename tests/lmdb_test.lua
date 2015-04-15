--[[
luadb :: lmdb_test.lua

Test LMDB environment functions.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

local LuaTest = require("test")
local lt = LuaTest.new("lmdb")

--[[ MODULE PRIVATE VARIABLES ]]--
local testpath = "/Users/christopher/ClionProjects/luadb/bin/Debug/test"
local testdb = nil
local testtx = nil
local testcur = nil
local dbopts = {
  fixedmap = false,     -- Use fixed mmap
  nosubdir = false,     -- Do not use subdirectory
  nosync = false,       -- Do not fsync after commit
  rdonly = false,       -- Read only
  nometasync = false,   -- Do not fsync metapage after commit
  writemap = false,     -- Use writeable mmap
  mapasync = false,     -- Use asynchronous msync with `writemap`
  notls = false,        -- Tie locktable slots to Tx
  nolock = false,       -- Let callers handle locks
  nordahead = false,    -- Don't use readahead
  nomeminit = false,    -- Do not initialize malloc'ed memory
  maxreaders = 150,     -- Maximum simultaneous readers
  mapsize = 499712,     -- Map size (multiple of OS page size)
}
local maxkeysize = 511  -- Max key size (compile-time constant)

--[[ ENVIRONMENT TESTS ]]--

-- Test that we get a new Tx
function test_env_begin_tx()
  local tx = testdb:begin()

  -- Verify that a transaction is created
  lt:assert_not_equal(tx, nil)

  -- Verify we get an entry in the ref table
  local reg = debug.getregistry()
  local uuid = testdb:_uuid()
  lt:assert_contains(reg, uuid)
  local t = reg[uuid]
  lt:assert_not_equal(t, nil)
  lt:assert_equal(type(t), "table")
  lt:assert_contains(t, tx)

  tx:close()
end

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
  -- Create a coroutine which creates readers to populate the reader table
  --[[
  local thread = coroutine.create(function()
    local tdb = lmdb.open(testpath)
    coroutine.yield()
    tdb:close()
  end)
  --]]

  -- Should be no readers at first
  local readers = testdb:readers()
  lt:assert_equal(#readers, 1)

  -- Start the other thread
  --[[
  coroutine.resume(thread)

  -- Should be one reader now
  readers = testdb:readers()
  lt:assert_equal(#readers, 2)

  -- Finish up the coroutine so it cleans itself up
  coroutine.resume(thread)
  --]]
end

-- Test that we get the number of "dead" readers
function test_env_reader_check()
  local dead = testdb:reader_check()
  lt:assert_equal(type(dead), "number")
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

-- Test that we get a UUID
function test_env__uuid()
  local uuid = testdb:_uuid()
  lt:assert_equal(type(uuid), "string")
end

-- Test that the environment reference table is created
function test_env___ref_table()
  local reg = debug.getregistry()
  local uuid = testdb:_uuid()
  lt:assert_contains(reg, uuid)

  local t = reg[uuid]
  lt:assert_not_equal(t, nil)
  lt:assert_equal(type(t), "table")
end

--[[ TRANSACTION TESTS ]]--

-- Test that there is a DBI associated with the Txn
function test_tx__dbi()
  local tx = testdb:begin()
  local dbi = tx:_dbi()
  lt:assert_equal(type(dbi), "number")
  tx:close()
end

-- Test that we can delete values and get them back
function test_tx_delete()
  local tx1 = testdb:begin()
  local keys = { "Key", "Sub1", "Sub2" }
  local vals = { "Val1", "Val2", "Val3" }

  tx1:put(vals[1], keys[1])
  tx1:put(vals[2], keys[1], keys[2])
  tx1:put(vals[3], keys[1], keys[2], keys[3])
  tx1:commit()
  tx1 = nil

  local tx2 = testdb:begin()
  local rets = {
    tx2:delete(keys[1]),
    tx2:delete(keys[1], keys[2]),
    tx2:delete(keys[1], keys[2], keys[3]),
    tx2:delete(keys[2], keys[1], keys[3])
  }

  lt:assert_equal(true, rets[1])
  lt:assert_equal(true, rets[2])
  lt:assert_equal(true, rets[3])
  lt:assert_equal(false, rets[4])

  tx2:close()
  tx2 = nil
end

-- Test that we get values back when they exist
function test_tx_get()
  local tx1 = testdb:begin()
  local keys = { "Key", "Sub1", "Sub2" }
  local vals = { "Val1", "Val2", "Val3", nil }

  tx1:put(vals[1], keys[1])
  tx1:put(vals[2], keys[1], keys[2])
  tx1:put(vals[3], keys[1], keys[2], keys[3])
  tx1:commit()
  tx1 = nil

  local tx2 = testdb:begin()
  local rets = {
    tx2:get(keys[1]),
    tx2:get(keys[1], keys[2]),
    tx2:get(keys[1], keys[2], keys[3]),
    tx2:get(keys[2], keys[3], keys[1])
  }

  lt:assert_equal(vals[1], rets[1])
  lt:assert_equal(vals[2], rets[2])
  lt:assert_equal(vals[3], rets[3])
  lt:assert_equal(vals[4], rets[4])

  tx2:close()
  tx2 = nil
end

-- Test that we can put values into the database correctly
function test_tx_put()
  local tx1 = testdb:begin()
  local keys = { "Key", "Sub1", "Sub2" }
  local vals = { "Val1", "Val2", "Val3" }

  tx1:put(vals[1], keys[1])
  tx1:put(vals[2], keys[1], keys[2])
  tx1:put(vals[3], keys[1], keys[2], keys[3])
  tx1:commit()
  tx1 = nil

  local tx2 = testdb:begin()
  local rets = {
    tx2:get(keys[1]),
    tx2:get(keys[1], keys[2]),
    tx2:get(keys[1], keys[2], keys[3])
  }

  lt:assert_equal(vals[1], rets[1])
  lt:assert_equal(vals[2], rets[2])
  lt:assert_equal(vals[3], rets[3])

  tx2:close()
  tx2 = nil
end

-- Test that values are not entered into the database if rolled back
function test_tx_rollback()
  local tx1 = testdb:begin()
  local keys = { "RBKey", "RBSub1", "RBSub2" }
  local vals = { "RBVal1", "RBVal2", "RBVal3" }

  tx1:put(vals[1], keys[1])
  tx1:put(vals[2], keys[1], keys[2])
  tx1:put(vals[3], keys[1], keys[2], keys[3])
  tx1:rollback()
  tx1 = nil

  local tx2 = testdb:begin()
  local rets = {
    tx2:get(keys[1]),
    tx2:get(keys[1], keys[2]),
    tx2:get(keys[1], keys[2], keys[3])
  }

  lt:assert_equal(nil, rets[1])
  lt:assert_equal(nil, rets[2])
  lt:assert_equal(nil, rets[3])

  tx2:close()
  tx2 = nil
end

--[[ ADD TEST CASES ]]--

-- Add setup and teardown code

lt:add_setup(function()
  testdb = lmdb.open(testpath, dbopts)
  -- testtx = testdb:begin()
end)

lt:add_teardown(function()
  -- pcall(testtx.close, testtx)
  testdb:close()
  testdb = nil
end)

-- Add test runners to the test suite

lt:add_case("environment", function()
  test_env_begin_tx()
  test_env_flags()
  test_env_info()
  test_env_max_key_size()
  test_env_max_readers()
  test_env_path()
  test_env_readers()
  test_env_reader_check()
  test_env_stat()
  test_env_sync()
  test_env__uuid()
  test_env___ref_table()
end)

lt:add_case("txn", function()
  test_tx__dbi()
  test_tx_delete()
  test_tx_get()
  test_tx_put()
  test_tx_rollback()
end)

lt:add_case("cursor", function()

end)

return lt
