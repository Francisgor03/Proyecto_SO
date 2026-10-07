"""Comprueba datos, trazas por hilo y cuotas usando la configuración del binario."""
import math
import re
import sys
from collections import Counter, defaultdict

text = open(sys.argv[1], encoding='utf-8').read()
config = re.search(r'\[CONFIG\] productores=(\d+) consumidores=(\d+) items=(\d+) buffer=(\d+)', text)
assert config, 'Falta configuración'
p, c, items, size = map(int, config.groups())
assert '[OK] Todos los hilos concluyeron exitosamente.' in text
assert '[BUFFER FINAL] contador=0' in text
assert 'ESTADO: ERROR' not in text
produced, consumed = [], []
counts = Counter()
for role, ident, action, value, position in re.findall(
        r'\[(Productor|Consumidor) (\d+)\] (Inserto|Consumio): (\d+) en posicion \[(\d+)\]', text):
    assert (role == 'Productor') == (action == 'Inserto')
    assert 0 <= int(position) < size
    ident, value = int(ident), int(value)
    counts[role, ident] += 1
    if role == 'Productor':
        assert (ident - 1) * items < value <= ident * items
        produced.append(value)
    else:
        consumed.append(value)
expected = list(range(1, p * items + 1))
assert sorted(produced) == expected, 'Pérdida o duplicado en producción'
assert sorted(consumed) == expected, 'Pérdida o duplicado en consumo'
states = defaultdict(list)
for role, ident, state in re.findall(r'\[HILO (Productor|Consumidor) (\d+)\] -> ESTADO: ([A-Z ]+)\n', text):
    states[role, int(ident)].append(state)
sequence = ['LISTO'] + ['ESPERANDO RECURSO', 'EN SECCION CRITICA', 'LIBERANDO'] * items + ['TERMINADO']
summaries = {}
for role, ident, done, total, maximum in re.findall(
        r'\[RESUMEN (Productor|Consumidor) (\d+)\] completadas=(\d+) espera_total_ms=([\d.]+) espera_max_ms=([\d.]+)', text):
    key = role, int(ident)
    assert key not in summaries, 'Resumen duplicado'
    total, maximum = float(total), float(maximum)
    assert math.isfinite(total) and math.isfinite(maximum)
    assert total >= maximum >= 0
    summaries[key] = int(done), maximum
keys = {(role, i) for role, n in [('Productor', p), ('Consumidor', c)] for i in range(1, n + 1)}
assert set(states) == set(summaries) == set(counts) == keys
for key in keys:
    assert counts[key] == items, f'Cuota incompleta: {key}'
    assert states[key] == sequence, f'Transiciones inválidas: {key}'
    assert summaries[key][0] == items
print(f'{p * items} datos únicos; {p + c} hilos completaron su cuota; espera máxima={max(v[1] for v in summaries.values()):.3f} ms')
