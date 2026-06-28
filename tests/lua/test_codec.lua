#!/usr/bin/env lua
-- test_codec.lua - consume encode/decode golden vectors and verify.
--
-- Reads vectors_codec.txt (format: "# struct:case\n# dump: ...\nhex\n")
-- For each vector:
--   1. encode: parse dump -> construct table -> encode -> compare with golden hex
--   2. decode: golden hex -> decode -> format dump -> compare with golden dump
--
-- Usage: luabind/test tests/lua/test_codec.lua <vectors_codec.txt>

local zproto = require "zproto"

local vectors_path = select(1, ...)
assert(vectors_path, "usage: test_codec.lua <vectors_codec.txt>")

-- Load the same schema used by the generator
local protostr = [[
all_scalars {
    .b:boolean 1
    .i32:int32 2
    .u32:uint32 3
    .i64:int64 4
    .u64:uint64 5
    .f:float 6
    .s:string 7
    .bl:blob 8
}
edge_values {
    .max_i64:int64 1
    .min_i64:int64 2
    .max_u64:uint64 3
    .zero_u64:uint64 4
    .empty_str:string 5
    .empty_blob:blob 6
    .max_f32:float 7
    .min_f32:float 8
    .neg_f32:float 9
}
arrays_test {
    .ints:int32[] 1
    .strs:string[] 2
}
inner {
    .x:int32 1
    .y:float 2
}
outer {
    .val:inner 1
    .name:string 2
}
sparse_fields {
    .a:int32 1
    .b:int32 3
    .c:int32 5
}
]]

local proto, err = zproto:parse(protostr)
assert(proto, err)

-- ---- hex helpers ----

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

-- ---- dump formatting (must match gen_codec_vectors.lua exactly) ----

local function format_float(f)
    local packed = string.pack("<f", f)
    local bits = string.unpack("<I4", packed)
    return string.format("f32:%08x", bits)
end

local function needs_escape(s)
    if s == "" then return false end
    for i = 1, #s do
        local b = s:byte(i)
        if b < 32 or b > 126 then return true end
        local c = string.char(b)
        if c == ";" or c == "=" or c == "[" or c == "]" or c == "{" or c == "}" or c == "," then
            return true
        end
    end
    return false
end

local function hex_string(s)
    local t = {}
    for i = 1, #s do
        t[#t + 1] = string.format("%02x", s:byte(i))
    end
    return table.concat(t)
end

local function format_value(v)
    local t = type(v)
    if t == "boolean" then
        return v and "true" or "false"
    elseif t == "number" then
        return tostring(v)
    elseif t == "string" then
        if needs_escape(v) then
            return "str:" .. hex_string(v)
        else
            return v
        end
    elseif t == "table" then
        if v[1] ~= nil or next(v) == nil then
            local parts = {}
            for _, elem in ipairs(v) do
                parts[#parts + 1] = format_value(elem)
            end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            local parts = {}
            for k, val in pairs(v) do
                parts[#parts + 1] = k .. "=" .. format_value(val)
            end
            return "{" .. table.concat(parts, ";") .. "}"
        end
    end
    return "?"
end

local function format_struct(tbl, field_order)
    local parts = {}
    for _, fi in ipairs(field_order) do
        local name, ftype = fi[1], fi[2]
        local v = tbl[name]
        if v ~= nil then
            local val_str
            if ftype == "float" then
                val_str = format_float(v)
            elseif ftype == "blob" then
                val_str = "hex:" .. hex_string(v)
            else
                val_str = format_value(v)
            end
            parts[#parts + 1] = name .. "=" .. val_str
        end
    end
    return "{" .. table.concat(parts, ";") .. "}"
end

local function format_array(values, elem_formatter)
    local parts = {}
    for _, v in ipairs(values) do
        parts[#parts + 1] = elem_formatter(v)
    end
    return "[" .. table.concat(parts, ",") .. "]"
end

local function dump_all_scalars(t)
    return format_struct(t, {
        {"b", "boolean"}, {"i32", "int"}, {"u32", "int"},
        {"i64", "int"}, {"u64", "int"}, {"f", "float"}, {"s", "string"}, {"bl", "blob"},
    })
end

local function dump_edge_values(t)
    return format_struct(t, {
        {"max_i64", "int"}, {"min_i64", "int"}, {"max_u64", "int"},
        {"zero_u64", "int"}, {"empty_str", "string"}, {"empty_blob", "blob"},
        {"max_f32", "float"}, {"min_f32", "float"}, {"neg_f32", "float"},
    })
end

local function dump_arrays_test(t)
    local ints_str = format_array(t.ints or {}, function(v) return tostring(v) end)
    local strs_str = format_array(t.strs or {}, function(v) return format_value(v) end)
    return "{ints=" .. ints_str .. ";strs=" .. strs_str .. "}"
end

local function dump_inner(t)
    return format_struct(t, {{"x", "int"}, {"y", "float"}})
end

local function dump_outer(t)
    local val_str = dump_inner(t.val)
    local name_str = format_value(t.name)
    return "{val=" .. val_str .. ";name=" .. name_str .. "}"
end

local function dump_sparse_fields(t)
    return format_struct(t, {{"a", "int"}, {"b", "int"}, {"c", "int"}})
end

-- ---- dump parser ----
-- Parses the dump string back into a Lua table.
-- This is needed for the encode test: parse dump -> construct table -> encode.

local function parse_value(s, pos)
    -- skip whitespace
    pos = (s:find("^%s", pos) and s:find("%S", pos) or pos)
    local c = s:sub(pos, pos)

    if c == "{" then
        -- struct
        pos = pos + 1
        local tbl = {}
        while true do
            pos = s:find("%S", pos) or pos
            if s:sub(pos, pos) == "}" then
                pos = pos + 1
                break
            end
            -- parse key=value
            local eq_pos = s:find("=", pos)
            if not eq_pos then break end
            local key = s:sub(pos, eq_pos - 1):match("^%s*(.-)%s*$")
            local val, new_pos = parse_value(s, eq_pos + 1)
            tbl[key] = val
            pos = new_pos
            -- skip semicolons
            local semi = s:find("^%s*;", pos)
            if semi then pos = semi + 1 end
        end
        return tbl, pos
    elseif c == "[" then
        -- array
        pos = pos + 1
        local arr = {}
        while true do
            pos = s:find("%S", pos) or pos
            if s:sub(pos, pos) == "]" then
                pos = pos + 1
                break
            end
            local val, new_pos = parse_value(s, pos)
            arr[#arr + 1] = val
            pos = new_pos
            local comma = s:find("^%s*,", pos)
            if comma then pos = comma + 1 end
        end
        return arr, pos
    elseif c == "t" and s:sub(pos, pos + 3) == "true" then
        return true, pos + 4
    elseif c == "f" and s:sub(pos, pos + 4) == "false" then
        return false, pos + 5
    elseif s:sub(pos, pos + 3) == "f32:" then
        -- float: f32:XXXXXXXX (8 hex digits)
        local hex = s:sub(pos + 4, pos + 11)
        local bits = tonumber(hex, 16)
        local packed = string.pack("<I4", bits)
        local f = string.unpack("<f", packed)
        return f, pos + 12
    elseif s:sub(pos, pos + 3) == "hex:" then
        -- blob: hex:HEXSTRING
        local hex_start = pos + 4
        local hex_end = s:find("[;%s,}%]]", hex_start) or (#s + 1)
        local hex = s:sub(hex_start, hex_end - 1)
        local t = {}
        for i = 1, #hex, 2 do
            t[#t + 1] = string.char(tonumber(hex:sub(i, i + 1), 16))
        end
        return table.concat(t), hex_end
    elseif s:sub(pos, pos + 3) == "str:" then
        -- escaped string: str:HEXSTRING
        local hex_start = pos + 4
        local hex_end = s:find("[;%s,}%]]", hex_start) or (#s + 1)
        local hex = s:sub(hex_start, hex_end - 1)
        local t = {}
        for i = 1, #hex, 2 do
            t[#t + 1] = string.char(tonumber(hex:sub(i, i + 1), 16))
        end
        return table.concat(t), hex_end
    else
        -- number or raw string
        -- find the end of the value (delimiter: ; , } ])
        local val_end = s:find("[;%s,}%]]", pos) or (#s + 1)
        local val_str = s:sub(pos, val_end - 1)
        -- try to parse as number
        local num = tonumber(val_str)
        if num then
            return num, val_end
        else
            return val_str, val_end
        end
    end
end

local function parse_dump(dump_str)
    local val, _ = parse_value(dump_str, 1)
    return val
end

-- ---- read vectors file ----

local f = assert(io.open(vectors_path, "r"))
local vectors = {}
local state = "name" -- name -> dump -> hex
local current = {}

for line in f:lines() do
    if line:sub(1, 1) == "#" then
        local rest = line:sub(2):match("^%s*(.-)%s*$")
        if rest == "END" then break end
        if rest:find("^[%w_]+:[%w_]") then
            -- "# struct:case" line
            current = {struct_case = rest}
            state = "dump"
        elseif rest:find("^dump:") then
            current.dump = rest:sub(6):match("^%s*(.-)%s*$")
            state = "hex"
        end
    elseif line:match("^%s*$") then
        -- skip empty lines
    elseif state == "hex" then
        current.hex = line:match("^%s*(.-)%s*$")
        vectors[#vectors + 1] = current
        current = {}
        state = "name"
    end
end
f:close()

assert(#vectors > 0, "no vectors loaded from " .. vectors_path)

-- ---- run tests ----

local pass = 0
local fail = 0

for _, v in ipairs(vectors) do
    local struct_name, case_name = v.struct_case:match("^([%w_]+):([%w_]+)$")
    local ok = true
    local msg = nil

    -- Parse dump -> construct table
    local data = parse_dump(v.dump)

    -- Encode test: construct -> encode -> compare with golden hex
    local tag = proto:tag(struct_name)
    local encoded = proto:encode(tag, data)
    if not encoded then
        ok = false
        msg = string.format("ENCODE failed [%s:%s]", struct_name, case_name)
    else
        local encoded_hex = hex_encode(encoded)
        if encoded_hex ~= v.hex then
            ok = false
            msg = string.format("ENCODE mismatch [%s:%s]\n  golden: %s\n  lua:    %s",
                struct_name, case_name, v.hex, encoded_hex)
        end
    end

    -- Decode test: golden hex -> decode -> format dump -> compare
    if ok then
        local binary = hex_decode(v.hex)
        local decoded = proto:decode(struct_name, binary)
        if not decoded then
            ok = false
            msg = string.format("DECODE failed [%s:%s]", struct_name, case_name)
        else
            -- Format the decoded data using the appropriate dump function
            local dump_fn = {
                all_scalars = dump_all_scalars,
                edge_values = dump_edge_values,
                arrays_test = dump_arrays_test,
                outer = dump_outer,
                sparse_fields = dump_sparse_fields,
            }
            local fn = dump_fn[struct_name]
            if fn then
                local decoded_dump = fn(decoded)
                if decoded_dump ~= v.dump then
                    ok = false
                    msg = string.format("DECODE mismatch [%s:%s]\n  golden: %s\n  lua:    %s",
                        struct_name, case_name, v.dump, decoded_dump)
                end
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

print(string.format("lua test_codec: %d pass, %d fail", pass, fail))
if fail > 0 then
    os.exit(1)
end
