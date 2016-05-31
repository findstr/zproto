local engine = require "zproto.c"

local zproto = {}

local cachemt = {__mode = "kv"}
local indexmt = {
        __index = zproto,
        __gc = function(table)
                if t.proto then
                        engine.free(t.proto)
                end
        end
}



local function create(self)
        local t = {}
	t.ncache = {}   -- name cache
        t.tcache = {}   -- tag cache
        t.nametag = {}  -- name tag cache
        setmetatable(t, indexmt)
        setmetatable(t.ncache, cachemt)
        setmetatable(t.tcache, cachemt)
        setmetatable(t.nametag, cachemt)

        return t;
end

function zproto:load(path)
        local t = create(self)
        t.proto = engine.load(path)
        return t;
end

function zproto:parse(str)
        local t = create(self)
        t.proto = engine.parse(str)
        return t
end

local function query(self, typ)
        local cache
        if type(typ) == "number" then
                cache = self.tcache
        elseif type(typ) == "string" then
                cache = self.ncache
        end
        local record = cache[typ]
        assert(self.proto)
        if not record then
                record, tag = engine.query(self.proto, typ)
                cache[typ] = record
                self.nametag[typ] = tag
        end
        assert(record)
        return record
end

function zproto:encode(typ, packet)
        local record = query(self, typ)
        assert(record)
        assert(typ)
        assert(packet)
        return engine.encode(self.proto, record, packet)
end

function zproto:querytag(typ)
        local tag = self.nametag[typ]
        if not tag then
                query(self, typ)
                tag = self.nametag[typ]
        end
        assert(tag)
        return tag
end

function zproto:decode(typ, data, sz)
        local record = query(self, typ)
        return engine.decode(self.proto, record, data, sz)
end

function zproto:tostring(data, sz)
        return engine.tostring(self.proto, data, sz)
end

function zproto:pack(data, sz)
        return engine.pack(self.proto, data, sz)
end

function zproto:unpack(data, sz)
        return engine.unpack(self.proto, data, sz);
end

return zproto

