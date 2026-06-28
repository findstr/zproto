# zproto Benchmark & Conformance Suite — Design

Date: 2026-06-27
Status: Draft (pending user review)

## Goal

Replace the toy `hello.world.zproto` with a representative, multi-purpose suite that simultaneously serves four goals, all driven by **one shared schema**:

1. **Coverage breadth** — exercise every zproto field type, container, and codec path.
2. **Performance** — measure marshal/unmarshal throughput across the C++, Lua, and C# bindings.
3. **Cross-binding interop** — prove the three bindings produce/consume identical wire bytes (message level) and identical pack/unpack output (codec level).
4. **protobuf comparison** — compare zproto serialized **size** and **CPU** against C++ libprotobuf, as the single pb baseline.

### The zproto pack/no-pack dimension (core design point)

zproto serialization has two layers:

- **encode/decode** — the wire framing (`[u32 datasize][u16 tagcount][u16 deltas...][body]`), driven by generated code. Always applied.
- **pack/unpack** — an **optional** zero-suppression pass over the encoded bytes (`pack()`/`unpack()` in the codec).

Therefore "marshal" has two zproto variants that must both be measured:

| variant | size | marshal cost | unmarshal cost |
|---|---|---|---|
| nopack | encoded wire bytes | encode | decode |
| pack | encoded then `pack()`-ed bytes | encode + pack | unpack + decode |

pb has one variant only. The size comparison is `zproto-nopack` vs `zproto-pack` vs `pb`; the CPU comparison adds the pack pass as a separate throughput column.

## Scope / YAGNI

**In scope:**
- One zproto schema (realistic message family + one kitchen-sink coverage message).
- A proto3 schema hand-written to match (for the C++ pb baseline only).
- Codec-level pack/unpack conformance test (raw bytes, special patterns).
- Message-level end-to-end cross-binding decode test (value equality — not byte-exact, since maps are non-canonical).
- Per-language perf drivers (C++/Lua/C#) + one C++ pb driver, unified CSV output + an aggregator.
- pb baseline is **C++ libprotobuf only**. No pb in Lua/C#.

**Out of scope:**
- protobuf bindings for Lua/C#.
- Compression beyond zproto's built-in zero-suppression (no zlib/lz4).
- Network/transport benchmarks (in-process only).
- Automated CI integration (manual run for now).

## §1 — Schema

### Realistic message family (game networking, size-tiered)

All field values are **hardcoded identically** in every language driver so all bindings marshal the exact same bytes.

| message | tag | ~size | fields | exercises |
|---|---|---|---|---|
| `heartbeat` | 0x01 | ~12B | `seq:uinteger`, `ack:uinteger` | minimal message, hot-path latency floor |
| `frame` | 0x10 | ~30B | `eid:ulong`, `x/y/z/yaw:float`, `seq:uinteger` | float+int dense, highest-frequency path |
| `login` | 0x20 | ~50B | `userid:ulong`, `token:string`, `version:uinteger`, `platform:ubyte` | scalar + string mix |
| `chat` | 0x30 | ~300B | `from/to:ulong`, `text:string`, `ts:ulong`, `attrs:attr[]` (`attr{key:string,val:string}`) | string + nested struct array |
| `snapshot` | 0x40 | ~10KB | `userid:ulong`, `pos:vec3`, `hp/mp:integer`, `inventory:item[]`, `buffs:integer[]`, `flags:boolean[]`, `friends:playerlevel[userid]`, `level:uinteger` | nested structs, multiple arrays, int-key map |

Where:
- `vec3 { x:float, y:float, z:float }`
- `item { id:uinteger, count:uinteger }`
- `playerlevel { userid:ulong, level:uinteger }` (map value; key = `userid`)

### Kitchen-sink coverage message `alltypes` (tag 0xFF)

One struct that hits **every** type/container. Its instance data is **fully specified with fixed array lengths and map entry counts** (identical across all bindings), so it is measured with the same data as the family and joins the horizontal perf/size comparison:

- scalars: `boolean`, `byte/ubyte`, `short/ushort`, `integer/uinteger`, `long/ulong`, `float`
- `string`, `blob`
- arrays of each: `integer[]`, `string[]`, `blob[]`, `boolean[]`
- nested struct + array: `nest{x:integer,y:integer}`, `n:nest`, `na:nest[]`
- maps: `m_int` keyed by integer, `m_float` keyed by float
- instance data carries boundary values per width: `0`, `-1`, `INTx_MIN`, `INTx_MAX`, `UINTx_MAX`

### Element counts & data shape (methodology)

Element counts are chosen deliberately, not arbitrarily:

1. **Hit the size tiers** — counts sized so each realistic message lands in its target tier (heartbeat ~12B … snapshot ~10KB), giving a clean small→large scaling view across the family.
2. **Zero-heavy where realistic** — game data is naturally sparse in places (inactive buff slots = 0, padding, zero-valued stat components). Those fields make `pack`'s zero-suppression visibly pay off; without them the pack column looks useless. Concretely `snapshot.buffs` is ~50% zeros; other arrays use representative nonzero values.
3. **Edge cases** — at least one empty container (count 0) and one singleton (count 1) appear in the suite (in `alltypes`) to cover the empty / single-element encode+decode paths.
4. **Big enough to surface array effects** — scalar arrays sized so the pb `packed=true` repeated-field advantage shows (zproto writes each scalar element raw; pb packs scalar repeated fields more tightly). Expected: pb ≤ zproto-nopack on large scalar arrays; whether zproto-pack beats pb depends on zero density.
5. **Identical across bindings** — every count and value hardcoded the same in C++/Lua/C#/pb.

Confirmed counts (instance data):
- `chat.attrs`: 4
- `snapshot.inventory`: 50 items · `snapshot.buffs`: 32 (≈½ zero) · `snapshot.flags`: 64 · `snapshot.friends`: 100
- `alltypes`: each scalar/string/blob array = 64; each struct array = 32; each map = 50; plus one array at 0 (empty) and one at 1 (singleton) for edge coverage

**Map determinism note:** zproto serializes a map by iterating the host language's hash map, whose order is **not canonical** across C++/Lua/C#. Therefore interop is verified by **end-to-end cross-decode** (§4: decode each other's bytes, compare values) rather than byte-for-byte matching. Size & perf are unaffected (total bytes are order-independent).

### proto3 equivalent

Hand-written `*.proto` (proto3) matching the family + `alltypes`, used **only** to build the C++ pb baseline. Mapping notes:
- zproto `struct` → proto3 `message`.
- zproto arrays `T[]` → `repeated T`.
- zproto map `valuestruct[keyfield]` → proto3 `map<keytype, ValueMsg>` where the key field is lifted out of the value message into the map key (size-comparable; minor semantic shift noted).

## §2 — Codec pack/unpack conformance test (byte-stream exact)

Raw-byte level, **no schema, no message encode/decode**. This is **byte-stream verification, not end-to-end**: `pack` and `unpack` outputs are compared as exact byte strings against the reference. (Contrast §4, which is end-to-end value interop for whole messages and tolerates map-order differences — §2 tolerates none.)

The codec is **block-based (8-byte groups, no tail-length field)**: `unpack` always zero-pads its output to a multiple of 8 (verified against the C reference — a 1-byte input packs then unpacks to 8 bytes). So the two assertions are:

1. `pack(input) == golden_packed` — packed output bytes must equal the reference **byte-for-byte**, in every binding. (This is the core cross-binding consistency check.)
2. `unpack(golden_packed)` recovers `input` as a **prefix**, zero-padded to the next multiple of 8: output length == `ceil(len(input)/8)*8`, the first `len(input)` bytes equal `input`, and the trailing bytes are all zero. (Byte-exact `unpack == input` only holds for multiple-of-8 inputs; the prefix check is the real contract.)

Vectors include the block-boundary cases: `ff×14` (one full group + a 6-byte all-nonzero tail — exercises partial-tail handling inside a run), plus `ff×7/9/15/17`, `00×1`, run-counter 2040/2048, etc.

**Golden vectors** generated once from the C reference (`zproto_pack`/`zproto_unpack`, same oracle as `test_codec`), stored in a shared file read by all three bindings' tests.

**Input patterns** (cover `packseg` / `packff` / full-run / run-counter paths):

| group | pattern | hits |
|---|---|---|
| empty/min | `ff×0`, `00×1` | empty input, single byte |
| all-zero | `00×8`, `00×16`, `00×64` | header `0x00`, max compression |
| all-nonzero | `ff×8`, `ff×16` | full-run + run-counter |
| counter saturate/wrap | `ff×2040` (255×8), `ff×2048` (256×8) | counter 255 / restart-new-segment |
| sparse nonzero | `01×8`, `01×16` | packseg 1–5 nonzero path |
| run in middle | `00×8 · ff×16 · 00×8` | enter + exit run |
| run tail count=6/7/8 | `ff×8 · [ff×6 00×2]`, `[ff×7 00×1]`, `ff×8` | packff ≥6 threshold, stays in run |
| run break count=5 | `ff×8 · [ff×5 00×3]` | <6 → break, group re-fed to packseg |
| tail partial (all-ff, non-multiple-of-8) | `ff×7`, `ff×9`, `ff×15`, `ff×17` | tail segment: `ff×9`/`ff×17` break (1<6, leaves a `0x00` counter); `ff×15` joins run (7≥6) |
| segment boundary lengths | lengths 7/8/9/15/17 with mixed content | partial-segment handling |
| raw fidelity | run containing distinct nonzeros `01 02 … ff` | raw bytes preserved verbatim (not just `0xFF`) |
| seeded pseudo-random | 256, 4096 bytes (fixed seed) | comprehensive path coverage, reproducible |

**Cross-binding consistency is implied**: all three bindings assert against the same golden vectors, so agreement with the reference == agreement with each other.

## §3 — Performance harness

**Per-language native drivers** (`bench.cpp` / `bench.lua` / `bench.cs`) plus one C++ pb driver (`bench_pb.cpp`). All run in-process on the same machine/flags, each emits unified CSV:

```
lang,message,op,mode,size_bytes,ops_per_sec
```

`op ∈ {encode, decode, encode_pack, unpack_decode, marshal, unmarshal}`, `mode ∈ {zproto_nopack, zproto_pack, pb}`.

**Matrix per message (family + `alltypes`, all with identical hardcoded data):**

| | size | throughput |
|---|---|---|
| zproto nopack | encoded bytes | encode / decode |
| zproto pack | encoded+packed bytes | encode+pack / unpack+decode |
| pb (C++ libprotobuf) | marshalled bytes | marshal / unmarshal |

Reported comparisons:
- **size**: `zproto-nopack` vs `zproto-pack` vs `pb` (zproto size is wire-format-defined, identical across bindings → reported once).
- **throughput**: `zproto-C++` / `zproto-Lua` / `zproto-C#` each vs `pb-C++`, per op.

**Fairness rules:**
- Identical payload: field values hardcoded the same in every driver (no RNG → no cross-language data divergence).
- Hot loop: warmup, then run until ≥2 s elapsed or a minimum iteration count; exclude setup/construction.
- Pre-allocate output buffers (avoid allocator noise; consistent with the `reserve()` work).
- Report env: CPU, compiler, flags, iterations.
- Aggregator (small shell/python script) collates CSVs into one markdown comparison table.

**pb baseline**: C++ libprotobuf only. C# and Lua zproto results are each compared against this single C++ pb baseline (size is format-level and language-independent; perf intentionally compares each language's zproto against C++ pb).

## §4 — Message-level end-to-end interop

Goal: every binding can **decode what every other binding encoded**, and the decoded field values equal the canonical instance. This is stronger and more meaningful than byte-for-byte matching — and it's the only thing that works for maps, whose wire order is non-canonical across languages (see §1 "Map determinism note").

Canonical instance: one hardcoded instance per message (same across bindings), using the element counts from §1.

Mechanism — shared wire files (the bindings are separate runtimes, so they exchange bytes on disk):
1. **Produce** — each binding encodes the canonical instance for each message → writes `<lang>_<msg>.bin` (nopack) and `<lang>_<msg>.pack.bin` (packed). e.g. `cpp_snapshot.bin`, `lua_snapshot.bin`, `cs_snapshot.bin`.
2. **Verify** — each binding decodes **every** produced `.bin` (including the other langs') → asserts decoded field values == canonical instance, field-by-field. Map fields compared by key/value contents, ignoring entry order.
3. **Pass** = all produces succeed + all verifies match.

This directly tests cross-binding decode (not just same-reference round-trip). Byte-for-byte equivalence vs the C reference stays as an optional drift guard for non-map messages (the §3 size sanity check already covers the wire-format drift angle).

## Components & file layout

```
bench/                                   (new, top-level)
  schema/
    bench.zproto                         shared zproto schema (family + alltypes)
    bench.proto                          hand-written proto3 equivalent
  vectors/
    codec_golden.{json,txt}              §2 raw-byte pack/unpack golden vectors
    msg_wire/                            §4 produced <lang>_<msg>[.pack].bin files for cross-decode
  cpp/
    bench_codec.cpp                      §2 conformance (C++)
    bench_interop.cpp                    §4 message interop (C++)
    bench.cpp                            §3 perf driver (C++ zproto)
    bench_pb.cpp                         §3 C++ libprotobuf baseline
  lua/
    bench_codec.lua                      §2 (Lua)
    bench_interop.lua                    §4 (Lua)
    bench.lua                            §3 (Lua zproto)
  cs/
    BenchCodec.cs                        §2 (C#)
    BenchInterop.cs                      §4 (C#)
    Bench.cs                             §3 (C# zproto)
  aggregate.{sh,py}                      collate CSVs → comparison table
```

Bindings consume the shared `bench.zproto` via their existing generators (`cppbind/`, `luabind/`, `csbind/` generators) so the schema is the single source of truth.

## Testing / verification

- **§2 pass**: each binding's `bench_codec` exits 0 — every golden vector's `pack` output matches **byte-for-byte**, and `unpack` output satisfies the prefix-check (input prefix + zero-pad to a multiple of 8). Run on all three bindings.
- **§4 pass**: each binding's `bench_interop` exits 0 — every binding decodes every other binding's produced `.bin` files and the decoded values match the canonical instance. Run on all three bindings.
- **§3 sanity**: the C++ driver's zproto-nopack size matches the C reference `zproto_encode` output for the same instance (regression guard against wire-format drift); pb size is whatever libprotobuf produces.
- perf numbers are informational (not pass/fail); recorded to the comparison table for manual reading.

## Open questions / decisions captured

- pb baseline = C++ libprotobuf only (decided).
- payload = hardcoded identical values across drivers (decided; revisit if a message needs runtime-varied data).
- C# pb lib: N/A — only C++ pb is used.
- `alltypes` **is** included in perf/size, with the same hardcoded instance as the family (fixed array/map sizes), for horizontal comparison.
