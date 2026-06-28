#!/usr/bin/env lua
-- test_pack.lua - consume pack/unpack golden vectors and verify.
--
-- Reads vectors_pack.txt (format: "# name\n hex_input hex_packed\n")
-- For each vector:
--   1. pack(input) == golden packed
--   2. unpack(golden packed) == input padded to 8 with zeros
--
-- Usage: luabind/test tests/lua/test_pack.lua <vectors_pack.txt>

local zproto = require "zproto"

local vectors_path = select(1, ...)
assert(vectors_path, "usage: test_pack.lua <vectors_pack.txt>")

local function hex_decode(hex)
    local t = {}
    for i = 1, #hex, 2 do
        t[#t + 1] = string.char(tonumber(hex:sub(i, i + 1), 16))
    end
    return table.concat(t)
end

local function hex_encode(s)
    local t = {}
    for i = 1, #s do
        t[#t + 1] = string.format("%02x", s:byte(i))
    end
    return table.concat(t)
end

local function round8(s)
    local n = #s % 8
    if n == 0 then return s end
    return s .. string.rep("\0", 8 - n)
end

-- Read and parse vectors file
local f = assert(io.open(vectors_path, "r"))
local vectors = {}
local current_name = nil
for raw_line in f:lines() do
    local line = raw_line:match("^%s*(.-)%s*$") or raw_line
    if line == "" then
        -- skip
    elseif line:sub(1, 1) == "#" then
        local comment = line:sub(2):match("^%s*(.-)%s*$")
        if comment == "END" then break end
        current_name = comment
    else
        -- data line: hex_input hex_packed
        local input_hex, packed_hex = line:match("^(%S+)%s+(%S+)$")
        if input_hex and packed_hex then
            vectors[#vectors + 1] = {
                name = current_name,
                input = hex_decode(input_hex),
                packed = hex_decode(packed_hex),
            }
        end
    end
end
f:close()

assert(#vectors > 0, "no vectors loaded from " .. vectors_path)

-- Run tests
local pass = 0
local fail = 0

for _, v in ipairs(vectors) do
    local ok = true
    local msg = nil

    -- Test 1: pack(input) == golden packed
    local packed = zproto:pack(v.input)
    if packed ~= v.packed then
        ok = false
        msg = string.format("PACK mismatch [%s]\n  golden: %s\n  lua:    %s",
            v.name, hex_encode(v.packed), hex_encode(packed))
    end

    -- Test 2: unpack(golden packed) == input padded to 8
    if ok then
        local unpacked = zproto:unpack(v.packed)
        if unpacked == nil then
            ok = false
            msg = string.format("UNPACK returned nil [%s]", v.name)
        else
            local expected = round8(v.input)
            if unpacked ~= expected then
                ok = false
                msg = string.format("UNPACK mismatch [%s]\n  golden: %s\n  lua:    %s",
                    v.name, hex_encode(expected), hex_encode(unpacked))
            end
        end
    end

    if ok then
        pass = pass + 1
    else
        fail = fail + 1
        io.stderr:write(msg .. "\n")
    end
end

print(string.format("lua test_pack: %d pass, %d fail", pass, fail))

-- ---- malformed/truncated unpack tests ----
-- Truncated inputs are rejected. The Lua binding returns nil for zproto_unpack
-- errors instead of silently padding missing bytes with zeros.
local malformed_cases = {
    { "truncated-ff-only",       "\xff" },
    { "truncated-01-only",       "\x01" },
    { "truncated-ff-1byte",      "\xff\x42" },
    { "truncated-55-1byte",      "\x55\xaa" },
    { "truncated-80-0byte",      "\x80" },
}

local mpass = 0
local mfail = 0
for _, c in ipairs(malformed_cases) do
    local unpacked = zproto:unpack(c[2])
    if unpacked == nil then
        mpass = mpass + 1
    else
        mfail = mfail + 1
        io.stderr:write(string.format("MALFORMED [%s]: expected nil but got %s\n",
            c[1], hex_encode(unpacked)))
    end
end

print(string.format("lua test_malformed: %d pass, %d fail", mpass, mfail))
if fail > 0 or mfail > 0 then
    os.exit(1)
end
