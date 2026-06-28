#include "zproto.hpp"
#include <assert.h>
#include <stdint.h>
#include <string.h>

namespace zproto {

// ---- zero-suppression codec: near-verbatim copy of zproto.c (origin/master,
// cbecf36): packseg/packff/pack/unpackseg/unpack. Static + renamed drivers
// (pack_impl/unpack_impl) to avoid clashing with zproto::pack/zproto::unpack.
// Error codes mapped to zproto::errc. Re-sync from zproto.c by copy, then
// re-apply: (1) make all fns static, (2) rename pack->pack_impl, unpack->
// unpack_impl, (3) ZPROTO_OOM -> (int)errc::oom, ZPROTO_ERROR -> (int)errc::error.
// Do NOT copy zproto_pack/zproto_unpack (thin bound-check shells; the Buffer
// wrappers below own sizing). pack does NOT pad the wire bytes (master base);
// the bound uses srcsz directly (no sz8 round-up).

static int
packseg(const uint8_t *src, int sn, uint8_t *dst)
{
	uint8_t *hdr = dst;
	uint8_t *out = dst + 1;
	int pack_sz;
	sn = sn < 8 ? sn : 8;
	if (sn == 8) {
		uint64_t w;
		memcpy(&w, src, 8);
		if (w == 0) {            // all-zero: zero header, no data
			*hdr = 0;
			pack_sz = 0;
		} else {
			// branchless 8-way scatter: store every byte, advance out only
			// for non-zeros. A store at a zero position is overwritten by
			// the next store, so the emitted bytes are exactly the non-zeros
			// in order. (src[i] != 0) compiles to a flag extract, no branch.
			uint8_t h = 0;
			int nz;
			nz = (src[0] != 0); h |= (uint8_t)(nz << 0); *out = src[0]; out += nz;
			nz = (src[1] != 0); h |= (uint8_t)(nz << 1); *out = src[1]; out += nz;
			nz = (src[2] != 0); h |= (uint8_t)(nz << 2); *out = src[2]; out += nz;
			nz = (src[3] != 0); h |= (uint8_t)(nz << 3); *out = src[3]; out += nz;
			nz = (src[4] != 0); h |= (uint8_t)(nz << 4); *out = src[4]; out += nz;
			nz = (src[5] != 0); h |= (uint8_t)(nz << 5); *out = src[5]; out += nz;
			nz = (src[6] != 0); h |= (uint8_t)(nz << 6); *out = src[6]; out += nz;
			nz = (src[7] != 0); h |= (uint8_t)(nz << 7); *out = src[7]; out += nz;
			*hdr = h;
			pack_sz = (int)(out - (hdr + 1));
		}
	} else {
		// tail: fewer than 8 bytes remain -- conditional-store loop (uncommon)
		uint8_t h = 0;
		int i;
		pack_sz = 0;
		for (i = 0; i < sn; i++) {
			if (src[i]) {
				h |= (uint8_t)(1 << i);
				*out++ = src[i];
				++pack_sz;
			}
		}
		*hdr = h;
	}
	return pack_sz;
}

static int
packff(const uint8_t *src, int sn, uint8_t *dst)
{
	int packsz;
	sn = sn < 8 ? sn : 8;
	if (sn == 8) { // unrolled per-byte count: correct (the SWAR hasZero trick
		              // has borrow-chain false positives that miscount) and faster
		              // (independent checks beat the SWAR dependency chain)
		packsz = (src[0]!=0)+(src[1]!=0)+(src[2]!=0)+(src[3]!=0)
		       + (src[4]!=0)+(src[5]!=0)+(src[6]!=0)+(src[7]!=0);
	} else {
		int i;
		packsz = 0;
		for (i = 0; i < sn; i++)
			if (src[i])
				++packsz;
	}
	if (packsz >= 6) { //6, 7, 8
		memcpy(dst, src, sn);
		packsz = sn;
	}
	return packsz;
}

// master pack() driver, renamed to avoid clashing with the Buffer pack() below
static int
pack_impl(const uint8_t *src, int sn, uint8_t *dst)
{
	int packsz;
	uint8_t *ffn = NULL;
	uint8_t *dstart = dst;
	packsz = -1;
	while (sn > 0) {
		if (packsz != 8) { //pack segment
			packsz = packseg(src, sn, dst);
			src += 8;
			sn -= 8;
			dst += packsz + 1; //skip the data+header
			if (packsz == 8 && sn > 0) { //it's 0xff
				ffn = dst;
				++dst;
			} else {
				ffn = NULL;
			}
		} else if (packsz == 8) {
			*ffn = 0;
			for (;;) {
				packsz = packff(src, sn, dst);
				if (packsz >= 6 && packsz <= 8) {
					src += 8;
					sn -= 8;
					dst += packsz;
					++(*ffn);
					if (*ffn == 255) {
						ffn = NULL;
						packsz = -1;
						break;
					}
				} else {
					break;
				}
			}
		}
	}
	return (int)(dst - dstart);
}

static int
unpackseg(const uint8_t *src, int sn, uint8_t *dst, int dn)
{
	if (dn < 8)
		return (int)errc::oom;
	uint8_t hdr = *src++;
	const uint8_t *data0 = src;
	if (hdr == 0) {
		memset(dst, 0, 8);
	} else if (sn >= 10) {
		// >=10 bytes remain, so a byte follows this segment's data: the
		// branchless read (which touches one byte past on trailing clear
		// bits) stays inside the caller's buffer.
		int i;
		for (i = 0; i < 8; i++) {
			uint8_t bit = hdr & 1;
			hdr >>= 1;
			*dst++ = (uint8_t)((0 - bit) & *src);
			src += bit;
		}
	} else {                     // <=9 bytes remain: bounded expand, no over-read
		int i;
		const uint8_t *end = data0 + (sn - 1);
		for (i = 0; i < 8; i++) {
			if ((hdr & 1) && src < end)
				*dst++ = *src++;
			else
				*dst++ = 0;
			hdr >>= 1;
		}
	}
	return (int)(src - data0);
}

// master unpack() driver, renamed; bulk-memcpy run (packff pads every run segment to 8)
static int
unpack_impl(const uint8_t *src, int sn, uint8_t *dst, int dn)
{
	int unpacksz = -1;
	int ffn = -1;
	uint8_t *dstart = dst;
	while (sn > 0) {
		if (unpacksz != 8) {
			unpacksz = unpackseg(src, sn, dst, dn);
			if (unpacksz < 0)    //not enough storage space
				return unpacksz;
			src += unpacksz + 1;
			sn -= unpacksz + 1;
			dst += 8;
			dn -= 8;
			if (unpacksz == 8 && sn > 0) {
				ffn = *src;
				++src;
				--sn;
			}
		} else if (unpacksz == 8) {
			int run = 8 * ffn;
			int copy;
			//ffn - 1, because the last ff pack size may 6, 7, 8
			if ((ffn - 1) * 8 > sn)
				return (int)errc::error;
			if (run > dn)
				return (int)errc::oom;
			copy = run < sn ? run : sn;
			memcpy(dst, src, (size_t)copy);
			if (copy < run)
				memset(dst + copy, 0, (size_t)(run - copy));
			src += copy;
			sn -= copy;
			dst += run;
			dn -= run;
			unpacksz = -1; //restart unpack
		}
	}
	return (int)(dst - dstart);
}

int pack(const uint8_t *src, int srcsz, Buffer &dst) {
	// master's bound: pack emits <= srcsz + overhead (no pad), so no sz8 round-up.
	assert(dst.data() == nullptr || src != (const uint8_t *)dst.data());
	int needn = ((srcsz + 2047) / 2048) * 2 + srcsz + 1;
	dst.resize((size_t)needn);
	int n = pack_impl(src, srcsz, (uint8_t *)dst.data());
	dst.set_size((size_t)n);
	return n;
}

int unpack(const uint8_t *src, int srcsz, Buffer &dst) {
	// bound: the caller's max (set_max — bomb defense) if set, else the loose 8x.
	size_t bound = (dst.max != SIZE_MAX) ? dst.max : ((size_t)srcsz * 8 + 8);
	if (!dst.ensure(bound))
		return (int)errc::oom;
	int n = unpack_impl(src, srcsz, (uint8_t *)dst.data(), (int)bound);
	if (n < 0) {
		dst.clear();
		return n;
	}
	dst.set_size((size_t)n);
	return n;
}

}  // namespace zproto
