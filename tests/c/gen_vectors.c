/*
 * gen_vectors.c - generate golden pack/unpack vectors from the C reference.
 *
 * Output format (stdout), one vector per data line, names as comments:
 *   # <name>
 *   <hex-input> <hex-packed>
 *   ...
 *   # END
 *
 * The unpacked output is always the input padded to a multiple of 8 with
 * zeros, so it is derivable and not stored.
 *
 * Each vector is roundtrip-verified before emission (pack then unpack,
 * compare to input + zero pad). Any mismatch aborts with non-zero exit.
 *
 * Build:  cc -O2 -Wall -I<repo-root> -o gen_vectors gen_vectors.c <repo-root>/zproto.c
 * Run:    ./gen_vectors > vectors_pack.txt
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zproto.h"

static void
print_hex(const uint8_t *buf, int len)
{
    for (int i = 0; i < len; i++)
        printf("%02x", buf[i]);
}

/*
 * Pack input, verify roundtrip (pack -> unpack -> compare), then emit the
 * vector line. Aborts the process on any inconsistency.
 */
static void
emit_case(const char *name, const uint8_t *input, int input_len)
{
    uint8_t packed[8192];
    uint8_t unpacked[8192];

    int packed_len = zproto_pack(input, input_len, packed, sizeof(packed));
    if (packed_len < 0) {
        fprintf(stderr, "FAIL pack [%s]: %d\n", name, packed_len);
        exit(1);
    }

    int unpacked_len = zproto_unpack(packed, packed_len, unpacked, sizeof(unpacked));
    if (unpacked_len < 0) {
        fprintf(stderr, "FAIL unpack [%s]: %d\n", name, unpacked_len);
        exit(1);
    }

    int expected_len = ((input_len + 7) / 8) * 8;
    if (unpacked_len != expected_len) {
        fprintf(stderr, "FAIL len [%s]: unpacked %d, expected %d\n",
                name, unpacked_len, expected_len);
        exit(1);
    }
    if (memcmp(unpacked, input, input_len) != 0) {
        fprintf(stderr, "FAIL data [%s]: unpacked != input\n", name);
        exit(1);
    }
    for (int i = input_len; i < expected_len; i++) {
        if (unpacked[i] != 0) {
            fprintf(stderr, "FAIL pad [%s] at %d: expected 0\n", name, i);
            exit(1);
        }
    }

    printf("# %s\n", name);
    print_hex(input, input_len);
    printf(" ");
    print_hex(packed, packed_len);
    printf("\n");
}

/* ---- vector table -------------------------------------------------- *
 * Organised by category. Each entry is {name, input, len}.
 * Cases are deliberately redundant at boundaries (6/7/8 bytes) because
 * those are exactly where packff's >=6 threshold and the no-pad rule
 * interact, and where cross-language interop historically breaks.
 */

struct case_def {
    const char *name;
    const uint8_t *input;
    int len;
};

#define CASE(name, ...) \
    { name, (const uint8_t[]){__VA_ARGS__}, (int)(sizeof((uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t)) }

static const struct case_def CASES[] = {
    /* --- basic single-segment (8 bytes) --- */
    CASE("all-zero-8",                 0,0,0,0,0,0,0,0),
    CASE("single-nonzero-head",        1,0,0,0,0,0,0,0),
    CASE("single-nonzero-tail",        0,0,0,0,0,0,0,1),
    CASE("sparse-even",                1,0,2,0,3,0,4,0),
    CASE("all-ff-8",                   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff),

    /* --- full-run (0xff header) chains --- */
    CASE("all-ff-16-fullrun",          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff),
    CASE("all-ff-24-fullrun",          0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff),

    /* --- full-run tail: last segment has 6/7/8 bytes (critical interop) --- */
    /* 8 full + 6 full-tail: packff>=6 path, no pad */
    CASE("fullrun-tail-6",             1,2,3,4,5,6,7,8,  9,10,11,12,13,14),
    CASE("fullrun-tail-7",             1,2,3,4,5,6,7,8,  9,10,11,12,13,14,15),
    /* full-run then a sparse segment follows (exits full-run) */
    CASE("fullrun-then-sparse",        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                         0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
                                         1,0),

    /* --- non-multiple-of-8 tails (last segment < 8 bytes) --- */
    CASE("tail-1-byte",                0x42),
    CASE("tail-3-bytes",               1,0,2),
    CASE("tail-5-bytes-all-ff",        0xff,0xff,0xff,0xff,0xff),
    CASE("tail-7-bytes-full",          1,2,3,4,5,6,7),
    /* 8 full + 1-byte tail: smallest full-run-with-tail case */
    CASE("exact-9-fullrun-tail-1",     1,2,3,4,5,6,7,8, 9),

    /* --- mixed patterns --- */
    CASE("mixed-sparse-fullrun-sparse",
        1,0,2,0,3,0,4,0,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0,5,0,6,0,7,0,8),
    CASE("alternating-zero-ff",
        0,0,0,0,0,0,0,0,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0,0,0,0,0,0,0,0,
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff),

    /* --- full-run boundary: 6/7/8 nonzero in packff tail --- */
    CASE("fullrun-then-6-nonzero",
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        1,2,3,4,5,6,0,0),

    /* --- full-run tail: 0~5 bytes after a full 8-byte segment --- */
    CASE("fullrun-tail-0",             1,2,3,4,5,6,7,8),
    CASE("fullrun-tail-1",             1,2,3,4,5,6,7,8, 9),
    CASE("fullrun-tail-2",             1,2,3,4,5,6,7,8, 9,10),
    CASE("fullrun-tail-3",             1,2,3,4,5,6,7,8, 9,10,11),
    CASE("fullrun-tail-4",             1,2,3,4,5,6,7,8, 9,10,11,12),
    CASE("fullrun-tail-5",             1,2,3,4,5,6,7,8, 9,10,11,12,13),

    /* --- FF + zero mixed within a single 8-byte segment --- */
    CASE("ff6-zero2-segment",          0xff,0xff,0xff,0xff,0xff,0xff,0,0),
    CASE("ff5-zero3-segment",          0xff,0xff,0xff,0xff,0xff,0,0,0),

    /* --- full-run then segment with FF+zero mix --- */
    CASE("fullrun-then-ff6-zero2",
        0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
        0xff,0xff,0xff,0xff,0xff,0xff,0,0),

    /* --- all-zero large blocks --- */
    CASE("all-zero-16",                0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0),
};

int
main(void)
{
    size_t n = sizeof(CASES) / sizeof(CASES[0]);
    for (size_t i = 0; i < n; i++) {
        emit_case(CASES[i].name, CASES[i].input, CASES[i].len);
    }

    /* --- 2K boundary: run-length counter at 255*8=2040 and 256*8=2048 --- *
     * These are too large for inline array literals, so generate them.
     * 2040 = 255 * 8: maximum single run before counter wraps to 0
     * 2048 = 256 * 8: counter wraps (255 + reset), tests multi-run packing
     */
    {
        uint8_t buf[4096];
        memset(buf, 0xff, 2040);
        emit_case("all-ff-2040", buf, 2040);
        memset(buf, 0xff, 2048);
        emit_case("all-ff-2048", buf, 2048);
        /* 2047: one byte short of 2K boundary (non-multiple of 8) */
        memset(buf, 0xff, 2047);
        emit_case("all-ff-2047", buf, 2047);
        /* 2041: just past max single run, 1-byte tail */
        memset(buf, 0xff, 2041);
        emit_case("all-ff-2041", buf, 2041);
        /* all-zero 2K */
        memset(buf, 0x00, 2048);
        emit_case("all-zero-2048", buf, 2048);
    }

    printf("# END\n");
    return 0;
}
