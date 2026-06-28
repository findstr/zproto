#include "zprotowire.hpp"
#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace zproto {

// ---- zero-suppression codec, ported from zproto.c (schema-independent) ----
// Writes into a pre-sized buffer via a raw cursor (resize(upper) once, in-place
// writes, resize(actual) at the end) — no per-byte push_back/append, which was
// the hot-path bottleneck. Output is byte-identical to the push_back version.

static int packseg(const uint8_t *src, int sn, char *d, size_t &cur) {
	int pack_sz = 0;
	size_t hdr_pos = cur;
	cur++;                             // header byte placeholder
	sn = sn < 8 ? sn : 8;
	uint8_t hdr = 0;
	for (int i = 0; i < sn; i++) {
		if (src[i]) {
			hdr |= (uint8_t)(1 << i);
			d[cur++] = (char)src[i];
			++pack_sz;
		}
	}
	d[hdr_pos] = (char)hdr;
	return pack_sz;
}

static int packff(const uint8_t *src, int sn, char *d, size_t &cur) {
	int packsz = 0;
	sn = sn < 8 ? sn : 8;
	for (int i = 0; i < sn; i++) {
		if (src[i])
			++packsz;
	}
	if (packsz >= 6) {                // 6,7,8 non-zero: emit raw
		memcpy(d + cur, src, sn);
		cur += sn;
		packsz = sn;
	}
	return packsz;
}

static void pack_impl(const uint8_t *src, int sn, char *d, size_t &cur) {
	int packsz = -1;
	size_t ffn_pos = 0;               // index of the run-counter byte (valid only in full-run state)
	while (sn > 0) {
		if (packsz != 8) {            // pack a segment
			packsz = packseg(src, sn, d, cur);
			src += 8;
			sn -= 8;
			if (packsz == 8 && sn > 0) {   // full segment: reserve a run-counter byte
				ffn_pos = cur;
				cur++;
			}
		} else {                      // packsz == 8: accumulate a full run
			d[ffn_pos] = (char)0;     // reset counter
			for (;;) {
				packsz = packff(src, sn, d, cur);
				if (packsz >= 6 && packsz <= 8) {
					src += 8;
					sn -= 8;
					d[ffn_pos] = (char)((unsigned char)d[ffn_pos] + 1);
					if ((unsigned char)d[ffn_pos] == 255) {
						packsz = -1;
						break;
					}
				} else {
					break;
				}
			}
		}
	}
}

int pack(const uint8_t *src, int srcsz, std::string &dst) {
	// output upper bound (matches zproto_pack's needn): run-counter bytes
	// (2 per 2048 input) + at most one raw byte per input byte + 1.
	size_t cap = (size_t)(((srcsz + 2047) / 2048) * 2 + srcsz + 1);
	// write into a raw scratch buffer (malloc does not zero-fill), then assign to
	// dst — avoids zero-filling `cap` bytes only to overwrite/trim them.
	char *buf = (char *)malloc(cap);
	size_t cur = 0;
	pack_impl(src, srcsz, buf, cur);
	dst.assign(buf, cur);
	free(buf);
	return (int)cur;
}

static int unpackseg(const uint8_t *src, int sn, uint8_t *dst) {
	const uint8_t *end;
	int unpacksz = 0;
	sn = sn < 9 ? sn : 9;             // header + data
	end = src + sn;
	uint8_t hdr = *src++;
	if (hdr == 0) {
		memset(dst, 0, 8);
	} else {
		int i;
		for (i = 0; i < 8; i++) {
			if ((hdr & 0x01) && src != end) {
				*dst++ = *src++;
				++unpacksz;
			} else {
				*dst++ = 0;
			}
			hdr >>= 1;
		}
	}
	return unpacksz;
}

static int unpackff(const uint8_t *src, int sn, uint8_t *dst) {
	sn = sn < 8 ? sn : 8;
	memcpy(dst, src, sn);
	memset(&dst[sn], 0, 8 - sn);
	return sn;
}

// returns number of output (unpacked) bytes, or <0 on malformed input
static int unpack_impl(const uint8_t *src, int sn, std::string &dst) {
	int unpacksz = -1;
	int ffn = -1;
	uint8_t buf[8];
	while (sn > 0) {
		if (unpacksz != 8) {
			unpacksz = unpackseg(src, sn, buf);
			int consumed = unpacksz + 1;
			if (consumed > sn)
				return ZPROTO_ERR_ERROR;
			src += consumed;
			sn -= consumed;
			dst.append((const char *)buf, 8);
			if (unpacksz == 8 && sn > 0) {
				if (sn < 1)
					return ZPROTO_ERR_ERROR;
				ffn = *src;
				++src;
				--sn;
			}
		} else {                     // unpacksz == 8: full run
			int i;
			int n;
			if ((ffn - 1) * 8 > sn)
				return ZPROTO_ERR_ERROR;
			for (i = 0; i < ffn; i++) {
				n = unpackff(src, sn, buf);
				if (n < 0)
					return n;
				src += n;
				sn -= n;
				dst.append((const char *)buf, 8);
			}
			unpacksz = -1;            // restart
		}
	}
	return (int)dst.size();
}

int unpack(const uint8_t *src, int srcsz, std::string &dst) {
	dst.clear();
	dst.reserve(srcsz);                // reserve packed input size
	return unpack_impl(src, srcsz, dst);
}

// ---- schema-agnostic wire decoder (mirrors zproto.c zproto_decode) ----

// ---- little-endian read primitives ----
static inline uint16_t get_u16(const uint8_t *p) {
	return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint64_t get_u64(const uint8_t *p) {
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) {
		v = (v << 8) | p[i];
	}
	return v;
}

decoder::decoder(const uint8_t *data, size_t n, int basetag)
	: ok(true), data(data), cur(data), end(data + n), tag_ptr(NULL),
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

size_t decoder::size() const {
	return end - data;
}

bool decoder::next(int &tag) {
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

uint8_t decoder::r_u8() {
	if (end - cur < 1) {
		ok = false;
		return 0;
	}
	return *cur++;
}

uint16_t decoder::r_u16() {
	if (end - cur < 2) {
		ok = false;
		return 0;
	}
	uint16_t v = get_u16(cur);
	cur += 2;
	return v;
}

uint32_t decoder::r_u32() {
	if (end - cur < 4) {
		ok = false;
		return 0;
	}
	uint32_t v = get_u32(cur);
	cur += 4;
	return v;
}

uint64_t decoder::r_u64() {
	if (end - cur < 8) {
		ok = false;
		return 0;
	}
	uint64_t v = get_u64(cur);
	cur += 8;
	return v;
}

float decoder::r_f32() {
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

void decoder::r_bytes(std::string &out) {
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

uint32_t decoder::r_array() {
	if (end - cur < 4) {
		ok = false;
		return 0;
	}
	uint32_t c = get_u32(cur);
	cur += 4;
	return c;
}

const uint8_t *decoder::struct_bytes(size_t &n) {
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

}  // namespace zproto
