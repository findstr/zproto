local zproto = require "zproto"
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
        info = {{name = "lucy", age = 18, girl = false, boy = true}, {name="lilei", age = 24, girl = true, boy = false},},
        address = "China.shanghai",
        luck = {1, 3, 9},
}

local tag = proto:querytag("packet")
print("packet tag:", tag)
local tag = proto:querytag("info")
print("info tag:", tag)

local data, sz = proto:encode(0xfe, packet)
local packed = proto:pack(data, sz)

print("packed:", data, sz, #packed)
local data1, sz1 = proto:unpack(packed)

local unpack = proto:decode("packet", data1, sz1);
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
proto = nil


