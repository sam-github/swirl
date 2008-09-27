require"swirl"

require"table2str"

local function q(t)
  return serialize(t)
end

function create(il, profile)
  local function notify_lower(c, op) print("cb lower", c, op) end
  local function notify_upper(c, chno, op) print("cb upper", c, chno, op) end

  local core = swirl.core(
    notify_lower,
    notify_upper,
    il,
    nil, -- features
    nil, -- localize
    profile or {}
    )

  return core
end

function pull(c)
  local b = c:pull()

  if b then
    print("["..tostring(c).."] > \n"..b)
  end

  return b
end

function push(c, b)
  print("["..tostring(c).."] < ["..#b.."])")
  c:push(b)
end

function dolower(i, l)
  local function pullpush(from, to)
    local b = pull(from)
    if b then
      push(to, b)
    end
    return b
  end

  while pullpush(i, l) or pullpush(l,i) do
    print"... lower looped"
  end
end

function frame_read(c)
  local f = c:frame_read()
  print("["..tostring(c).."] > "..tostring(f))
  print(f:payload())
  return f
end

function chan0_read(c)
  local f = c:chan0_read()
  print("["..tostring(c).."] > "..tostring(f))
  for i,v in ipairs(f:profiles()) do
    print(" profile="..q(v))
  end
  return f
end

print"session establishment..."

i = create("I")
l = create("L")

print("I="..tostring(i))
print("L="..tostring(l))

--[[
bi = pull(i)
bl = pull(l)

push(l, bi)
push(i, bl)
]]

dolower(i,l)

fi = chan0_read(i)
fl = chan0_read(l)

print(fi:session(), i)

fi:destroy()
fl:destroy()

dolower(i,l)

print"channel start..."

chno = i:chan_start({{uri="http://example.org/beep/echo"}}, "beep.example.com")

print("chno="..chno)

dolower(i,l)

fl = chan0_read(l)

-- test gc order
print"no gc"

i = nil
l = nil

print"yes gc"

fi = nil

collectgarbage()

print"done"

