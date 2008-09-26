require"swirl"

require"table2str"

local function q(t)
  return serialize(t)
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

i = swirl.core("I")
l = swirl.core("L")

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

print"no gc"

i = nil
l = nil

print"yes gc"

fi = nil

collectgarbage()

print"done"

