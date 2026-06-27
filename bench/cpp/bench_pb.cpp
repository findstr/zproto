#include <stdio.h>
#include <chrono>
#include <string>
#include "bench.pb.h"

using namespace benchpb;

// mirror the zproto canonical values into pb messages (kept in sync with instances.h)
static void fill(Heartbeat &m) { m.set_seq(0x12345678); m.set_ack(0x9abcdef0); }
static void fill(Frame &m) {
	m.set_eid(0x0123456789abcdefULL); m.set_x(1.5f); m.set_y(-2.25f);
	m.set_z(3.0f); m.set_yaw(-0.5f); m.set_seq(42);
}
static void fill(Login &m) {
	m.set_userid(0xfedcba9876543210ULL); m.set_token("token-v1-abc");
	m.set_version(0x01020304); m.set_platform(3);
}
static void fill(Chat &m) {
	m.set_from(1); m.set_to(2); m.set_text("hello bench"); m.set_ts(1700000000);
	const char *k[] = {"k1","k2","lang","ver"}, *v[] = {"v1","v2","cpp","1"};
	for (int i = 0; i < 4; i++) { auto *a = m.add_attrs(); a->set_key(k[i]); a->set_val(v[i]); }
}
static void fill(Snapshot &m) {
	m.set_userid(0xabc); auto *p = m.mutable_pos(); p->set_x(10.0f); p->set_y(20.0f); p->set_z(30.0f);
	m.set_hp(100); m.set_mp(50); m.set_level(99);
	for (int i = 0; i < 50; i++) { auto *it = m.add_inventory(); it->set_id(i+1); it->set_count((i+1)*2); }
	for (int i = 0; i < 32; i++) m.add_buffs((i % 2) ? 0 : i);
	for (int i = 0; i < 64; i++) m.add_flags(i < 32);
	for (int k = 1; k <= 100; k++) (*m.mutable_friends())[k].set_v(k * 10);   // key=userid, value=level
}
static void fill(Alltypes &m) {
	m.set_b(true); m.set_i8(-1); m.set_u8(0xff); m.set_i16(-1); m.set_u16(0xffff);
	m.set_i32(-1); m.set_u32(0xffffffffu); m.set_i64(-1); m.set_u64(0xffffffffffffffffULL);
	m.set_f(3.14f); m.set_s("str"); m.set_bl("\xab\xcd");
	for (int i = 0; i < 64; i++) m.add_ai32(i);
	for (int i = 0; i < 64; i++) m.add_as("s");
	for (int i = 0; i < 64; i++) m.add_abl("\x01");
	for (int i = 0; i < 64; i++) m.add_abool(i % 2);
	m.mutable_nest_n()->set_x(1); m.mutable_nest_n()->set_y(2);
	for (int i = 0; i < 32; i++) { auto *n = m.add_nest_na(); n->set_x(i); n->set_y(-i); }
	for (int k = 0; k < 50; k++) (*m.mutable_m_int())[k].set_v(k);
	for (int k = 0; k < 50; k++) { (*m.mutable_m_float())[k].set_v(k); }
	m.add_asingle(7);
}

// timed hot-loop: run fn() until ~1s elapsed, report ops/sec as a CSV row (mode is fixed "pb").
template <typename F>
static void run_bench_pb(const char *name, const char *op, size_t sz, F &&fn) {
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
	printf("cpp,%s,%s,pb,%zu,%.0f\n", name, op, sz, ops);
}

template <typename M>
static void emit(const char *name, M &m) {
	std::string s; m.SerializeToString(&s);
	std::string buf = s;
	run_bench_pb(name, "marshal", s.size(), [&] { std::string o; m.SerializeToString(&o); });
	run_bench_pb(name, "unmarshal", s.size(), [&] { M d; d.ParseFromString(buf); });
}

#define RUN(NAME) { NAME m; fill(m); emit(#NAME, m); }

int main(void) {
	RUN(Heartbeat); RUN(Frame); RUN(Login); RUN(Chat); RUN(Snapshot); RUN(Alltypes);
	return 0;
}
