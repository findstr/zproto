local zproto = require "zproto"
local json = require "json"
local rand = require "rand"

local protostr1 = [[
info 0xfd {
	.name:string 1
	.age:float 6
	.girl:boolean 9
	.boy:boolean 10
	.int8:byte 11
	.int16:short 12
	.blob:byte[] 13
}

packet 0xfe {
	phone {
		.home:long 1
		.work:integer 2
		.negwork:long 3
	}
	dummy {
	}
	.phone:phone 1
	.info:info[name] 2
	.empty:string[] 8
	.address:string 13
	.dummy2:string 18
	.dummy3:string[] 39
	.luck:integer[] 45
	.dummy4:dummy 56
}
]]

local protostr2 = [[
info 0xfd {
	.name:string 1
	.age:float 6
	.girl:boolean 9
	.boy:boolean 10
	.int8:byte 11
	.int16:short 12
	.blob:byte[] 13
	.new:string[] 18
	.uint8:ubyte 19
	.uint16:ushort 20
	.uint32:uinteger 21
	.uint64:ulong 23
}

packet 0xfe {
	phone {
		.home:long 1
		.work:integer 2
		.negwork:long 3
	}
	dummy {
	}
	.phone:phone 1
	.info:info[name] 2
	.empty:string[] 8
	.address:string 13
	.dummy2:string 18
	.dummy3:string[] 39
	.luck:integer[] 45
	.dummy4:dummy 56
	.new:info[name] 57
}
]]

local proto, err1 = zproto:parse (protostr1)
local newproto, err2 = zproto:parse(protostr2)

assert(proto, err1)
assert(newproto, err2)

local packet = {
	phone = {home=0x12345678abcf, work=654321, negwork = -654321},
	info = {
		["lucy"] = {name = "lucy", age = 18.3, girl = false, boy = true, int8 = 127, int16 = 256, blob = "hello",
				uint8 = 255, uint16 = 65535, uint32 = 0xffffffff, uint64 = 0xffffffffffffffff},
		["lilei"] = {name="lilei", age = 24.5, girl = true, boy = false, int8 = -128, int16 = -256, blob = "world"},
	},
	address = "China.shanghai",
	luck = {1, -3, 9},
	empty = {},
	new = {
		["hanmeimei"] = {name = "hanmeimei", age=25, girl = true, boy = false, new = {"hnew1", "hnew2"}},
		["jim"] = {name = "jim", age=27, girl = false , boy = true, new = {"jnew1", "jnew2"}},
	}
}

local function print_hex(dat)
	local tbl = {}
	local str = ""
	for i = 1, #dat do
		tbl[#tbl + 1] = string.format("%x ", dat:byte(i))
	end
	print(table.concat(tbl))
end

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

local function test_eq(a, b)
	if b == nil then
		return
	end
	for k, v in pairs(a) do
		local t = type(v)
		if t == "table" then
			test_eq(v, b[k])
		elseif t == "number" then
			assert(math.abs(v - b[k]) < 0.1, k)
		else
			assert(v == b[k], k)
		end
	end
end

--test proto
local function testproto()
	print("========begin test proto===========")
	local tag = proto:tag("packet")
	assert(tag == 0xfe, tag)
	print("packet tag:", tag)
	local tag = proto:tag("info")
	assert(tag == 0xfd, tag)
	print("info tag:", tag)
	local tag = proto:tag("infox")
	assert(tag == nil)
	print("infox tag:", tag)
	print("========stop test proto===========")
end

--test encode/decode
local function testwire()
	print("=========begin test wire=============")
	print("+++++++source table")
	print("data", 1)
	print(json.encode(packet))
	print("data", 2)
	local data = newproto:encode(0xfe, packet)
	print("data", #data)
	print_hex(data)
	data = newproto:pack(data)
	data = newproto:unpack(data)
	local unpack = newproto:decode("packet", data)
	print("------dest table", unpack)
	print(json.encode(unpack))
	test_eq(unpack, packet)
	test_eq(packet, unpack)
	print("=========stop test wire=============")
end

local function testnonexist()
	print("========begin test nonexist fault===========")
	local ok, err = pcall(newproto.encode, newproto, 0xff, packet)
	print("tesnonexist fault:", ok, err)
	assert(not ok)
	print("========stop test nonexist fault===========")
end

local function testarrayfault()
	packet.empty = false
	print("========begin test array fault===========")
	local ok, err = pcall(newproto.encode, newproto, 0xfe, packet)
	print("testarray fault:", ok, err)
	assert(not ok)
	print("========stop test array fault===========")
	packet.empty = {}
end

local function testcompatible(name, e, d)
	print(string.format("=========begin test compatible %s =============", name))
	print("+++++++source table")
	print(name, "source", json.encode(packet))
	local data = e:encode(0xfe, packet)
	data = e:pack(data)
	data = d:unpack(data)
	local unpack = d:decode("packet", data)
	print(name, "destination", json.encode(unpack))
	print("empty table", unpack.empty, #unpack.empty)
	print(string.format("=========stop test compatible %s =============", name))
end

local testcount = 1024 * 1024

local function testpackunpack()
	print("========begin pack/unpack============")
	--[[
	local file1 = io.open("origin.dat", "w+")
	local file2 = io.open("packed.dat", "w+")
	local file3 = io.open("unpacked.dat", "w+")
	]]--
	local function testmode(n)
		print(string.format("test mode%d start", n))
		for i = 1, testcount do
			local origin = rand.rand(n)
			local packed = proto:pack(origin)
			local unpacked = proto:unpack(packed)
			local cooked = round8(origin)
			if (unpacked ~= cooked) then
				--[[
				file1:write(origin)
				file2:write(packed)
				file3:write(unpacked)
				]]--
				print("fail")
				return false
			end
		end
		print(string.format("test mode%d stop", n))
		return true
	end
	local function testsize(n)
		local tbl = {}
		for i = 1, n do
			tbl[i] = '0';
		end
		local dat = table.concat(tbl)
		local pack = zproto:pack(dat)
		local unpack = zproto:unpack(pack)
		assert(unpack == round8(dat))
		print("pack", n,  "data:", #pack)
		return #pack
	end
	for i = 1, 8191 do
		assert(testsize(i) <= i + (i + 2047) // 2048 * 2 + 1)
	end
	if not testmode(0) then
		return
	end
	if not testmode(1) then
		return
	end

	if not testmode(2) then
		return
	end
	print("========stop pack/unpack============")
end

local function testdecodedefend()
	print("=======start decode defend==========")
	for i = 1, testcount do
		local dat = rand.rand(0)
		proto:decode("packet", dat)
	end
	print("=======stop decode defend===========")
end

local function testunpackdefend()
	print("======start unpack defend===========")
	for i = 1, testcount do
		local dat = rand.rand(0)
		proto:unpack(dat)
	end
	print("======start unpack defend===========")
end

--test pack/unpack
--test empty table
--test decode defend
--test unpack defend

testproto()
testwire()
testnonexist()
testarrayfault()
testcompatible("old", proto, newproto)
testcompatible("new", newproto, proto)
testpackunpack()
testdecodedefend()
testunpackdefend()
proto = nil

