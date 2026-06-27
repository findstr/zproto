#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "zprotowire.h"   // zprotobuf::pack / unpack

using namespace zprotobuf;

static int hexval(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}
static std::vector<uint8_t> from_hex(const std::string &s) {
	std::vector<uint8_t> b;
	b.reserve(s.size() / 2);
	for (size_t i = 0; i + 1 < s.size(); i += 2)
		b.push_back((uint8_t)((hexval(s[i]) << 4) | hexval(s[i + 1])));
	return b;
}

int main(void) {
	FILE *fp = fopen("/home/findstr/zproto/bench/vectors/codec_golden.txt", "rb");
	if (!fp) { fprintf(stderr, "cannot open golden\n"); return 2; }
	size_t n;
	if (fscanf(fp, "%zu", &n) != 1) { fprintf(stderr, "bad golden\n"); return 2; }
	// fscanf("%zu") leaves the count's trailing newline in the stream; consume
	// the rest of line 1 so the per-vector raw-line reads below stay aligned.
	int c2;
	while ((c2 = fgetc(fp)) != '\n' && c2 != EOF) { }
	int fail = 0;
	for (size_t i = 0; i < n; i++) {
		char inbuf[1 << 16], pkbuf[1 << 16];
		// input line may be empty; fscanf %s skips empty lines, so read raw lines
		std::string in_hex, pk_hex;
		int c;
		// consume newline then read until newline
		while ((c = fgetc(fp)) != '\n' && c != EOF) in_hex.push_back((char)c);
		while ((c = fgetc(fp)) != '\n' && c != EOF) pk_hex.push_back((char)c);
		(void)inbuf; (void)pkbuf;
		std::vector<uint8_t> in = from_hex(in_hex);
		std::vector<uint8_t> expect = from_hex(pk_hex);

		// assertion 1: pack(input) == golden_packed, byte-exact
		std::string got;
		pack(in.data(), (int)in.size(), got);
		if (got.size() != expect.size() ||
		    memcmp(got.data(), expect.data(), got.size()) != 0) {
			fprintf(stderr, "vec %zu PACK mismatch (got %zu, want %zu)\n",
			        i, got.size(), expect.size());
			fail++;
		}
		// assertion 2: unpack recovers input as a prefix, zero-padded to a multiple of 8.
		// (the codec is block-based, 8-byte groups, no tail-length field)
		std::string back;
		unpack(expect.data(), (int)expect.size(), back);
		size_t padded = ((in.size() + 7) / 8) * 8;
		bool ok = (back.size() == padded) &&
		          (in.empty() || memcmp(back.data(), in.data(), in.size()) == 0);
		for (size_t k = in.size(); ok && k < back.size(); k++) {
			if ((uint8_t)back[k] != 0) ok = false;
		}
		if (!ok) {
			fprintf(stderr, "vec %zu UNPACK mismatch (got %zu, want padded %zu, input %zu)\n",
			        i, back.size(), padded, in.size());
			fail++;
		}
	}
	fclose(fp);
	if (fail) { fprintf(stderr, "CODEC CONFORMANCE FAILED: %d\n", fail); return 1; }
	printf("CODEC CONFORMANCE OK (%zu vectors, byte-exact)\n", n);
	return 0;
}
