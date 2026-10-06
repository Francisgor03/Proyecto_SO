#!/usr/bin/env bash
set -euo pipefail

bin="${1:-./fase2_concurrente_stress}"
runs="${RUNS:-20}"
expected=$((10 * 6))
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

for ((i = 1; i <= runs; i++)); do
    log="$tmp_dir/run-$i.log"

    if timeout 10s "$bin" >"$log" 2>&1; then
        :
    else
        status=$?
        echo "FALLO: ejecución $i terminó con código $status o excedió 10 segundos."
        cat "$log"
        exit 1
    fi

    if ! grep -q "Todos los hilos concluyeron exitosamente" "$log"; then
        echo "FALLO: no terminó correctamente la ejecución $i."
        cat "$log"
        exit 1
    fi

    producidos=$(grep -cE '\[Productor [0-9]+\] Inserto:' "$log" || true)
    consumidos=$(grep -cE '\[Consumidor [0-9]+\] Consumio:' "$log" || true)

    if [[ "$producidos" -ne "$expected" || "$consumidos" -ne "$expected" ]]; then
        echo "FALLO: ejecución $i; producidos=$producidos, consumidos=$consumidos; se esperaban $expected."
        cat "$log"
        exit 1
    fi

    grep -E '\[Productor [0-9]+\] Inserto:' "$log" |
        sed -E 's/.*Inserto: ([0-9]+).*/\1/' |
        LC_ALL=C sort -n >"$tmp_dir/producidos"

    grep -E '\[Consumidor [0-9]+\] Consumio:' "$log" |
        sed -E 's/.*Consumio: ([0-9]+).*/\1/' |
        LC_ALL=C sort -n >"$tmp_dir/consumidos"

    if ! cmp -s "$tmp_dir/producidos" "$tmp_dir/consumidos"; then
        echo "FALLO: elementos producidos y consumidos distintos en la ejecución $i."
        cat "$log"
        exit 1
    fi

    echo "Ejecución $i/$runs: OK; $expected elementos coinciden."
done

echo "PRUEBA DE ESTRÉS SUPERADA: $runs ejecuciones sin bloqueo."
