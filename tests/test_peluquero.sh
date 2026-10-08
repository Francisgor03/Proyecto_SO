#!/usr/bin/env bash
set -euo pipefail
bin="${1:-./fase2_peluquero}"
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
timeout 15s "$bin" >"$log"
line=$(grep '^\[RESUMEN BARBERIA\]' "$log")
clientes=$(printf '%s\n' "$line" | sed 's/.*clientes=\([0-9]*\).*/\1/')
atendidos=$(printf '%s\n' "$line" | sed 's/.*atendidos=\([0-9]*\).*/\1/')
rechazados=$(printf '%s\n' "$line" | sed 's/.*rechazados=\([0-9]*\).*/\1/')
test "$((atendidos + rechazados))" -eq "$clientes"
grep -q '^\[OK\] Barberia cerrada\.$' "$log"
echo "[OK] Barbero dormilon: todos los clientes fueron atendidos o rechazados."
