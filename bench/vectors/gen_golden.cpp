#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
extern "C" {
#include "zproto.h"   // zproto_pack
}

static std::string to_hex(const uint8_t *p, int n) {
	static const char *h = "0123456789abcdef";
	std::string s;
	s.reserve(n * 2);
	for (int i = 0; i < n; i++) {
		s.push_back(h[p[i] >> 4]);
		s.push_back(h[p[i] & 0xf]);
	}
	return s;
}

// one input pattern: bytes + length
struct pattern {
	const char *name;
	std::vector<uint8_t> bytes;
};

static std::vector<uint8_t> fill(int n, uint8_t v) {
	return std::vector<uint8_t>(n, v);
}
static std::vector<uint8_t> rep(int groups, uint8_t v) {
	return fill(groups * 8, v);
}
// groups full of 0xFF then a tail group with `nonzero` leading 0xFF bytes, rest 0x00
static std::vector<uint8_t> run_tail(int full_groups, int nonzero) {
	std::vector<uint8_t> b;
	for (int g = 0; g < full_groups; g++)
		for (int i = 0; i < 8; i++) b.push_back(0xff);
	for (int i = 0; i < 8; i++) b.push_back(i < nonzero ? 0xff : 0x00);
	return b;
}
static std::vector<uint8_t> concat(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b) {
	std::vector<uint8_t> c = a;
	c.insert(c.end(), b.begin(), b.end());
	return c;
}

int main(void) {
	std::vector<pattern> ps;
	// empty / min / all-zero
	ps.push_back({"ff0", fill(0, 0xff)});
	ps.push_back({"001", fill(1, 0x00)});
	ps.push_back({"00x8", fill(8, 0x00)});
	ps.push_back({"00x16", fill(16, 0x00)});
	ps.push_back({"00x64", fill(64, 0x00)});
	// all-nonzero
	ps.push_back({"ffx8", rep(1, 0xff)});
	ps.push_back({"ffx16", rep(2, 0xff)});
	// run-counter boundaries
	ps.push_back({"ffx2040", rep(255, 0xff)});
	ps.push_back({"ffx2048", rep(256, 0xff)});
	// sparse nonzero
	ps.push_back({"01x8", fill(8, 0x01)});
	ps.push_back({"01x16", fill(16, 0x01)});
	// run in middle
	ps.push_back({"mid", concat(concat(fill(8, 0x00), rep(2, 0xff)), fill(8, 0x00))});
	// run tail count = 6/7/8 then break at 5
	ps.push_back({"tail6", run_tail(1, 6)});
	ps.push_back({"tail7", run_tail(1, 7)});
	ps.push_back({"tail8", run_tail(2, 8)});    // two full groups
	ps.push_back({"brk5", run_tail(1, 5)});
	// tail partial (all-ff, non-multiple-of-8)
	ps.push_back({"ffx7", fill(7, 0xff)});
	ps.push_back({"ffx9", fill(9, 0xff)});
	ps.push_back({"ffx14", fill(14, 0xff)});
	ps.push_back({"ffx15", fill(15, 0xff)});
	ps.push_back({"ffx17", fill(17, 0xff)});
	// raw fidelity: a run of distinct nonzeros
	{
		std::vector<uint8_t> r;
		for (int i = 0; i < 8; i++) r.push_back((uint8_t)(i + 1));
		for (int i = 0; i < 8; i++) r.push_back((uint8_t)(i + 1));
		ps.push_back({"rawdistinct", r});
	}
	// seeded pseudo-random (len 256, 4096)
	{
		uint32_t s = 0xdeadbeef;
		for (int n : {256, 4096}) {
			std::vector<uint8_t> r(n);
			for (int i = 0; i < n; i++) {
				s = s * 1103515245u + 12345u;
				r[i] = (uint8_t)((s >> 16) & 0xff);
			}
			ps.push_back({n == 256 ? "rnd256" : "rnd4096", r});
		}
	}

	FILE *fp = fopen("/home/findstr/zproto/bench/vectors/codec_golden.txt", "wb");
	fprintf(fp, "%zu\n", ps.size());
	for (auto &p : ps) {
		int n = (int)p.bytes.size();
		int n8 = (n + 7) & ~7;   // packff pads the trailing partial segment to 8
		int needn = (((n8 + 2047) / 2048) * 2) + n8 + 1;
		std::vector<uint8_t> out(needn, 0);
		int psz = zproto_pack(p.bytes.data(), n, out.data(), needn);
		fprintf(fp, "%s\n", to_hex(p.bytes.data(), n).c_str());
		fprintf(fp, "%s\n", to_hex(out.data(), psz).c_str());
	}
	fclose(fp);
	printf("wrote %zu vectors\n", ps.size());
	return 0;
}
