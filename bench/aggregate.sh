#!/usr/bin/env bash
# Collates bench/out/*.csv (lang,message,op,mode,size_bytes,ops_per_sec) into
# a markdown comparison table. Requires awk.
#
# Robust to two driver-CSV variants in this repo:
#   - presence/absence of a 'lang,...' header row (skipped via $1=="lang");
#   - message-name casing (zproto emits lowercase, protoc capitalizes) — keys
#     are lower-cased so rows join across bindings.
set -euo pipefail
DIR="$(dirname "$0")/out"
mkdir -p "$DIR"
echo "# zproto benchmark results"
echo
echo "## size (bytes)"
echo
echo "| message | zproto-nopack | zproto-pack | pb |"
echo "|---|---:|---:|---:|"
awk -F, '$1!="cpp"{next} {m=tolower($2); key[m]=m}
	$3=="encode"      && $4=="zproto_nopack"{np[m]=$5}
	$3=="encode_pack" && $4=="zproto_pack"  {pk[m]=$5}
	$3=="marshal"     && $4=="pb"           {pb[m]=$5}
	END{for(m in key) printf "| %s | %s | %s | %s |\n", m, np[m], pk[m], pb[m]}' \
	< <(cat "$DIR"/*.csv 2>/dev/null || true)
echo
echo "## throughput (ops/sec)"
echo
echo "| message | op | cpp-zproto | cpp-pb |"
echo "|---|---|---:|---:|"
awk -F, '$1!="cpp"{next}
	{m=tolower($2)}
	$4!="pb"{zt[m"@"$3]=$6}
	$4=="pb" && $3=="marshal"   {pp[m"@encode"]=$6;      pp[m"@encode_pack"]=$6}
	$4=="pb" && $3=="unmarshal" {pp[m"@decode"]=$6;      pp[m"@unpack_decode"]=$6}
	END{for(k in zt) { split(k, a, "@"); printf "| %s | %s | %s | %s |\n", a[1], a[2], zt[k], pp[k] } }' \
	< <(cat "$DIR"/*.csv 2>/dev/null || true) | sort
