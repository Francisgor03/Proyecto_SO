#!/usr/bin/env bash
set -euo pipefail
bin="${1:-./fase2_concurrente_stress}"
runs="${RUNS:-20}"
limit="${LIMIT:-15s}"
[[ "$runs" =~ ^[1-9][0-9]*$ ]] || { echo 'RUNS debe ser positivo'; exit 1; }
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
for ((i = 1; i <= runs; i++)); do
    log="$tmp_dir/run-$i.log"
    if timeout "$limit" "$bin" >"$log" 2>&1; then
        :
    else
        status=$?
        echo "FALLO: ejecución $i terminó con código $status (límite $limit)."
        cat "$log"
        exit 1
    fi
    if report=$(python3 tests/validar_concurrente.py "$log"); then
        echo "Ejecución $i/$runs: OK; $report"
    else
        cat "$log"
        exit 1
    fi
done
echo "PRUEBA SUPERADA: $runs ejecuciones de $bin (límite $limit por ejecución)."
