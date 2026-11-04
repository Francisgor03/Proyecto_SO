"""Contrasta CPU, memoria y demoras con un modelo independiente por tick."""
from collections import deque
import csv
import io
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile

BIN = str(Path(sys.argv[1] if len(sys.argv) > 1 else './fase3_integrada').resolve())


def ejecutar(args):
    return subprocess.run([BIN, *args], text=True, capture_output=True, timeout=10)


def modelo(procesos, cargas, quantum, marcos, tam, costo):
    pendientes = deque(sorted(range(len(procesos)), key=lambda i: (procesos[i][1], i)))
    cola = deque()
    restantes = {pid: rafaga for pid, _, rafaga in procesos}
    inicios, finales, bloqueos = {}, {}, {pid: 0 for pid, _, _ in procesos}
    posiciones = {pid: 0 for pid, _, _ in procesos}
    pila, residentes, eventos, gantt = [], {}, [], []
    tiempo = 0

    def admitir():
        while pendientes and procesos[pendientes[0]][1] <= tiempo:
            cola.append(pendientes.popleft())

    while pendientes or cola:
        if not cola and procesos[pendientes[0]][1] > tiempo:
            hasta = procesos[pendientes[0]][1]
            gantt.append(('IDLE', tiempo, hasta))
            tiempo = hasta
        admitir()
        idx = cola.popleft()
        pid = procesos[idx][0]
        inicio = tiempo
        inicios.setdefault(pid, tiempo)
        for _ in range(min(quantum, restantes[pid])):
            refs = cargas[pid]
            virtual = refs[posiciones[pid] % len(refs)]
            posiciones[pid] += 1
            pag, desplazamiento = divmod(virtual, tam)
            clave = pid, pag
            fallo = clave not in residentes
            reemplazo = fallo and len(residentes) == marcos
            victima = (0, 0)
            if fallo:
                if reemplazo:
                    victima = pila.pop(0)
                    marco = residentes.pop(victima)
                else:
                    marco = min(set(range(marcos)) - set(residentes.values()))
                residentes[clave] = marco
            else:
                pila.remove(clave)
            pila.append(clave)
            demora = costo if fallo else 0
            eventos.append([len(eventos) + 1, tiempo, pid, virtual, pag, desplazamiento,
                            residentes[clave], residentes[clave] * tam + desplazamiento,
                            int(fallo), int(reemplazo), *victima, demora])
            bloqueos[pid] += demora
            tiempo += demora + 1
            restantes[pid] -= 1
            admitir()
        gantt.append((pid, inicio, tiempo))
        if restantes[pid]:
            cola.append(idx)
        else:
            finales[pid] = tiempo
    metricas = [[pid, llegada, rafaga, finales[pid], finales[pid] - llegada,
                 finales[pid] - llegada - rafaga - bloqueos[pid], inicios[pid] - llegada]
                for pid, llegada, rafaga in procesos]
    return eventos, gantt, metricas, bloqueos, tiempo


def verificar(procesos, cargas, q, m, tam=256, paginas=16, costo=0):
    with tempfile.TemporaryDirectory() as temporal:
        traza = Path(temporal) / 'traza.csv'
        args = ['-q', str(q), '-m', str(m), '-s', str(tam), '-p', str(paginas),
                '-f', str(costo), '-t', str(traza)]
        args += [f'{pid}:{l}:{r}:'+','.join(map(str, cargas[pid])) for pid, l, r in procesos]
        r = ejecutar(args)
        assert r.returncode == 0, r.stderr
        esperado, gantt, metricas, bloqueos, total = modelo(procesos, cargas, q, m, tam, costo)
        with traza.open() as f:
            filas = [[int(v) for v in row.values()] for row in csv.DictReader(f)]
        assert filas == esperado, (filas, esperado)
        obtenido = [(int(pid) if pid else 'IDLE', int(a), int(b))
                    for pid, idle, a, b in re.findall(r'(?:P(\d+)|(CPU IDLE)) \[(\d+) - (\d+)\]', r.stdout)]
        assert obtenido == gantt, (obtenido, gantt)
        tabla = [[int(v.strip()) for v in l.split('|')[1:-1]]
                 for l in r.stdout.splitlines() if re.match(r'^\|\s*\d+\s*\|', l)]
        assert tabla == metricas, (tabla, metricas)
        fallos = sum(row[8] for row in esperado)
        cpu = sum(p[2] for p in procesos)
        servicio = fallos * costo
        ocioso = sum(b-a for pid, a, b in gantt if pid == 'IDLE')
        assert total == cpu + servicio + ocioso
        assert f'Total: referencias={cpu} fallos={fallos} aciertos={cpu-fallos}' in r.stdout
        assert f'Tiempo_total={total} CPU_util={cpu} Servicio_fallos={servicio} Ocioso={ocioso}' in r.stdout
        for pid in bloqueos:
            assert f'bloqueo={bloqueos[pid]} ms' in r.stdout
        util, paginacion, rendimiento = re.search(
            r'Utilizacion_CPU=([\d.]+)% Fraccion_paginacion=([\d.]+)% Rendimiento=([\d.]+)', r.stdout).groups()
        assert abs(float(util) - 100 * cpu / total) <= .0051
        assert abs(float(paginacion) - 100 * servicio / total) <= .0051
        assert abs(float(rendimiento) - cpu / total) <= .00000051
        estado = 'ALTO' if 2 * fallos > cpu else 'ADVERTENCIA' if 10 * fallos >= 3 * cpu else 'NORMAL'
        assert f'Riesgo_thrashing={estado}' in r.stdout
        return fallos, total


cargas = {1: [0, 256], 2: [0, 256]}
assert verificar([(1,0,40),(2,0,40)], cargas, 1, 2, costo=5) == (80, 480)
assert verificar([(1,0,40),(2,0,40)], cargas, 1, 4, costo=5) == (4, 100)
assert verificar([(1,0,40),(2,0,40)], cargas, 4, 2, costo=5) == (40, 280)
verificar([(1,5,3),(2,7,2),(3,40,1)], {1:[1,17],2:[0],3:[50]}, 2, 2, tam=17, costo=3)
verificar([(1,0,1)], {1:[0]}, 1, 1, costo=2147483646)
verificar([(1,0,10)], {1:[0,256,512,0,0,0,0,0,0,0]}, 20, 3)  # 30% exacto
verificar([(1,0,10)], {1:[0,256,512,768,1024,0,0,0,0,0]}, 20, 5)  # 50% exacto
azar = random.Random(7023)
for _ in range(80):
    n, tam, paginas = azar.randint(1,7), azar.choice([1,17,256]), azar.randint(1,20)
    procs = [(i+1, azar.randrange(25), azar.randint(1,20)) for i in range(n)]
    cargas = {p:[azar.randrange(paginas*tam) for _ in range(azar.randint(1,9))] for p,_,_ in procs}
    verificar(procs, cargas, azar.randint(1,12), azar.randint(1,12), tam, paginas, azar.randint(0,9))
invalidos = [[], ['-q'], ['-x'], ['-s','0'], ['-p','0'], ['-m','0'], ['-f','-1'],
            ['-f','2147483648'], ['-q','abc'], ['-q','2147483648'],
            ['1:0:0:0'], ['1:0:1:4096'], ['1:0:1:0,4096'], ['1:0:1:0,'],
            ['1:0:1:'], ['1:0:1:,0'], ['1:0:1:0,,1'], ['1:0:1:-1'],
            ['1:0:1:18446744073709551616'], ['2:0:1:0'], ['1:-1:1:0'],
            ['1:0:1:0','1:0:1:0'], ['1:2147483647:1:0'],
            ['-f','2147483647','1:0:1:0'], ['-f','2147483646','1:0:2:0'],
            ['-t','/directorio-inexistente/traza.csv','1:0:1:0'],
            ['-t','/dev/full','1:0:1:0']]
for args in invalidos:
    r = ejecutar(args)
    assert r.returncode != 0, (args, r.stdout, r.stderr)
assert ejecutar(['-h']).returncode == 0
print('[OK] Integrada: 7 casos dirigidos, 80 aleatorios, traza CSV completa, tiempos, LRU, limites y 27 entradas invalidas.')
