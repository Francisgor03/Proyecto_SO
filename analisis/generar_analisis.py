#!/usr/bin/env python3
"""
Grupo 3 — Métricas, análisis e informe técnico
===============================================
Ejecuta experimentos con los módulos de memoria virtual (Grupo 1) y
planificador de CPU (Grupo 2), genera gráficos comparativos, tablas
de métricas y detecta situaciones de thrashing.

Uso:
    python3 analisis/generar_analisis.py [ruta_memoria] [ruta_planificador] [ruta_integrada]
    python3 analisis/generar_analisis.py --demo --solo-tablas

Modos de operación:
    1. Con binarios compilados (make all): ejecuta los binarios reales.
    2. Con --demo explicito: usa simulaciones internas
       en Python puro para generar los mismos gráficos y tablas.

Requisitos:
    - matplotlib; --solo-tablas permite ejecutar sin graficos.
    - Para modo real: fase3_memoria, fase3_planificador y fase3_integrada (make all).
"""

import argparse
import csv
import hashlib
import json
from datetime import datetime, timezone
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
BIN_MEMORIA = str(RAIZ / "fase3_memoria")
BIN_PLANIFICADOR = str(RAIZ / "fase3_planificador")
BIN_INTEGRADA = str(RAIZ / "fase3_integrada")
DIR_GRAFICOS = RAIZ / "docs" / "graficos"
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
    """Modelo de conteos LRU para el modo demo explicito."""
    if min(marcos, tam, paginas, procs) <= 0:
        raise ValueError("Configuracion invalida")
    pila = []        # Menos reciente al frente
    residentes = {}  # (pid, pagina) -> marco
    fallos = 0
    total = 0

    for pid, direccion in refs:
        pagina = direccion // tam
        if direccion < 0 or pagina >= paginas or pid < 1 or pid > procs:
            raise ValueError("Referencia fuera de rango")
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
    if not procesos or not promedios or not despachos:
        raise ValueError("Salida del planificador incompleta o incompatible")
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
        # Recorrido secuencial contiguo en memoria con localidad espacial
        # (ej. recorrido de un vector con 4 accesos por página de 256 bytes)
        paso = max(1, tam_pagina // 4)
        for i in range(num_refs):
            pid = (i % procesos) + 1
            refs.append((pid, ((i // procesos) * paso) % (num_paginas * tam_pagina)))
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
            refs.append((pid, ((i // procesos) % num_paginas) * tam_pagina))
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
        raise RuntimeError(f"Memoria fallo: {stderr.strip()}")
    resultado = parsear_resumen_memoria(stderr)
    if resultado is None or resultado["referencias"] != len(refs):
        raise ValueError("Resumen de memoria incompleto o incompatible")
    return resultado


def ejecutar_planificador(procs, quantum):
    """Ejecuta fase3_planificador o usa simulación interna."""
    if MODO_DEMO:
        return _simular_round_robin(procs, quantum)

    args = ["-q", str(quantum)] + [f"{p}:{l}:{r}" for p, l, r in procs]
    stdout, stderr, rc = _ejecutar(BIN_PLANIFICADOR, args)
    if rc != 0:
        raise RuntimeError(f"Planificador fallo con quantum {quantum}: {stderr.strip()}")
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
        writer = csv.writer(f, lineterminator="\n")
        writer.writerow(cabeceras)
        writer.writerows(filas)
    print(f"  → CSV:     {ruta}")


# ──────────────────────────────────────────────────────────
# EXPERIMENTO 1 — Fallos de página vs marcos disponibles
# ──────────────────────────────────────────────────────────

def clasificar_fallos(fallos, referencias):
    """Indicador heuristico del equipo; no es una definicion universal de thrashing."""
    if not 0 <= fallos <= referencias or referencias <= 0:
        raise ValueError("Conteos de referencias invalidos")
    if 2 * fallos > referencias:
        return "RIESGO ALTO"
    if 10 * fallos >= 3 * referencias:
        return "ADVERTENCIA"
    return "NORMAL"


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
            resultados[patron].append(r["fallos"])

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

    # Cada PID conserva su misma carga al aumentar la multiprogramacion.
    rngs = {pid: random.Random(2026 + pid) for pid in range(1, 13)}
    cargas = {pid: [rngs[pid].randrange(8) * TAM for _ in range(REFS_POR_PROC)]
              for pid in rngs}

    print(f"\nMarcos totales: {MARCOS_TOTAL}  |  Págs/proceso: {NUM_PAG}")
    print(f"Refs/proceso: {REFS_POR_PROC}  |  Umbral thrashing: {UMBRAL}%")
    print(f"\n{'Procesos':>10} {'Fallos':>8} {'Refs':>8} {'Tasa':>8} {'Estado'}")
    print("-" * 50)

    filas_csv = []
    for num_proc in procesos_rango:
        refs = []
        for paso in range(REFS_POR_PROC):
            for pid in range(1, num_proc + 1):
                refs.append((pid, cargas[pid][paso]))
        r = ejecutar_memoria(MARCOS_TOTAL, refs, TAM, NUM_PAG, num_proc)
        tasas.append(r["tasa_fallos"])
        fallos_lista.append(r["fallos"])
        estado = clasificar_fallos(r["fallos"], r["referencias"])
        print(f"{num_proc:>10} {r['fallos']:>8} {r['referencias']:>8} "
              f"{r['tasa_fallos']:>7.1f}% {estado}")
        filas_csv.append([num_proc, r["referencias"], r["fallos"],
                          r["tasa_fallos"], estado.strip()])

    punto = next((procesos_rango[i] for i, t in enumerate(tasas) if t > UMBRAL), None)
    if punto:
        print(f"\n  ⚠ Riesgo heuristico desde {punto} procesos (tasa > {UMBRAL}%)")
    else:
        print(f"\n  ✓ Sin riesgo heuristico hasta {procesos_rango[-1]} procesos")

    guardar_csv("exp3_thrashing.csv",
                ["procesos", "referencias", "fallos", "tasa_fallos_pct", "estado"],
                filas_csv)

    if HAS_MPL:
        fig, ax = plt.subplots(figsize=(12, 6))
        colores = ["#e74c3c" if t > UMBRAL else "#f39c12" if t >= 30
                   else "#27ae60" for t in tasas]
        ax.bar(procesos_rango, tasas, color=colores, edgecolor="black",
               alpha=0.85)
        ax.axhline(y=UMBRAL, color="red", ls="--", lw=2,
                   label=f"Umbral heuristico ({UMBRAL}%)")
        ax.set_xlabel("Grado de multiprogramación (procesos)")
        ax.set_ylabel("Tasa de fallos de página (%)")
        ax.set_title(f"Detección de thrashing ({MARCOS_TOTAL} marcos, "
                     f"{NUM_PAG} págs/proc)")
        ax.set_xticks(procesos_rango)
        ax.legend()
        if punto:
            idx = procesos_rango.index(punto)
            ax.annotate(
                f"Riesgo alto\n({punto} procs)",
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
        "Rafagas cortas": [(1, 0, 2), (2, 1, 3), (3, 2, 1), (4, 3, 2), (5, 4, 1)],
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
# Experimento integrado
# ──────────────────────────────────────────────────────────

def ejecutar_integrada(marcos, quantum, costo):
    args = ["-q", str(quantum), "-m", str(marcos), "-f", str(costo),
            "1:0:40:0,256", "2:0:40:0,256"]
    stdout, stderr, rc = _ejecutar(BIN_INTEGRADA, args)
    if rc:
        raise RuntimeError(f"Integrada fallo: {stderr.strip()}")
    memoria = re.search(r"Total: referencias=(\d+) fallos=(\d+) aciertos=(\d+)", stdout)
    tiempos = re.search(r"Tiempo_total=(\d+) CPU_util=(\d+) Servicio_fallos=(\d+) Ocioso=(\d+)", stdout)
    if not memoria or not tiempos:
        raise ValueError("Resumen integrado incompleto")
    refs, fallos, aciertos = map(int, memoria.groups())
    total, cpu, servicio, ocioso = map(int, tiempos.groups())
    if refs != 80 or fallos + aciertos != refs or total != cpu + servicio + ocioso:
        raise ValueError("Contadores integrados inconsistentes")
    return {"referencias": refs, "fallos": fallos, "aciertos": aciertos,
            "tasa": 100 * fallos / refs, "total": total, "cpu": cpu,
            "servicio": servicio, "utilizacion": 100 * cpu / total,
            "rendimiento": refs / total, **parsear_metricas_planificador(stdout)}


def experimento_7():
    print("\nEXP 7: CPU + memoria, quantum, marcos y coste de fallos")
    filas, resultados = [], {}
    for costo in (0, 5):
        for q in (1, 2, 4):
            resultados[costo, q] = []
            for m in range(1, 9):
                r = ejecutar_integrada(m, q, costo)
                resultados[costo, q].append(r)
                filas.append([m, q, costo, r["referencias"], r["fallos"], r["tasa"],
                              r["total"], r["servicio"], r["utilizacion"],
                              r["rendimiento"], r["promedio_retorno"],
                              r["promedio_espera"], r["promedio_respuesta"],
                              clasificar_fallos(r["fallos"], r["referencias"])])
    guardar_csv("exp7_integracion.csv",
                ["marcos", "quantum", "costo_fallo_ms", "referencias", "fallos",
                 "tasa_fallos_pct", "tiempo_total_ms", "servicio_fallos_ms",
                 "utilizacion_cpu_pct", "rendimiento_refs_ms", "promedio_retorno",
                 "promedio_espera", "promedio_respuesta", "riesgo_heuristico"], filas)
    for costo in (0, 5):
        for m in (2, 4):
            r = resultados[costo, 1][m - 1]
            print(f"  Q=1 marcos={m} coste={costo}: fallos={r['fallos']}, "
                  f"total={r['total']} ms, CPU util={r['utilizacion']:.2f}%")
    if HAS_MPL:
        fig, ejes = plt.subplots(1, 3, figsize=(16, 5))
        for q in (1, 2, 4):
            datos = resultados[5, q]
            for ax, clave in zip(ejes, ("fallos", "total", "utilizacion")):
                ax.plot(range(1, 9), [d[clave] for d in datos], "o-", label=f"Q={q}")
                ax.set_xlabel("Marcos fisicos")
                ax.set_xticks(range(1, 9))
                ax.legend()
        ejes[0].set_ylabel("Fallos de pagina")
        ejes[1].set_ylabel("Tiempo total (ms)")
        ejes[2].set_ylabel("CPU productiva (%)")
        fig.suptitle("Round Robin + LRU: 2 procesos, 80 referencias, servicio sincrono de 5 ms/fallo")
        fig.tight_layout()
        guardar(fig, "exp7_integracion.png")
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
         "Riesgo heuristico >50% al incrementar multiprogramacion"],
        ["Exp4", "Carga de trabajo", "Localidad vs aleatorio",
         "Comparacion de localidad y aleatorio con cuatro marcos fijos"],
        ["Exp5", "Quantum Round Robin", "Q=1..13",
         "Quantum bajo: mejor respuesta y mas cambios (sin coste de contexto)"],
        ["Exp6", "Tipos de carga", "Largas/Cortas/Mixta/Escalonada",
         "Rafagas cortas y llegadas escalonadas reducen tiempos en estas cargas"],
    ]

    if not MODO_DEMO:
        resumen.append(["Exp7", "CPU + memoria", "Quantum, marcos y coste de fallo",
                        "Servicio sincrono de fallos reduce CPU util y aumenta tiempos"])

    guardar_csv("resumen_experimentos.csv",
                ["experimento", "titulo", "configuracion", "conclusion_clave"],
                resumen)

    for fila in resumen:
        print(f"  {fila[0]:>5}: {fila[1]:<25} → {fila[3]}")


# ──────────────────────────────────────────────────────────
# Punto de entrada
# ──────────────────────────────────────────────────────────

def main():
    global MODO_DEMO, BIN_MEMORIA, BIN_PLANIFICADOR, BIN_INTEGRADA
    global DIR_GRAFICOS, DIR_CSV, HAS_MPL
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("memoria", nargs="?", default=BIN_MEMORIA)
    parser.add_argument("planificador", nargs="?", default=BIN_PLANIFICADOR)
    parser.add_argument("integrada", nargs="?", default=BIN_INTEGRADA)
    parser.add_argument("--demo", action="store_true", help="6 experimentos Python en docs/graficos/demo")
    parser.add_argument("--solo-tablas", action="store_true", help="Generar CSV sin requerir matplotlib")
    args = parser.parse_args()
    BIN_MEMORIA, BIN_PLANIFICADOR, BIN_INTEGRADA = args.memoria, args.planificador, args.integrada
    MODO_DEMO = args.demo
    if not args.solo_tablas and not HAS_MPL:
        parser.error("Falta matplotlib. Instale requirements.txt en .venv o use --solo-tablas.")
    HAS_MPL = HAS_MPL and not args.solo_tablas
    binarios = [BIN_MEMORIA, BIN_PLANIFICADOR, BIN_INTEGRADA]
    if not MODO_DEMO:
        for ruta in binarios:
            try:
                r = subprocess.run([ruta, "-h"], capture_output=True, timeout=5)
            except (OSError, subprocess.TimeoutExpired) as error:
                parser.error(f"No se pudo ejecutar {ruta}: {error}. Ejecute make all.")
            if r.returncode:
                parser.error(f"El binario {ruta} fallo al consultar -h.")
    DIR_GRAFICOS = RAIZ / "docs" / "graficos"
    if MODO_DEMO:
        DIR_GRAFICOS /= "demo"
    DIR_CSV = DIR_GRAFICOS
    DIR_GRAFICOS.mkdir(parents=True, exist_ok=True)
    print("MODO DEMO EXPLICITO (6 experimentos Python)" if MODO_DEMO else
          "MODO REAL: 7 experimentos con los ejecutables C")
    experimento_1()
    experimento_2()
    experimento_3()
    experimento_4()
    experimento_5()
    experimento_6()
    if not MODO_DEMO:
        experimento_7()
    generar_resumen_consolidado()
    metadata = {
        "modo": "demo" if MODO_DEMO else "real",
        "fecha_utc": datetime.now(timezone.utc).isoformat(),
        "python": sys.version.split()[0],
        "matplotlib": matplotlib.__version__ if HAS_MPL else None,
        "binarios_sha256": {} if MODO_DEMO else {
            str(Path(b).resolve()): hashlib.sha256(Path(b).read_bytes()).hexdigest() for b in binarios},
        "experimentos": 6 if MODO_DEMO else 7,
        "semillas": {"patrones": 42, "multiprogramacion": "2026 + PID"},
        "modelo_integrado": "Servicio sincrono; coste de fallo 0 y 5 ms; sin E/S asincrona",
    }
    (DIR_CSV / "metadata.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Analisis completado: {DIR_GRAFICOS}")


if __name__ == "__main__":
    main()
