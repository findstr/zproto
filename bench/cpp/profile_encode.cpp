#include <string>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"
using namespace zprotobuf;
int main() {
	bench::frame m;
	fill(m);
	long sink = 0;
	for (long i = 0; i < 1000000; i++) {   // 1M frame encodes
		std::string w;
		m._encode(w);
		sink += (long)w.size();
	}
	return (int)(sink & 1);
}
