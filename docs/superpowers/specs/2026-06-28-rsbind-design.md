# rsbind Rust Binding Design

Date: 2026-06-28
Status: Draft (pending user review)

## Goal

Add a Rust binding generator for zproto with the same broad role as
`cppbind`, while keeping parser ownership centralized in `zproto.c`.

The first version of `rsbind` generates Rust code from `.zproto` schemas.
The generated Rust runtime path is pure Rust and does not link to `zproto.c`.
The C parser is used only by the generator executable.

## Core Principles

- Keep `.zproto` parsing centralized in `zproto.c`.
- Do not add a Rust parser in the first version.
- Do not use runtime FFI from generated Rust code into `zproto.c`.
- Generate owned Rust structs; do not generate lifetime parameters.
- Do not generate `Box`, `Rc`, `Arc`, or reference-counted structures.
- Use standard library types only in generated code.
- Reject schema shapes that do not map cleanly to Rust value types.
- Keep the wire format byte-compatible with existing zproto encode/decode and
  pack/unpack behavior.

## Architecture

`rsbind` has two parts:

1. `rsbind/zproto`
   - A generator executable.
   - Implemented in the existing generator style, using C++ and linking
     `../zproto.c`.
   - Reads a `.zproto` file, uses the C parser AST, validates Rust-specific
     constraints, then writes a generated `.rs` file.

2. `rsbind/src/zproto.rs`
   - A pure Rust runtime module.
   - Provides the `Message` trait, `Error` type, wire encoder/decoder, and
     `pack`/`unpack`.
   - Uses safe Rust.
   - Uses `Vec`, slices, cursor indexes, and `to_le_bytes`/`from_le_bytes`.

Generated schema files reference the runtime as:

```rust
crate::zproto
```

The user project layout is expected to look like:

```rust
mod zproto;
mod hello_world;
```

The generated code must not depend on C, third-party crates, build scripts, or
runtime schema reflection.

## Generator CLI

The first version keeps a `cppbind`-like CLI shape:

```sh
rsbind/zproto path/to/hello.world.zproto
```

It writes the generated file using the schema basename with dots converted to
underscores:

```text
hello.world.zproto -> hello_world.rs
hello_world.zproto -> hello_world.rs
```

The generated Rust namespace still preserves the filename namespace.

## Namespace Mapping

Filename segments map to nested Rust modules:

```text
hello.world.zproto -> pub mod hello { pub mod world { ... } }
hello_world.zproto -> pub mod hello_world { ... }
```

If a module segment is a Rust keyword, it is emitted as a raw identifier:

```rust
pub mod r#type { ... }
```

## Schema Restrictions

Rust binding generation rejects schemas that do not map cleanly to owned Rust
value types.

### Nested Structs

Nested zproto struct definitions are not supported.

If a `zproto_struct` has children, the generator exits with an error. The
generator does not simulate nested structs with Rust modules and does not
flatten names.

Rust-compatible schemas must define all referenced structs at top level.

### Map Keys

Map fields generate:

```rust
std::collections::HashMap<K, V>
```

The first version supports only stable, hashable key types that Rust can store
in `HashMap` without wrappers.

Unsupported map key types:

- `float`
- `blob`
- `struct`

When these are used as map keys, the generator exits with an error. This
validation can later move into `zproto.c`, but the first version performs it in
`rsbind`.

Recursive struct constraints are assumed to remain enforced by the existing
parser. `rsbind` does not add pointer indirection to represent recursive
schemas.

## Type Mapping

Scalar and container mapping:

| zproto type | Rust type |
|---|---|
| `boolean` | `bool` |
| `byte` | `i8` |
| `ubyte` | `u8` |
| `short` | `i16` |
| `ushort` | `u16` |
| `integer` | `i32` |
| `uinteger` | `u32` |
| `long` | `i64` |
| `ulong` | `u64` |
| `float` | `f32` |
| `string` | `String` |
| `blob` | `Vec<u8>` |
| `T[]` | `Vec<T>` |
| `Struct[key]` | `HashMap<K, Struct>` |

All generated structs own their data. Decoding copies byte/string payloads into
`Vec<u8>` and `String`, so decoded values do not borrow from the input buffer.

## Rust Naming

Struct names are converted to `UpperCamelCase` using underscores as word
boundaries:

```text
packet -> Packet
player_level -> PlayerLevel
playerlevel -> Playerlevel
```

Field names preserve the schema spelling whenever possible. If a field name is
a Rust keyword, the generator emits a raw identifier:

```text
.from -> pub r#from: u64
.type -> pub r#type: String
```

Raw identifiers are used only when required.

Generated structs derive:

```rust
#[derive(Debug, Clone, Default, PartialEq)]
```

## Public Runtime API

The runtime module is named `zproto.rs`.

It exposes a `Message` trait:

```rust
pub trait Message: Sized + Default {
    const TAG: i32;
    const NAME: &'static str;

    fn reset(&mut self);

    fn encode(&self) -> Result<Vec<u8>, Error> {
        let mut out = Vec::new();
        self.encode_to(&mut out)?;
        Ok(out)
    }

    fn encode_to(&self, out: &mut Vec<u8>) -> Result<usize, Error>;

    fn decode(data: &[u8]) -> Result<Self, Error> {
        let mut value = Self::default();
        value.decode_from(data)?;
        Ok(value)
    }

    fn decode_from(&mut self, data: &[u8]) -> Result<usize, Error>;
}
```

Generated structs implement `Message`.

`encode()` and `decode()` are convenience APIs. `encode_to()` and
`decode_from()` are reuse-oriented APIs.

`encode_to()` appends to the provided buffer and does not call `clear()`. This
matches Rust's common buffer-writing style and allows callers to either reuse a
buffer or concatenate multiple messages.

Typical encode reuse:

```rust
let mut buf = Vec::with_capacity(1024);

buf.clear();
packet.encode_to(&mut buf)?;
send(&buf);
```

`decode_from()` resets the object before reading new data:

```rust
fn decode_from(&mut self, data: &[u8]) -> Result<usize, crate::zproto::Error> {
    self.reset();
    // generated field decode follows
}
```

This avoids stale data when reusing an existing struct value.

`reset()` is generated as:

```rust
*self = Self::default();
```

## Generated Code Shape

Example shape:

```rust
pub mod hello {
    pub mod world {
        use crate::zproto::{self, Message};
        use std::collections::HashMap;

        #[derive(Debug, Clone, Default, PartialEq)]
        pub struct PlayerLevel {
            pub userid: u64,
            pub level: u32,
        }

        impl Message for PlayerLevel {
            const TAG: i32 = 0x10;
            const NAME: &'static str = "playerlevel";

            fn reset(&mut self) {
                *self = Self::default();
            }

            fn encode_to(&self, out: &mut Vec<u8>) -> Result<usize, zproto::Error> {
                // generated encoder calls
                Ok(0)
            }

            fn decode_from(&mut self, data: &[u8]) -> Result<usize, zproto::Error> {
                self.reset();
                // generated decoder calls
                Ok(data.len())
            }
        }

        #[derive(Debug, Clone, Default, PartialEq)]
        pub struct Packet {
            pub friends: HashMap<u64, PlayerLevel>,
        }
    }
}
```

For map decode, the decoded value is moved into the `HashMap`; it is not cloned:

```rust
let mut value = PlayerLevel::default();
value.decode_from(bytes)?;
let key = value.userid;
self.friends.insert(key, value);
```

If the key field is a non-`Copy` supported type, such as `String`, the key must
be cloned because the map stores an owned key and the value also retains the key
field.

## Wire Runtime

The Rust runtime mirrors the existing zproto wire format, not the C++ runtime
implementation style.

Encoding flow:

1. `Encoder::new(out, basetag, fieldcount)` reserves:

   ```text
   [u32 datasize][u16 tagcount][u16 delta ...][body]
   ```

2. Generated code emits fields in schema/tag order:

   - `present(tag)`
   - scalar values as little-endian bytes
   - `f32` using `to_bits().to_le_bytes()`
   - `string`/`blob` as `[u32 len][bytes]`
   - arrays/maps as `[u32 count]` followed by elements
   - structs by recursively calling `encode_to`

3. `finish()` patches `datasize` and `tagcount`, compacts unused tag slots,
   and returns the byte count written for this message.

Decoding flow:

1. `Decoder::new(data, basetag)` validates the top-level message header and
   establishes cursor bounds.
2. Generated code loops over decoded tags and `match`es each known field tag.
3. Scalar/string/blob/array/map/struct values are read from the decoder with
   checked `Result`-returning methods.
4. Unknown tags stop decoding and return the consumed byte count, matching the
   forward-compatible behavior used by `cppbind`.

The runtime uses safe Rust only:

- no raw pointer arithmetic
- no `unsafe`
- no `memmove`
- no `ok` flag state machine

Cursor reads return `Result<T, Error>`.

## Pack And Unpack

The runtime exposes both convenience APIs and caller-buffer reuse APIs:

```rust
pub fn pack(src: &[u8]) -> Vec<u8>;
pub fn pack_to(src: &[u8], out: &mut Vec<u8>) -> usize;

pub fn unpack(src: &[u8]) -> Result<Vec<u8>, Error>;
pub fn unpack_to(src: &[u8], out: &mut Vec<u8>) -> Result<usize, Error>;
```

`pack()` and `unpack()` allocate and return a new `Vec<u8>`.

`pack_to()` and `unpack_to()` append to the provided output buffer and do not
call `clear()`. This matches `Message::encode_to()` and lets callers reuse
buffers or concatenate multiple byte streams.

`pack_to()` returns the number of bytes written by this call.

`unpack_to()` returns the number of bytes written by this call. If unpacking
fails, it truncates `out` back to its original length before returning the
error, so callers do not observe partial output from malformed input.

The Rust implementation uses slices and `Vec`, but the byte output must match
the C reference exactly.

Pack/unpack byte compatibility is validated with golden vectors produced from
`zproto_pack` and `zproto_unpack`.

## Error Model

Runtime errors are represented as a Rust enum:

```rust
pub enum Error {
    Eof,
    Malformed,
    Utf8,
    Unsupported(&'static str),
}
```

Expected usage:

- `Eof`: input ends before a required fixed-width value or header.
- `Malformed`: length fields, tag data, struct boundaries, or packed streams
  are invalid.
- `Utf8`: a zproto `string` field is not valid UTF-8.
- `Unsupported`: reserved for runtime-level unsupported operations. Most schema
  incompatibilities should fail in the generator before code is emitted.

The public Rust API does not expose C integer error codes.

## Tests And Acceptance

Runtime tests:

- `pack` output equals C reference golden vectors byte-for-byte.
- `unpack` output equals C reference golden vectors byte-for-byte.
- `pack_to()` and `unpack_to()` support caller-owned buffer reuse.
- `unpack_to()` rolls back partial output on malformed input.
- `Encoder`/`Decoder` focused tests cover scalars, strings, blobs, arrays,
  structs, and message headers.
- malformed input tests cover short headers, out-of-bounds lengths, invalid
  packed streams, and invalid UTF-8.

Generated-code tests:

- A Rust-compatible schema with only top-level structs is generated and compiled.
- Construct value -> `encode()` -> `decode()` -> `PartialEq` succeeds.
- `encode_to()` supports caller-owned buffer reuse.
- `decode_from()` resets previous state before decoding new bytes.
- keyword fields such as `.from` and `.type` generate raw identifiers and
  compile.

Generator validation tests:

- nested struct definitions fail generation.
- float map keys fail generation.
- blob map keys fail generation.
- struct map keys fail generation.

Acceptance criteria:

- `cargo test` under `rsbind/` passes.
- generated Rust code depends only on `std` and `crate::zproto`.
- generated Rust runtime does not link to `zproto.c`.
- pack/unpack are byte-compatible with the C reference.
- existing `cppbind`, `csbind`, and `luabind` behavior is not changed.
