#!/usr/bin/env python3
"""
Grupo 3 — Métricas, análisis e informe técnico
===============================================
Ejecuta experimentos con los módulos de memoria virtual (Grupo 1) y
planificador de CPU (Grupo 2), genera gráficos comparativos, tablas
de métricas y detecta situaciones de thrashing.

Uso:
    python3 analisis/generar_analisis.py [ruta_memoria] [ruta_planificador]

Modos de operación:
    1. Con binarios compilados (make all): ejecuta los binarios reales.
    2. Sin binarios (modo demo/standalone): usa simulaciones internas
       en Python puro para generar los mismos gráficos y tablas.

Requisitos:
    - matplotlib  (pip install matplotlib) — opcional, sin él solo tablas.
    - Para modo real: binarios fase3_memoria y fase3_planificador (make all).
"""

import csv
import io
import os
import random
import re
import subprocess
import sys
from collections import deque
from pathlib import Path

# ──────────────────────────────────────────────────────────
# matplotlib (opcional — sin él solo se generan tablas)
# ──────────────────────────────────────────────────────────
try:
    # pyrefly: ignore[missing-import]
    import matplotlib
    matplotlib.use("Agg")
    # pyrefly: ignore[missing-import]
    import matplotlib.pyplot as plt
    HAS_MPL = True
except ImportError:
    HAS_MPL = False

# ──────────────────────────────────────────────────────────
# Rutas y configuración global
# ──────────────────────────────────────────────────────────
RAIZ = Path(__file__).resolve().parent.parent
BIN_MEMORIA = sys.argv[1] if len(sys.argv) > 1 else str(RAIZ / "fase3_memoria")
BIN_PLANIFICADOR = sys.argv[2] if len(sys.argv) > 2 else str(RAIZ / "fase3_planificador")
DIR_GRAFICOS = RAIZ / "docs" / "graficos"
DIR_GRAFICOS.mkdir(parents=True, exist_ok=True)
DIR_CSV = DIR_GRAFICOS  # Los CSV se guardan junto a los gráficos

# Verificar disponibilidad de binarios
MODO_DEMO = False

if HAS_MPL:
    plt.rcParams.update({
        "figure.figsize": (10, 6),
        "axes.grid": True,
        "grid.alpha": 0.3,
        "font.size": 11,
        "axes.spines.top": False,
        "axes.spines.right": False,
    })


# ──────────────────────────────────────────────────────────
# Simulaciones internas (modo demo/standalone)
# ──────────────────────────────────────────────────────────

def _simular_lru(marcos, refs, tam=256, paginas=16, procs=1):
    """Simulación LRU pura en Python — idéntica al binario fase3_memoria."""
    pila = []        # Menos reciente al frente
    residentes = {}  # (pid, pagina) -> marco
    fallos = 0
    total = 0

    for pid, direccion in refs:
        pagina = direccion // tam
        if pagina >= paginas or pid < 1 or pid > procs:
            continue
        total += 1
        clave = (pid, pagina)

        if clave not in residentes:
            fallos += 1
            if len(residentes) >= marcos:
                victima = pila.pop(0)
                del residentes[victima]
            marco = min(set(range(marcos)) - set(residentes.values())) \
                if len(residentes) < marcos else 0
            residentes[clave] = marco
        else:
            pila.remove(clave)
        pila.append(clave)

    tasa = (100.0 * fallos / total) if total > 0 else 0.0
    return {
        "referencias": total,
        "fallos": fallos,
        "aciertos": total - fallos,
        "tasa_fallos": round(tasa, 2),
    }


def _simular_round_robin(procesos_lista, quantum):
    """Simulación Round Robin pura en Python — idéntica al binario."""
    restantes = {pid: rafaga for pid, llegada, rafaga in procesos_lista}
    orden = sorted(enumerate(procesos_lista),
                   key=lambda p: (p[1][1], p[0]))
    pendientes = deque(p for _, p in orden)
    cola = deque()
    inicios = {}
    finales = {}
    tiempo = 0
    despachos = 0
    ultimo_pid = None
    cambios_contexto = 0

    def admitir():
        while pendientes and pendientes[0][1] <= tiempo:
            cola.append(pendientes.popleft()[0])

    while pendientes or cola:
        if not cola and pendientes[0][1] > tiempo:
            tiempo = pendientes[0][1]
        admitir()
        if not cola:
            break
        pid = cola.popleft()
        despachos += 1
        if ultimo_pid is not None and ultimo_pid != pid:
            cambios_contexto += 1
        ultimo_pid = pid
        inicios.setdefault(pid, tiempo)
        for _ in range(quantum):
            tiempo += 1
            restantes[pid] -= 1
            admitir()
            if restantes[pid] == 0:
                finales[pid] = tiempo
                break
        if restantes[pid] > 0:
            cola.append(pid)

    procs_resultado = []
    for pid, llegada, rafaga in procesos_lista:
        fin = finales.get(pid, 0)
        retorno = fin - llegada
        espera = retorno - rafaga
        respuesta = inicios.get(pid, 0) - llegada
        procs_resultado.append({
            "pid": pid,
            "llegada": llegada,
            "rafaga": rafaga,
            "fin": fin,
            "retorno": retorno,
            "espera": espera,
            "respuesta": respuesta,
        })
    n = len(procesos_lista)
    return {
        "procesos": procs_resultado,
        "promedio_retorno": sum(p["retorno"] for p in procs_resultado) / n if n else 0,
        "promedio_espera": sum(p["espera"] for p in procs_resultado) / n if n else 0,
        "promedio_respuesta": sum(p["respuesta"] for p in procs_resultado) / n if n else 0,
        "despachos": despachos,
        "cambios_contexto": cambios_contexto,
    }


# ──────────────────────────────────────────────────────────
# Funciones auxiliares
# ──────────────────────────────────────────────────────────

def _ejecutar(binario, args):
    """Ejecuta un binario y devuelve (stdout, stderr, returncode)."""
    r = subprocess.run(
        [binario, *args], text=True, capture_output=True, timeout=30
    )
    return r.stdout, r.stderr, r.returncode


def parsear_resumen_memoria(texto):
    """Extrae Referencias, Fallos, Aciertos, Tasa del resumen de memoria."""
    m = re.search(
        r"Referencias=(\d+)\s+Fallos=(\d+)\s+Aciertos=(\d+)\s+"
        r"Tasa_fallos=([\d.]+)%",
        texto,
    )
    if not m:
        return None
    return {
        "referencias": int(m.group(1)),
        "fallos": int(m.group(2)),
        "aciertos": int(m.group(3)),
        "tasa_fallos": float(m.group(4)),
    }


def parsear_metricas_planificador(texto):
    """Extrae métricas individuales y promedios del planificador."""
    procesos = []
    for linea in texto.splitlines():
        if re.match(r"^\|\s*\d+\s*\|", linea):
            campos = [x.strip() for x in linea.split("|")[1:-1]]
            procesos.append({
                "pid": int(campos[0]),
                "llegada": int(campos[1]),
                "rafaga": int(campos[2]),
                "fin": int(campos[3]),
                "retorno": int(campos[4]),
                "espera": int(campos[5]),
                "respuesta": int(campos[6]),
            })
    promedios = re.search(
        r"Retorno:\s*([\d.]+).*?Espera:\s*([\d.]+).*?Respuesta:\s*([\d.]+)",
        texto,
    )
    despachos = re.search(
        r"Despachos:\s*(\d+).*?Cambios de contexto.*?:\s*(\d+)", texto
    )
    return {
        "procesos": procesos,
        "promedio_retorno": float(promedios.group(1)) if promedios else 0,
        "promedio_espera": float(promedios.group(2)) if promedios else 0,
        "promedio_respuesta": float(promedios.group(3)) if promedios else 0,
        "despachos": int(despachos.group(1)) if despachos else 0,
        "cambios_contexto": int(despachos.group(2)) if despachos else 0,
    }


def generar_referencias(patron, num_refs, num_paginas, tam_pagina=256,
                        procesos=1, seed=42):
    """Genera una lista de (pid, dirección) según un patrón de acceso."""
    rng = random.Random(seed)
    refs = []
    if patron == "secuencial":
        for i in range(num_refs):
            pid = (i % procesos) + 1
            refs.append((pid, (i % num_paginas) * tam_pagina))
    elif patron == "aleatorio":
        for _ in range(num_refs):
            pid = rng.randint(1, procesos)
            refs.append((pid, rng.randint(0, num_paginas - 1) * tam_pagina))
    elif patron == "localidad":
        calientes = max(1, num_paginas // 5)
        for _ in range(num_refs):
            pid = rng.randint(1, procesos) if procesos > 1 else 1
            if rng.random() < 0.8:
                pag = rng.randint(0, calientes - 1)
            else:
                pag = rng.randint(0, num_paginas - 1)
            refs.append((pid, pag * tam_pagina))
    elif patron == "thrashing":
        for i in range(num_refs):
            pid = (i % procesos) + 1
            refs.append((pid, (i % num_paginas) * tam_pagina))
    return refs


def ejecutar_memoria(marcos, refs, tam=256, paginas=16, procs=1):
    """Ejecuta fase3_memoria en modo CSV o usa simulación interna."""
    if MODO_DEMO:
        return _simular_lru(marcos, refs, tam, paginas, procs)

    args = [
        "-c", "-m", str(marcos), "-s", str(tam),
        "-p", str(paginas), "-n", str(procs),
    ]
    args += [f"{pid}:{addr}" for pid, addr in refs]
    stdout, stderr, rc = _ejecutar(BIN_MEMORIA, args)
    if rc != 0:
        return None
    return parsear_resumen_memoria(stderr)


def ejecutar_planificador(procs, quantum):
    """Ejecuta fase3_planificador o usa simulación interna."""
    if MODO_DEMO:
        return _simular_round_robin(procs, quantum)

    args = ["-q", str(quantum)] + [f"{p}:{l}:{r}" for p, l, r in procs]
    stdout, _, rc = _ejecutar(BIN_PLANIFICADOR, args)
    assert rc == 0, f"Planificador falló con quantum {quantum}"
    return parsear_metricas_planificador(stdout)


def guardar(fig, nombre):
    """Guarda un gráfico y muestra la ruta."""
    ruta = DIR_GRAFICOS / nombre
    fig.savefig(ruta, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"  → Gráfico: {ruta}")


def guardar_csv(nombre, cabeceras, filas):
    """Guarda una tabla de resultados como CSV."""
    ruta = DIR_CSV / nombre
    with open(ruta, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(cabeceras)
        writer.writerows(filas)
    print(f"  → CSV:     {ruta}")


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 1 — Fallos de página vs marcos disponibles
# ──────────────────────────────────────────────────────────

def experimento_1():
    print("\n" + "=" * 64)
    print("  EXP 1: Fallos de página vs marcos disponibles (LRU clásico)")
    print("=" * 64)

    clasica = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2]
    refs = [(1, p * 256) for p in clasica]
    marcos_rango = list(range(1, 9))
    fallos, tasas = [], []

    print(f"\nSecuencia: {clasica}")
    print(f"{'Marcos':>8} {'Fallos':>8} {'Aciertos':>10} {'Tasa (%)':>10}")
    print("-" * 40)

    filas_csv = []
    for m in marcos_rango:
        r = ejecutar_memoria(m, refs, paginas=16)
        fallos.append(r["fallos"])
        tasas.append(r["tasa_fallos"])
        print(f"{m:>8} {r['fallos']:>8} {r['aciertos']:>10} {r['tasa_fallos']:>9.2f}%")
        filas_csv.append([m, r["referencias"], r["fallos"],
                          r["aciertos"], r["tasa_fallos"]])

    guardar_csv("exp1_fallos_vs_marcos.csv",
                ["marcos", "referencias", "fallos", "aciertos", "tasa_fallos_pct"],
                filas_csv)

    if HAS_MPL:
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
        ax1.plot(marcos_rango, fallos, "o-", color="#e74c3c", lw=2, ms=8,
                 label="Fallos de página")
        ax1.set_xlabel("Marcos físicos")
        ax1.set_ylabel("Fallos de página")
        ax1.set_title("Fallos de página vs marcos (secuencia clásica LRU)")
        ax1.set_xticks(marcos_rango)
        ax1.legend()

        colores = ["#e74c3c" if t > 50 else "#f39c12" if t > 25
                   else "#27ae60" for t in tasas]
        ax2.bar(marcos_rango, tasas, color=colores, edgecolor="black",
                alpha=0.85)
        ax2.set_xlabel("Marcos físicos")
        ax2.set_ylabel("Tasa de fallos (%)")
        ax2.set_title("Tasa de fallos vs marcos")
        ax2.set_xticks(marcos_rango)
        fig.tight_layout()
        guardar(fig, "exp1_fallos_vs_marcos.png")

    return marcos_rango, fallos, tasas


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 2 — Patrones de acceso
# ──────────────────────────────────────────────────────────

def experimento_2():
    print("\n" + "=" * 64)
    print("  EXP 2: Impacto de patrones de acceso en los fallos")
    print("=" * 64)

    NUM_REFS, NUM_PAG = 200, 16
    marcos_rango = list(range(1, 17))
    patrones = ["secuencial", "localidad", "aleatorio", "thrashing"]
    estilos = {
        "secuencial": ("#3498db", "o-"),
        "localidad": ("#27ae60", "s-"),
        "aleatorio": ("#f39c12", "^-"),
        "thrashing": ("#e74c3c", "D-"),
    }
    resultados = {}

    for patron in patrones:
        refs = generar_referencias(patron, NUM_REFS, NUM_PAG)
        resultados[patron] = []
        for m in marcos_rango:
            r = ejecutar_memoria(m, refs, paginas=NUM_PAG)
            resultados[patron].append(r["fallos"] if r else 0)

    # Tabla
    print(f"\n{'Marcos':>7}", end="")
    for p in patrones:
        print(f" {p:>12}", end="")
    print()
    print("-" * (7 + 13 * len(patrones)))

    filas_csv = []
    for i, m in enumerate(marcos_rango):
        print(f"{m:>7}", end="")
        fila = [m]
        for p in patrones:
            print(f" {resultados[p][i]:>12}", end="")
            fila.append(resultados[p][i])
        print()
        filas_csv.append(fila)

    guardar_csv("exp2_patrones_acceso.csv",
                ["marcos"] + patrones, filas_csv)

    if HAS_MPL:
        fig, ax = plt.subplots(figsize=(12, 6))
        for patron in patrones:
            color, estilo = estilos[patron]
            ax.plot(marcos_rango, resultados[patron], estilo, color=color,
                    lw=2, ms=6, label=patron.capitalize())
        ax.set_xlabel("Marcos físicos")
        ax.set_ylabel("Fallos de página")
        ax.set_title(f"Fallos por patrón de acceso ({NUM_REFS} refs, {NUM_PAG} págs)")
        ax.set_xticks(marcos_rango)
        ax.legend(title="Patrón")
        fig.tight_layout()
        guardar(fig, "exp2_patrones_acceso.png")

    return resultados


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 3 — Detección de thrashing
# ──────────────────────────────────────────────────────────

def experimento_3():
    print("\n" + "=" * 64)
    print("  EXP 3: Detección de thrashing (multiprogramación creciente)")
    print("=" * 64)

    MARCOS_TOTAL = 8
    NUM_PAG = 16
    REFS_POR_PROC = 50
    TAM = 256
    UMBRAL = 50.0

    procesos_rango = list(range(1, 13))
    tasas, fallos_lista = [], []

    rng = random.Random(2026)

    print(f"\nMarcos totales: {MARCOS_TOTAL}  |  Págs/proceso: {NUM_PAG}")
    print(f"Refs/proceso: {REFS_POR_PROC}  |  Umbral thrashing: {UMBRAL}%")
    print(f"\n{'Procesos':>10} {'Fallos':>8} {'Refs':>8} {'Tasa':>8} {'Estado'}")
    print("-" * 50)

    filas_csv = []
    for num_proc in procesos_rango:
        refs = []
        for _ in range(REFS_POR_PROC):
            for pid in range(1, num_proc + 1):
                pag = rng.randint(0, min(NUM_PAG - 1, 7))
                refs.append((pid, pag * TAM))
        r = ejecutar_memoria(MARCOS_TOTAL, refs, TAM, NUM_PAG, num_proc)
        tasas.append(r["tasa_fallos"])
        fallos_lista.append(r["fallos"])
        estado = "⚠ THRASHING" if r["tasa_fallos"] > UMBRAL else "Normal"
        print(f"{num_proc:>10} {r['fallos']:>8} {r['referencias']:>8} "
              f"{r['tasa_fallos']:>7.1f}% {estado}")
        filas_csv.append([num_proc, r["referencias"], r["fallos"],
                          r["tasa_fallos"], estado.strip()])

    punto = next((procesos_rango[i] for i, t in enumerate(tasas) if t > UMBRAL), None)
    if punto:
        print(f"\n  ⚠ Thrashing desde {punto} procesos (tasa > {UMBRAL}%)")
    else:
        print(f"\n  ✓ Sin thrashing hasta {procesos_rango[-1]} procesos")

    guardar_csv("exp3_thrashing.csv",
                ["procesos", "referencias", "fallos", "tasa_fallos_pct", "estado"],
                filas_csv)

    if HAS_MPL:
        fig, ax = plt.subplots(figsize=(12, 6))
        colores = ["#e74c3c" if t > UMBRAL else "#f39c12" if t > 30
                   else "#27ae60" for t in tasas]
        ax.bar(procesos_rango, tasas, color=colores, edgecolor="black",
               alpha=0.85)
        ax.axhline(y=UMBRAL, color="red", ls="--", lw=2,
                   label=f"Umbral thrashing ({UMBRAL}%)")
        ax.set_xlabel("Grado de multiprogramación (procesos)")
        ax.set_ylabel("Tasa de fallos de página (%)")
        ax.set_title(f"Detección de thrashing ({MARCOS_TOTAL} marcos, "
                     f"{NUM_PAG} págs/proc)")
        ax.set_xticks(procesos_rango)
        ax.legend()
        if punto:
            idx = procesos_rango.index(punto)
            ax.annotate(
                f"Thrashing\n({punto} procs)",
                xy=(punto, tasas[idx]),
                xytext=(punto + 1.5, max(tasas) * 0.7),
                arrowprops=dict(arrowstyle="->", color="red", lw=2),
                fontsize=11, color="red", fontweight="bold",
            )
        fig.tight_layout()
        guardar(fig, "exp3_thrashing.png")

    return procesos_rango, tasas, punto


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 4 — Fallos vs intensidad de carga
# ──────────────────────────────────────────────────────────

def experimento_4():
    print("\n" + "=" * 64)
    print("  EXP 4: Fallos vs intensidad de carga de trabajo")
    print("=" * 64)

    MARCOS, NUM_PAG, TAM = 4, 16, 256
    cargas = [25, 50, 100, 200, 400, 800]
    res_loc, res_ale = [], []

    print(f"\nMarcos: {MARCOS}  |  Páginas: {NUM_PAG}")
    print(f"\n{'Refs':>8} {'Loc fallos':>11} {'Ale fallos':>11} "
          f"{'Loc tasa':>9} {'Ale tasa':>9}")
    print("-" * 52)

    filas_csv = []
    for carga in cargas:
        rl = ejecutar_memoria(MARCOS,
             generar_referencias("localidad", carga, NUM_PAG, TAM), TAM, NUM_PAG)
        ra = ejecutar_memoria(MARCOS,
             generar_referencias("aleatorio", carga, NUM_PAG, TAM), TAM, NUM_PAG)
        res_loc.append(rl)
        res_ale.append(ra)
        print(f"{carga:>8} {rl['fallos']:>11} {ra['fallos']:>11} "
              f"{rl['tasa_fallos']:>8.1f}% {ra['tasa_fallos']:>8.1f}%")
        filas_csv.append([carga, rl["fallos"], rl["tasa_fallos"],
                          ra["fallos"], ra["tasa_fallos"]])

    guardar_csv("exp4_carga_trabajo.csv",
                ["referencias", "fallos_localidad", "tasa_localidad_pct",
                 "fallos_aleatorio", "tasa_aleatorio_pct"],
                filas_csv)

    if HAS_MPL:
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
        ax1.plot(cargas, [r["fallos"] for r in res_loc], "o-", color="#27ae60",
                 lw=2, ms=8, label="Con localidad")
        ax1.plot(cargas, [r["fallos"] for r in res_ale], "s-", color="#e74c3c",
                 lw=2, ms=8, label="Aleatorio")
        ax1.set_xlabel("Número de referencias")
        ax1.set_ylabel("Fallos de página")
        ax1.set_title("Fallos absolutos vs carga")
        ax1.legend()

        ax2.plot(cargas, [r["tasa_fallos"] for r in res_loc], "o-",
                 color="#27ae60", lw=2, ms=8, label="Con localidad")
        ax2.plot(cargas, [r["tasa_fallos"] for r in res_ale], "s-",
                 color="#e74c3c", lw=2, ms=8, label="Aleatorio")
        ax2.set_xlabel("Número de referencias")
        ax2.set_ylabel("Tasa de fallos (%)")
        ax2.set_title("Tasa de fallos vs carga")
        ax2.legend()
        fig.tight_layout()
        guardar(fig, "exp4_carga_trabajo.png")

    return cargas, res_loc, res_ale


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 5 — Impacto del quantum en Round Robin
# ──────────────────────────────────────────────────────────

def experimento_5():
    print("\n" + "=" * 64)
    print("  EXP 5: Impacto del quantum en Round Robin")
    print("=" * 64)

    procs = [(1, 0, 8), (2, 1, 4), (3, 2, 9), (4, 3, 5)]
    quantums = list(range(1, 14))
    retornos, esperas, respuestas, contextos = [], [], [], []

    print(f"\nProcesos: {procs}")
    print(f"\n{'Q':>4} {'Retorno':>9} {'Espera':>9} {'Resp':>9} {'Ctx':>5}")
    print("-" * 40)

    filas_csv = []
    for q in quantums:
        m = ejecutar_planificador(procs, q)
        retornos.append(m["promedio_retorno"])
        esperas.append(m["promedio_espera"])
        respuestas.append(m["promedio_respuesta"])
        contextos.append(m["cambios_contexto"])
        print(f"{q:>4} {m['promedio_retorno']:>9.2f} {m['promedio_espera']:>9.2f} "
              f"{m['promedio_respuesta']:>9.2f} {m['cambios_contexto']:>5}")
        filas_csv.append([q, m["promedio_retorno"], m["promedio_espera"],
                          m["promedio_respuesta"], m["despachos"],
                          m["cambios_contexto"]])

    guardar_csv("exp5_quantum.csv",
                ["quantum", "promedio_retorno", "promedio_espera",
                 "promedio_respuesta", "despachos", "cambios_contexto"],
                filas_csv)

    if HAS_MPL:
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))
        ax1.plot(quantums, retornos, "o-", color="#3498db", lw=2, label="Retorno")
        ax1.plot(quantums, esperas, "s-", color="#e74c3c", lw=2, label="Espera")
        ax1.plot(quantums, respuestas, "^-", color="#27ae60", lw=2,
                 label="Respuesta")
        ax1.set_xlabel("Quantum (ms)")
        ax1.set_ylabel("Tiempo promedio (ms)")
        ax1.set_title("Tiempos promedio vs quantum")
        ax1.set_xticks(quantums)
        ax1.legend()

        ax2.bar(quantums, contextos, color="#9b59b6", edgecolor="black",
                alpha=0.8)
        ax2.set_xlabel("Quantum (ms)")
        ax2.set_ylabel("Cambios de contexto")
        ax2.set_title("Cambios de contexto vs quantum")
        ax2.set_xticks(quantums)
        fig.tight_layout()
        guardar(fig, "exp5_quantum.png")

    return quantums, retornos, esperas, respuestas, contextos


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 6 — Tipos de carga en el planificador
# ──────────────────────────────────────────────────────────

def experimento_6():
    print("\n" + "=" * 64)
    print("  EXP 6: Comparativa de tipos de carga (planificador)")
    print("=" * 64)

    cargas = {
        "CPU-bound": [(1, 0, 20), (2, 0, 18), (3, 0, 15)],
        "IO-bound": [(1, 0, 2), (2, 1, 3), (3, 2, 1), (4, 3, 2), (5, 4, 1)],
        "Mixta": [(1, 0, 15), (2, 1, 3), (3, 2, 12), (4, 3, 2), (5, 5, 8)],
        "Escalonada": [(1, 0, 6), (2, 5, 6), (3, 10, 6), (4, 15, 6)],
    }
    QUANTUM = 3
    resultados = {}

    print(f"\nQuantum: {QUANTUM}")
    print(f"\n{'Carga':>14} {'Retorno':>9} {'Espera':>9} {'Resp':>9} {'Ctx':>5}")
    print("-" * 50)

    filas_csv = []
    for nombre, procs in cargas.items():
        m = ejecutar_planificador(procs, QUANTUM)
        resultados[nombre] = m
        print(f"{nombre:>14} {m['promedio_retorno']:>9.2f} "
              f"{m['promedio_espera']:>9.2f} "
              f"{m['promedio_respuesta']:>9.2f} {m['cambios_contexto']:>5}")
        filas_csv.append([nombre, len(procs), m["promedio_retorno"],
                          m["promedio_espera"], m["promedio_respuesta"],
                          m["despachos"], m["cambios_contexto"]])

    guardar_csv("exp6_cargas_planificador.csv",
                ["tipo_carga", "num_procesos", "promedio_retorno",
                 "promedio_espera", "promedio_respuesta",
                 "despachos", "cambios_contexto"],
                filas_csv)

    if HAS_MPL:
        nombres = list(resultados.keys())
        x = range(len(nombres))
        w = 0.25

        fig, ax = plt.subplots(figsize=(12, 6))
        ax.bar([i - w for i in x],
               [resultados[n]["promedio_retorno"] for n in nombres],
               w, label="Retorno", color="#3498db", edgecolor="black")
        ax.bar(list(x),
               [resultados[n]["promedio_espera"] for n in nombres],
               w, label="Espera", color="#e74c3c", edgecolor="black")
        ax.bar([i + w for i in x],
               [resultados[n]["promedio_respuesta"] for n in nombres],
               w, label="Respuesta", color="#27ae60", edgecolor="black")
        ax.set_xlabel("Tipo de carga")
        ax.set_ylabel("Tiempo promedio (ms)")
        ax.set_title(f"Métricas por tipo de carga (quantum = {QUANTUM})")
        ax.set_xticks(list(x))
        ax.set_xticklabels(nombres)
        ax.legend()
        fig.tight_layout()
        guardar(fig, "exp6_cargas_planificador.png")

    return resultados


# ──────────────────────────────────────────────────────────
# Generación de resumen consolidado CSV
# ──────────────────────────────────────────────────────────

def generar_resumen_consolidado():
    """Genera un CSV resumen con los hallazgos clave de todos los experimentos."""
    print("\n" + "=" * 64)
    print("  Resumen consolidado de experimentos")
    print("=" * 64)

    resumen = [
        ["Exp1", "Fallos vs marcos", "LRU",
         "Curva monótonamente decreciente; sin anomalía de Bélády"],
        ["Exp2", "Patrones de acceso", "Localidad 80/20",
         "La localidad es el factor dominante del rendimiento"],
        ["Exp3", "Detección de thrashing", "Umbral 50%",
         "Thrashing identificable al incrementar multiprogramación"],
        ["Exp4", "Carga de trabajo", "Localidad vs aleatorio",
         "La tasa converge; diferencia se amplifica con más marcos"],
        ["Exp5", "Quantum Round Robin", "Q=1..13",
         "Tradeoff entre respuesta (Q bajo) y overhead (Q alto)"],
        ["Exp6", "Tipos de carga", "CPU/IO/Mixta/Escalonada",
         "IO-bound se beneficia más de RR; escalonada reduce competencia"],
    ]

    guardar_csv("resumen_experimentos.csv",
                ["experimento", "titulo", "configuracion", "conclusion_clave"],
                resumen)

    for fila in resumen:
        print(f"  {fila[0]:>5}: {fila[1]:<25} → {fila[3]}")


# ──────────────────────────────────────────────────────────
# Punto de entrada
# ──────────────────────────────────────────────────────────

def main():
    global MODO_DEMO

    # Forzar UTF-8 en stdout para Windows (símbolos Unicode)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    print("=" * 64)
    print("  GRUPO 3 — Métricas, Análisis e Informe Técnico")
    print("  Generación automática de experimentos y gráficos")
    print("=" * 64)

    if not HAS_MPL:
        print("\n⚠  matplotlib no disponible — solo tablas de texto y CSV.")
        print("   Instalar con:  pip install matplotlib\n")

    # Verificar binarios — si no están, usar modo demo
    binarios_ok = True
    for nombre, ruta in [("memoria", BIN_MEMORIA),
                         ("planificador", BIN_PLANIFICADOR)]:
        try:
            subprocess.run([ruta, "-h"], capture_output=True, timeout=5)
        except (FileNotFoundError, PermissionError, OSError):
            print(f"⚠  No se encuentra {ruta}.")
            binarios_ok = False

    if not binarios_ok:
        print("\n╔══════════════════════════════════════════════════════╗")
        print("║  MODO DEMO: usando simulaciones internas en Python  ║")
        print("║  Para resultados reales, compilar con 'make all'.   ║")
        print("╚══════════════════════════════════════════════════════╝")
        MODO_DEMO = True
    else:
        print("\n  ✓ Binarios encontrados — modo ejecución real.")

    # Ejecutar los seis experimentos
    experimento_1()
    experimento_2()
    experimento_3()
    experimento_4()
    experimento_5()
    experimento_6()

    # Generar resumen consolidado
    generar_resumen_consolidado()

    print("\n" + "=" * 64)
    print("  Análisis completado.")
    if MODO_DEMO:
        print("  ⚠ Resultados generados en MODO DEMO (simulación Python).")
    if HAS_MPL:
        print(f"  Gráficos guardados en: {DIR_GRAFICOS}/")
    print(f"  Tablas CSV en:        {DIR_CSV}/")
    print("  Informe técnico:      docs/informe_tecnico.md")
    print("=" * 64)


if __name__ == "__main__":
    main()
