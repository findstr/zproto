// Codec byte-compat oracle. Links ../../zproto.c (the C reference, via the
// oracle shim) and checks the C++ runtime + generated message<T> codec
// byte-for-byte against the C functions: zproto_pack/zproto_unpack (raw codec)
// and zproto_encode (struct encode, driven through oracle.cpp so this TU never
// includes zproto.h -- see oracle.hpp for why the two cannot share a TU).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "zproto.hpp"
#include "oracle.hpp"
#include "hello_world.hpp"
#include "hello_world.message.hpp"

using namespace zproto;

// build inputs of varied patterns/sizes to exercise pack segments
static std::vector<std::vector<uint8_t>> pack_inputs() {
    std::vector<std::vector<uint8_t>> v;
    { std::vector<uint8_t> a(8, 0);          v.push_back(a); }      // all-zero (1 segment)
    { std::vector<uint8_t> a(16, 0);         v.push_back(a); }      // 2 zero segments
    { std::vector<uint8_t> a(8, 0xAB);       v.push_back(a); }      // all-non-zero (full run)
    { std::vector<uint8_t> a(64, 0x55);      v.push_back(a); }      // long full run
    { std::vector<uint8_t> a = {1,0,2,0,3,0,4,0}; v.push_back(a); } // mixed
    { std::vector<uint8_t> a = {1,2,3,4,5,6,7,8,9}; v.push_back(a); } // unaligned tail
    { std::vector<uint8_t> a(3, 0);          v.push_back(a); }      // short, <8
    { std::vector<uint8_t> a;                v.push_back(a); }      // empty
    return v;
}

// ---- pack / unpack: raw zero-suppression codec vs zproto_pack/zproto_unpack ----

static void test_pack() {
    for (auto &in : pack_inputs()) {
        int needn = (((int)in.size() + 2047) / 2048) * 2 + (int)in.size() + 1;
        std::vector<uint8_t> ref(needn, 0);
        int refsz = oracle_pack(in.data(), (int)in.size(), ref.data(), needn);
        assert(refsz >= 0);
        Buffer out;
        int sz = pack(in.data(), (int)in.size(), out);
        assert(sz == refsz);
        assert(out.size() == (size_t)refsz);
        assert(memcmp(out.cdata(), ref.data(), refsz) == 0);
    }
    printf("test_pack ok\n");
}

static void test_unpack() {
    for (auto &in : pack_inputs()) {
        int needn = (((int)in.size() + 2047) / 2048) * 2 + (int)in.size() + 1;
        std::vector<uint8_t> packed(needn, 0);
        int psz = oracle_pack(in.data(), (int)in.size(), packed.data(), needn);
        assert(psz >= 0);
        std::vector<uint8_t> ref(needn * 8 + 8, 0);
        int refsz = oracle_unpack(packed.data(), psz, ref.data(), (int)ref.size());
        assert(refsz >= 0);
        Buffer out;
        int sz = unpack(packed.data(), psz, out);
        assert(sz == refsz);
        assert(out.size() == (size_t)refsz);
        assert(memcmp(out.cdata(), ref.data(), refsz) == 0);
    }
    printf("test_unpack ok\n");
}

// ---- encoder/decoder: generated message<packet> vs zproto_encode ----
//
// Byte-compat strategy: drive BOTH the C++ encode and the C oracle encode from
// the same hello::world::packet object. The generated message<packet>::write_at
// iterates pa.phone / pa.phone2 directly; the C oracle callback (in oracle.cpp)
// snapshots the same maps' iteration order. Because an unordered_map's
// iteration order is stable across two passes over the same instance, both
// sides visit the map entries in identical order, yielding a byte-exact match
// even for the map fields. No deterministic-field restriction is needed.

// read hello.world.zproto (the schema lives next to the test binary at runtime)
static std::string read_schema() {
    FILE *fp = fopen("hello.world.zproto", "rb");
    assert(fp);
    std::string src;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) src.append(buf, n);
    fclose(fp);
    return src;
}

// populate a packet covering every schema field (scalars, strings, vectors, and
// both maps).
static void fill_packet(hello::world::packet &pa) {
    pa.phone[1].home = -3389;
    pa.phone[1].work = 999.98f;
    pa.phone[1].main = false;
    pa.phone[1].fooval.bar1 = 9.9f;
    pa.phone[1].fooval2.bar2 = 8.9f;
    pa.phone[2].home = 0x1024;
    pa.phone[2].work = 1.25f;
    pa.phone[2].main = true;
    pa.phone[2].fooval.bar1 = -1.5f;
    pa.phone[2].fooval2.bar2 = 0.5f;

    pa.phone2[999.98f].home = -3399;
    pa.phone2[999.98f].work = 999.98f;
    pa.phone2[999.98f].main = true;
    pa.phone2[999.98f].fooval.bar1 = 9.8f;
    pa.phone2[999.98f].fooval2.bar2 = 8.8f;

    pa.address = "ShangHai";
    pa.luck = {3, 7, 5, -1, 0x7fffffffffffffffLL};
    pa.address1 = {"hello", "world", "zproto"};
    pa.ii = -123456;
    pa.ff = 3.14159f;
    pa.ll = 0x0123456789abcdefLL;
    pa.bb = true;
    pa.int8 = -127;
    pa.int16 = -32000;
    pa.ull = 0xffffffffffffffffULL;
    pa.uii = 4000000000U;
    pa.uint16 = 60000;
    pa.uint8 = 250;
    pa.args = -5;
}

// the packet POD has both a member named `phone` (the map) and a nested type
// `struct phone`; spell the type with the `struct` keyword so lookup finds it.
using phone_t = struct hello::world::packet::phone;

static void assert_phone_eq(const phone_t &a, const phone_t &b) {
    assert(a.home == b.home);
    assert(a.work == b.work);
    assert(a.main == b.main);
    assert(a.fooval.bar1 == b.fooval.bar1);
    assert(a.fooval2.bar2 == b.fooval2.bar2);
}

static void test_encoder() {
    std::string schema = read_schema();

    hello::world::packet pa;
    fill_packet(pa);

    // oracle: C reference (zproto_encode, driven via oracle.cpp)
    uint8_t ref[4096];
    int refsz = oracle_encode_packet(pa, schema.c_str(), ref, (int)sizeof(ref));
    assert(refsz > 0);

    // new: generated message<packet>::encode
    Buffer out;
    message<hello::world::packet>::encode(pa, out);
    assert((int)out.size() == refsz);
    assert(memcmp(out.cdata(), ref, refsz) == 0);

    printf("test_encoder ok\n");
}

static void test_decoder() {
    std::string schema = read_schema();

    hello::world::packet pa;
    fill_packet(pa);

    // oracle bytes: C-encoded reference
    uint8_t ref[4096];
    int refsz = oracle_encode_packet(pa, schema.c_str(), ref, (int)sizeof(ref));
    assert(refsz > 0);

    // decode the C-encoded bytes with the generated message<packet>::decode and
    // assert field-by-field round-trip.
    hello::world::packet pb;
    message<hello::world::packet>::decode(pb, ref, (size_t)refsz);

    assert(pb.phone.size() == pa.phone.size());
    for (const auto &kv : pa.phone) {
        auto it = pb.phone.find(kv.second.home);
        assert(it != pb.phone.end());
        assert_phone_eq(it->second, kv.second);
    }
    assert(pb.phone2.size() == pa.phone2.size());
    for (const auto &kv : pa.phone2) {
        auto it = pb.phone2.find(kv.second.work);
        assert(it != pb.phone2.end());
        assert_phone_eq(it->second, kv.second);
    }
    assert(pb.address == pa.address);
    assert(pb.luck == pa.luck);
    assert(pb.address1 == pa.address1);
    assert(pb.ii == pa.ii);
    assert(pb.ff == pa.ff);
    assert(pb.ll == pa.ll);
    assert(pb.bb == pa.bb);
    assert(pb.int8 == pa.int8);
    assert(pb.int16 == pa.int16);
    assert(pb.ull == pa.ull);
    assert(pb.uii == pa.uii);
    assert(pb.uint16 == pa.uint16);
    assert(pb.uint8 == pa.uint8);
    assert(pb.args == pa.args);

    printf("test_decoder ok\n");
}

// Decode of an array whose wire count is inflated far past the actual data must
// NOT allocate proportionally to the claimed count (memory-amplification / OOM
// defense). The decoder underruns once the real elements run out; the array must
// end up holding only what was actually decodable, not the (untrusted) count.
static void test_array_oom_defense() {
    // hand-built packet wire: a single field (luck, tag 4). packet basetag is 1,
    // so a delta of 3 recovers tag = 1 + 3 = 4. The count is forged to 1000 but
    // only two int64 elements follow -- the decode must stop at 2, not reserve
    // 1000 and fill the rest with zeros.
    static const uint8_t buf[] = {
        0x18, 0x00, 0x00, 0x00,                                       // [u32 datasize = 24]
        0x01, 0x00,                                                   // [u16 tagcount = 1]
        0x03, 0x00,                                                   // [u16 delta = 3] -> tag 4
        0xe8, 0x03, 0x00, 0x00,                                       // [u32 count = 1000] (forged)
        0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              // [u64 3]
        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,              // [u64 7]
    };
    hello::world::packet pb;
    message<hello::world::packet>::decode(pb, buf, sizeof(buf));
    assert(pb.luck.size() == 2);
    assert(pb.luck[0] == 3);
    assert(pb.luck[1] == 7);
    printf("test_array_oom_defense ok\n");
}

// Same memory-amplification root cause, map flavor. The map is memory-safe (no
// reserve; on underrun every iteration inserts at the same default key, so the
// map dedups and never balloons), BUT the inner loop is still bounded by the
// untrusted count with no ok-bail, so a forged huge count makes it spin and
// leaves one spurious default-key entry. Assert the map stays empty.
static void test_map_oom_defense() {
    // hand-built packet wire: one field (phone map, tag 1). basetag=1 -> delta=0.
    // count is forged to 1000 but no struct elements follow.
    static const uint8_t buf[] = {
        0x08, 0x00, 0x00, 0x00,          // [u32 datasize = 8]
        0x01, 0x00,                      // [u16 tagcount = 1]
        0x00, 0x00,                      // [u16 delta = 0] -> tag 1 (phone)
        0xe8, 0x03, 0x00, 0x00,          // [u32 count = 1000] (forged, no data)
    };
    hello::world::packet pb;
    message<hello::world::packet>::decode(pb, buf, sizeof(buf));
    assert(pb.phone.size() == 0);
    printf("test_map_oom_defense ok\n");
}

int main() {
    test_pack();
    test_unpack();
    test_encoder();
    test_decoder();
    test_array_oom_defense();
    test_map_oom_defense();
    printf("ALL CODEC TESTS PASSED\n");
    return 0;
}
