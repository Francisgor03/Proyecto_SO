#!/usr/bin/env bash
# tests/test_filosofos.sh — Valida la simulación de Filósofos Comensales
set -euo pipefail

bin="${1:-./fase2_filosofos}"
scenario="${2:-FILOSOFOS COMENSALES}"
runs="${RUNS:-10}"
limit="${LIMIT:-30s}"

[[ "$runs" =~ ^[1-9][0-9]*$ ]] || { echo 'RUNS debe ser un entero positivo'; exit 1; }

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

    if timeout "$limit" "$bin" > "$log" 2>&1; then
        :
    else
        status=$?
        printf '\n[FALLO] Ejecucion %d/%d: codigo %d (limite %s).\n' \
               "$i" "$runs" "$status" "$limit"
        cat "$log"
        exit 1
    fi

    # ── Validar contenido del log ──────────────────────────────────────
    # 1. Debe terminar con la línea de éxito
    if ! grep -q '\[OK\] Todos los filosofos completaron sus rondas sin deadlock\.' "$log"; then
        printf '\n[FALLO] Ejecucion %d/%d: no se encontro la linea de exito.\n' "$i" "$runs"
        cat "$log"
        exit 1
    fi

    # 2. No debe haber ERROR CRITICO ni ESTADO: ERROR
    if grep -qE 'ERROR CRITICO|ESTADO: ERROR' "$log"; then
        printf '\n[FALLO] Ejecucion %d/%d: se detecto un error critico o invariante violado.\n' \
               "$i" "$runs"
        cat "$log"
        exit 1
    fi

    # 3. Extraer configuración de la línea [CONFIG]
    config_line=$(grep '^\[CONFIG\]' "$log")
    filosofos=$(echo "$config_line" | sed 's/.*filosofos=\([0-9]*\).*/\1/')
    rondas=$(echo    "$config_line" | sed 's/.*rondas=\([0-9]*\).*/\1/')

    # 4. Verificar que todos los filósofos completaron sus rondas
    failed=0
    for ((f = 0; f < filosofos; f++)); do
        expected="[RESUMEN Filosofo ${f}] completadas=${rondas}/${rondas}"
        if ! grep -qF "$expected" "$log"; then
            printf '\n[FALLO] Ejecucion %d/%d: Filosofo %d no completo sus rondas.\n' \
                   "$i" "$runs" "$f"
            cat "$log"
            failed=1
            break
        fi
    done
    [[ "$failed" -eq 0 ]] || exit 1

    if ((i == 1)); then
        printf '%d filosofos | %d rondas cada uno\n\n' "$filosofos" "$rondas"
        printf '  %-9s %-8s %10s %10s\n' 'Ejecucion' 'Estado' 'Filosofos' 'Rondas'
        printf '  %s\n' '--------------------------------------'
    fi

    total_rondas=$(( filosofos * rondas ))
    printf '  %02d/%-6d %s    %10d %10d\n' \
           "$i" "$runs" "$ok" "$filosofos" "$total_rondas"

done

printf '\n[OK] PRUEBA SUPERADA: %d/%d ejecuciones sin deadlock ni race conditions.\n' \
       "$runs" "$runs"
printf '     Invariante verificada: ningun par de vecinos comio simultaneamente.\n'
