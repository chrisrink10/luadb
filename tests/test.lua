--[[
luadb :: test.lua

Lua Unit Test framework.

Author:  Chris Rink <chrisrink10@gmail.com>

License: MIT (see LICENSE document at source tree root)
]]--

--[[ MODULE DEFINITION AND CONSTRUCTOR ]]--

-- Create a class and make it callable
local LuaTest = {}
LuaTest.__index = LuaTest

-- New LuaTest object constructor
function LuaTest.new(name)
  local self = setmetatable({}, LuaTest)

  self.name = name
  self.cases = {}
  self.cur = nil

  return self
end

--[[ PRIVATE FUNCTIONS ]]--

-- Increment the success counter for the current case
function LuaTest:_success()
  self.cases[self.cur]["successes"] = self.cases[self.cur]["successes"] + 1
end

-- Increment the fail counter for the current case
function LuaTest:_fail(info)
  self.cases[self.cur]["fails"] = self.cases[self.cur]["fails"] + 1
  local debuginfo = debug.getinfo(3, "n")
  local func = debuginfo["name"]
  local infostr = string.format("[%s] %s", func, info)
  table.insert(self.cases[self.cur]["fail_info"], infostr)
end

-- Deep compare two tables
local function _table_equal(left, right)
  local ln = (left == nil)
  local rn = (right == nil)

  if (ln or rn) and not (ln and rn) then
    return false
  end

  if #left ~= #right then
    return false
  end

  for lk, lv in pairs(left) do
    local rv = right[lk]

    if rv == nil then
      return false
    end

    if type(lv) ~= type(rv) then
      return false
    end

    if type(lv) == "table" then
      if not _table_equal(lv, rv) then
        return false
      end
    else
      if lv ~= rv then
        return false
      end
    end
  end

  return true
end

--[[ PUBLIC FUNCTIONS ]]--

-- Add a new test case to the suite
function LuaTest:add_case(name, func)
  if type(func) ~= "function" then
    error(string.format("test cases must be functions, not '%s'", type(func)))
  end

  if self.cases[name] ~= nil then
    error(string.format("test case '%s' is already defined", name))
  end

  self.cases[name] = {
    ["runner"] = func,
    ["fails"] = 0,
    ["successes"] = 0,
    ["fail_info"] = {},
  }
end

-- Run all of the test cases defined on this suite
function LuaTest:run()
  if self.cases == nil then
    error("no test cases defined")
  end

  print(string.format("Test results for '%s' test suite", self.name))
  i = 1
  for name, case in pairs(self.cases) do
    self.cur = name
    if case["runner"] == nil then
      error(string.format("no test runner defined for '%s'", name))
    end

    -- Generate test case result string
    local res
    if pcall(case.runner) then
      res = string.format("fails: %d, successes: %d", case.fails, case.successes)
    else
      res = "FATAL ERROR"
    end

    -- Print the case result
    print(string.format("%d. %s (%s)", i, name, res))

    -- Print individual failed cases
    if case.fails > 0 then
      for k, info in pairs(case.fail_info) do
        print(string.format("    %s", info))
      end
    end

    i = i+1 -- Increment our counter
  end
end

-- Turn a table into a string value for debugging purposes
function LuaTest.table_tostring(t)
  -- Printing functions for various types
  local pfuncs = {
    ["string"] = function(s)
      return string.format("%q", s)
    end,
    ["number"] = function(n)
      if math.floor(n) == n then
        return string.format("%d", n)
      else
        return string.format("%f", n)
      end
    end,
    ["boolean"] = function(b)
      if b then
        return "true"
      else
        return "false"
      end
    end,
    ["table"] = LuaTest.table_tostring,
    ["nil"] = function(n)
      return "nil"
    end,
  }

  -- Dispatch non-table values to other print functions
  local tp = type(t)
  if tp ~= "table" then
    if pfuncs[tp] ~= nil then
      return pfuncs.tp(t)
    else
      return tp
    end
  end

  -- Generate the output string for the table
  local out = "{"
  for k,v in pairs(t) do
    local ktype = type(k)
    local vtype = type(v)
    local kfunc = pfuncs[ktype]
    local vfunc = pfuncs[vtype]

    if kfunc ~= nil and vfunc ~= nil then
      local field = string.format("[%s] = %s,", kfunc(k), vfunc(v))
      out = out .. field
    else
      print(string.format("types { key = %s, value = %s }", ktype, vtype))
    end
  end

  out = out .. "}"
  return out
end


-- Generic assert function for LuaTest
function LuaTest:assert(expr)
  if expr then
    self:_success()
  else
    self:_fail(string.format("~%s", expr))
  end
end

-- Assert that the left and right values are equal
function LuaTest:assert_equal(left, right)
  if left == right then
    self:_success()
  else
    self:_fail(string.format("%s ~= %s", left, right))
  end
end

-- Assert that left and right values are equal
function LuaTest:assert_not_equal(left, right)
  if left ~= right then
    self:_success()
  else
    self:_fail(string.format("%s == %s", left, right))
  end
end

-- Assert that left and right values are raw-equal
function LuaTest:assert_raw_equal(left, right)
  if rawequal(left, right) then
    self:_success()
  else
    self:_fail(string.format("~rawequal(%s, %s)", left, right))
  end
end

-- Assert that two tables are content equal
function LuaTest:assert_table_equal(left, right)
  if _table_equal(left, right) then
    self:_success()
  else
    self:_fail(string.format("%s ~= %s",
                             LuaTest.table_tostring(left),
                             LuaTest.table_tostring(right)))
  end
end

-- Return the tester module
return LuaTest
