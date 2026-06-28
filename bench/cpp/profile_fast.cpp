#include <string.h>
#include <string>
#include <chrono>
#include <stdio.h>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"
using namespace zprotobuf;

// hand-written fast frame encoder: resize exact, direct index writes, no encoder object/push_back.
// frame wire = [u32 datasize=40][u16 tagcount=6][6*u16 delta=0][eid u64][x f32][y f32][z f32][yaw f32][seq u32] = 46 bytes
static void frame_encode_fast(const bench::frame &m, std::string &out) {
	const size_t SZ = 46;
	size_t off = out.size();
	out.resize(off + SZ);            // one alloc + zero-fill (deltas land 0)
	char *b = &out[off];
	b[0] = 42; b[1] = 0; b[2] = 0; b[3] = 0;   // datasize = 2(tagcount)+12(deltas)+28(body) = 42
	b[4] = 6;  b[5] = 0;                        // tagcount = 6
	// b[6..17] deltas already 0 from resize
	uint64_t eid = m.eid;
	for (int i = 0; i < 8; i++) { b[18 + i] = (char)(eid & 0xff); eid >>= 8; }
	uint32_t u;
	memcpy(&u, &m.x, 4);   b[26]=(char)u; b[27]=(char)(u>>8); b[28]=(char)(u>>16); b[29]=(char)(u>>24);
	memcpy(&u, &m.y, 4);   b[30]=(char)u; b[31]=(char)(u>>8); b[32]=(char)(u>>16); b[33]=(char)(u>>24);
	memcpy(&u, &m.z, 4);   b[34]=(char)u; b[35]=(char)(u>>8); b[36]=(char)(u>>16); b[37]=(char)(u>>24);
	memcpy(&u, &m.yaw, 4); b[38]=(char)u; b[39]=(char)(u>>8); b[40]=(char)(u>>16); b[41]=(char)(u>>24);
	uint32_t seq = (uint32_t)m.seq;
	b[42]=(char)seq; b[43]=(char)(seq>>8); b[44]=(char)(seq>>16); b[45]=(char)(seq>>24);
}

int main() {
	bench::frame m; fill(m);
	const int N = 3000000;
	long sink = 0;
	double ns;
	// correctness: fast must equal _encode
	{ std::string a, b; m._encode(a); frame_encode_fast(m, b);
	  printf("correctness: _encode=%zu fast=%zu %s\n", a.size(), b.size(), a==b?"EQUAL":"DIFFER"); }
	// current encoder
	{ auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ std::string w; m._encode(w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("current _encode   : %6.0f ns/op\n", ns);
	// fast (fresh string each iter, same condition)
	{ auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ std::string w; frame_encode_fast(m, w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("hand fast encode  : %6.0f ns/op\n", ns);
	// fast reused (clear) — best case
	{ std::string w; auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ w.clear(); frame_encode_fast(m, w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("hand fast reused  : %6.0f ns/op\n", ns);
	return (int)(sink & 1);
}
