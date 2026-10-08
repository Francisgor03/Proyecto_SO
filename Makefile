# V=1 muestra los comandos completos; los errores siempre son visibles.
V ?= 0
ifeq ($(V),0)
.SILENT:
endif

CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L
CPPFLAGS := -Iinclude
TARGET := mi_shell
SOURCES := src/main.c src/procesos.c src/archivos.c src/senales.c
OBJECTS := $(SOURCES:.c=.o)
CONCURRENT_TARGET := fase2_concurrente
CONCURRENT_SOURCES := src/main_concurrente.c src/hilos.c src/sincronizacion.c src/monitor.c
STRESS_TARGET := fase2_concurrente_stress
EXTRA_TARGETS := fase3_productor_lento fase3_consumidor_lento fase3_fallos
CONCURRENT_HEADERS := include/concurrencia.h include/sincronizacion.h include/monitor.h
FILOSOFOS_TARGET := fase2_filosofos
FILOSOFOS_SOURCES := src/main_filosofos.c src/filosofos.c
FILOSOFOS_STRESS_TARGET := fase2_filosofos_stress
FILOSOFOS_HEADERS := include/filosofos.h
RW_TARGET := fase2_lectores_escritores
RW_SOURCES := src/main_lectores_escritores.c src/lectores_escritores.c
RW_HEADERS := include/lectores_escritores.h
PELUQUERO_TARGET := fase2_peluquero
PELUQUERO_SOURCES := src/main_peluquero.c src/peluquero.c
PELUQUERO_HEADERS := include/peluquero.h

.PHONY: all run concurrent filosofos lectores-escritores peluquero test test-shell test-concurrent test-stress test-fallos test-filosofos test-lectores-escritores test-peluquero clean

all: $(TARGET) $(CONCURRENT_TARGET) $(FILOSOFOS_TARGET) $(RW_TARGET) $(PELUQUERO_TARGET)

$(TARGET): $(OBJECTS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(CONCURRENT_TARGET): $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(CONCURRENT_SOURCES) -o $@

$(STRESS_TARGET): $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=1000 -DBUFFER_SIZE=1 -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=0 -pthread $(CONCURRENT_SOURCES) -o $@

fase3_productor_lento: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=100 -DPAUSA_PRODUCTOR_MS=1 -DPAUSA_CONSUMIDOR_MS=0 $(CONCURRENT_SOURCES) -o $@

fase3_consumidor_lento: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=100 -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=1 $(CONCURRENT_SOURCES) -o $@

fase3_fallos: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS) tests/fallos_concurrente.c
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=0 $(CONCURRENT_SOURCES) tests/fallos_concurrente.c -Wl,--wrap=pthread_create -Wl,--wrap=sem_wait -o $@

$(FILOSOFOS_TARGET): $(FILOSOFOS_SOURCES) $(FILOSOFOS_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(FILOSOFOS_SOURCES) -o $@

$(FILOSOFOS_STRESS_TARGET): $(FILOSOFOS_SOURCES) $(FILOSOFOS_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DNUM_FILOSOFOS=5 -DRONDAS_FILOSOFO=500 -DPAUSA_COMER_MS=0 -DPAUSA_PENSAR_MS=0 $(FILOSOFOS_SOURCES) -o $@

$(RW_TARGET): $(RW_SOURCES) $(RW_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(RW_SOURCES) -o $@

$(PELUQUERO_TARGET): $(PELUQUERO_SOURCES) $(PELUQUERO_HEADERS)
	@printf "[COMPILAR] %s\n" "$@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(PELUQUERO_SOURCES) -o $@

concurrent: $(CONCURRENT_TARGET)
	./$(CONCURRENT_TARGET)

filosofos: $(FILOSOFOS_TARGET)
	./$(FILOSOFOS_TARGET)

lectores-escritores: $(RW_TARGET)
	./$(RW_TARGET)

peluquero: $(PELUQUERO_TARGET)
	./$(PELUQUERO_TARGET)

run: $(TARGET)
	./$(TARGET)

test: test-shell test-concurrent test-stress test-fallos test-filosofos test-lectores-escritores test-peluquero
	@printf "\n[OK] Suite completa: todas las pruebas superadas.\n"

test-shell: $(TARGET)
	@printf "\n=== SHELL: REDIRECCION ===\n"
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "[OK] Redireccion de entrada y salida"

test-concurrent: $(CONCURRENT_TARGET)
	RUNS=1 bash tests/test_stress_concurrente.sh ./$(CONCURRENT_TARGET) "CONCURRENCIA NORMAL"

test-stress: $(STRESS_TARGET) fase3_productor_lento fase3_consumidor_lento
	bash tests/test_stress_concurrente.sh ./$(STRESS_TARGET) "ESTRES: ALTA CONTENCION"
	RUNS=3 bash tests/test_stress_concurrente.sh ./fase3_productor_lento "PRODUCTORES MAS LENTOS"
	RUNS=3 bash tests/test_stress_concurrente.sh ./fase3_consumidor_lento "CONSUMIDORES MAS LENTOS"

test-fallos: fase3_fallos
	@printf "\n=== RECUPERACION ANTE ERRORES ===\n"
	bash tests/test_fallos_concurrente.sh ./fase3_fallos

test-filosofos: $(FILOSOFOS_STRESS_TARGET)
	@printf "\n=== FILOSOFOS COMENSALES: PRUEBA NORMAL ===\n"
	RUNS=1 bash tests/test_filosofos.sh ./$(FILOSOFOS_TARGET) "FILOSOFOS NORMAL"
	@printf "\n=== FILOSOFOS COMENSALES: ESTRES (5 filosofos, 500 rondas) ===\n"
	bash tests/test_filosofos.sh ./$(FILOSOFOS_STRESS_TARGET) "FILOSOFOS ESTRES"

test-lectores-escritores: $(RW_TARGET)
	bash tests/test_lectores_escritores.sh ./$(RW_TARGET)

test-peluquero: $(PELUQUERO_TARGET)
	bash tests/test_peluquero.sh ./$(PELUQUERO_TARGET)

clean:
	rm -f $(TARGET) $(CONCURRENT_TARGET) $(STRESS_TARGET) $(EXTRA_TARGETS) \
	       $(FILOSOFOS_TARGET) $(FILOSOFOS_STRESS_TARGET) $(RW_TARGET) \
	       $(PELUQUERO_TARGET) $(OBJECTS)
