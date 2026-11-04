"""Evita resultados falsos cuando faltan binarios o cambia su salida."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('analisis', ROOT/'analisis/generar_analisis.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
assert m.clasificar_fallos(29, 100) == 'NORMAL'
assert m.clasificar_fallos(30, 100) == 'ADVERTENCIA'
assert m.clasificar_fallos(50, 100) == 'ADVERTENCIA'
assert m.clasificar_fallos(51, 100) == 'RIESGO ALTO'
assert m.clasificar_fallos(50001, 100000) == 'RIESGO ALTO'
for fallos,refs in [(-1,10),(11,10),(0,0)]:
    try:
        m.clasificar_fallos(fallos,refs)
        raise AssertionError('Conteos invalidos aceptados')
    except ValueError:
        pass
for salida in ['', 'salida modificada']:
    try:
        m.parsear_metricas_planificador(salida)
        raise AssertionError('Salida incompleta aceptada')
    except ValueError:
        pass
with patch.object(m,'_ejecutar', return_value=('', 'error simulado', 1)):
    for funcion,args in [(m.ejecutar_memoria,(2,[(1,0)])),
                         (m.ejecutar_planificador,([(1,0,1)],1)),
                         (m.ejecutar_integrada,(2,1,5))]:
        try:
            funcion(*args)
            raise AssertionError('Error del ejecutable ignorado')
        except RuntimeError:
            pass
for patron in ['secuencial','thrashing']:
    refs = m.generar_referencias(patron, 12, 4, procesos=2)
    assert [v for pid,v in refs if pid==1] == [v for pid,v in refs if pid==2]
with tempfile.TemporaryDirectory() as temporal:
    raiz = Path(temporal)
    (raiz/'analisis').mkdir()
    script = raiz/'analisis/generar_analisis.py'
    shutil.copy(ROOT/'analisis/generar_analisis.py',script)
    r = subprocess.run([sys.executable,str(script),'--solo-tablas'],capture_output=True,text=True)
    assert r.returncode != 0 and not (raiz/'docs').exists(), r.stderr
    r = subprocess.run([sys.executable,str(script),'--demo','--solo-tablas'],capture_output=True,text=True)
    assert r.returncode == 0, r.stderr
    assert len(list((raiz/'docs/graficos/demo').glob('*.csv'))) == 7
    assert not (raiz/'docs/graficos/exp1_fallos_vs_marcos.csv').exists()
print('[OK] Analisis: umbrales, errores, cargas por PID, modo real obligatorio y demo aislado.')
