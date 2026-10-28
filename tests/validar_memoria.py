"""Contrasta la traza con una pila LRU independiente (menos a mas reciente)."""
import csv
import io
import random
import subprocess
import sys

binario = sys.argv[1] if len(sys.argv) > 1 else "./fase3_memoria"


def ejecutar(argumentos):
    return subprocess.run([binario, *argumentos], text=True, capture_output=True, timeout=10)


def verificar(referencias, marcos, tam=256, procesos=1, esperado=None):
    resultado = ejecutar(["-c", "-m", str(marcos), "-s", str(tam), "-n", str(procesos),
                          *[f"{pid}:{direccion}" for pid, direccion in referencias]])
    assert resultado.returncode == 0, resultado.stderr
    filas = list(csv.DictReader(io.StringIO(resultado.stdout)))
    assert len(filas) == len(referencias)
    pila, residentes = [], {}
    fallos = 0
    for paso, ((pid, direccion), fila) in enumerate(zip(referencias, filas), 1):
        pagina, desplazamiento = divmod(direccion, tam)
        clave = (pid, pagina)
        fallo = clave not in residentes
        victima = (0, 0)
        reemplazo = fallo and len(residentes) == marcos
        if fallo:
            fallos += 1
            if reemplazo:
                victima = pila.pop(0)
                marco = residentes.pop(victima)
            else:
                marco = min(set(range(marcos)) - set(residentes.values()))
            residentes[clave] = marco
        else:
            pila.remove(clave)
        pila.append(clave)
        valores = [paso, pid, direccion, pagina, desplazamiento, residentes[clave],
                   residentes[clave] * tam + desplazamiento, int(fallo), int(reemplazo), *victima]
        assert list(map(int, fila.values())) == valores, (fila, valores)
    assert f"Referencias={len(referencias)} Fallos={fallos} " in resultado.stderr
    if esperado is not None:
        assert fallos == esperado, (fallos, esperado)


clasica = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2]
verificar([(1, p * 256) for p in clasica], 3, esperado=9)
verificar([(1, p * 256) for p in clasica], 4, esperado=6)
verificar([(1, 0), (1, 255), (1, 1)], 1, esperado=1)
verificar([(1, 0), (1, 256), (1, 0), (1, 512)], 2, esperado=3)
verificar([(1, 0), (2, 0), (1, 1), (2, 256), (2, 0)], 2, procesos=2, esperado=4)
azar = random.Random(2026)
for tam in (1, 17, 256):
    referencias = [(azar.randint(1, 3), azar.randrange(16 * tam)) for _ in range(150)]
    for marcos in (1, 2, 3, 8, 50):
        verificar(referencias, marcos, tam, 3)

for argumentos in (["-m", "0"], ["-s", "-1"], ["-p", "abc"], ["-n", "0"],
                   ["-m", "999999999999999999999999999"], ["4096"], ["-1"],
                   ["2:0"], ["0:0"], ["1:"], ["1:2:3"], ["1:-1"], ["12abc"]):
    assert ejecutar(argumentos).returncode != 0, argumentos
demo = ejecutar([])
assert demo.returncode == 0 and "Fallos=9" in demo.stdout
print("[OK] CLI: secuencias conocidas, 15 escenarios aleatorios, traza CSV y entradas invalidas.")
