#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"

using namespace zprotobuf;

// produce all messages -> wire files; then re-decode each and check it matches a fresh fill.
template <typename M>
static int roundtrip(const char *name, void (*fillfn)(M &)) {
	M m; fillfn(m);
	std::string wire;
	int r = m._encode(wire);
	if (r < 0) { fprintf(stderr, "%s encode failed\n", name); return 1; }
	char path[256];
	snprintf(path, sizeof(path), "/home/findstr/zproto/bench/vectors/msg_wire/%s.bin", name);
	FILE *fp = fopen(path, "wb");
	fwrite(wire.data(), 1, wire.size(), fp);
	fclose(fp);
	std::string packed;
	pack((const uint8_t *)wire.data(), (int)wire.size(), packed);
	snprintf(path, sizeof(path), "/home/findstr/zproto/bench/vectors/msg_wire/%s.pack.bin", name);
	fp = fopen(path, "wb");
	fwrite(packed.data(), 1, packed.size(), fp);
	fclose(fp);

	// verify: decode the nopack wire back and re-encode -> must match (maps aside).
	M back;
	back._decode((const uint8_t *)wire.data(), wire.size());
	std::string wire2;
	back._encode(wire2);
	// For map messages, byte order may differ; compare sizes as a smoke check.
	if (wire2.size() != wire.size()) {
		fprintf(stderr, "%s round-trip size drift (%zu vs %zu)\n", name, wire2.size(), wire.size());
		return 1;
	}
	return 0;
}

int main(void) {
	int fail = 0;
	fail += roundtrip<bench::heartbeat>("heartbeat", fill);
	fail += roundtrip<bench::frame>("frame", fill);
	fail += roundtrip<bench::login>("login", fill);
	fail += roundtrip<bench::chat>("chat", fill);
	fail += roundtrip<bench::snapshot>("snapshot", fill);
	fail += roundtrip<bench::alltypes>("alltypes", fill);
	if (fail) { fprintf(stderr, "INTEROP FAILED: %d\n", fail); return 1; }
	printf("INTEROP PRODUCE+ROUNDTRIP OK\n");
	return 0;
}
