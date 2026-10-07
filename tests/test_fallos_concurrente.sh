#!/usr/bin/env bash
set -euo pipefail
bin="$1"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
FALLO=eintr timeout 15s "$bin" >"$tmp_dir/eintr.log" 2>&1
python3 tests/validar_concurrente.py "$tmp_dir/eintr.log"
for mode in crear esperar; do
    status=0
    FALLO="$mode" timeout 15s "$bin" >"$tmp_dir/$mode.log" 2>&1 || status=$?
    if [[ "$status" != 1 ]] || grep -q '\[OK\]' "$tmp_dir/$mode.log"; then
        cat "$tmp_dir/$mode.log"
        echo "FALLO: $mode debe terminar con error 1, sin bloquearse ni declarar éxito."
        exit 1
    fi
    if [[ "$mode" == crear ]]; then
        [[ $(grep -c '^\[RESUMEN ' "$tmp_dir/$mode.log") == 3 ]]
        ! grep -qE 'Inserto:|Consumio:' "$tmp_dir/$mode.log"
    else
        [[ $(grep -c '^\[RESUMEN ' "$tmp_dir/$mode.log") == 4 ]]
    fi
    echo "Recuperación ante fallo de $mode: OK"
done
