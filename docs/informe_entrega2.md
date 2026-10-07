# Informe de la Entrega 2

## Diseño concurrente

Se implementó el problema Productor–Consumidor con un buffer circular de cinco espacios. Los hilos productores y consumidores se crean con pthread_create; el hilo principal espera su terminación con pthread_join.

El semáforo sem_vacios cuenta los espacios disponibles y comienza con el tamaño del buffer. El semáforo sem_llenos cuenta los elementos disponibles y comienza en cero. El mutex mutex_buffer protege el arreglo, los índices in y out, y el contador.

El productor espera un espacio con sem_wait antes de tomar el mutex. Dentro de la sección crítica, inserta un elemento y actualiza el índice y el contador. Después libera el mutex y publica el elemento con sem_post(sem_llenos). El consumidor sigue el orden complementario: espera un elemento, toma el mutex, extrae el elemento, actualiza el índice y el contador, libera el mutex y publica un espacio con sem_post(sem_vacios).

## Diagrama de estados de los hilos

Cada transición usa la forma evento(argumentos)[guarda]/acción.

    INICIO
      |
crearHilo(id)[hiloCreado]/encolar 
      | 
      v
    LISTO --despachar(id)[CPUDisponible]/asignarCPU--> EJECUTANDO
                                                            |
                                  +-------------------------+----------------------+
                                  |                                                |
              esperarEspacio(id)[sem_vacios==0]/bloquear         esperarElemento(id)[sem_llenos==0]/bloquear
                                  |                                                |
                                  +--------------------> BLOQUEADO <---------------+
                                                            |
                                    semPostVacios()[productorEnEspera]/despertarProductor
                                    semPostLlenos()[consumidorEnEspera]/despertarConsumidor
                                                            |
                                                            v
                                                          LISTO
 
                     EJECUTANDO --completarIteraciones(id)[itemsProcesados==TOTAL_ITEMS]/terminar--> TERMINADO

El estado BLOQUEADO representa una espera normal: el productor espera cuando el buffer está lleno y el consumidor cuando está vacío. Cuando se publica el semáforo que esperaba, el hilo vuelve al estado LISTO.

## Prevención de interbloqueos

Los hilos esperan en su semáforo antes de adquirir mutex_buffer. Ninguno espera un semáforo mientras mantiene el mutex, y el buffer utiliza un solo mutex. Cada hilo libera el mutex antes de publicar el semáforo complementario.

Por este orden no se forma una espera circular entre mutexes: un hilo que espera espacio o elementos no retiene mutex_buffer, y el hilo que posee el mutex puede terminar su sección crítica y liberarlo. La espera en sem_wait corresponde a esperar un recurso que puede producir el otro tipo de hilo; por sí sola no indica un interbloqueo.

## Evidencia empírica

Se ejecutó la prueba automatizada con el comando:

    make -B test-stress

La prueba compiló una configuración con 10 productores y 10 consumidores, con seis elementos por hilo. En cada ejecución verificó que el programa terminara antes de 10 segundos, que apareciera el mensaje de finalización de los hilos, que se produjeran y consumieran 60 elementos, y que los valores producidos coincidieran con los consumidos. El programa también devuelve error si el contador final del buffer no es cero.

Resultado observado: las 20 ejecuciones terminaron correctamente. Ninguna excedió el límite de tiempo y en todas coincidieron los elementos producidos y consumidos.

Este resultado aporta evidencia empírica para la configuración probada. Se complementa con la explicación del orden de adquisición y liberación de los recursos para sustentar la prevención de interbloqueos. No se afirma haber demostrado la ausencia de inanición.

## Reproducción

Desde la raíz del proyecto, se puede repetir la prueba con:

    make -B test-stress
