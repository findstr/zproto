#ifndef __bench_instances_h
#define __bench_instances_h

#include "bench.hpp"

// ---- canonical instance builders (identical values across all bindings) ----

inline void fill(bench::heartbeat &m) { m.seq = 0x12345678; m.ack = 0x9abcdef0; }
inline void fill(bench::frame &m) {
	m.eid = 0x0123456789abcdefULL;
	m.x = 1.5f; m.y = -2.25f; m.z = 3.0f; m.yaw = -0.5f; m.seq = 42;
}
inline void fill(bench::login &m) {
	m.userid = 0xfedcba9876543210ULL; m.token = "token-v1-abc";
	m.version = 0x01020304; m.platform = 3;
}
inline void fill(bench::chat &m) {
	m.from = 1; m.to = 2; m.text = "hello bench"; m.ts = 1700000000;
	const char *k[] = {"k1","k2","lang","ver"}, *v[] = {"v1","v2","cpp","1"};
	m.attrs.resize(4);
	for (int i = 0; i < 4; i++) { m.attrs[i].key = k[i]; m.attrs[i].val = v[i]; }
}
inline void fill(bench::snapshot &m) {
	m.userid = 0xabc; m.pos.x = 10.0f; m.pos.y = 20.0f; m.pos.z = 30.0f;
	m.hp = 100; m.mp = 50; m.level = 99;
	m.inventory.resize(50);
	for (int i = 0; i < 50; i++) { m.inventory[i].id = i + 1; m.inventory[i].count = (i + 1) * 2; }
	m.buffs.resize(32);
	for (int i = 0; i < 32; i++) m.buffs[i] = (i % 2) ? 0 : i;   // ~½ zero
	m.flags.resize(64);
	for (int i = 0; i < 64; i++) m.flags[i] = (i < 32);
	// map value must carry its own key field (userid) for the entry to
	// round-trip: the decoder re-inserts by the value's key field, not the
	// external map key, so leaving it at 0 would collapse all entries to key 0.
	for (int k = 1; k <= 100; k++) { m.friends[k].userid = k; m.friends[k].level = k * 10; }
}
inline void fill(bench::alltypes &m) {
	m.b = true; m.i8 = -1; m.u8 = 0xff; m.i16 = -1; m.u16 = 0xffff;
	m.i32 = -1; m.u32 = 0xffffffffu; m.i64 = -1; m.u64 = 0xffffffffffffffffULL;
	m.f = 3.14f; m.s = "str"; m.bl = {(char)0xab, (char)0xcd};
	m.ai32.resize(64); for (int i = 0; i < 64; i++) m.ai32[i] = i;
	m.as.resize(64); for (int i = 0; i < 64; i++) m.as[i] = "s";
	m.abl.resize(64); for (int i = 0; i < 64; i++) m.abl[i] = {(char)0x01};
	m.abool.resize(64); for (int i = 0; i < 64; i++) m.abool[i] = (i % 2);
	m.nest_n.x = 1; m.nest_n.y = 2;
	m.nest_na.resize(32); for (int i = 0; i < 32; i++) { m.nest_na[i].x = i; m.nest_na[i].y = -i; }
	// mapi/mapf values carry their key field (.k); set it to match the
	// external map key so decode re-inserts under the same key (see snapshot).
	for (int k = 0; k < 50; k++) { m.m_int[k].k = k; m.m_int[k].v = k; }
	for (int k = 0; k < 50; k++) { m.m_float[k].k = k; m.m_float[k].fv = (float)k + 0.5f; }
	m.aempty.clear();
	m.asingle = {7};
}

#endif
