#!/usr/bin/env bash
#
# tests/lua/run.sh - Lua test orchestrator.
#
# Subcommands:
#   gen-codec     Generate encode/decode golden vectors -> stdout
#   test-pack <vectors_pack.txt>    Test pack/unpack against golden vectors
#   test-codec <vectors_codec.txt>  Test encode/decode against golden vectors
#   all <pack_vectors> <codec_vectors>  Run all tests
#
# Requires: luabind/test binary and zproto.so built (make -C luabind linux)

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LUA_BIN="$REPO_ROOT/luabind/test"

if [ ! -x "$LUA_BIN" ]; then
    echo "ERROR: $LUA_BIN not found. Run 'make -C $REPO_ROOT/luabind linux' first." >&2
    exit 1
fi

# Lua needs to find zproto.so and other .so modules
export LUA_PATH="$REPO_ROOT/luabind/?.lua;;"
export LUA_CPATH="$REPO_ROOT/luabind/?.so;;"
export LD_LIBRARY_PATH="$REPO_ROOT/luabind:${LD_LIBRARY_PATH:-}"

case "${1:-all}" in
    gen-codec)
        "$LUA_BIN" "$SCRIPT_DIR/gen_codec_vectors.lua"
        ;;
    test-pack)
        shift
        "$LUA_BIN" "$SCRIPT_DIR/test_pack.lua" "$@"
        ;;
    test-codec)
        shift
        "$LUA_BIN" "$SCRIPT_DIR/test_codec.lua" "$@"
        ;;
    all)
        PACK_VECTORS="${2:-$SCRIPT_DIR/../vectors_pack.txt}"
        CODEC_VECTORS="${3:-$SCRIPT_DIR/../vectors_codec.txt}"

        echo "=== lua pack/unpack test ==="
        if [ -f "$PACK_VECTORS" ]; then
            "$LUA_BIN" "$SCRIPT_DIR/test_pack.lua" "$PACK_VECTORS"
        else
            echo "SKIP: $PACK_VECTORS not found"
        fi

        echo "=== lua encode/decode test ==="
        if [ -f "$CODEC_VECTORS" ]; then
            "$LUA_BIN" "$SCRIPT_DIR/test_codec.lua" "$CODEC_VECTORS"
        else
            echo "SKIP: $CODEC_VECTORS not found"
        fi
        ;;
    *)
        echo "Usage: $0 {gen-codec|test-pack <file>|test-codec <file>|all [pack_file] [codec_file]}" >&2
        exit 1
        ;;
esac
