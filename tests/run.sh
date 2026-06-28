#!/usr/bin/env bash
#
# tests/run.sh - cross-language interop orchestrator.
#
# Pipeline:
#   1. compile & run C reference generator -> vectors_pack.txt
#   2. run Lua generator (C engine) -> vectors_codec.txt
#   3. generate Rust code from compat.zproto for compat_codec test
#   4. for each enabled language runner, run pack + codec tests
#
# Usage:
#   tests/run.sh                  # run all enabled languages
#   tests/run.sh rust             # run only rust
#   tests/run.sh lua              # run only lua
#
# Environment:
#   CC              C compiler (default: cc)
#   KEEP_VECTORS    if set, don't regenerate vectors (reuse existing)

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PACK_VECTORS="${ZPROTO_PACK_VECTORS:-$SCRIPT_DIR/vectors_pack.txt}"
CODEC_VECTORS="${ZPROTO_CODEC_VECTORS:-$SCRIPT_DIR/vectors_codec.txt}"
CC="${CC:-cc}"

# --- colours --------------------------------------------------------------
if [ -t 1 ]; then
    GREEN=$'\033[32m'; RED=$'\033[31m'; YELLOW=$'\033[33m'; DIM=$'\033[2m'; RESET=$'\033[0m'
else
    GREEN=""; RED=""; YELLOW=""; DIM=""; RESET=""
fi

pass() { echo "${GREEN}PASS${RESET} $1"; }
fail() { echo "${RED}FAIL${RESET} $1"; FAILURES=$((FAILURES+1)); }
info() { echo "${DIM}$1${RESET}"; }

FAILURES=0

# --- step 1: generate pack/unpack vectors from C reference ----------------
gen_pack_vectors() {
    if [ -n "${KEEP_VECTORS:-}" ] && [ -f "$PACK_VECTORS" ]; then
        info "reusing existing pack vectors: $PACK_VECTORS"
        return 0
    fi

    info "compiling C reference generator..."
    local bin="$SCRIPT_DIR/c/gen_vectors"
    if ! "$CC" -O2 -Wall -I"$REPO_ROOT" -o "$bin" \
            "$SCRIPT_DIR/c/gen_vectors.c" "$REPO_ROOT/zproto.c"; then
        fail "compile C reference generator"
        return 1
    fi

    info "generating pack vectors -> $PACK_VECTORS"
    if ! "$bin" > "$PACK_VECTORS"; then
        fail "C reference generator aborted"
        rm -f "$PACK_VECTORS"
        return 1
    fi
    pass "C reference: $(grep -c '^[0-9a-f]' "$PACK_VECTORS") pack vectors generated"
}

# --- step 2: generate encode/decode vectors from Lua (C engine) -----------
gen_codec_vectors() {
    if [ -n "${KEEP_VECTORS:-}" ] && [ -f "$CODEC_VECTORS" ]; then
        info "reusing existing codec vectors: $CODEC_VECTORS"
        return 0
    fi

    if [ ! -x "$REPO_ROOT/luabind/test" ] || [ ! -f "$REPO_ROOT/luabind/zproto.so" ]; then
        info "building Lua test binary and zproto.so..."
        if ! make -C "$REPO_ROOT/luabind" linux; then
            fail "build luabind (make -C luabind linux)"
            return 1
        fi
    fi

    info "generating codec vectors -> $CODEC_VECTORS"
    local lua_bin="$REPO_ROOT/luabind/test"
    export LUA_PATH="$REPO_ROOT/luabind/?.lua;;"
    export LUA_CPATH="$REPO_ROOT/luabind/?.so;;"
    export LD_LIBRARY_PATH="$REPO_ROOT/luabind:${LD_LIBRARY_PATH:-}"
    if ! "$lua_bin" "$SCRIPT_DIR/lua/gen_codec_vectors.lua" > "$CODEC_VECTORS"; then
        fail "Lua codec generator aborted"
        rm -f "$CODEC_VECTORS"
        return 1
    fi
    pass "Lua: $(grep -c '^[0-9a-f]' "$CODEC_VECTORS") codec vectors generated"
}

# --- step 3: generate Rust code from compat.zproto ------------------------
gen_rust_compat() {
    local compat_schema="$SCRIPT_DIR/proto/compat.zproto"
    local out_dir="$REPO_ROOT/rust/runtime/tests/generated"
    local out_file="$out_dir/compat.rs"

    if [ -n "${KEEP_VECTORS:-}" ] && [ -f "$out_file" ]; then
        info "reusing existing generated Rust code: $out_file"
        return 0
    fi

    info "building Rust emitter..."
    if ! make -C "$REPO_ROOT/rust/emitter" zproto-gen-rust 2>/dev/null; then
        fail "build rust emitter"
        return 1
    fi

    mkdir -p "$out_dir"
    info "generating Rust code from compat.zproto -> $out_file"
    if ! "$REPO_ROOT/rust/emitter/zproto-gen-rust" "$compat_schema" --out "$out_dir"; then
        fail "Rust code generation from compat.zproto"
        return 1
    fi
    pass "Rust code generated: $out_file"
}

# --- language runners -----------------------------------------------------

run_rust() {
    local crate_dir="$REPO_ROOT/rust/runtime"
    if ! command -v cargo >/dev/null 2>&1; then
        info "  rust: cargo not found, skipping"
        return 0
    fi

    info "running rust pack/unpack tests..."
    if ZPROTO_VECTORS="$PACK_VECTORS" cargo test --manifest-path "$crate_dir/Cargo.toml" \
            --test golden_vectors -- --nocapture; then
        pass "rust pack/unpack"
    else
        fail "rust pack/unpack"
    fi

    # compat_codec requires generated code from compat.zproto
    if [ ! -f "$crate_dir/tests/generated/compat.rs" ]; then
        info "  rust encode/decode: generated code not found, skipping"
        return 0
    fi

    info "running rust encode/decode tests..."
    if ZPROTO_CODEC_VECTORS="$CODEC_VECTORS" cargo test --manifest-path "$crate_dir/Cargo.toml" \
            --test compat_codec -- --nocapture; then
        pass "rust encode/decode"
    else
        fail "rust encode/decode"
    fi
}

run_cpp() {
    if [ ! -d "$SCRIPT_DIR/cpp" ]; then
        info "  cpp: no runner yet, skipping"
        return 0
    fi
    info "running cpp runner..."
    if "$SCRIPT_DIR/cpp/run.sh"; then
        pass "cpp"
    else
        fail "cpp"
        return 1
    fi
}

run_lua() {
    if [ ! -d "$SCRIPT_DIR/lua" ]; then
        info "  lua: no runner yet, skipping"
        return 0
    fi
    info "running lua pack/unpack tests..."
    if "$SCRIPT_DIR/lua/run.sh" test-pack "$PACK_VECTORS"; then
        pass "lua pack/unpack"
    else
        fail "lua pack/unpack"
    fi

    if [ ! -f "$CODEC_VECTORS" ]; then
        info "  lua encode/decode: codec vectors not found, skipping"
        return 0
    fi

    info "running lua encode/decode tests..."
    if "$SCRIPT_DIR/lua/run.sh" test-codec "$CODEC_VECTORS"; then
        pass "lua encode/decode"
    else
        fail "lua encode/decode"
    fi
}

# --- main -----------------------------------------------------------------
LANGS=("rust" "cpp" "lua")

# filter to requested languages if args given
if [ $# -gt 0 ]; then
    LANGS=("$@")
fi

echo "=== zproto cross-language interop ==="
echo "pack vectors:  $PACK_VECTORS"
echo "codec vectors: $CODEC_VECTORS"
echo "languages:     ${LANGS[*]}"
echo

# Generate vectors
if ! gen_pack_vectors; then
    echo "${RED}cannot proceed without pack vectors${RESET}"
    exit 1
fi

if ! gen_codec_vectors; then
    echo "${YELLOW}warning: codec vectors not generated, encode/decode tests will be skipped${RESET}"
fi

# Generate Rust compat code (needed for rust encode/decode test)
for lang in "${LANGS[@]}"; do
    if [ "$lang" = "rust" ]; then
        gen_rust_compat || true
        break
    fi
done

echo

for lang in "${LANGS[@]}"; do
    case "$lang" in
        rust) run_rust ;;
        cpp)  run_cpp  ;;
        lua)  run_lua  ;;
        *)    fail "unknown language: $lang" ;;
    esac
done

echo
if [ "$FAILURES" -eq 0 ]; then
    echo "${GREEN}all language runners passed${RESET}"
    exit 0
else
    echo "${RED}$FAILURES failure(s)${RESET}"
    exit 1
fi
