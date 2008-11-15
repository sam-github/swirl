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
function expand(t)
  for i=1,#t do
    local a = t[i]
    local s,e,k,v = find(a, "^([^=]+)=?(.*)$")
    t[k] = v
    t[i] = nil
  end
end

