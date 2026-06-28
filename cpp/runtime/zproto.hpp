#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace zproto {

// error codes returned negative by pack/unpack on failure.
enum class errc : int {
	oom   = -1,   // output buffer OOM / growth cap exceeded
	error = -3,   // malformed input
};

// raw-backed, auto-growing byte buffer with an optional growth cap.
//
// encode/pack use it freely growing (no cap); the business logic calls set_max()
// on the buffer passed to unpack to cap expansion (lz4 maxDstSize — the
// memory-amplification defense: a malicious packed message cannot force unpack
// past the cap). Raw malloc/realloc backing avoids the std::string resize
// zero-fill. Reusable: the allocation persists across calls (grow-once).
//
// method names (resize/data/clear/size) match the old std::string-backed Buffer
// so generated encode/write_at need no change.
struct Buffer {
	char *buf;
	size_t cap;     // allocated capacity (grows on demand)
	size_t max;     // growth cap; SIZE_MAX = unlimited
	size_t len;     // logical size

	Buffer() : buf(nullptr), cap(0), max(SIZE_MAX), len(0) {}
	explicit Buffer(size_t max_) : buf(nullptr), cap(0), max(max_), len(0) {}
	Buffer(const Buffer &) = delete;
	Buffer &operator=(const Buffer &) = delete;
	~Buffer() { free(buf); }

	char *data() { return buf; }
	const char *cdata() const { return buf; }
	size_t size() const { return len; }
	size_t capacity() const { return cap; }
	void clear() { len = 0; }
	void set_max(size_t m) { max = m; }

	// grow allocation to at least n bytes (within max). Returns false on OOM or
	// if n would exceed max. realloc(buf, ...) with buf==NULL acts as malloc.
	bool ensure(size_t n) {
		if (n > max)
			return false;
		if (n > cap) {
			size_t nc = cap ? cap : 64;
			while (nc < n)
				nc <<= 1;
			if (nc > max)
				nc = max;
			char *nb = (char *)realloc(buf, nc);
			if (!nb)
				return false;
			buf = nb;
			cap = nc;
		}
		return true;
	}
	// ensure + set logical size (encode/pack: pre-size to the known output).
	void resize(size_t n) {
		ensure(n);
		len = n;
	}
	// set logical size after a cursor-write (pack/unpack: trim to actual).
	void set_size(size_t n) { len = n; }
};

// message<T>: serialization traits, specialized per message type (like std::hash<T>).
// The data type T is a clean POD struct; message<T> carries tag/name/byte_size/encode/decode.
// T itself knows nothing of zproto.
template<class T> struct message;

// host endianness: the wire format is little-endian. On LE hosts a native
// load/store (memcpy of N bytes) is a single mov; on BE hosts we fall back to
// the byte shift so the wire bytes stay correct everywhere.
#if (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) || defined(_MSC_VER) || defined(_WIN32)
#define ZPROTO_LE 1
#else
#define ZPROTO_LE 0
#endif

// little-endian write primitives into a raw char* (the generator writes the wire
// format directly via these, into a pre-sized Buffer region).
#if ZPROTO_LE
inline void put_u8(char *p, uint8_t v) {
	memcpy(p, &v, 1);
}
inline void put_u16(char *p, uint16_t v) {
	memcpy(p, &v, 2);
}
inline void put_u32(char *p, uint32_t v) {
	memcpy(p, &v, 4);
}
inline void put_u64(char *p, uint64_t v) {
	memcpy(p, &v, 8);
}
#else
inline void put_u8(char *p, uint8_t v) {
	p[0] = (char)(v & 0xff);
}
inline void put_u16(char *p, uint16_t v) {
	p[0] = (char)(v & 0xff);
	p[1] = (char)((v >> 8) & 0xff);
}
inline void put_u32(char *p, uint32_t v) {
	p[0] = (char)(v & 0xff);
	p[1] = (char)((v >> 8) & 0xff);
	p[2] = (char)((v >> 16) & 0xff);
	p[3] = (char)((v >> 24) & 0xff);
}
inline void put_u64(char *p, uint64_t v) {
	for (int i = 0; i < 8; i++) {
		p[i] = (char)(v & 0xff);
		v >>= 8;
	}
}
#endif

// little-endian read primitives (mirror of put_uXX). Defined inline here so the
// decoder methods below inline into generated code; on BE hosts they byte-merge.
#if ZPROTO_LE
inline uint16_t get_u16(const uint8_t *p) {
	uint16_t v;
	memcpy(&v, p, 2);
	return v;
}
inline uint32_t get_u32(const uint8_t *p) {
	uint32_t v;
	memcpy(&v, p, 4);
	return v;
}
inline uint64_t get_u64(const uint8_t *p) {
	uint64_t v;
	memcpy(&v, p, 8);
	return v;
}
#else
inline uint16_t get_u16(const uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}
inline uint32_t get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline uint64_t get_u64(const uint8_t *p) {
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) {
		v = (v << 8) | p[i];
	}
	return v;
}
#endif

// schema-agnostic wire decoder. Driven by generated code's switch(tag).
// All methods are inline (defined in-class) so the generated decode loop has no
// call overhead -- next()/ctor/r_* are called hundreds of times per message.
// On an unknown tag the generated code stops (forward-compat).
class decoder {
public:
	decoder(const uint8_t *data, size_t n, int basetag)
		: ok(true), data(data), cur(data), end(data + n), tag_ptr(nullptr),
		  tags_left(0), last_tag(basetag - 1) {
		if (n < 4) {
			ok = false;
			return;
		}
		uint32_t datasize = get_u32(cur);
		cur += 4;
		if ((size_t)(end - data) < 4 + datasize) {
			ok = false;
			return;
		}
		end = data + 4 + datasize;        // trust datasize only
		if ((size_t)(end - cur) < 2) {
			ok = false;
			return;
		}
		uint16_t tagcount = get_u16(cur);
		cur += 2;
		size_t tagsz = (size_t)tagcount * 2;
		if ((size_t)(end - cur) < tagsz) {
			ok = false;
			return;
		}
		tag_ptr = cur;
		cur += tagsz;                     // body cursor
		tags_left = tagcount;
	}
	bool ok;                                  // false after any read past end / malformed input
	bool next(int &tag) {                     // read next delta -> absolute tag; false at end/invalid
		if (!ok || tags_left <= 0) {
			return false;
		}
		tags_left--;
		int delta = (int)get_u16(tag_ptr);
		tag_ptr += 2;
		last_tag += delta + 1;
		tag = last_tag;
		return true;
	}
	uint8_t r_u8() {
		if (end - cur < 1) {
			ok = false;
			return 0;
		}
		return *cur++;
	}
	uint16_t r_u16() {
		if (end - cur < 2) {
			ok = false;
			return 0;
		}
		uint16_t v = get_u16(cur);
		cur += 2;
		return v;
	}
	uint32_t r_u32() {
		if (end - cur < 4) {
			ok = false;
			return 0;
		}
		uint32_t v = get_u32(cur);
		cur += 4;
		return v;
	}
	uint64_t r_u64() {
		if (end - cur < 8) {
			ok = false;
			return 0;
		}
		uint64_t v = get_u64(cur);
		cur += 8;
		return v;
	}
	float r_f32() {
		if (end - cur < 4) {
			ok = false;
			return 0;
		}
		uint32_t u = get_u32(cur);
		cur += 4;
		float f;
		memcpy(&f, &u, 4);
		return f;
	}
	void r_bytes(std::string &out) {
		if (end - cur < 4) {
			ok = false;
			return;
		}
		uint32_t len = get_u32(cur);
		cur += 4;
		if ((size_t)(end - cur) < len) {
			ok = false;
			return;
		}
		out.assign((const char *)cur, len);
		cur += len;
	}
	uint32_t r_array() {                      // read [u32 count]
		if (end - cur < 4) {
			ok = false;
			return 0;
		}
		uint32_t c = get_u32(cur);
		cur += 4;
		return c;
	}
	const uint8_t *struct_bytes(size_t &n) {  // read [u32 datasize]; return ptr to sub-message
		if (end - cur < 4) {
			ok = false;
			n = 0;
			return cur;
		}
		const uint8_t *start = cur;        // includes [u32 datasize]
		uint32_t ds = get_u32(cur);
		if ((size_t)(end - cur) < 4 + ds) {
			ok = false;
			n = 0;
			return cur;
		}
		cur += 4 + ds;
		n = 4 + ds;
		return start;
	}
	size_t size() const {                     // bytes spanning the whole decoded message
		return (size_t)(end - data);
	}
private:
	const uint8_t *data;
	const uint8_t *cur;
	const uint8_t *end;
	const uint8_t *tag_ptr;
	int tags_left;
	int last_tag;
};

// schema-independent zero-suppression codec (ported from zproto.c).
// pack output is <= input + small overhead; the optional layer applied after encode.
int pack(const uint8_t *src, int srcsz, Buffer &dst);
int unpack(const uint8_t *src, int srcsz, Buffer &dst);

}  // namespace zproto
