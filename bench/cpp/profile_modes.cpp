#include <string>
#include <chrono>
#include <stdio.h>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"
using namespace zprotobuf;
template <typename M>
static void run(const char *name, M &m, int N) {
	long sink = 0; double ns;
	std::string ref; m._encode(ref);
	// A: fresh string each iter
	{ auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ std::string w; m._encode(w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("  %s A fresh        : %6.0f ns/op\n", name, ns);
	// B: fresh + reserve
	{ auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ std::string w; w.reserve(ref.size()); m._encode(w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("  %s B fresh+reserve: %6.0f ns/op\n", name, ns);
	// C: reused (clear)
	{ std::string w; auto t0 = std::chrono::steady_clock::now();
	  for (int i=0;i<N;i++){ w.clear(); m._encode(w); sink+=(long)w.size(); }
	  ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count()*1e9/N; }
	printf("  %s C reused clear : %6.0f ns/op\n", name, ns);
	if (sink==0) printf("");
}
int main() {
	bench::frame fr; fill(fr); run("frame", fr, 3000000);
	bench::snapshot sn; fill(sn); run("snapshot", sn, 100000);
	return 0;
}
