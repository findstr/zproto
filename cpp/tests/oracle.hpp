// Byte-compat oracle shim: bridges the C reference (zproto.h) to the C++ test
// types (hello::world::packet) so test_codec.cpp can compare the C++ runtime
// against the C functions without including zproto.h itself.
//
// Why a separate TU: the C++ runtime header zproto.hpp defines `namespace zproto`,
// while the C reference zproto.h declares an opaque `struct zproto` at global
// scope. C++ forbids a tag and a namespace sharing a name in `::`, so the two
// headers cannot be included together. This TU includes zproto.h (and the POD
// header hello_world.hpp, which only defines namespace hello::world -- no
// collision) and exposes the C oracle behind a clean interface.
#pragma once
#include <cstdint>
#include <string>

namespace hello { namespace world { struct packet; } }

// raw zero-suppression codec (C reference: zproto_pack / zproto_unpack).
// Return value mirrors the C functions: byte count, or negative on error.
int oracle_pack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz);
int oracle_unpack(const uint8_t *src, int srcsz, uint8_t *dst, int dstsz);

// struct encode via the C reference: parse `schema_text` with zproto_parse,
// query `packet`, and zproto_encode it using a callback that reads the fields
// of `pa`. On success returns the encoded byte count (>0) and writes the
// resulting bytes into `out` (caller-allocated, capacity `cap`); on failure
// returns a negative value. The map iteration order of pa.phone / pa.phone2 is
// observed identically by this C-side pass and by the C++ message<T>::encode,
// because both iterate the same unordered_map instance, so the byte output is
// deterministic and matches byte-for-byte.
int oracle_encode_packet(const hello::world::packet &pa, const char *schema_text,
                         uint8_t *out, int cap);
