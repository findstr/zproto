local zproto = require "zproto"
local rand = require "rand"

local proto = zproto:parse ([[
info {
        .name:string 1
        .age:integer 2
        .girl:boolean 3
        .boy:boolean 4
}

packet 0xfe {
        phone {
                .home:integer 1
                .work:integer 2
        }
        .phone:phone 1
        .info:info[] 2
        .address:string 3
        .luck:integer[] 4
}
]])

local packet = {
        phone = {home=123456, work=654321},
        info = {
                        {name = "lucy", age = 18, girl = false, boy = true},
                        {name="lilei", age = 24, girl = true, boy = false},
                },
        address = "China.shanghai",
        luck = {1, 3, 9},
}

local tag = proto:querytag("packet")
print("packet tag:", tag)
local tag = proto:querytag("info")
print("info tag:", tag)

local data = proto:encode(0xfe, packet)
local packed = proto:pack(data)

print("packed:", #packed)
local data2 = proto:unpack(packed)
local unpack = proto:decode("packet", data2);
local function dump_tbl(s, tbl, n)
        for k, v in pairs(tbl) do
                str = ""
                for i = 1, n do
                        str = str .. " "
                end
                str = str .. s .. k .. ":"
                
                if type(v) == "table" then
                        dump_tbl(str, v, n + 1)
                else
                        print(str, v)
                end
        end
end

dump_tbl("", unpack, 0)


print("==============test pack/unpack=============================")
local function round8(str)
        local cat = ""
        local n = #str % 8
        if n == 0 then
                return str
        end
        for i = 1, 8 - n do
                cat = cat .. string.pack("<B", 0)
        end
        local r = str .. cat
        assert(#r % 8 == 0, string.format("%d:%d",n, #r))
        return r
end
local file1 = io.open("origin.dat", "w+")
local file2 = io.open("packed.dat", "w+")
local file3 = io.open("unpacked.dat", "w+")

for i = 1, 1024 * 1024 do
        local origin = rand.rand()
        local packed = proto:pack(origin)
        local unpacked = proto:unpack(packed)
        local cooked = round8(origin)
        if (unpacked ~= cooked) then
                file1:write(origin)
                file2:write(packed)
                file3:write(unpacked)
                print("fail")
                break
        end
end
print("test 1024 * 1024 time, ok")
print("==============test pack/unpack finish======================")

proto = nil

