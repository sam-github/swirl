-- Copyright (c) 2008 Sam Roberts
-- This file is placed in the public domain.

local ipairs = ipairs
local find = string.find

--[[-
** argp - a module for expanding command line argument tables

Simple, but still useful!
]]
module"argp"

--[[-
-- argp.expand(argt)

Find all arguments looking like
  key=value
and assign value to [key] in the argument table, argt.
]]
function expand(t, usage)
  for i,a in ipairs(t) do
    local s,e,k,v = find(a, "^([^=]+)=?(.*)$")
    t[k] = v
  end
end

