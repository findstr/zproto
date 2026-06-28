#!/usr/bin/env lua
-- gen_codec_vectors.lua - generate encode/decode golden vectors using the
-- Lua zproto binding (which calls the C engine internally).
--
-- Output format (stdout), three lines per case:
--   # struct:case
--   # dump: <canonical-dump-string>
--   <hex-encoded-binary>
--   ...
--   # END
--
-- Each case is roundtrip-verified before emission.
--
-- Run: luabind/test tests/lua/gen_codec_vectors.lua

local zproto = require "zproto"

-- Load the compat schema
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

-- ---- dump format helpers ----

local function hex_encode(s)
    local t = {}
    for i = 1, #s do
        t[#t + 1] = string.format("%02x", s:byte(i))
    end
    return table.concat(t)
end

local function needs_escape(s)
    if s == "" then return false end
    -- escape if contains any delimiter or non-printable
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

local function format_float(f)
    -- Pack as LE float, read as LE uint32, format as 8-digit hex (big-endian representation)
    local packed = string.pack("<f", f)
    local bits = string.unpack("<I4", packed)
    return string.format("f32:%08x", bits)
end

local function format_value(v)
    local t = type(v)
    if t == "boolean" then
        return v and "true" or "false"
    elseif t == "number" then
        -- integer or float? Lua 5.4 distinguishes.
        -- For our purposes, all numbers from zproto decode are either
        -- integers (for int types) or floats (for float type).
        -- We can't tell the type from the value alone, but the dump
        -- function knows the schema, so we use format_float for floats
        -- and decimal for integers. The caller handles this.
        return tostring(v)
    elseif t == "string" then
        if needs_escape(v) then
            return "str:" .. hex_encode(v)
        else
            return v
        end
    elseif t == "table" then
        -- Check if array (has [1]) or struct (has named keys)
        if v[1] ~= nil or next(v) == nil then
            -- array
            local parts = {}
            for _, elem in ipairs(v) do
                parts[#parts + 1] = format_value(elem)
            end
            return "[" .. table.concat(parts, ",") .. "]"
        else
            -- struct: keys should be in schema order, but we can't guarantee
            -- that here. The caller should use format_struct with field order.
            -- As fallback, iterate in whatever order Lua gives us.
            local parts = {}
            for k, val in pairs(v) do
                parts[#parts + 1] = k .. "=" .. format_value(val)
            end
            return "{" .. table.concat(parts, ";") .. "}"
        end
    end
    return "?"
end

-- Format a struct's fields in a specific order.
-- field_order is a list of {name, type} pairs.
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
                val_str = "hex:" .. hex_encode(v)
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

-- ---- dump formatters per struct (fields in schema order) ----

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

-- ---- emit helper ----

local function emit_case(struct_name, case_name, data, dump_fn)
    local tag = proto:tag(struct_name)
    assert(tag, "unknown struct: " .. struct_name)

    -- encode
    local encoded = proto:encode(tag, data)
    assert(encoded, "encode failed for " .. struct_name .. ":" .. case_name)

    -- decode roundtrip
    local decoded, err = proto:decode(struct_name, encoded)
    assert(decoded, "decode failed for " .. struct_name .. ":" .. case_name .. " err=" .. tostring(err))

    -- re-encode to verify determinism
    local encoded2 = proto:encode(tag, decoded)
    assert(encoded2 == encoded,
        "re-encode mismatch for " .. struct_name .. ":" .. case_name)

    -- format dump from the ORIGINAL data (not decoded, to avoid any
    -- decode-side normalization issues)
    local dump = dump_fn(data)

    io.write("# " .. struct_name .. ":" .. case_name .. "\n")
    io.write("# dump: " .. dump .. "\n")
    io.write(hex_encode(encoded) .. "\n")
end

-- ---- test cases ----

-- all_scalars: defaults (all zero values, empty strings)
emit_case("all_scalars", "defaults", {
    b = false, i32 = 0, u32 = 0,
    i64 = 0, u64 = 0, f = 0.0, s = "", bl = "",
}, dump_all_scalars)

-- all_scalars: max values
-- Note: u64 max (18446744073709551615) can't be represented as Lua integer,
-- so we use i64 max which is a valid u64 value.
emit_case("all_scalars", "max_values", {
    b = true,
    i32 = 2147483647, u32 = 4294967295,
    i64 = 9223372036854775807, u64 = 9223372036854775807,
    f = 3.14, s = "hello", bl = "\x00\x01\x02\xff",
}, dump_all_scalars)

-- all_scalars: min/negative values
-- Note: -9223372036854775808 is parsed as -(9223372036854775808) in Lua,
-- and 9223372036854775808 overflows i64 → becomes float. Use the split form
-- to keep it as integer.
emit_case("all_scalars", "min_values", {
    b = false,
    i32 = -2147483648, u32 = 0,
    i64 = -9223372036854775807 - 1, u64 = 0,
    f = -3.14, s = "world", bl = "\xff\xfe\xfd",
}, dump_all_scalars)

-- all_scalars: special float values
emit_case("all_scalars", "float_special", {
    b = false, i32 = 0, u32 = 0,
    i64 = 0, u64 = 0, f = 1.0/0.0, s = "", bl = "",
}, dump_all_scalars)

emit_case("all_scalars", "float_neg_zero", {
    b = false, i32 = 0, u32 = 0,
    i64 = 0, u64 = 0, f = -0.0, s = "", bl = "",
}, dump_all_scalars)

emit_case("all_scalars", "float_nan", {
    b = false, i32 = 0, u32 = 0,
    i64 = 0, u64 = 0, f = 0.0/0.0, s = "", bl = "",
}, dump_all_scalars)

-- edge_values: max/min
-- Note: u64 max can't be represented as Lua integer, use i64 max instead
emit_case("edge_values", "extremes", {
    max_i64 = 9223372036854775807,
    min_i64 = -9223372036854775807 - 1,
    max_u64 = 9223372036854775807,
    zero_u64 = 0,
    empty_str = "",
    empty_blob = "",
    max_f32 = 3.4028234663852886e+38,
    min_f32 = -3.4028234663852886e+38,
    neg_f32 = -1.5,
}, dump_edge_values)

-- arrays_test: empty arrays
emit_case("arrays_test", "empty", {
    ints = {},
    strs = {},
}, dump_arrays_test)

-- arrays_test: non-empty
emit_case("arrays_test", "nonempty", {
    ints = {1, -3, 256, -2147483648, 2147483647},
    strs = {"hello", "world"},
}, dump_arrays_test)

-- outer: with inner struct
emit_case("outer", "with_inner", {
    val = {x = 42, y = 3.14},
    name = "test",
}, dump_outer)

-- outer: inner with defaults
emit_case("outer", "inner_defaults", {
    val = {x = 0, y = 0.0},
    name = "",
}, dump_outer)

-- sparse_fields: skip middle field b
emit_case("sparse_fields", "skip_b", {
    a = 1,
    c = 3,
}, dump_sparse_fields)

-- sparse_fields: all three present
emit_case("sparse_fields", "all_present", {
    a = 10,
    b = 20,
    c = 30,
}, dump_sparse_fields)

-- sparse_fields: only middle
emit_case("sparse_fields", "only_b", {
    b = 99,
}, dump_sparse_fields)

io.write("# END\n")
