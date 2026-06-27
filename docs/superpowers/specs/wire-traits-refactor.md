# wire<T> Architecture Refactor — Design & State

Branch: `wire-traits` (off master `34ed4ad`). Goal: clean POD structs + `wire<T>` traits (non-intrusive, fast Buffer-direct encode) + type-erased `binding` dispatch. Pack optimized last.

## Locked decisions
- namespace `zproto` (renamed from `zprotobuf`).
- traits `wire<T>` (in `namespace zproto`), specialized per message type.
- headers `.hpp` + `#pragma once`; generated output = `X.hpp` (POD struct) + `X.wire.hpp` (`wire<X>` specialization, pure-header inline, no `.cc`).
- `wire`/`wirep` base classes + `encoder` class REMOVED. Generated `wire<T>::encode` writes directly to a pre-sized `Buffer` (cursor, no `push_back`).
- `decoder` + `pack`/`unpack` KEPT (namespace `zproto`). `pack` optimized last (deferred — see memory `pack-pushback-optimization`).
- router polymorphism → `binding` type-erasure (`(cmd, void* ptr, fn-pointer ops)`), validated in `bench/cpp/proto/`.

## Progress
- ✅ Phase 1 (commit `263de4f`): `zprotowire.h`→`.hpp` — dropped `wire`/`wirep`/`encoder`, added `Buffer` + `wire<T>` primary + `put_uXX(char*)`, kept decoder/pack/unpack. Compiles clean.
- ✅ Phase 2a (commit `4980b65`): `header.cpp` emits clean POD structs (`#pragma once`, zproto-free includes, no inheritance/virtuals).
- ✅ Phase 2b: `body.cpp` — emits `X.wire.hpp` (`wire<T>` specializations). **THE BIG PIECE.** Done; byte-identical to the C reference across all field types (see Verification).
- ⬜ Phase 3: regen + update all callers (test_codec, bench, hello_world; includes `.h`→`.hpp`; namespace `zprotobuf`→`zproto`).
- ⬜ Phase 4: verify byte-identical (codec conformance, interop, bench all pass; wire bytes unchanged).
- ⬜ Phase 5: optimize `pack` (Buffer-direct-write).

## Phase 2b: body.cpp — emit `X.wire.hpp` per struct

Output per struct (in `X.wire.hpp`, inline, `namespace zproto`):
```cpp
template<> struct wire<NS::X> {
    static constexpr int tag() { return TAG; }              // only if protocol struct
    static constexpr const char *name() { return "X"; }     // only if protocol
    static size_t byte_size(const NS::X &o);
    static size_t write_at(const NS::X &o, Buffer &buf, size_t cur);
    static void encode(const NS::X &o, Buffer &buf) {        // top-level: resize + write_at
        size_t off = buf.size(); buf.resize(off + byte_size(o)); write_at(o, buf, off);
    }
    static void decode(NS::X &o, const uint8_t *p, size_t n);
};
```

**byte_size (per field, summed):**
- scalar u8/16/32/64, f32: width (1/2/4/8)
- string/blob: `4 + o.F.size()`
- array scalar: `4 + o.F.size() * WIDTH`
- array string/blob: `4 + Σ(4 + e.size())` (loop)
- array struct / map: `4 + Σ(wire<E>::byte_size(e))` (loop)
- nested struct: `wire<N>::byte_size(o.F)`
- total = `6 + fieldcount*2 + body`

**write_at (header + body, cursor-threaded):**
- write datasize placeholder(4), tagcount(2), precomputed deltas (fieldcount×2, from basetag+tags), then body per field, then patch datasize = cur-start-4.
- body per field: scalar `put_uXX(b+cur,CAST o.F); cur+=W`; string `put_u32(len); memcpy; cur+=4+len`; array scalar `put_u32(count); for(e){put_uXX; cur+=W}`; array struct/map `put_u32(count); for(e) cur=wire<E>::write_at(e,buf,cur)`; nested `cur=wire<N>::write_at(o.F,buf,cur)`.

**decode (decoder loop + switch, into POD fields):**
- scalar `o.F = CAST d.r_xxx()`; string `d.r_bytes(o.F)`; array scalar `c=d.r_array(); resize; for i<c F[i]=CAST d.r_xxx()`; array struct/map `c=d.r_array(); for i<c {struct_bytes; wire<E>::decode(F[i],p,n)}`; nested `struct_bytes; wire<N>::decode(o.F,p,n)`.

Generator restructure: replace `encode_stmt`→`byte_size_stmt`+`write_at_stmt`; `decode_stmt` keeps the switch shape but emits `wire<E>::decode` for nested (no virtual). `format_encode`→emit the `wire<X>` shell + byte_size/encode bodies; add `format_write_at`/`format_byte_size`. Output filename `X.cc`→`X.wire.hpp`. Drop reset (POD has NSDMI defaults) or add `wire<T>::reset` if router reuse needs it.

## Reference
- Prototype (validated, all 3 gates green): `bench/cpp/proto/` — `frame.hpp` + `frame.wire.hpp` + `wire.hpp` + `dispatch.hpp` + `proto.cpp`. byte-identical, encode 36ns (4.7× old, ≈ pb), dispatch works.

## Phase 2b — realization & verification (done)
`body.cpp` now emits `X.wire.hpp`: a `namespace zproto` block of `template<> wire<T>` specializations (one per struct, post-order so `wire<E>` precedes any `wire<X>` referencing it). Each carries `tag()`/`name()` (tagged structs only), `byte_size`, `write_at` (header + precomputed deltas + body, cursor-threaded into a pre-sized `Buffer`), `encode` (resize + write_at), `decode` (decoder switch, `wire<E>::decode` for nested). Reset dropped (POD NSDMI defaults). Output `X.cc`→`X.wire.hpp`.

Supporting changes:
- `zprotowire.hpp`: added `put_u8` so every scalar width shares the `put_uXX` family.
- Generator emits each type as an **elaborated type-specifier** (`struct ::NS::X`) in all `wire<...>` references. This forces type lookup when a field shares its name with a nested struct (schema `.phone:phone[home]` → a member `phone` and a nested struct `phone`); a plain `wire<NS::X>` would resolve to the member.

Byte-equivalence (all green, scratch harness in `.wire_verify/`):
- **bench** (all field types via `alltypes`): `wire<T>::encode` == golden `msg_wire/*.bin` (old `_encode`/C-reference bytes) for heartbeat/frame/login/chat/snapshot/alltypes (alltypes 3492 B); decode→re-encode reproduces golden.
- **hello_world**: maps (`unordered_map` keyed by int and float), non-sequential tags (packet gaps at 13-14/16), nested `hello::world` namespace round-trip cleanly.
- **gap** (tags 1/5/10): non-sequential delta header `[datasize][tagcount=3][0,3,4][body]` byte-identical to the original C `zproto_encode`.

Known carry-over (not phase-2b): `namespace zproto` (runtime) collides with `struct zproto` (parser, `zproto.h`) if both are included in one TU — the gap cross-check sidesteps this by running the C reference as a separate process. Phase 3 (regen + caller updates) will surface this wherever a TU needs both.
