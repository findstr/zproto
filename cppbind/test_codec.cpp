// Codec regression tests. Links zproto.o as the byte-compat oracle.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
extern "C" {
#include "zproto.h"
}
#include "zprotowire.h"

using namespace zprotobuf;

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

static void test_pack() {
    for (auto &in : pack_inputs()) {
        // oracle: zproto_pack into a max-sized scratch buffer
        int needn = (((int)in.size() + 2047) / 2048) * 2 + (int)in.size() + 1;
        std::vector<uint8_t> ref(needn, 0);
        int refsz = zproto_pack(in.data(), (int)in.size(), ref.data(), needn);
        assert(refsz >= 0);
        // new
        std::string out;
        int sz = pack(in.data(), (int)in.size(), out);
        assert(sz == refsz);
        assert(out.size() == (size_t)refsz);
        assert(memcmp(out.data(), ref.data(), refsz) == 0);
    }
    printf("test_pack ok\n");
}

static void test_unpack() {
    for (auto &in : pack_inputs()) {
        // pack with oracle, then unpack with both oracle and new, byte-compare.
        // unpack is segment-based (output rounded up to a multiple of 8), so the
        // acceptance gate is byte-for-byte against zproto_unpack, not the original
        // input length.
        int needn = (((int)in.size() + 2047) / 2048) * 2 + (int)in.size() + 1;
        std::vector<uint8_t> packed(needn, 0);
        int psz = zproto_pack(in.data(), (int)in.size(), packed.data(), needn);
        assert(psz >= 0);
        std::vector<uint8_t> ref(needn * 8 + 8, 0);
        int refsz = zproto_unpack(packed.data(), psz, ref.data(), (int)ref.size());
        assert(refsz >= 0);
        std::string out;
        int sz = unpack(packed.data(), psz, out);
        assert(sz == refsz);
        assert(out.size() == (size_t)refsz);
        assert(memcmp(out.data(), ref.data(), refsz) == 0);
    }
    printf("test_unpack ok\n");
}

// ---- encoder tests (oracle: zproto_encode) ----

// schema covers: scalars, string, int array, nested struct, struct map
static const char *kSchema =
    "person 0x10 {\n"
    "    addr {\n"
    "        .city:string 1\n"
    "        .zip:integer 2\n"
    "    }\n"
    "    .name:string 1\n"
    "    .age:integer 2\n"
    "    .lucky:integer[] 3\n"
    "    .home:addr 4\n"
    "    .places:addr[] 5\n"
    "    .byzip:addr[zip] 6\n"
    "}\n";

struct addrval { std::string city; int32_t zip; };

// oracle encode callback: writes fields from the globals below by tag
struct enc_ud {
    std::string name; int32_t age;
    std::vector<int32_t> lucky;
    addrval home;
    std::vector<addrval> places;      // places array
    std::vector<addrval> byzip;       // map values
};

// inner addr field callback: writes one addr's city/zip by tag.
// Mirrors the generated addr::_encode_field.
static struct zproto_struct *g_addr_st;
static int enc_addr_field_cb(struct zproto_args *a) {
    const addrval *ad = (const addrval *)a->ud;
    switch (a->tag) {
    case 1: { a->buffsz = (int)ad->city.size(); memcpy(a->buff, ad->city.data(), a->buffsz); return a->buffsz; }
    case 2: { *(int32_t *)a->buff = ad->zip; return 4; }
    }
    return ZPROTO_ERROR;
}

// recurse zproto_encode on the addr struct for one addr value (mirrors
// the generated addr::_encode that zproto_encode STRUCT fields rely on).
static int enc_addr(struct zproto_args *a, const addrval &ad) {
    return zproto_encode(g_addr_st, a->buff, a->buffsz, enc_addr_field_cb, (void *)&ad);
}

static int enc_cb(struct zproto_args *a) {
    enc_ud *u = (enc_ud *)a->ud;
    switch (a->tag) {
    case 1: { a->buffsz = (int)u->name.size(); memcpy(a->buff, u->name.data(), a->buffsz); return a->buffsz; }
    case 2: { *(int32_t *)a->buff = u->age; return 4; }
    case 3: { // array
        if (a->idx >= (int)u->lucky.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        *(int32_t *)a->buff = u->lucky[a->idx]; return 4;
    }
    case 4: return enc_addr(a, u->home);             // nested struct
    case 5: { // struct array
        if (a->idx >= (int)u->places.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        return enc_addr(a, u->places[a->idx]);
    }
    case 6: { // struct map (wire form == struct array)
        if (a->idx >= (int)u->byzip.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        return enc_addr(a, u->byzip[a->idx]);
    }
    }
    return ZPROTO_ERROR;
}

// hand-driven new encoder, mirroring what body.cpp will generate
static void new_encode_addr(std::string &out, const addrval &ad) {
    encoder e(out, 1, 2);
    e.present(1); e.w_bytes(ad.city.data(), ad.city.size());
    e.present(2); e.w_u32((uint32_t)ad.zip);
    e.finish();
}
static void new_encode_person(std::string &out, const enc_ud &u) {
    encoder e(out, 1, 6);
    e.present(1); e.w_bytes(u.name.data(), u.name.size());
    e.present(2); e.w_u32((uint32_t)u.age);
    e.present(3); e.w_array((uint32_t)u.lucky.size());
        for (auto v : u.lucky) e.w_u32((uint32_t)v);
    e.present(4); new_encode_addr(out, u.home);
    e.present(5); e.w_array((uint32_t)u.places.size());
        for (auto &ad : u.places) new_encode_addr(out, ad);
    e.present(6); e.w_array((uint32_t)u.byzip.size());
        for (auto &ad : u.byzip) new_encode_addr(out, ad);
    e.finish();
}

static void test_encoder() {
    struct zproto_parser p;
    assert(zproto_parse(&p, kSchema) == 0);
    struct zproto *z = p.z;
    struct zproto_struct *person = zproto_query(z, "person");
    assert(person);
    // addr is a nested type inside person; it lives in person->child, not in
    // the top-level namecache, so grab it from there.
    assert(person->child && person->child->count >= 1);
    g_addr_st = person->child->buf[0];
    assert(g_addr_st);

    enc_ud u;
    u.name = "ShangHai"; u.age = 127;
    u.lucky = {3, 7, 5};
    u.home = {"BJ", 100000};
    u.places = { {"A",1}, {"BB",22} };
    u.byzip = { {"C",333}, {"DD",4444} };

    // oracle
    uint8_t ref[4096];
    int refsz = zproto_encode(person, ref, sizeof(ref), enc_cb, &u);
    assert(refsz > 0);

    // new
    std::string out;
    new_encode_person(out, u);
    assert((int)out.size() == refsz);
    assert(memcmp(out.data(), ref, refsz) == 0);

    zproto_free(z);
    printf("test_encoder ok\n");
}

// ---- decoder tests (oracle: zproto_encode; round-trip) ----
static void test_decoder() {
    struct zproto_parser p;
    assert(zproto_parse(&p, kSchema) == 0);
    struct zproto *z = p.z;
    struct zproto_struct *person = zproto_query(z, "person");
    assert(person);
    // addr is a nested type inside person; enc_cb -> enc_addr recurses via
    // g_addr_st, so set it here (mirrors test_encoder).
    assert(person->child && person->child->count >= 1);
    g_addr_st = person->child->buf[0];
    assert(g_addr_st);

    enc_ud u;
    u.name = "ShangHai"; u.age = 127;
    u.lucky = {3, 7, 5};
    u.home = {"BJ", 100000};
    u.places = { {"A",1}, {"BB",22} };
    u.byzip = { {"C",333}, {"DD",4444} };

    uint8_t buf[4096];
    int sz = zproto_encode(person, buf, sizeof(buf), enc_cb, &u);
    assert(sz > 0);

    // decode with new decoder, driven like body.cpp will generate
    std::string dname; int32_t dage = 0;
    std::vector<int32_t> dlucky;
    addrval dhome; dhome.zip = 0;
    std::vector<addrval> dplaces;
    std::vector<addrval> dbyzip;

    auto decode_addr = [](const uint8_t *p, size_t n, addrval &ad) {
        decoder d(p, n, 1);
        assert(d.ok);
        for (int tag; d.next(tag); ) {
            if (tag == 1) d.r_bytes(ad.city);
            else if (tag == 2) ad.zip = (int32_t)d.r_u32();
            else break;
        }
    };

    decoder d(buf, sz, 1);
    assert(d.ok);
    for (int tag; d.next(tag); ) {
        switch (tag) {
        case 1: d.r_bytes(dname); break;
        case 2: dage = (int32_t)d.r_u32(); break;
        case 3: { uint32_t c = d.r_array(); dlucky.resize(c);
                  for (uint32_t i = 0; i < c; i++) dlucky[i] = (int32_t)d.r_u32(); } break;
        case 4: { size_t an; const uint8_t *ap = d.struct_bytes(an); decode_addr(ap, an, dhome); } break;
        case 5: { uint32_t c = d.r_array(); dplaces.resize(c);
                  for (uint32_t i = 0; i < c; i++) { size_t an; const uint8_t *ap = d.struct_bytes(an);
                      decode_addr(ap, an, dplaces[i]); } } break;
        case 6: { uint32_t c = d.r_array();
                  for (uint32_t i = 0; i < c; i++) { size_t an; const uint8_t *ap = d.struct_bytes(an);
                      addrval ad; ad.zip = 0; decode_addr(ap, an, ad); dbyzip.push_back(ad); } } break;
        default: goto done;   // unknown tag -> stop
        }
    }
    done:

    // assert round-trip
    assert(dname == u.name);
    assert(dage == u.age);
    assert(dlucky == u.lucky);
    assert(dhome.city == u.home.city && dhome.zip == u.home.zip);
    assert(dplaces.size() == u.places.size());
    for (size_t i = 0; i < u.places.size(); i++) {
        assert(dplaces[i].city == u.places[i].city && dplaces[i].zip == u.places[i].zip);
    }
    assert(dbyzip.size() == u.byzip.size());
    for (size_t i = 0; i < u.byzip.size(); i++) {
        assert(dbyzip[i].city == u.byzip[i].city && dbyzip[i].zip == u.byzip[i].zip);
    }

    zproto_free(z);
    printf("test_decoder ok\n");
}

int main() {
    test_pack();
    test_unpack();
    test_encoder();
    test_decoder();
    printf("ALL CODEC TESTS PASSED\n");
    return 0;
}
