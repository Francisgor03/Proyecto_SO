"""Verifica Gantt y metricas contra una simulacion de referencia por milisegundo."""
from collections import deque
import random
import re
import subprocess
import sys

binario = sys.argv[1] if len(sys.argv) > 1 else "./fase3_planificador"


def ejecutar(args):
    return subprocess.run([binario, *args], text=True, capture_output=True, timeout=10)


def modelo(procesos, quantum):
    restantes = {pid: rafaga for pid, llegada, rafaga in procesos}
    orden = sorted(enumerate(procesos), key=lambda p: (p[1][1], p[0]))
    pendientes = deque(p for _, p in orden)
    cola, traza, inicios, finales = deque(), [], {}, {}
    tiempo = 0

    def admitir():
        while pendientes and pendientes[0][1] <= tiempo:
            cola.append(pendientes.popleft()[0])

    while pendientes or cola:
        if not cola and pendientes[0][1] > tiempo:
            traza.append(("IDLE", tiempo, pendientes[0][1]))
            tiempo = pendientes[0][1]
        admitir()
        pid = cola.popleft()
        inicio = tiempo
        inicios.setdefault(pid, tiempo)
        for _ in range(quantum):
            tiempo += 1
            restantes[pid] -= 1
            admitir()
            if restantes[pid] == 0:
                finales[pid] = tiempo
                break
        traza.append((pid, inicio, tiempo))
        if restantes[pid]:
            cola.append(pid)
    metricas = [[pid, llegada, rafaga, finales[pid], finales[pid] - llegada,
                 finales[pid] - llegada - rafaga, inicios[pid] - llegada]
                for pid, llegada, rafaga in procesos]
    return traza, metricas


def verificar(procesos, quantum):
    resultado = ejecutar(["-q", str(quantum), *[f"{p}:{l}:{r}" for p, l, r in procesos]])
    assert resultado.returncode == 0, resultado.stderr
    traza = []
    for pid, idle, inicio, fin in re.findall(r"(?:P(\d+)|(CPU IDLE)) \[(\d+) - (\d+)\]", resultado.stdout):
        traza.append((int(pid) if pid else "IDLE", int(inicio), int(fin)))
    filas = []
    for linea in resultado.stdout.splitlines():
        if re.match(r"^\|\s*\d+\s*\|", linea):
            filas.append([int(x.strip()) for x in linea.split("|")[1:-1]])
    esperado, metricas = modelo(procesos, quantum)
    assert traza == esperado, (procesos, quantum, traza, esperado)
    assert filas == metricas, (filas, metricas)
    pids = [p for p, _, _ in traza if p != "IDLE"]
    cambios = sum(a != b for a, b in zip(pids, pids[1:]))
    assert f"Despachos: {len(pids)} | Cambios de contexto entre procesos: {cambios}" in resultado.stdout
    promedios = re.search(r"Promedios -> Retorno: ([\d.]+) ms \| Espera: ([\d.]+) ms \| Respuesta: ([\d.]+) ms", resultado.stdout)
    assert promedios
    for columna, obtenido in zip((4, 5, 6), promedios.groups()):
        assert abs(float(obtenido) - sum(f[columna] for f in metricas) / len(metricas)) < 0.006


verificar([(1, 0, 8), (2, 1, 4), (3, 2, 9), (4, 3, 5)], 3)
verificar([(1, 0, 4), (2, 3, 1), (3, 1, 1)], 4)  # Arreglo desordenado
verificar([(1, 0, 4), (2, 2, 1)], 2)  # Llegada justo al agotar quantum
verificar([(9, 5, 1), (3, 20, 2)], 1)  # CPU ociosa al inicio y entre procesos
verificar([(8, 0, 2), (2, 0, 2), (5, 0, 1)], 1)  # Empates estables
verificar([(7, 0, 101)], 1)  # Regresion de desbordamiento
verificar([(i + 1, 0, 2) for i in range(151)], 1)
azar = random.Random(702)
for _ in range(80):
    n = azar.randint(1, 15)
    pids = azar.sample(range(1, 1000), n)
    procesos = [(pid, azar.randrange(25), azar.randint(1, 20)) for pid in pids]
    verificar(procesos, azar.randint(1, 25))

for argumentos in (["-q", "0"], ["-q", "-1"], ["-q", "abc"], ["-q", "2147483648"],
                   ["-q"], ["1:0:0"], ["1:0:-1"], ["1:-1:1"], ["0:0:1"],
                   ["1:0:1", "1:1:2"], ["1:2147483647:1"],
                   ["1:0:2147483647", "2:0:1"], ["1:0:1x"], ["1:0"], ["1::1"],
                   ["1:0:1:2"], ["999999999999999999999:0:1"]):
    resultado = ejecutar(argumentos)
    assert resultado.returncode != 0 and resultado.stderr, argumentos
assert ejecutar([]).returncode == 0
assert ejecutar(["-h"]).returncode == 0
print("[OK] CLI Round Robin: Gantt y metricas, 7 casos dirigidos, 80 aleatorios y errores de entrada.")
