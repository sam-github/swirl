-- Copyright (c) 2008 Sam Roberts
-- This file is placed in the public domain.

local ipairs = ipairs
local find = string.find

module"argp"

function expand(t, usage)
  for i,a in ipairs(t) do
    local s,e,k,v = find(a, "^([^=]+)=?(.*)$")
    t[k] = v
  end
end

