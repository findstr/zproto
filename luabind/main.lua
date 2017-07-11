local zproto = require "zproto"
local rand = require "rand"

local protostr1 = [[
info 0xfd {
	.name:string 1
	.age:float 6
	.girl:boolean 9
	.boy:boolean 10
}

packet 0xfe {
	phone {
		.home:integer 1
		.work:integer 2
	}
	dummy {
	}
	.phone:phone 1
	.info:info[] 2
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
	.new:string[] 12
}

packet 0xfe {
	phone {
		.home:integer 1
		.work:integer 2
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

local proto = zproto:parse (protostr1)
local newproto = zproto:parse(protostr2)

assert(proto)
assert(newproto)

local packet = {
	phone = {home=0x123456, work=0x654321},
	info = {
			{name = "lucy", age = 18.3, girl = false, boy = true},
			{name="lilei", age = 24.5, girl = true, boy = false},
		},
	address = "China.shanghai",
	luck = {1, 3, 9},
	empty = {},
	new = {
		{name = "hanmeimei", age=25, girl = true, boy = false, new = {"hnew1", "hnew2"}},
		{name = "jim", age=27, girl = false , boy = true, new = {"jnew1", "jnew2"}},
	}
}

local function print_r(tbl, s, n)
	n = n or 0
	s = s or ""
	for k, v in pairs(tbl) do
		str = ""
		for i = 1, n do
			str = str .. " "
		end
		str = str .. s .. k .. ":"

		if type(v) == "table" then
			print_r(v, str, n + 1)
		else
			print(str, v)
		end
	end
end

local function print_hex(dat)
	local str = ""
	for i = 1, #dat do
		str = str .. string.format("%x ", dat:byte(i))
	end
	print(str)
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

--test proto
local function testproto()
	print("========begin test proto===========")
	local tag = proto:querytag("packet")
	print("packet tag:", tag)
	local tag = proto:querytag("info")
	print("info tag:", tag)
	print("========stop test proto===========")
end

--test encode/decode
local function testwire()
	print("=========begin test wire=============")
	print("+++++++source table")
	print("empty table", packet.empty, #packet.empty)
	print_r(packet)
	local data = newproto:encode(0xfe, packet)
	data = newproto:pack(data)
	data = newproto:unpack(data)
	local unpack = newproto:decode("packet", data)
	print("------dest table", unpack)
	print("empty table", unpack.empty, #unpack.empty)
	print_r(unpack)
	print("=========stop test wire=============")
end

local function testarrayfault()
	packet.empty = false
	print("========begin test array fault===========")
	local ok, err = pcall(newproto.encode, newproto, 0xfe, packet)
	print("testarray fault:", ok, err)
	print("========stop test array fault===========")
	packet.empty = {}
end

local function testcompatible(name, e, d)
	print(string.format("=========begin test compatible %s =============", name))
	print("+++++++source table")
	print("empty table", packet.empty, #packet.empty)
	print_r(packet)
	local data = e:encode(0xfe, packet)
	data = e:pack(data)
	data = d:unpack(data)
	local unpack = d:decode("packet", data)
	print("------dest table")
	print("empty table", unpack.empty, #unpack.empty)
	print_r(unpack)
	print(string.format("=========stop test compatible %s =============", name))
end

local testcount = 512 * 512

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
			local origin = rand.rand(0)
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
testarrayfault()
testcompatible("old", proto, newproto)
testcompatible("new", newproto, proto)
testpackunpack()
testdecodedefend()
testunpackdefend()

proto = nil

