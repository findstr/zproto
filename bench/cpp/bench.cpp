#include <stdio.h>
#include <chrono>
#include <string>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"

using namespace zprotobuf;

// timed hot-loop: run fn() until ~1s elapsed, report ops/sec as a CSV row.
template <typename F>
static void run_bench(const char *name, const char *op, const char *mode, size_t sz, F &&fn) {
	int n = 0;
	auto t0 = std::chrono::steady_clock::now();
	auto t1 = t0;
	while (true) {
		fn();
		n++;
		t1 = std::chrono::steady_clock::now();
		if (std::chrono::duration<double>(t1 - t0).count() > 1.0) break;
	}
	double secs = std::chrono::duration<double>(t1 - t0).count();
	double ops = n / secs;
	printf("cpp,%s,%s,%s,%zu,%.0f\n", name, op, mode, sz, ops);
}

template <typename M>
static void emit(const char *name, M &m) {
	std::string wire;
	m._encode(wire);
	std::string packed;
	pack((const uint8_t *)wire.data(), (int)wire.size(), packed);
	run_bench(name, "encode", "zproto_nopack", wire.size(), [&] {
		std::string w; m._encode(w);
	});
	run_bench(name, "encode_pack", "zproto_pack", packed.size(), [&] {
		std::string w; m._encode(w); std::string p; pack((const uint8_t *)w.data(), (int)w.size(), p);
	});
	std::string dtmp = wire;
	run_bench(name, "decode", "zproto_nopack", wire.size(), [&] {
		M d; d._decode((const uint8_t *)dtmp.data(), dtmp.size());
	});
	std::string ptmp = packed;
	run_bench(name, "unpack_decode", "zproto_pack", packed.size(), [&] {
		std::string up; unpack((const uint8_t *)ptmp.data(), ptmp.size(), up);
		M d; d._decode((const uint8_t *)up.data(), up.size());
	});
}

#define RUN(NAME) { bench::NAME m; fill(m); emit(#NAME, m); }

int main(void) {
	RUN(heartbeat);
	RUN(frame);
	RUN(login);
	RUN(chat);
	RUN(snapshot);
	RUN(alltypes);
	return 0;
}
