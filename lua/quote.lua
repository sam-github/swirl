-- Copyright (c) 2008 Sam Roberts
-- This file is placed in the public domain.

local ipairs = ipairs
local pairs = pairs
local string = string
local table = table
local type = type

--[[-
** quote - utility functions for quoting strings and tables
]]
module("quote", package.seeall)

--[[-
-- str = quote.q(str)
Quote a string into lua form (including the non-printable characters from
0-31, and from 127-255, unlike the %q format specifier).
]]
function q(_)
  local function fmt(x)
    --print("x=", x)
    return string.format("\\%03d",string.byte(x))
  end
  _ = string.gsub(_, "\\", "\\\\")
  _ = string.gsub(_, "\n", "\\n")
  _ = string.gsub(_, "\r", "\\r")
  _ = string.gsub(_, "\t", "\\t")
  _ = string.gsub(_, "[%z\1-\31,\127-\255]", fmt)

  return '"'.._..'"'
end

--[[-
-- str = quote.quote(t)

Quotes a table, recursively, into a single string.
]]
local function _quote(_, tseen)
  local t = type(_)
  if t == "number" then
    return tostring(_)
  elseif t == "string" then
    return q(_)
  elseif type(_) == "userdata" then
    return "<"..tostring(_)..">"
  elseif type(_) == "function" then
    return "<"..tostring(_)..">"
  elseif type(_) == "table" then
    if tseen[_] then
      return '<'..tostring(_)..'>'
    end
    tseen[_] = true
    local seen = {}
    local out = {}
    for k,v in ipairs(_) do
      table.insert(seen, k)
      table.insert(out, _quote(v, tseen))
    end
    for k,v in pairs(_) do
      local function fmt(k)
	if type(k) == "string" and string.find(k, "^[_%a][_%w]*$") then
	  return k
	else
	  return "[".._quote(k, tseen).."]"
	end
      end
      if not seen[k] then
	table.insert(out, fmt(k).."=".._quote(v, tseen))
      end
    end
    return "{"..table.concat(out, ", ").."}"
  else
    return tostring(_)
  end
end

function quote(_)
  return _quote(_, {})
end

