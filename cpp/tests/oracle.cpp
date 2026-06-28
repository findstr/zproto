// Byte-compat oracle shim implementation. The ONLY TU in the test suite that
// includes zproto.h (the C reference). See oracle.hpp for the rationale.
#include "oracle.hpp"
#include "hello_world.hpp"
#include <cassert>
#include <cstring>
#include <vector>
extern "C" {
#include "zproto.h"
}

// nested struct types the callbacks recurse into. Resolved once by
// oracle_resolve() against the parsed schema.
static struct zproto_struct *g_phone_st;    // packet::phone
static struct zproto_struct *g_foo_st;      // packet::foo (bar1)
static struct zproto_struct *g_foo2_st;     // top-level foo2 (bar2)

// the packet POD has both a member named `phone` (the map) and a nested type
// `struct phone`. The member shadows the type under unqualified lookup, so spell
// the type with the `struct` keyword (matches the generated code's style).
using phone_t = struct hello::world::packet::phone;

// ---- nested callbacks (recurse via zproto_encode) ----

static int enc_cb_foo(struct zproto_args *a) {
    const hello::world::packet::foo *f = (const hello::world::packet::foo *)a->ud;
    if (a->tag == 1) { uint32_t u; memcpy(&u, &f->bar1, 4); *(uint32_t *)a->buff = u; return 4; }
    return ZPROTO_ERROR;
}

static int enc_cb_foo2(struct zproto_args *a) {
    const hello::world::foo2 *f = (const hello::world::foo2 *)a->ud;
    if (a->tag == 1) { uint32_t u; memcpy(&u, &f->bar2, 4); *(uint32_t *)a->buff = u; return 4; }
    return ZPROTO_ERROR;
}

static int enc_cb_phone(struct zproto_args *a) {
    const phone_t *ph = (const phone_t *)a->ud;
    switch (a->tag) {
    case 1: *(int32_t *)a->buff = ph->home; return 4;
    case 2: { uint32_t u; memcpy(&u, &ph->work, 4); *(uint32_t *)a->buff = u; return 4; }
    case 3: *(uint8_t *)a->buff = ph->main ? 1 : 0; return 1;
    case 5: return zproto_encode(g_foo_st,  a->buff, a->buffsz, enc_cb_foo,  (void *)&ph->fooval);
    case 6: return zproto_encode(g_foo2_st, a->buff, a->buffsz, enc_cb_foo2, (void *)&ph->fooval2);
    }
    return ZPROTO_ERROR;
}

// recurse zproto_encode on one phone value. Mirrors the generated
// message<packet::phone>::write_at tag layout (1=home,2=work,3=main,5=foo,6=foo2).
static int enc_phone(struct zproto_args *a, const phone_t &ph) {
    return zproto_encode(g_phone_st, a->buff, a->buffsz, enc_cb_phone, (void *)&ph);
}

// ---- packet-level callback ----
//
// Map handling: the C oracle drives arrays/maps by idx (0,1,2,...) and signals
// end-of-array via ZPROTO_NOFIELD after the last element. The generated C++
// message<packet>::write_at iterates pa.phone / pa.phone2 directly. To make the
// C side visit entries in the SAME order as the C++ side, snapshot the map's
// iteration order into an indexable vector on idx==0. Both passes iterate the
// same map instance, so the orders match byte-for-byte.

struct pa_state {
    const hello::world::packet *pa;
    std::vector<const phone_t *> phone;
    std::vector<const phone_t *> phone2;
};

static int enc_cb_pa(struct zproto_args *a) {
    pa_state *s = (pa_state *)a->ud;
    const hello::world::packet *pa = s->pa;
    switch (a->tag) {
    case 1: { // phone[home] map (struct array on the wire)
        if (a->idx == 0) {
            s->phone.clear();
            for (const auto &kv : pa->phone) s->phone.push_back(&kv.second);
        }
        if (a->idx >= (int)s->phone.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        return enc_phone(a, *s->phone[a->idx]);
    }
    case 2: { // phone2[work] map
        if (a->idx == 0) {
            s->phone2.clear();
            for (const auto &kv : pa->phone2) s->phone2.push_back(&kv.second);
        }
        if (a->idx >= (int)s->phone2.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        return enc_phone(a, *s->phone2[a->idx]);
    }
    case 3: { // address : byte[] (BLOB; callback returns byte count)
        a->buffsz = (int)pa->address.size();
        memcpy(a->buff, pa->address.data(), a->buffsz);
        return a->buffsz;
    }
    case 4: { // luck : long[]
        if (a->idx >= (int)pa->luck.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        *(int64_t *)a->buff = pa->luck[a->idx]; return 8;
    }
    case 5: { // address1 : string[]
        if (a->idx >= (int)pa->address1.size()) { a->len = a->idx; return ZPROTO_NOFIELD; }
        const std::string &str = pa->address1[a->idx];
        a->buffsz = (int)str.size();
        memcpy(a->buff, str.data(), a->buffsz);
        return a->buffsz;
    }
    case 6: *(int32_t *)a->buff = pa->ii; return 4;
    case 7: { uint32_t u; memcpy(&u, &pa->ff, 4); *(uint32_t *)a->buff = u; return 4; }
    case 8: *(int64_t *)a->buff = pa->ll; return 8;
    case 9: *(uint8_t *)a->buff = pa->bb ? 1 : 0; return 1;
    case 10: *(int8_t *)a->buff = pa->int8; return 1;
    case 11: *(int16_t *)a->buff = pa->int16; return 2;
    case 12: *(uint64_t *)a->buff = pa->ull; return 8;
    case 15: *(uint32_t *)a->buff = pa->uii; return 4;
    case 17: *(uint16_t *)a->buff = pa->uint16; return 2;
    case 18: *(uint8_t *)a->buff = pa->uint8; return 1;
    case 19: *(int8_t *)a->buff = pa->args; return 1;
    }
    return ZPROTO_ERROR;
}

// resolve the nested struct pointers (phone, packet::foo) and top-level foo2
// from the parsed schema, so the callbacks can recurse via zproto_encode.
static void resolve_structs(struct zproto *z, struct zproto_struct *packet) {
    g_phone_st = g_foo_st = g_foo2_st = nullptr;
    assert(packet->child && packet->child->count >= 2);
    for (int i = 0; i < packet->child->count; i++) {
        struct zproto_struct *c = packet->child->buf[i];
        if (strcmp(c->name, "phone") == 0) g_phone_st = c;
        else if (strcmp(c->name, "foo") == 0) g_foo_st = c;
    }
    g_foo2_st = zproto_query(z, "foo2");
    assert(g_phone_st && g_foo_st && g_foo2_st);
}

int oracle_pack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz) {
    return zproto_pack(src, srcsz, dst, dstsz);
}

int oracle_unpack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz) {
    return zproto_unpack(src, srcsz, dst, dstsz);
}

int oracle_encode_packet(const hello::world::packet &pa, const char *schema_text,
                         uint8_t *out, int cap) {
    struct zproto_parser p;
    if (zproto_parse(&p, schema_text) != 0)
        return -1;
    struct zproto_struct *packet = zproto_query(p.z, "packet");
    if (!packet) { zproto_free(p.z); return -1; }
    resolve_structs(p.z, packet);

    pa_state s;
    s.pa = &pa;
    int n = zproto_encode(packet, out, cap, enc_cb_pa, &s);

    zproto_free(p.z);
    return n;
}
