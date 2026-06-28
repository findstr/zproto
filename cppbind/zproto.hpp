#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// runtime error codes (match zproto.h values; defined locally so this header
// does not depend on zproto.h).
#define ZPROTO_ERR_OOM     (-1)
#define ZPROTO_ERR_NOFIELD (-2)
#define ZPROTO_ERR_ERROR   (-3)

namespace zproto {

// pre-sized byte buffer: encode resizes once then writes by index (no push_back,
// no growth); decode reads raw. The fast path for wire serialization.
struct Buffer {
	std::string s;
	void resize(size_t n) { s.resize(n); }
	void clear() { s.clear(); }
	char *data() { return &s[0]; }
	const char *cdata() const { return s.data(); }
	size_t size() const { return s.size(); }
};

// wire<T>: serialization traits, specialized per message type (like std::hash<T>).
// The data type T is a clean POD struct; wire<T> carries tag/name/byte_size/encode/decode.
// T itself knows nothing of zproto.
template<class T> struct wire;

// little-endian write primitives into a raw char* (the generator writes the wire
// format directly via these, into a pre-sized Buffer region).
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

// schema-agnostic wire decoder. Driven by generated code's switch(tag).
// On an unknown tag the generated code stops (forward-compat).
class decoder {
public:
	decoder(const uint8_t *data, size_t n, int basetag);
	bool ok;                                  // false after any read past end / malformed input
	bool next(int &tag);                      // read next delta -> absolute tag; false at end/invalid
	uint8_t r_u8();
	uint16_t r_u16();
	uint32_t r_u32();
	uint64_t r_u64();
	float r_f32();
	void r_bytes(std::string &out);
	uint32_t r_array();                       // read [u32 count]
	const uint8_t *struct_bytes(size_t &n);   // read [u32 datasize]; return ptr to sub-message
	size_t size() const;                      // bytes spanning the whole decoded message
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
int pack(const uint8_t *src, int srcsz, std::string &dst);
int unpack(const uint8_t *src, int srcsz, std::string &dst);

}  // namespace zproto
