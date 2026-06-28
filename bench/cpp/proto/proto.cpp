// Prototype: validates the wire<T> architecture (clean POD struct + traits + Buffer + binding)
// against three gates: byte-identical output, encode speed, router-style dispatch.
#include "frame.hpp"
#include "frame.wire.hpp"
#include "dispatch.hpp"

#include "bench.hpp"        // old architecture: bench::frame (wirep base, virtual _encode)
#include "zprotowire.h"     // old runtime
#include "bench.pb.h"       // pb baseline

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

int main() {
	// identical data in all three representations
	frame f{0x0123456789abcdefULL, 1.5f, -2.25f, 3.0f, -0.5f, 42};
	bench::frame oldf;
	oldf.eid = f.eid; oldf.x = f.x; oldf.y = f.y; oldf.z = f.z; oldf.yaw = f.yaw; oldf.seq = f.seq;
	benchpb::Frame pbf;
	pbf.set_eid(f.eid); pbf.set_x(f.x); pbf.set_y(f.y); pbf.set_z(f.z); pbf.set_yaw(f.yaw); pbf.set_seq(f.seq);

	// === 1. correctness: new encode == old _encode, byte-for-byte ===
	printf("=== 1. correctness (byte-identical to current frame::_encode) ===\n");
	zproto::Buffer buf;
	zproto::wire<frame>::encode(f, buf);
	std::string oldwire;
	oldf._encode(oldwire);
	bool ok = (buf.size() == oldwire.size() &&
	           std::memcmp(buf.cdata(), oldwire.data(), buf.size()) == 0);
	printf("new wire<frame>::encode = %zu bytes\n", buf.size());
	printf("old frame::_encode      = %zu bytes\n", oldwire.size());
	printf("byte-identical: %s\n", ok ? "YES" : "NO");
	if (!ok) {
		printf("new: "); for (size_t i = 0; i < buf.size(); i++) printf("%02x", (unsigned char)buf.cdata()[i]); printf("\n");
		printf("old: "); for (size_t i = 0; i < oldwire.size(); i++) printf("%02x", (unsigned char)oldwire[i]); printf("\n");
	}

	// === 2. speed ===
	printf("\n=== 2. speed (ns/op, lower = faster) ===\n");
	const int N = 3000000;
	long sink = 0;
	double ns;
	{
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) { zproto::Buffer b; zproto::wire<frame>::encode(f, b); sink += (long)b.size(); }
		ns = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e9 / N;
	}
	printf("new wire<frame>::encode : %6.0f ns/op\n", ns);
	{
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) { std::string w; oldf._encode(w); sink += (long)w.size(); }
		ns = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e9 / N;
	}
	printf("old frame::_encode      : %6.0f ns/op\n", ns);
	{
		std::string o;
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) { o.clear(); pbf.SerializeToString(&o); sink += (long)o.size(); }
		ns = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e9 / N;
	}
	printf("pb Frame marshal        : %6.0f ns/op\n", ns);

	// zproto encode + pack (the full packed path) — the realistic "send" cost when using the optional pack.
	{
		std::string packed;
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) {
			zproto::Buffer b; zproto::wire<frame>::encode(f, b);
			zprotobuf::pack((const uint8_t *)b.cdata(), (int)b.size(), packed);
			sink += (long)packed.size();
		}
		ns = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e9 / N;
	}
	printf("new encode+pack         : %6.0f ns/op\n", ns);

	// sizes: nopack / packed / pb
	{
		zproto::Buffer b; zproto::wire<frame>::encode(f, b);
		std::string packed;
		zprotobuf::pack((const uint8_t *)b.cdata(), (int)b.size(), packed);
		std::string pbs; pbf.SerializeToString(&pbs);
		printf("\n--- sizes (bytes) ---\n");
		printf("zproto nopack: %zu\n", b.size());
		printf("zproto pack  : %zu\n", packed.size());
		printf("pb           : %zu\n", pbs.size());
	}

	// === 3. router-style dispatch (reg/call, type-erased) ===
	printf("\n=== 3. dispatch (router reg/call) ===\n");
	reg(f, [](void *p) -> uint32_t {
		frame *d = static_cast<frame *>(p);
		printf("[handler] got frame: eid=0x%lx x=%.2f y=%.2f z=%.2f yaw=%.2f seq=%u\n",
		       (unsigned long)d->eid, d->x, d->y, d->z, d->yaw, d->seq);
		bool match = (d->eid == 0x0123456789abcdefULL && d->x == 1.5f && d->y == -2.25f &&
		              d->z == 3.0f && d->yaw == -0.5f && d->seq == 42);
		printf("[handler] decoded values match original: %s\n", match ? "YES" : "NO");
		return 7;
	});
	zproto::Buffer wire;
	zproto::wire<frame>::encode(f, wire);
	uint32_t err = call(zproto::wire<frame>::tag(), (const uint8_t *)wire.cdata(), wire.size());
	printf("call(tag) returned: %u (expected 7): %s\n", err, err == 7 ? "YES" : "NO");

	return (int)(sink & 1);
}
