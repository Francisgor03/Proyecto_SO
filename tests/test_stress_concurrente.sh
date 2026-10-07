#!/usr/bin/env bash
set -euo pipefail
bin="${1:-./fase2_concurrente_stress}"
scenario="${2:-PRUEBA CONCURRENTE}"
runs="${RUNS:-20}"
limit="${LIMIT:-15s}"
[[ "$runs" =~ ^[1-9][0-9]*$ ]] || { echo 'RUNS debe ser positivo'; exit 1; }
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
ok='OK'
if [[ -t 1 && -z "${NO_COLOR+x}" && "${TERM:-dumb}" != dumb ]]; then
    ok=$'\033[32mOK\033[0m'
fi
printf '\n=== %s ===\n' "$scenario"
printf 'Limite por ejecucion: %s | Repeticiones: %d\n' "$limit" "$runs"
for ((i = 1; i <= runs; i++)); do
    log="$tmp_dir/run-$i.log"
    if timeout "$limit" "$bin" >"$log" 2>&1; then
        :
    else
        status=$?
        printf '\n[FALLO] Ejecucion %d/%d: codigo %d (limite %s).\n' "$i" "$runs" "$status" "$limit"
        cat "$log"
        exit 1
    fi
    if report=$(python3 tests/validar_concurrente.py "$log" --tsv); then
        IFS=$'\t' read -r datos hilos espera productores consumidores items buffer <<< "$report"
        printf '%s\n' "$report" >> "$tmp_dir/resultados.tsv"
        if ((i == 1)); then
            printf '%d productores | %d consumidores | Buffer: %d | Cuota: %d\n\n' \
                "$productores" "$consumidores" "$buffer" "$items"
            printf '  %-9s %-8s %10s %10s %16s\n' 'Ejecucion' 'Estado' 'Datos' 'Hilos' 'Espera max. (ms)'
            printf '  %s\n' '---------------------------------------------------------'
        fi
        # El validador ya entrega tres decimales; imprimir como texto evita
        # que printf interprete el punto según la configuración regional.
        printf '  %02d/%-6d %s       %10d %10s %16s\n' \
            "$i" "$runs" "$ok" "$datos" "$hilos/$hilos" "$espera"
    else
        printf '\n[FALLO] Ejecucion %d/%d: datos o trazas invalidos.\n' "$i" "$runs"
        cat "$log"
        exit 1
    fi
done
LC_ALL=C awk -v runs="$runs" '
    { datos += $1; if ($3 > maximum) maximum = $3 }
    END {
        printf "\n[OK] PRUEBA SUPERADA: %d/%d ejecuciones; %.0f datos verificados.\n", runs, runs, datos
        printf "     Todas las cuotas completas, sin perdidas ni duplicados.\n"
        printf "     Mayor espera observada: %.3f ms\n", maximum
    }
' "$tmp_dir/resultados.tsv"
