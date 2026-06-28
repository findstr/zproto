# C++ Reorganization + `wire`→`message` Migration — Design

Date: 2026-06-28
Branch context: `perf-pack`

## Background

The 5-phase wire refactor (commits `263de4f`..`a381e58`, plus `3024ec4`) migrated
the C++ **runtime** and **generator** away from the old API (`wirep` base class,
virtual `_encode/_decode/_pack/_unpack/_tag`, `zprotowire.h`, `zprotobuf` namespace)
to a new one:

- `namespace zproto`
- `Buffer` (raw-backed, auto-growing byte buffer)
- `wire<T>` traits (`encode/decode/byte_size/write_at/tag/name`)
- schema-agnostic `decoder` + little-endian `put_uXX/get_uXX`
- free functions `pack(src, n, Buffer&)` / `unpack(src, n, Buffer&)`

The migration **stopped at the runtime + generator**. Every downstream consumer
still references the deleted old API and does not compile:

- `cppbind/test.cpp` — calls dead `pk._encode()/_tag()/_pack()/_unpack()`.
- `cppbind/test_codec.cpp` — `#include "zprotowire.h"` (deleted); the C-reference
  byte-compat oracle is therefore down.
- `cppbind/Makefile` — `test` target links nonexistent `hello_world.cc`.
- `bench/cpp/*` — all `#include "zprotowire.h"` + old `wirep` API (out of scope here).

A hand-written round-trip confirmed the new runtime is functionally correct
(encode→pack→unpack→decode verified for scalars/strings/vectors). The problem is
purely that the tree was left mid-migration and nothing downstream builds.

## Goal

Reorganize `cppbind/` into a clean `cpp/{emitter,runtime,tests}` layout,
distribute the runtime as **source-copy**, rename `wire`→`message`, and migrate
the tests to the new API so the C++ tree builds and the oracle runs again.

## Decisions (locked)

1. **Scope**: full migration of `test.cpp` + `test_codec.cpp` to the new API.
   `bench/cpp` is **out of scope** — it will be redone later as part of a
   cross-language comparison (Rust + Lua + Lua-protobuf), and committed last.
2. **Layout**: `cpp/emitter/`, `cpp/runtime/`, `cpp/tests/` (tests sibling of
   runtime, not nested).
3. **Schema extension**: keep `.zproto` (do **not** rename to `.proto`; avoids
   collision with protobuf semantics).
4. **Test language**: C++ `.cpp` (the runtime is template-based; C tests are not
   possible without a C wrapper).
5. **Distribution model**: **source-copy**. Users copy `zproto.hpp` + `zproto.cpp`
   into their project and compile directly. No `libzproto.a`, no `make install`,
   no pkg-config. Consequently `runtime/` has **no Makefile**; build/test happens
   in `tests/`.
6. **`wire`→`message`**: pure rename. Trait `wire<T>`→`message<T>`, generated file
   `X.wire.hpp`→`X.message.hpp`, all `body.cpp`/`zproto.hpp` references updated.
   Method signatures unchanged, behavior unchanged, namespace stays `zproto`.
7. **`runtime/zproto.cpp` provenance**: the codec is a near-verbatim copy of
   **master's** `zproto.c` `pack`/`unpack` drivers + helpers
   (`packseg`/`packff`/`unpackseg`), renamed `pack`→`pack_impl` /
   `unpack`→`unpack_impl` to avoid clashing with `zproto::pack`/`zproto::unpack`.
   A header comment annotates the source for re-sync. The `zproto_pack`/
   `zproto_unpack` C wrappers are **not** copied (thin bound-check shells,
   redundant with the C++ Buffer sizing).
8. **Commit phasing**: not required. The `perf-pack` branch will not be published
   as-is; the user will regenerate a single clean commit on `master` later,
   discarding history. No effort spent on `git mv` history preservation.
9. **Codec base = master (`cbecf36`, pack-side pad removed).** The perf-pack
   working tree's `zproto.c` + `cppbind/zproto.cpp` currently carry the divergent
   pad-present codec (perf-pack `3024ec4`: SWAR count, `packff` pads the trailing
   segment to 8, `unpack` bulk-`memcpy`s `8*ffn` runs, bound via `sz8`). Master
   removed the pack-side pad: `packff` emits `sn` bytes with an unrolled per-byte
   count, `unpack` copies `min(run, sn)` and zero-fills the **output** to a
   multiple of 8, and the bound uses `srcsz` directly (no `sz8`). The first
   migration step aligns perf-pack's `zproto.c` + the runtime codec to master so
   the runtime and the C oracle agree byte-for-byte. (Master's unrolled count is
   also more correct — its comment notes the SWAR hasZero trick has borrow-chain
   false positives.)
10. **Error codes → `namespace zproto` enum.** The C-style macros
    `#define ZPROTO_ERR_*` are inappropriate in a C++ header. Replace with a
    scoped enum inside the namespace:
    `enum class errc : int { oom = -1, error = -3 };`.
    `NOFIELD` is dropped (unused in the runtime codec). `pack`/`unpack` still
    return `int` (size on success); on failure they return
    `static_cast<int>(errc::…)`.

## Target Layout

```
cpp/
├── emitter/                 # code generator (build-time tool)
│   ├── main.cpp             # was cppbind/proto.cpp (also fix delete→delete[])
│   ├── header.cpp  header.h # emit clean POD structs (X.hpp)
│   ├── body.cpp    body.h   # emit message<T> traits (X.message.hpp)
│   └── Makefile             # builds `zproto` generator; links ../../zproto.c (C parser)
├── runtime/                 # source-copy distributable (no Makefile)
│   ├── zproto.hpp           # namespace zproto: Buffer, message<T>, decoder, put/get_uXX, error codes
│   └── zproto.cpp           # pack/unpack (zproto.c codec, renamed pack_impl/unpack_impl)
└── tests/
    ├── hello.world.zproto   # sample schema
    ├── test.cpp             # functional round-trip on generated code
    ├── test_codec.cpp       # byte-compat oracle vs C reference (zproto.h)
    └── Makefile             # build generator → generate → build runtime+tests → run
```

`csbind/` and `luabind/` are sibling flat directories and are **not** touched.

## Component Changes

### emitter/
- Rename `proto.cpp` → `main.cpp`. Fix the `new[]`/`delete` mismatch: `proto.cpp:88`
  `delete proto;` → `delete[] proto;` (clears the `-Wmismatched-new-delete` warning).
- `header.cpp`/`body.cpp`: update for the `wire`→`message` rename (see below).
- `Makefile`: build `main.cpp + header.cpp + body.cpp` + `../../zproto.c` (compiled
  as `zproto_c.o`, the C parser/serializer used by the generator) → `zproto` binary.
- The generator still takes `<name>.zproto` and emits `<name>.hpp` + `<name>.message.hpp`.

### runtime/
- `zproto.hpp` + `zproto.cpp` move here verbatim (with the `wire`→`message` rename
  in the header). No Makefile — source-copy distribution.
- `zproto.hpp` defines error codes as a scoped enum `zproto::errc` (see Decision 10),
  so the runtime stays free of any `zproto.h` dependency (users copy only 2 files).

### tests/
- `hello.world.zproto` moves here (sample schema driving both tests).
- `test.cpp` and `test_codec.cpp` move here and are migrated to the new API.
- `Makefile` orchestrates the full flow (see Build model).

## `wire` → `message` Rename Mapping

| Location | Old | New |
|---|---|---|
| `runtime/zproto.hpp` primary template | `template<class T> struct wire;` | `template<class T> struct message;` |
| `runtime/zproto.hpp` comments / doc | `wire<T>` | `message<T>` |
| generated specialization | `template<> struct wire<X>` | `template<> struct message<X>` |
| generated include | `#include "X.wire.hpp"` | `#include "X.message.hpp"` |
| `emitter/body.cpp` emission | `wire<...>`, `.wire.hpp` | `message<...>`, `.message.hpp` |
| method signatures (`encode/decode/byte_size/write_at/tag/name`) | — | unchanged |
| namespace | `zproto` | `zproto` (unchanged) |

## `runtime/zproto.cpp` Provenance Model

The codec block is copied from **master's** `zproto.c` (cbecf36, ~lines 1300–1508):
`packseg`, `packff`, `pack`, `unpackseg`, `unpack`. Differences from the C source,
kept minimal and documented in a header comment:

- All helpers + drivers are `static` (file-local; not exported).
- Drivers renamed `pack`→`pack_impl`, `unpack`→`unpack_impl` to avoid clashing with
  the public `zproto::pack`/`zproto::unpack` (same name, different signature).
- Error codes: C `ZPROTO_OOM`→`zproto::errc::oom`, `ZPROTO_ERROR`→`zproto::errc::error`
  (returned as `static_cast<int>(errc::…)`), so the runtime includes no `zproto.h`.

`packseg`/`packff`/`unpackseg` keep their C names (no clash; static). The public
entry points are thin Buffer-sizing wrappers:

```cpp
namespace zproto {
int pack(const uint8_t *src, int srcsz, Buffer &dst) {
    // master's bound — pack does NOT pad, so no sz8 round-up
    int needn = ((srcsz + 2047) / 2048) * 2 + srcsz + 1;
    dst.resize((size_t)needn);
    int n = pack_impl(src, srcsz, (uint8_t *)dst.data());
    dst.set_size((size_t)n);
    return n;
}
int unpack(const uint8_t *src, int srcsz, Buffer &dst) {
    // ensure bound (caller's set_max or 8x loose), delegate, set actual size
}
}
```

Header comment to add at the top of the codec block:

```
// Zero-suppression codec: near-verbatim copy of zproto.c (master cbecf36):
// packseg/packff/pack/unpackseg/unpack. Static + renamed drivers
// (pack_impl/unpack_impl) to avoid clashing with zproto::pack/zproto::unpack.
// Error codes mapped to zproto::errc. Re-sync from zproto.c by copy, then
// re-apply the rename + errc mapping.
```

## Build & Include Model (source-copy)

- **runtime**: 2 files; nothing to build standalone. Consumers compile
  `zproto.cpp` alongside their own sources. The generated `X.message.hpp` does
  `#include "zproto.hpp"`, so consumers add the runtime dir to their include path.
- **emitter/Makefile**: `main.cpp + header.cpp + body.cpp` + `../../zproto.c`
  (→ `zproto_c.o`) → `zproto` generator binary.
- **tests/Makefile** flow:
  1. `$(MAKE) -C ../emitter` → produces the `zproto` generator.
  2. Run generator on `hello.world.zproto` → emits `hello_world.hpp` +
     `hello_world.message.hpp` (in tests/ build dir).
  3. Compile `test.cpp` + `test_codec.cpp` against `../runtime/zproto.cpp` +
     `../../zproto.c` (→ `zproto_c.o`, linked only into the oracle `test_codec`).
  4. Include path: `-I../runtime` (so generated `.message.hpp` resolves `zproto.hpp`).
  5. Run `test` and `test_codec`.

## Test Migration

### test.cpp — functional round-trip
Remove dead `_encode/_pack/_unpack/_tag` calls. Drive the generated code:

```cpp
zproto::Buffer w;   zproto::message<hello::world::packet>::encode(pa, w);
zproto::Buffer p;   zproto::pack((const uint8_t*)w.data(), w.size(), p);
zproto::Buffer up;  up.set_max(1<<20);
zproto::unpack((const uint8_t*)p.data(), p.size(), up);
hello::world::packet pb;
zproto::message<hello::world::packet>::decode(pb, (const uint8_t*)up.data(), up.size());
// assert pa == pb across all scalar/string/vector/map/nested types
```

Covers: all integer widths (signed/unsigned), float, bool, string, `vector<T>`,
`vector<string>`, `unordered_map` (int + float keyed), nested structs. This is the
functional correctness guard for the new encode/decode path.

### test_codec.cpp — byte-compat oracle vs C reference
- `#include "zprotowire.h"` + `using namespace zprotobuf` →
  `#include "zproto.hpp"` + `using namespace zproto`.
- **`test_pack` / `test_unpack`**: change `std::string out` → `zproto::Buffer out`.
  Keep the byte-for-byte comparison against `zproto_pack` / `zproto_unpack` (the
  most important regression net after the `pack()` rewrite).
- **`test_encoder` / `test_decoder`**: the old generic `encoder` class
  (`e.present()/w_bytes()/w_u32()/finish()`) no longer exists. Rewrite to drive the
  **generated** `message<hello::world::*>` encode/decode and compare against
  `zproto_encode` on the same schema, reusing the existing C callback (`enc_cb`)
  pattern that already populates a C-side mirror of the data.

### tests/Makefile
See Build model above.

## `.gitignore` & Cleanup

- **Remove** the mistakenly-committed binary `cppbind/test_codec` (847 KB).
- **Update `.gitignore`**: replace stale `cppbind/*` rules with the new `cpp/`
  paths — generated `cpp/tests/*.hpp`, `cpp/tests/*.message.hpp`, the
  `cpp/emitter/zproto` binary, `cpp/tests/test`, `cpp/tests/test_codec`. Remove
  the now-dead `!cppbind/zprotowire.hpp` exception.
- Fix `main.cpp` `delete[]` (emitter section above).

## Out of Scope

- `bench/cpp` (redone later with Rust/Lua cross-comparison; committed last).
- `csbind/`, `luabind/` (sibling bindings, untouched).
- Any change to the C reference (`zproto.c` / `zproto.h`).

## Notes

**Note A — pack does not pad (master base).** On master, `packff` emits exactly
`sn` bytes for a 6/7/8-nonzero segment (`packsz = sn`); the packed **wire bytes**
are not padded to 8. Accordingly `zproto::pack`'s bound uses `srcsz` directly —
there is no `sz8` round-up (the line the user flagged is correctly absent under
this base). `unpack` still yields 8-aligned **output**: for a run it copies
`min(run, sn)` then `memset`s the remaining `run - copy` bytes to zero, so decoded
data is always a whole number of 8-byte segments. (The earlier perf-pack variant
padded the packed bytes and bulk-`memcpy`'d runs; that design is discarded in
favor of master's.)

**Note B — schema naming hazard (pre-existing, not fixed here).** When a struct
has a field sharing a name with a nested type (the `hello.world` sample has
`packet::phone` as both a nested struct and a `std::unordered_map` member), users
cannot name the nested type as `packet::phone` (resolves to the member); they must
use `decltype`. This is a schema/generator quirk surfaced during exploration; a
generator guard could be added later but is out of scope.
