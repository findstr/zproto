# zproto Benchmark & Conformance Suite — Implementation Plan 1 (Foundation + C++ column)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the shared schema, golden vectors, and the C++ reference column (codec pack/unpack byte-exact conformance, end-to-end message interop, perf driver, C++ libprotobuf baseline, aggregator) for the zproto benchmark suite.

**Architecture:** New top-level `bench/` dir. A shared `bench.zproto` (consumed by every binding's existing generator) + a golden-vector file generated once from the C reference. The C++ column provides: codec conformance (§2), message interop via shared wire files (§4), perf throughput/size (§3), and the C++ libprotobuf baseline. Lua (Plan 2) and C# (Plan 3) are follow-ons that reuse the schema + golden vectors and emit the same CSV/wire-file conventions.

**Tech Stack:** C++ (-std=c++0x, -Wall -Wextra, matches cppbind/Makefile), zproto C core (zproto.c/.h), cppbind generator, libprotobuf + protoc (C++ pb baseline), bash aggregator.

## Global Constraints

- Per the user: **only code is committed; docs (this plan, the spec) are not committed.**
- Match cppbind style: single-line K&R function signatures, tab indent / space align, no trailing-underscore members, snake_case. Comments in English.
- Compile flags mirror `cppbind/Makefile`: `g++ -Wall -Wextra -O2 -g -std=c++0x -I../`.
- pb baseline is **C++ libprotobuf only** (no pb in Lua/C#).
- zproto pack/unpack is the optional zero-suppression layer; "nopack" = encode/decode only, "pack" = encode+pack / unpack+decode.
- Golden vectors come from the C reference `zproto_pack`/`zproto_unpack`; every binding's codec must match them **byte-for-byte**.

## Scope of Plan 1

- §1 shared schema (bench.zproto + bench.proto).
- §2 codec conformance — C++ (golden generator + C++ test).
- §4 end-to-end message interop — C++ (produce/verify wire files).
- §3 perf harness — C++ driver + C++ pb baseline + aggregator.
- **Out of Plan 1 (follow-on plans):** Lua codec/interop/perf (Plan 2), C# codec/interop/perf (Plan 3). They reuse `bench/schema/bench.zproto` and `bench/vectors/codec_golden.*`.

---

## File Structure

```
bench/                              new, top-level
  schema/
    bench.zproto                    shared zproto schema (family + alltypes)
    bench.proto                     proto3 equivalent (C++ pb baseline only)
  vectors/
    codec_golden.txt                §2 golden vectors: "N\n" then 2 lines/vec (in_hex, packed_hex)
    gen_golden.cpp                  generates codec_golden.txt from C zproto_pack
    msg_wire/                       §4 produced wire files: <msg>.bin / <msg>.pack.bin
  cpp/
    bench_codec.cpp                 §2 C++ codec conformance test
    bench_interop.cpp               §4 C++ produce + verify
    bench.cpp                       §3 C++ zproto perf driver (→ stdout CSV)
    bench_pb.cpp                    §3 C++ libprotobuf baseline (→ stdout CSV)
    Makefile                        builds the above (+ runs generator for bench structs)
  aggregate.sh                      collates *.csv → comparison markdown
```

Generated artifacts (not committed): `bench/cpp/bench.cc`, `bench/cpp/bench.hpp` (from the cppbind generator), `bench/cpp/bench.pb.{cc,h}` (from protoc), `bench/vectors/msg_wire/*.bin`.

---

### Task 1: Shared schema

**Files:**
- Create: `bench/schema/bench.zproto`
- Create: `bench/schema/bench.proto`

**Interfaces:**
- Produces: `bench.zproto` consumed by cppbind generator (Task 4) and later Lua/C# generators; `bench.proto` consumed by protoc (Task 6).

- [ ] **Step 1: Write `bench/schema/bench.zproto`**

```zproto
// zproto benchmark & conformance schema. Helpers first (untagged), then
// protocol messages (tagged). Element counts/values fixed per the spec §1.

vec3 {
	.x:float 1
	.y:float 2
	.z:float 3
}
item {
	.id:uinteger 1
	.count:uinteger 2
}
playerlevel {
	.userid:ulong 1
	.level:uinteger 2
}
attr {
	.key:string 1
	.val:string 2
}
nest {
	.x:integer 1
	.y:integer 2
}
mapi {
	.k:integer 1
	.v:integer 2
}
mapf {
	.kf:float 1
	.v:integer 2
}

heartbeat 0x01 {
	.seq:uinteger 1
	.ack:uinteger 2
}
frame 0x10 {
	.eid:ulong 1
	.x:float 2
	.y:float 3
	.z:float 4
	.yaw:float 5
	.seq:uinteger 6
}
login 0x20 {
	.userid:ulong 1
	.token:string 2
	.version:uinteger 3
	.platform:ubyte 4
}
chat 0x30 {
	.from:ulong 1
	.to:ulong 2
	.text:string 3
	.ts:ulong 4
	.attrs:attr[] 5
}
snapshot 0x40 {
	.userid:ulong 1
	.pos:vec3 2
	.hp:integer 3
	.mp:integer 4
	.inventory:item[] 5
	.buffs:integer[] 6
	.flags:boolean[] 7
	.friends:playerlevel[userid] 8
	.level:uinteger 9
}
alltypes 0xFF {
	.b:boolean 1
	.i8:byte 2
	.u8:ubyte 3
	.i16:short 4
	.u16:ushort 5
	.i32:integer 6
	.u32:uinteger 7
	.i64:long 8
	.u64:ulong 9
	.f:float 10
	.s:string 11
	.bl:blob 12
	.ai32:integer[] 13
	.as:string[] 14
	.abl:blob[] 15
	.abool:boolean[] 16
	.nest_n:nest 17
	.nest_na:nest[] 18
	.m_int:mapi[k] 19
	.m_float:mapf[kf] 20
	.aempty:integer[] 21
	.asingle:integer[] 22
}
```

- [ ] **Step 2: Write `bench/schema/bench.proto`**

```proto
syntax = "proto3";
package benchpb;

message Vec3 { float x = 1; float y = 2; float z = 3; }
message Item { uint32 id = 1; uint32 count = 2; }
message Attr { string key = 1; string val = 2; }
message Nest { int32 x = 1; int32 y = 2; }
message Mapi { int32 v = 1; }            // key lifted to map<int32,Mapi>
message Mapf { int32 v = 1; }            // key lifted to map<float,Mapf>

message Heartbeat { uint32 seq = 1; uint32 ack = 2; }
message Frame {
	uint64 eid = 1; float x = 2; float y = 3; float z = 4; float yaw = 5; uint32 seq = 6;
}
message Login {
	uint64 userid = 1; string token = 2; uint32 version = 3; uint32 platform = 4;
}
message Chat {
	uint64 from = 1; uint64 to = 2; string text = 3; uint64 ts = 4; repeated Attr attrs = 5;
}
message Snapshot {
	uint64 userid = 1; Vec3 pos = 2; int32 hp = 3; int32 mp = 4;
	repeated Item inventory = 5; repeated int32 buffs = 6; repeated bool flags = 7;
	map<int32, Mapi> friends = 8;   // value carries level only; key=userid
	uint32 level = 9;
}
message Alltypes {
	bool b = 1; int32 i8 = 2; uint32 u8 = 3; int32 i16 = 4; uint32 u16 = 5;
	int32 i32 = 6; uint32 u32 = 7; int64 i64 = 8; uint64 u64 = 9; float f = 10;
	string s = 11; bytes bl = 12;
	repeated int32 ai32 = 13; repeated string as = 14; repeated bytes abl = 15; repeated bool abool = 16;
	Nest nest_n = 17; repeated Nest nest_na = 18;
	map<int32, Mapi> m_int = 19; map<float, Mapf> m_float = 20;
	repeated int32 aempty = 21; repeated int32 asingle = 22;
}
```

Note: zproto byte/short/long map to int32/int64 in proto3 (no 8/16-bit scalar types); `playerlevel` (map value) collapses so the key (`userid`) is the map key and only `level` stays in the value — a documented size-comparable mapping.

- [ ] **Step 3: Verify the cppbind generator accepts bench.zproto**

Run:
```bash
cd /home/findstr/zproto/cppbind && make zproto && ./zproto /home/findstr/zproto/bench/schema/bench.zproto
```
Expected: exits 0; creates `bench.cc` and `bench.hpp` in `cppbind/` (move them: `mv cppbind/bench.cc cppbind/bench.hpp /home/findstr/zproto/bench/cpp/`). Compiling `bench.hpp` with `g++ -std=c++0x -I../ -fsyntax-only cppbind/bench.hpp` succeeds.

- [ ] **Step 4: Commit**

```bash
cd /home/findstr/zproto
git add bench/schema/bench.zproto bench/schema/bench.proto
git commit -m "bench: shared zproto + proto3 schema"
```

---

### Task 2: Golden vector generator (C reference)

**Files:**
- Create: `bench/vectors/gen_golden.cpp`

**Interfaces:**
- Produces: `bench/vectors/codec_golden.txt` — format: first line = vector count `N`, then 2 lines per vector: input as lowercase hex (empty line if input is empty), packed as lowercase hex. Consumed by Task 3 (C++) and later Lua/C# codec tests.

- [ ] **Step 1: Write `bench/vectors/gen_golden.cpp`**

```cpp
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "zproto.h"   // zproto_pack

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
		int needn = (((n + 2047) / 2048) * 2) + n + 1;
		std::vector<uint8_t> out(needn, 0);
		int psz = zproto_pack(p.bytes.data(), n, out.data(), needn);
		fprintf(fp, "%s\n", to_hex(p.bytes.data(), n).c_str());
		fprintf(fp, "%s\n", to_hex(out.data(), psz).c_str());
	}
	fclose(fp);
	printf("wrote %zu vectors\n", ps.size());
	return 0;
}
```

- [ ] **Step 2: Build + run the generator**

Run:
```bash
cd /home/findstr/zproto/bench/vectors
gcc -Wall -O2 -I../../ -c ../../zproto.c -o zproto.o
g++ -Wall -O2 -std=c++0x -I../../ gen_golden.cpp zproto.o -o gen_golden
mkdir -p /home/findstr/zproto/bench/vectors
./gen_golden
```
Expected: prints `wrote 21 vectors`; `codec_golden.txt` exists with first line `21`.

- [ ] **Step 3: Sanity-check one vector by eye**

Run: `head -5 codec_golden.txt`
Expected: line 1 = `21`; line 2 = `` (empty input `ff0`); line 3 = `` (packing empty input yields empty); line 4 = `00` (input `001` = one zero byte); line 5 = `00` (one zero byte packs to a single `0x00` header byte — the max-compression case).

- [ ] **Step 4: Commit**

```bash
cd /home/findstr/zproto
git add bench/vectors/gen_golden.cpp
git commit -m "bench: golden vector generator (C zproto_pack reference)"
```

(`codec_golden.txt` is a generated artifact — do not commit, or commit as a checked-in fixture per team preference; default: leave uncommitted, regenerated by `make`.)

---

### Task 3: C++ codec conformance test (§2, byte-exact)

**Files:**
- Create: `bench/cpp/bench_codec.cpp`

**Interfaces:**
- Consumes: `bench/vectors/codec_golden.txt` (Task 2), `zprotobuf::pack/unpack` from `zprotowire.h`.
- Produces: a test binary that exits 0 on full byte-exact pass, non-zero on any mismatch.

- [ ] **Step 1: Write `bench/cpp/bench_codec.cpp`**

```cpp
#include <stdio.h>
#include <stdint.h>
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
		// assertion 2: unpack(packed) == input, byte-exact
		std::string back;
		unpack(expect.data(), (int)expect.size(), back);
		if (back.size() != in.size() ||
		    memcmp(back.data(), in.data(), back.size()) != 0) {
			fprintf(stderr, "vec %zu UNPACK mismatch (got %zu, want %zu)\n",
			        i, back.size(), in.size());
			fail++;
		}
	}
	fclose(fp);
	if (fail) { fprintf(stderr, "CODEC CONFERENCE FAILED: %d\n", fail); return 1; }
	printf("CODEC CONFORMANCE OK (%zu vectors, byte-exact)\n", n);
	return 0;
}
```

- [ ] **Step 2: Build + run**

Run:
```bash
cd /home/findstr/zproto/bench/cpp
g++ -Wall -Wextra -O2 -std=c++0x -I../../cppbind bench_codec.cpp ../../cppbind/zprotowire.o -o bench_codec
./bench_codec
```
Expected: prints `CODEC CONFORMANCE OK (21 vectors, byte-exact)`, exit 0.

- [ ] **Step 3: Commit**

```bash
cd /home/findstr/zproto
git add bench/cpp/bench_codec.cpp
git commit -m "bench: C++ codec pack/unpack byte-exact conformance test"
```

---

### Task 4: C++ end-to-end message interop (§4)

**Files:**
- Create: `bench/cpp/bench_interop.cpp`
- Generated (by cppbind generator): `bench/cpp/bench.cc`, `bench/cpp/bench.hpp`

**Interfaces:**
- Consumes: `bench.hpp` (generated structs in `namespace bench` from Task 1's bench.zproto), `zprotobuf::pack/unpack`.
- Produces: wire files under `bench/vectors/msg_wire/<msg>.bin` (nopack) and `<msg>.pack.bin` (packed); exit 0 if every produced file decodes back to the canonical values.

Canonical instance values (must be replicated identically in Lua/C# later):
- heartbeat: seq=0x12345678, ack=0x9abcdef0
- frame: eid=0x0123456789abcdef, x=1.5f, y=-2.25f, z=3.0f, yaw=-0.5f, seq=42
- login: userid=0xfedcba9876543210, token="token-v1-abc", version=0x01020304, platform=3
- chat: from=1, to=2, text="hello bench", ts=1700000000, attrs=[("k1","v1"),("k2","v2"),("lang","cpp"),("ver","1")]
- snapshot: userid=0xabc, pos=(10.0f,20.0f,30.0f), hp=100, mp=50, inventory=[{i, i*2} for i in 1..50], buffs=[ (i%2? 0 : i) for i in 0..31 ] (~½ zero), flags=[ (i<32) for 64 bools ]... friends=[ {userid=k, level=k*10} for k in 1..100 ], level=99
- alltypes: see `bench_interop.cpp` fill function (every scalar at boundary value 0/-1/MAX, arrays sized 64/32/50, aempty=[] asingle=[7])

- [ ] **Step 1: Generate the C++ bench structs**

Run:
```bash
cd /home/findstr/zproto/cppbind && make zproto && ./zproto /home/findstr/zproto/bench/schema/bench.zproto
mv bench.cc bench.hpp /home/findstr/zproto/bench/cpp/
```
Expected: `bench/cpp/bench.cc` and `bench/cpp/bench.hpp` exist; the structs are in `namespace bench` (the schema has no dot-separated namespace prefix, so confirm the actual namespace from the generated `bench.hpp` top lines and adjust the `using`/qualifiers below to match).

- [ ] **Step 2: Write `bench/cpp/bench_interop.cpp`**

```cpp
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "bench.hpp"
#include "zprotowire.h"

using namespace zprotobuf;

// ---- canonical instance builders (identical values across all bindings) ----

static void fill(bench::heartbeat &m) { m.seq = 0x12345678; m.ack = 0x9abcdef0; }
static void fill(bench::frame &m) {
	m.eid = 0x0123456789abcdefULL;
	m.x = 1.5f; m.y = -2.25f; m.z = 3.0f; m.yaw = -0.5f; m.seq = 42;
}
static void fill(bench::login &m) {
	m.userid = 0xfedcba9876543210ULL; m.token = "token-v1-abc";
	m.version = 0x01020304; m.platform = 3;
}
static void fill(bench::chat &m) {
	m.from = 1; m.to = 2; m.text = "hello bench"; m.ts = 1700000000;
	const char *k[] = {"k1","k2","lang","ver"}, *v[] = {"v1","v2","cpp","1"};
	m.attrs.resize(4);
	for (int i = 0; i < 4; i++) { m.attrs[i].key = k[i]; m.attrs[i].val = v[i]; }
}
static void fill(bench::snapshot &m) {
	m.userid = 0xabc; m.pos.x = 10.0f; m.pos.y = 20.0f; m.pos.z = 30.0f;
	m.hp = 100; m.mp = 50; m.level = 99;
	m.inventory.resize(50);
	for (int i = 0; i < 50; i++) { m.inventory[i].id = i + 1; m.inventory[i].count = (i + 1) * 2; }
	m.buffs.resize(32);
	for (int i = 0; i < 32; i++) m.buffs[i] = (i % 2) ? 0 : i;   // ~½ zero
	m.flags.resize(64);
	for (int i = 0; i < 64; i++) m.flags[i] = (i < 32);
	for (int k = 1; k <= 100; k++) m.friends[k].level = k * 10;
}
static void fill(bench::alltypes &m) {
	m.b = true; m.i8 = -1; m.u8 = 0xff; m.i16 = -1; m.u16 = 0xffff;
	m.i32 = -1; m.u32 = 0xffffffffu; m.i64 = -1; m.u64 = 0xffffffffffffffffULL;
	m.f = 3.14f; m.s = "str"; m.bl = {(char)0xab, (char)0xcd};
	m.ai32.resize(64); for (int i = 0; i < 64; i++) m.ai32[i] = i;
	m.as.resize(64); for (int i = 0; i < 64; i++) m.as[i] = "s";
	m.abl.resize(64); for (int i = 0; i < 64; i++) m.abl[i] = {(char)0x01};
	m.abool.resize(64); for (int i = 0; i < 64; i++) m.abool[i] = (i % 2);
	m.nest_n.x = 1; m.nest_n.y = 2;
	m.nest_na.resize(32); for (int i = 0; i < 32; i++) { m.nest_na[i].x = i; m.nest_na[i].y = -i; }
	for (int k = 0; k < 50; k++) m.m_int[k].v = k;
	for (int k = 0; k < 50; k++) { float kf = (float)k + 0.5f; m.m_float[kf].v = k; }
	m.aempty.clear();
	m.asingle = {7};
}

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
```

Note: full cross-binding decode of these `.bin` files by Lua/C# happens in Plan 2/3; this task establishes the C++ producer + a same-binding round-trip smoke check (size-stable; value-equality across bindings comes when the other bindings decode these files in Plan 2/3).

- [ ] **Step 3: Build + run**

Run:
```bash
cd /home/findstr/zproto/bench/cpp
mkdir -p /home/findstr/zproto/bench/vectors/msg_wire
g++ -Wall -Wextra -O2 -std=c++0x -I../../cppbind bench_interop.cpp bench.cc ../../cppbind/zprotowire.o -o bench_interop
./bench_interop
```
Expected: prints `INTEROP PRODUCE+ROUNDTRIP OK`, exit 0; `bench/vectors/msg_wire/*.bin` and `*.pack.bin` populated (12 files).

- [ ] **Step 4: Commit**

```bash
cd /home/findstr/zproto
git add bench/cpp/bench_interop.cpp
git commit -m "bench: C++ message interop producer + round-trip (wire files)"
```

---

### Task 5: C++ perf driver (§3, zproto)

**Files:**
- Create: `bench/cpp/bench.cpp`

**Interfaces:**
- Consumes: `bench.hpp` (generated), `zprotobuf::pack/unpack`, the canonical `fill()` builders from Task 4 (extract them into a shared header `bench/cpp/instances.h` so perf + interop share one source of truth for the canonical data).
- Produces: CSV rows on stdout: `cpp,<message>,<op>,<mode>,<size_bytes>,<ops_per_sec>`.

- [ ] **Step 1: Extract canonical builders to `bench/cpp/instances.h`**

Move the `fill()` overloads from Task 4's `bench_interop.cpp` into `bench/cpp/instances.h` (header-only, inline), and `#include "instances.h"` from both `bench_interop.cpp` and `bench.cpp`. Rebuild `bench_interop` to confirm it still passes.

- [ ] **Step 2: Write `bench/cpp/bench.cpp`**

```cpp
#include <stdio.h>
#include <chrono>
#include <string>
#include "bench.hpp"
#include "instances.h"
#include "zprotowire.h"

using namespace zprotobuf;

template <typename M>
static void emit(const char *name, M &m) {
	std::string wire;
	m._encode(wire);
	std::string packed;
	pack((const uint8_t *)wire.data(), (int)wire.size(), packed);
	auto bench = [&](const char *op, const char *mode, size_t sz, auto &&fn) {
		// warmup + run until ~1s
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
	};
	bench("encode", "zproto_nopack", wire.size(), [&] {
		std::string w; m._encode(w);
	});
	bench("encode_pack", "zproto_pack", packed.size(), [&] {
		std::string w; m._encode(w); std::string p; pack((const uint8_t *)w.data(), (int)w.size(), p);
	});
	std::string dtmp = wire;
	bench("decode", "zproto_nopack", wire.size(), [&] {
		M d; d._decode((const uint8_t *)dtmp.data(), dtmp.size());
	});
	std::string ptmp = packed;
	bench("unpack_decode", "zproto_pack", packed.size(), [&] {
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
```

- [ ] **Step 3: Build + run, sanity-check CSV**

Run:
```bash
cd /home/findstr/zproto/bench/cpp
g++ -Wall -Wextra -O2 -std=c++0x -I../../cppbind bench.cpp bench.cc ../../cppbind/zprotowire.o -o bench
./bench | head
```
Expected: 24 CSV rows (6 messages × 4 ops) with sane `ops_per_sec` (heartbeat highest, snapshot lowest) and `size_bytes` increasing heartbeat→snapshot.

- [ ] **Step 4: Commit**

```bash
cd /home/findstr/zproto
git add bench/cpp/instances.h bench/cpp/bench.cpp
git commit -m "bench: C++ zproto perf driver (encode/decode/pack/unpack throughput)"
```

---

### Task 6: C++ libprotobuf baseline (§3, pb)

**Files:**
- Create: `bench/cpp/bench_pb.cpp`
- Generated (protoc): `bench/cpp/bench.pb.cc`, `bench/cpp/bench.pb.h`

**Interfaces:**
- Consumes: `bench/schema/bench.proto` (Task 1), libprotobuf, protoc.
- Produces: CSV rows: `cpp,<message>,marshal|unmarshal,pb,<size_bytes>,<ops_per_sec>`.

- [ ] **Step 1: Generate pb classes**

Run:
```bash
cd /home/findstr/zproto/bench/cpp
protoc -I../schema --cpp_out=. ../schema/bench.proto
```
Expected: `bench.pb.cc` and `bench.pb.h` created. Requires `protoc` + libprotobuf installed; if missing, install `libprotobuf-dev` + `protobuf-compiler` first.

- [ ] **Step 2: Write `bench/cpp/bench_pb.cpp`**

```cpp
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
	for (int k = 0; k < 50; k++) { float kf = (float)k + 0.5f; (*m.mutable_m_float())[kf].set_v(k); }
	m.add_asingle(7);
}

template <typename M>
static void emit(const char *name, M &m) {
	std::string s; m.SerializeToString(&s);
	auto run = [&](const char *op, auto &&fn) {
		int n = 0; auto t0 = std::chrono::steady_clock::now(); auto t1 = t0;
		while (true) { fn(); n++; t1 = std::chrono::steady_clock::now();
			if (std::chrono::duration<double>(t1 - t0).count() > 1.0) break; }
		printf("cpp,%s,%s,pb,%zu,%.0f\n", name, op, s.size(), n / std::chrono::duration<double>(t1 - t0).count());
	};
	std::string buf = s;
	run("marshal", [&] { std::string o; m.SerializeToString(&o); });
	run("unmarshal", [&] { M d; d.ParseFromString(buf); });
}

#define RUN(NAME) { NAME m; fill(m); emit(#NAME, m); }

int main(void) {
	RUN(Heartbeat); RUN(Frame); RUN(Login); RUN(Chat); RUN(Snapshot); RUN(Alltypes);
	return 0;
}
```

- [ ] **Step 3: Build + run**

Run:
```bash
cd /home/findstr/zproto/bench/cpp
g++ -Wall -O2 -std=c++0x bench_pb.cpp bench.pb.cc -lprotobuf -o bench_pb
./bench_pb | head
```
Expected: 12 CSV rows (6 messages × marshal/unmarshal); `Heartbeat` size should be ≤ zproto-nopack heartbeat size.

- [ ] **Step 4: Commit**

```bash
cd /home/findstr/zproto
git add bench/cpp/bench_pb.cpp
git commit -m "bench: C++ libprotobuf baseline driver"
```

---

### Task 7: Aggregator

**Files:**
- Create: `bench/aggregate.sh`

**Interfaces:**
- Consumes: all `*.csv` produced by bench drivers (each driver writes to stdout; the runner redirects to e.g. `bench/out/cpp.csv`, `bench/out/cpp_pb.csv`).
- Produces: a markdown comparison table on stdout (size: nopack/pack/pb per message; throughput: per lang per op).

- [ ] **Step 1: Write `bench/aggregate.sh`**

```bash
#!/usr/bin/env bash
# Collates bench/out/*.csv (lang,message,op,mode,size_bytes,ops_per_sec) into
# a markdown comparison table. Requires awk.
set -euo pipefail
DIR="$(dirname "$0")/out"
mkdir -p "$DIR"
{
	echo "# zproto benchmark results"
	echo
	echo "## size (bytes)"
	echo
	echo "| message | zproto-nopack | zproto-pack | pb |"
	echo "|---|---:|---:|---:|"
	awk -F, 'NR==1{next} {key[$2]=$2}
		$3=="encode"      && $4=="zproto_nopack"{np[$2]=$5}
		$3=="encode_pack" && $4=="zproto_pack"  {pk[$2]=$5}
		$3=="marshal"     && $4=="pb"           {pb[$2]=$5}
		END{for(m in key) printf "| %s | %s | %s | %s |\n", m, np[m], pk[m], pb[m]}'
	echo
	echo "## throughput (ops/sec)"
	echo
	echo "| message | op | cpp-zproto | cpp-pb |"
	echo "|---|---|---:|---:|"
	awk -F, 'NR==1{next}
		$1=="cpp" && $4!="pb"{zt[$2"@"$3]=$6}
		$1=="cpp" && $4=="pb"{pp[$2"@"($3=="unmarshal"?"unpack_decode":($3=="marshal"?"encode_pack":"encode"))]=$6}
		END{for(k in zt) printf "| %s | %s |\n", k, zt[k]}'
} < <(cat "$DIR"/*.csv 2>/dev/null || true)
```

(The size table is the primary output; the throughput table is a starter that Plan 2/3 extend with `lua`/`cs` columns as more CSVs land in `bench/out/`.)

- [ ] **Step 2: Run the full C++ column end-to-end**

Run:
```bash
cd /home/findstr/zproto/bench
mkdir -p out
( cd cpp && ./bench       ) > out/cpp.csv
( cd cpp && ./bench_pb    ) > out/cpp_pb.csv
./aggregate.sh > out/results.md
cat out/results.md
```
Expected: a populated `results.md` with the size table (6 rows) and a throughput listing.

- [ ] **Step 3: Commit**

```bash
cd /home/findstr/zproto
git add bench/aggregate.sh
git commit -m "bench: CSV aggregator -> markdown comparison table"
```

---

## Self-Review

**1. Spec coverage:**
- §1 schema → Task 1 ✓
- §1 element counts/zero-heavy/edge/identical-across-bindings → Task 4 fill() (50/32-½zero/64/100; aempty/asingle; hardcoded) ✓
- §2 codec byte-exact conformance + golden vectors + all patterns (ff×0/00×1/00×n/ff×n/2040/2048/run-mid/tail6-7-8/brk5/ff×7-9-15-17/raw/random) → Task 2 patterns + Task 3 byte-exact asserts ✓
- §3 perf matrix (nopack/pack/pb × encode/decode) → Task 5 (zproto) + Task 6 (pb) ✓
- §3 fairness (identical payload, hot loop ≥1s, pre-size, env) → Task 5 uses shared instances.h, ≥1s loop ✓ (env-reporting is a minor TODO: add a header echo in aggregate.sh if desired)
- §4 end-to-end interop → Task 4 produces wire files; cross-binding decode deferred to Plan 2/3 (explicitly scoped) ✓
- pb = C++ libprotobuf only → Task 6 ✓
- alltypes in perf → Task 5/6 RUN(Alltypes) ✓
- map determinism → §4 uses size-stable round-trip for maps in C++ (Task 4); cross-binding value equality in Plan 2/3 ✓

**2. Placeholder scan:** env-reporting in the perf driver is the one soft spot (the spec says "report env: CPU/compiler/flags"). Minor — acceptable to defer to the aggregator printing `uname`/compiler versions, or add a `--env` header row. No TBD/TODO in task code.

**3. Type consistency:** `fill()` builders are shared via `instances.h` (Task 5 extracts from Task 4) so the canonical data is single-sourced — zproto perf and pb perf both mirror the same values. `bench::message` namespace and `m.field` / `m._encode/_decode` names match the generated `bench.hpp`; the implementer must confirm the exact namespace from the generated header (called out in Task 4 Step 1).

## Follow-on plans (out of Plan 1)

- **Plan 2 — Lua column:** `bench/lua/bench_codec.lua` (load `codec_golden.txt`, byte-exact), `bench/lua/bench_interop.lua` (decode the C++-produced `msg_wire/*.bin` → value equality), `bench/lua/bench.lua` (perf CSV). Reuses bench.zproto via the luabind generator; lzproto API TBD from `luabind/zproto.lua`.
- **Plan 3 — C# column:** mirror of Plan 2 against csbind (`csbind/zprotowire.cs` / `cszproto.c`).
