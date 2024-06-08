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

local function testtravel()
	local n = 0
	for i, name, st in proto:travel("struct") do
		n = n + 1
		print(i, name, st)
	end
	assert(n == 2)
	n = 0
	for i, name, st in proto:travel("struct", "info") do
		n = n + 1
	end
	assert(n == 0)
	n = 0
	for i, name, st in proto:travel("struct", "packet") do
		n = n + 1
		print(i, name, st)
	end
	assert(n == 2)

	for i, field in proto:travel("field", "packet") do
		print(i, json.encode(field))
	end

end

--test default
local function testdefault()
	print("====begin test default============")
	local p, err = zproto:parse [[
		foo {
			bar {
				barc {
					.i1:byte	1
					.i2:short	2
					.i3:integer	3
					.i4:long	4
					.u1:byte	5
					.u2:short	6
					.u3:integer	7
					.u4:long	8
					.f1:float	9
					.f2:float	10
					.f3:float	11
					.f4:float	12
					.i1a:byte[]	13
					.i2a:short[]	14
					.i3a:integer[]	15
					.i4a:long[]	16
					.u1a:byte[]	17
					.u2a:short[]	18
					.u3a:integer[]	19
					.u4a:long[]	20
					.f1a:float[]	21
					.f2a:float[]	24
					.f3a:float[]	28
					.f4a:float[]	29
				}
				.f1:barc 1
				.f2:barc 2
			}
			.i1:byte	1
			.i2:short	2
			.i3:integer	3
			.i4:long	4
			.u1:byte	5
			.u2:short	6
			.u3:integer	7
			.u4:long	8
			.f1:float	9
			.f2:float	10
			.f3:float	11
			.f4:float	13
			.i1a:byte[]	14
			.i2a:short[]	17
			.i3a:integer[]	19
			.i4a:long[]	20
			.u1a:byte[]	21
			.u2a:short[]	22
			.u3a:integer[]	23
			.u4a:long[]	24
			.f1a:float[]	25
			.f2a:float[]	26
			.f3a:float[]	27
			.f4a:float[]	29
			.bar:bar	36
		}
		barx {
			.i1:byte	1
			.i2:short	2
			.i3:integer	3
			.i4:long	4
			.u1:byte	5
			.u2:short	6
			.u3:integer	7
			.u4:long	8
			.f1:float	9
			.f2:float	10
			.f3:float	11
			.f4:float	12
			.i1a:byte[]	13
			.i2a:short[]	14
			.i3a:integer[]	15
			.i4a:long[]	16
			.u1a:byte[]	17
			.u2a:short[]	18
			.u3a:integer[]	19
			.u4a:long[]	20
			.f1a:float[]	21
			.f2a:float[]	22
			.f3a:float[]	23
			.f4a:float[]	24
			.x1:byte	25
			.x2:byte	26
			.x3:byte	27
			.x4:byte	28
			.x5:byte	29
			.x6:byte	30
			.x7:byte	31
			.x8:byte	32
			.x9:byte	33
			.x10:byte	34
			.x11:byte	35
			.x12:byte	36
			.x13:byte	37
			.x14:byte	38
			.x15:byte	39
			.x16:byte	40
			.x17:byte	41
			.x18:byte	42
			.x19:byte	43
			.x20:byte	44
			.x22:byte	45
			.x23:byte	46
			.x24:byte	47
			.x25:byte	48
			.x26:byte	49
			.x27:byte	50
			.x28:byte	51
			.x29:byte	52
			.x30:byte	53
			.x31:byte	54
			.x32:byte	55
			.x33:byte	56
			.x34:byte	57
			.x35:byte	58
			.x36:byte	59
			.x37:byte	60
			.x38:byte	61
			.x39:byte	62
			.x40:byte	63
			.x41:byte	64
			.x42:byte	65
		}
	]]
	print(p, err)
	local obj = p:default("foo")
	local check_default = function(obj)
		for i = 1, 4 do
			assert(obj['i'..i] == 0)
			assert(#obj['i'..i..'a'] == 0)
			assert(obj['u'..i] == 0)
			assert(#obj['u'..i..'a'] == 0)
			assert(math.abs(obj['f'..i] - 0.0) < 0.0001)
			assert(#obj['f'..i .. 'a'] == 0)
			if i == 1 then
				assert(type(obj['u' .. i .. 'a']) == "string")
				assert(type(obj['i' .. i .. 'a']) == "string")
			else
				assert(type(obj['u' .. i .. 'a']) == "table")
				assert(type(obj['i' .. i .. 'a']) == "table")
			end
		end
	end
	print(json.encode(obj))
	check_default(obj)
	check_default(obj.bar.f1)
	check_default(obj.bar.f2)
	local x = p:default("barx")
	print(x)
	assert(x == nil)
	print("====finish test default============")
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
		local format = string.format
		print(format("test mode%d start", n))
		io.stdout:write("\27[?25l")
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
			io.stdout:write(format("%d/%d\r", i, testcount))
		end
		io.stdout:write("\27[?25h")
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
testtravel()
testdefault()
testwire()
testnonexist()
testarrayfault()
testcompatible("old", proto, newproto)
testcompatible("new", newproto, proto)
testpackunpack()
testdecodedefend()
testunpackdefend()
proto = nil
