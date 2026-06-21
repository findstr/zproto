#ifndef _ZPROTO_WIRE_H
#define _ZPROTO_WIRE_H

#include <assert.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <unordered_map>

// runtime error codes (match zproto.h values; defined locally so the
// runtime header does not depend on zproto.h). Kept under distinct names
// because test_codec.cpp includes BOTH zproto.h and this header, so
// reusing the ZPROTO_* names would be a macro-redefinition error.
#define ZPROTO_ERR_OOM     (-1)
#define ZPROTO_ERR_NOFIELD (-2)
#define ZPROTO_ERR_ERROR   (-3)

namespace zprotobuf {

class wire {
public:
	virtual void _reset();
	// direct-to-wire API (driven by encoder/decoder). Default impls are
	// errors; generated structs override them.
	virtual int _encode(std::string &out) const;
	virtual int _decode(const uint8_t *data, size_t sz);
	virtual int _decode(const std::string &dat);
};

class wirep : public wire {
public:
	virtual int _pack(const uint8_t *src, int srcsz, std::string &dat);
	virtual int _unpack(const uint8_t *src, int srcsz, std::string &dat);
	virtual int _tag() const = 0;
	virtual const char *_name() const = 0;
};

// schema-independent zero-suppression codec (ported from zproto.c)
int pack(const uint8_t *src, int srcsz, std::string &dst);
int unpack(const uint8_t *src, int srcsz, std::string &dst);

// Schema-agnostic wire encoder. Driven by generated code that knows the
// field layout at compile time. Output layout (little-endian):
//   [u32 datasize][u16 tagcount][u16 delta ...][body]
// Mirrors zproto.c zproto_encode/encode_field/encode_array byte-for-byte.
class encoder {
public:
	// reserves header: [u32 datasize][u16 tagcount][fieldcount * u16 delta slots]
	encoder(std::string &out, int basetag, int fieldcount);
	void present(int tag);                // record one present field (writes its delta)
	void w_u8(uint8_t v);
	void w_u16(uint16_t v);
	void w_u32(uint32_t v);
	void w_u64(uint64_t v);
	void w_f32(float v);
	void w_bytes(const char *p, size_t n);  // string/blob: [u32 n][bytes]
	void w_array(uint32_t count);           // array/map header: [u32 count]
	int finish();                           // patch tagcount/datasize, compact body; total bytes or <0
private:
	std::string &out;
	size_t tag_count_off;             // index of the [u32 datasize] slot
	size_t tag_off;                   // index of the next delta slot
	size_t body_off;                  // index where body begins
	int last_tag;
	int present_count;
};

// Schema-agnostic wire decoder. Driven by generated code's switch(tag).
// On an unknown tag the generated code stops (forward-compat).
class decoder {
public:
	decoder(const uint8_t *data, size_t n, int basetag);
	bool ok;                            // false after any read past end / malformed input
	bool next(int &tag);                // read next delta -> absolute tag; false at end/invalid
	uint8_t r_u8();
	uint16_t r_u16();
	uint32_t r_u32();
	uint64_t r_u64();
	float r_f32();
	void r_bytes(std::string &out);
	uint32_t r_array();                       // read [u32 count]
	const uint8_t *struct_bytes(size_t &n);   // read [u32 datasize]; return ptr to sub-message
	// bytes spanning the whole decoded message (== SIZEOF_LEN + datasize as
	// read from the header, before any caller-side padding). Matches the
	// zproto_decode return value (consumed message bytes, not buffer size).
	size_t size() const;
private:
	const uint8_t *data;
	const uint8_t *cur;
	const uint8_t *end;
	const uint8_t *tag_ptr;
	int tags_left;
	int last_tag;
};

};


#endif
