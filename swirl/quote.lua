local ipairs = ipairs
local pairs = pairs
local string = string
local table = table
local type = type

module("quote", package.seeall)

-- Quote a string into lua form (including the non-printable characters from
-- 0-31, and from 127-255, unlike the %q format specifier).
function q(_)
  local fmt = string.format
  _ = string.gsub(_, "\\", "\\\\")
  _ = string.gsub(_, "\n", "\\n")
  _ = string.gsub(_, "\r", "\\r")
  _ = string.gsub(_, "\t", "\\t")
  _ = string.gsub(_, "[%z\1-\31,\127-\255]", function (x)
    --print("x=", x)
    return fmt("\\%03d",string.byte(x))
  end)

  return '"'.._..'"'
end

function quote(_, tseen)
  -- TODO deal with recursion
  tseen = tseen or {}
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
    local seen = {}
    local out = {}
    for k,v in ipairs(_) do
      table.insert(seen, k)
      table.insert(out, quote(v))
    end
    for k,v in pairs(_) do
      local function fmt(k)
	if type(k) == "string" and string.find(k, "^[_%a][_%w]*$") then
	  return k
	else
	  return "["..quote(k).."]"
	end
      end
      if not seen[k] then
	table.insert(out, fmt(k).."="..quote(v))
      end
    end
    return "{"..table.concat(out, ", ").."}"
  else
    return tostring(_)
  end
end

