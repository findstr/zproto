# PRD: bounded reusable Buffer for the zproto C++ codec

Status: draft for review — Phase 5 of the codec consolidation
(plan: `~/.claude/plans/parsed-questing-swan.md`). The codec itself (pack/unpack
+ marshal + pad-to-8) is done and mirrored C↔C++; this PRD is only about the C++
output-buffer strategy.

## Problem

The C++ runtime (`cppbind/zprotowire.cpp`) allocates its pack/unpack **output
per call**, into a `std::string`:

- `unpack(src, sz, std::string&)`: internally `malloc(srcsz*8+8)` — the worst-case
  8× expansion bound — every call, then `assign` into the string.
- `pack(src, sz, std::string&)`: internally `malloc(needn)` every call.

Two issues:

1. **Memory-amplification DoS.** An attacker's small packed message made of
   all-zero header bytes is *valid* (a message of zero fields) yet forces `unpack`
   to allocate and write **8× its size**. 8:1 expansion is legitimate (the codec
   must honor it), so the defense is a **caller-set output cap** — but the current
   `unpack(...)` API has no cap parameter. The lua binding already avoids this:
   `zproto_unpack(src,srcsz,dst,dstsz)` takes a caller buffer, and `lzproto.c`
   reuses a `funcbuffer`. **The C++ is the outlier.**
2. **Allocation churn** — per-call `malloc`/`free` of buffers sized to the loose
   upper bound.

Secondary: `encode` uses a `Buffer` (std::string-backed) while `pack`/`unpack`
use `std::string&` — inconsistent, and the std::string backing pays a zero-fill
on `resize` that encode then overwrites.

## Goals

1. **Bounded output (lz4 `maxDstSize`).** The caller sets a max unpacked size;
   `unpack` writes at most that and errors on exceed. The per-segment check is
   already in the unpack codec (`if (dn < 8) return OOM`, `run > dn`) — measured
   cost ~3.4%.
2. **Reusable.** The caller creates a `Buffer` once (with their max) and reuses it
   across calls — no per-call `malloc`/`free`.
3. **Unified.** One `Buffer` type for `encode`, `pack`, `unpack`.
4. **No zero-fill.** Raw `malloc` backing (drops the std::string `resize` zero-fill).

## Non-goals

- **Length header** in the packed format — decided against (spoofable; the buffer
  cap is the defense).
- **Thread-local / global buffers** — decided against (caller-owned `Buffer`;
  `wire<T>` stays stateless).
- **Wire-format changes** — pad-to-8 etc. already done in `zproto.c`; C++ mirrors.

## Design

### `Buffer` (`zprotowire.hpp`)

A raw-backed, **auto-growing**, reusable byte buffer with an **optional growth
cap** set by the business logic:

- Auto-grows (realloc) on demand. No cap by default (`SIZE_MAX`).
- The caller may set a growth cap (`max`) — when set, growth stops at `max` and a
  write that would exceed it errors. This is the lz4 `maxDstSize`.
- Raw `malloc`/`realloc` backing (no zero-fill). Movable, non-copyable.
- Methods: `data()`, `cdata()`, `size()`, `clear()`, `set_max(n)` (the optional
  cap), `ensure(n)` (grow to `n` within `max`), `capacity()`.

### API

- `wire<T>::encode(const T&, Buffer&)` — already takes `Buffer`; switch backing to
  raw (drops the zero-fill). Uses a freely-growing Buffer (no cap).
- `pack(const uint8_t *src, int srcsz, Buffer &dst)` — writes packed bytes;
  **`src != dst`** required (the encode→pack flow uses two buffers). Freely-growing.
- `unpack(const uint8_t *src, int srcsz, Buffer &dst)` — the business logic sets
  `dst.set_max(cap)`; the per-segment `dn` check enforces it (returns `<0` on
  exceed). This is the bomb defense.

### State / thread-safety

`wire<T>` stays stateless (existing design). The `Buffer` is caller-owned, not
shared across threads — same model as lzproto.c's `funcbuffer`.

## Caller impact

- `.wire_verify/*.cpp` (~20 files) + `bench/cpp/proto/{proto.cpp,dispatch.hpp}`:
  `pack`/`unpack` call sites pass a `Buffer` and read the result via
  `data()/size()` instead of a `std::string`.
- The `Buffer` method set must cover what callers use today:
  `.size()/.cdata()/.clear()/.data()/.resize()` (→ `.ensure` + `set_size`).

## Acceptance criteria

- `.wire_verify/` goldens pass: pack 24/24, unpack 24/24, wire-equiv,
  decode-roundtrip.
- **ASan-clean** on the bounded unpack path (raw `vector<uint8_t>` input, no NUL
  padding) and on pack (the all-nonzero overflow test, already green).
- **No amplification**: a malicious packed input cannot force `unpack` to allocate
  or write past the caller's `cap`.
- Round-trip perf maintained (snapshot/alltypes still beat pb).
- `pack(src,…,buf)` aliasing (`src == buf`) is either rejected or documented UB.

## Open questions (need your call)

1. ~~encode/pack `Buffer` cap~~ — **resolved**: all callers auto-grow; the cap is
   optional, set by the business logic only where needed (unpack, bomb defense).
2. ~~default `Buffer()` cap~~ — **resolved**: default no cap (`SIZE_MAX`); the
   business logic sets `set_max(...)` where it wants a ceiling.
3. **`pack` aliasing**: just document `src != dst`, or add a debug assert?
   (lean: debug assert — cheap, catches the aliasing footgun.)

## Related refactor (separate step): rename `zprotowire` → `zproto`

Rename `cppbind/zprotowire.{hpp,cpp}` → `cppbind/zproto.{hpp,cpp}` so the C++
runtime is `zproto` (distinct from the C reference `zproto.h`/`zproto.c` at the
repo root — different extension `.hpp` vs `.h`, different dir). Moving parts:

- `body.cpp` (generator) emits `#include "zproto.hpp"` in `*.wire.hpp` →
  change to `#include "zproto.hpp"`, and regenerate the `.wire.hpp` files.
- `cppbind/Makefile`: `zprotowire.o` target → `zproto.o`; update `test`/`libs`/
  `test_codec` link lines.
- The generator itself (`proto.cpp`/`header.cpp`/`body.cpp`) includes the **C**
  `../zproto.h` (for `struct zproto_desc`) — must NOT pull in the new
  `cppbind/zproto.hpp`. Different extensions make `#include "zproto.h"` (C) vs
  `#include "zproto.hpp"` (C++) unambiguous, but the include paths need care.

Appropriate as a cleanup; suggests doing it as its own step (rename first, then
the Buffer lands in `zproto.hpp` — or Buffer first in `zprotowire.hpp`, rename
after).
