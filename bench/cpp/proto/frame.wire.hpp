#pragma once
#include "frame.hpp"
#include "wire.hpp"
#include <cstring>

namespace zproto {

// wire format of frame: [u32 datasize][u16 tagcount][tagcount*u16 delta][body]
//   tagcount=6, deltas all 0 (tags 1..6 from basetag 1), body = eid(8)+x(4)+y(4)+z(4)+yaw(4)+seq(4)=28
//   datasize = 2+12+28 = 42, total = 4+2+12+28 = 46 bytes.
template<> struct wire<::frame> {
	static constexpr int tag() { return 0x10; }
	static constexpr const char *name() { return "frame"; }
	static size_t byte_size(const ::frame &) { return 46; }   // frame is fixed-size

	// direct buffer writes — no encoder object, no std::string push_back.
	static void encode(const ::frame &o, Buffer &buf) {
		buf.resize(46);
		char *b = buf.data();
		b[0] = 42; b[1] = 0; b[2] = 0; b[3] = 0;        // datasize = 42
		b[4] = 6;  b[5] = 0;                              // tagcount = 6
		// b[6..17] deltas: tags 1..6 from basetag 1 -> all 0; resize() zero-fills
		uint64_t eid = o.eid;
		for (int i = 0; i < 8; i++) { b[18 + i] = (char)(eid & 0xff); eid >>= 8; }
		uint32_t u;
		std::memcpy(&u, &o.x, 4);   b[26]=(char)u; b[27]=(char)(u>>8); b[28]=(char)(u>>16); b[29]=(char)(u>>24);
		std::memcpy(&u, &o.y, 4);   b[30]=(char)u; b[31]=(char)(u>>8); b[32]=(char)(u>>16); b[33]=(char)(u>>24);
		std::memcpy(&u, &o.z, 4);   b[34]=(char)u; b[35]=(char)(u>>8); b[36]=(char)(u>>16); b[37]=(char)(u>>24);
		std::memcpy(&u, &o.yaw, 4); b[38]=(char)u; b[39]=(char)(u>>8); b[40]=(char)(u>>16); b[41]=(char)(u>>24);
		uint32_t seq = o.seq;
		b[42]=(char)seq; b[43]=(char)(seq>>8); b[44]=(char)(seq>>16); b[45]=(char)(seq>>24);
	}

	// read header (datasize/tagcount/deltas) then body fields in declaration order.
	static void decode(::frame &o, const uint8_t *p, size_t /*n*/) {
		uint16_t tagcount = (uint16_t)(p[4] | ((uint16_t)p[5] << 8));
		const uint8_t *b = p + 6 + (size_t)tagcount * 2;   // body start
		uint64_t eid = 0;
		for (int i = 0; i < 8; i++) eid |= ((uint64_t)b[i]) << (8 * i);
		o.eid = eid;
		auto rd = [&](size_t off) {
			return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) |
			       ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
		};
		uint32_t u;
		u = rd(8);  std::memcpy(&o.x, &u, 4);
		u = rd(12); std::memcpy(&o.y, &u, 4);
		u = rd(16); std::memcpy(&o.z, &u, 4);
		u = rd(20); std::memcpy(&o.yaw, &u, 4);
		o.seq = rd(24);
	}
};

}  // namespace zproto
