#!/usr/bin/env bash
set -euo pipefail
bin="${1:-./fase2_lectores_escritores}"
log="$(mktemp)"
trap 'rm -f "$log"' EXIT
timeout 15s "$bin" >"$log"
grep -q '^\[OK\] Lectores y escritores completaron' "$log"
grep -q '^\[Escritor ' "$log"
grep -q '^\[Lector ' "$log"
echo "[OK] Lectores-escritores: sin bloqueo y con acceso concurrente."
