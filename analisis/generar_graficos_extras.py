#!/usr/bin/env python3
"""Genera graficos y CSV para las comparaciones FIFO/LRU y SJF/Round Robin."""

import csv
import re
import subprocess
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as error:
    raise SystemExit(
        "matplotlib no esta instalado. Instale requirements.txt antes de continuar."
    ) from error


ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "docs" / "graficos"
BIN_MEMORIA = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "extra_memoria"
BIN_CPU = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "extra_cpu"


def ejecutar(binario):
    binario = binario.resolve()
    try:
        resultado = subprocess.run(
            [str(binario)], text=True, capture_output=True, timeout=30
        )
    except OSError as error:
        raise SystemExit(f"No se pudo ejecutar {binario}: {error}") from error
    if resultado.returncode != 0:
        raise SystemExit(
            f"{binario} termino con error:\n{resultado.stderr.strip()}"
        )
    return resultado.stdout


def leer_fifo_lru(texto):
    filas = []
    patron = re.compile(
        r"^(FIFO|LRU) marcos=(\d+) referencias=(\d+) fallos=(\d+) "
        r"aciertos=(\d+) tasa=([\d.]+)%$"
    )
    for linea in texto.splitlines():
        coincidencia = patron.match(linea.strip())
        if coincidencia:
            algoritmo, marcos, referencias, fallos, aciertos, tasa = coincidencia.groups()
            filas.append(
                [algoritmo, int(marcos), int(referencias), int(fallos),
                 int(aciertos), float(tasa)]
            )
    if len(filas) != 4:
        raise SystemExit("La salida de extra_memoria no tiene el formato esperado.")
    return filas


def leer_cpu(texto):
    algoritmos = []
    if re.search(r"ROUND ROBIN\s*:", texto):
        algoritmos.append("Round Robin")
    if re.search(r"SJF NO APROPIATIVO", texto):
        algoritmos.append("SJF")
    metricas = re.findall(
        r"Promedios\s*->\s*Retorno:\s*([\d.]+)\s*ms\s*\|\s*"
        r"Espera:\s*([\d.]+)\s*ms\s*\|\s*Respuesta:\s*([\d.]+)\s*ms",
        texto,
    )
    resultados = [
        [algoritmo, *map(float, valores)]
        for algoritmo, valores in zip(algoritmos, metricas)
    ]
    if len(resultados) != 2:
        raise SystemExit("La salida de extra_cpu no tiene el formato esperado.")
    return resultados


def guardar_csv(nombre, cabeceras, filas):
    with (OUTPUT / nombre).open("w", newline="", encoding="utf-8") as archivo:
        escritor = csv.writer(archivo)
        escritor.writerow(cabeceras)
        escritor.writerows(filas)


def generar_memoria(filas):
    guardar_csv(
        "extra_fifo_lru.csv",
        ["algoritmo", "marcos", "referencias", "fallos", "aciertos", "tasa_fallos"],
        filas,
    )
    fig, eje = plt.subplots()
    for algoritmo in ("FIFO", "LRU"):
        datos = [fila for fila in filas if fila[0] == algoritmo]
        eje.plot(
            [fila[1] for fila in datos],
            [fila[3] for fila in datos],
            marker="o",
            label=algoritmo,
        )
    eje.set_title("Comparacion FIFO vs. LRU")
    eje.set_xlabel("Marcos fisicos")
    eje.set_ylabel("Fallos de pagina")
    eje.set_xticks(sorted({fila[1] for fila in filas}))
    eje.legend()
    eje.grid(alpha=0.3)
    fig.savefig(OUTPUT / "extra_fifo_lru.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


def generar_cpu(filas):
    guardar_csv(
        "extra_sjf_rr.csv",
        ["algoritmo", "retorno_promedio", "espera_promedio", "respuesta_promedio"],
        filas,
    )
    algoritmos = [fila[0] for fila in filas]
    metricas = ["Retorno", "Espera", "Respuesta"]
    valores = [[fila[indice] for fila in filas] for indice in range(1, 4)]
    posiciones = range(len(algoritmos))
    ancho = 0.25
    fig, eje = plt.subplots()
    for indice, (metrica, datos) in enumerate(zip(metricas, valores)):
        desplazamiento = (indice - 1) * ancho
        eje.bar(
            [posicion + desplazamiento for posicion in posiciones],
            datos,
            ancho,
            label=metrica,
        )
    eje.set_title("Comparacion SJF vs. Round Robin")
    eje.set_ylabel("Tiempo promedio (ms)")
    eje.set_xticks(list(posiciones))
    eje.set_xticklabels(algoritmos)
    eje.legend()
    eje.grid(axis="y", alpha=0.3)
    fig.savefig(OUTPUT / "extra_sjf_rr.png", dpi=150, bbox_inches="tight")
    plt.close(fig)


def main():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    generar_memoria(leer_fifo_lru(ejecutar(BIN_MEMORIA)))
    generar_cpu(leer_cpu(ejecutar(BIN_CPU)))
    print(f"Graficos y CSV guardados en {OUTPUT}")


if __name__ == "__main__":
    main()
