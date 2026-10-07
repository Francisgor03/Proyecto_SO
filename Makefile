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

.PHONY: all run concurrent test test-shell test-concurrent test-stress test-fallos clean

all: $(TARGET) $(CONCURRENT_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(CONCURRENT_TARGET): $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(CONCURRENT_SOURCES) -o $@

$(STRESS_TARGET): $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=1000 -DBUFFER_SIZE=1 -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=0 -pthread $(CONCURRENT_SOURCES) -o $@

fase3_productor_lento: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=100 -DPAUSA_PRODUCTOR_MS=1 -DPAUSA_CONSUMIDOR_MS=0 $(CONCURRENT_SOURCES) -o $@

fase3_consumidor_lento: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DNUM_PRODUCTORES=10 -DNUM_CONSUMIDORES=10 -DTOTAL_ITEMS=100 -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=1 $(CONCURRENT_SOURCES) -o $@

fase3_fallos: $(CONCURRENT_SOURCES) $(CONCURRENT_HEADERS) tests/fallos_concurrente.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -DPAUSA_PRODUCTOR_MS=0 -DPAUSA_CONSUMIDOR_MS=0 $(CONCURRENT_SOURCES) tests/fallos_concurrente.c -Wl,--wrap=pthread_create -Wl,--wrap=sem_wait -o $@

concurrent: $(CONCURRENT_TARGET)
	./$(CONCURRENT_TARGET)

run: $(TARGET)
	./$(TARGET)

test: test-shell test-concurrent test-stress test-fallos

test-shell: $(TARGET)
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "Prueba de redireccion: OK"

test-concurrent: $(CONCURRENT_TARGET)
	RUNS=1 bash tests/test_stress_concurrente.sh ./$(CONCURRENT_TARGET)

test-stress: $(STRESS_TARGET) fase3_productor_lento fase3_consumidor_lento
	bash tests/test_stress_concurrente.sh ./$(STRESS_TARGET)
	RUNS=3 bash tests/test_stress_concurrente.sh ./fase3_productor_lento
	RUNS=3 bash tests/test_stress_concurrente.sh ./fase3_consumidor_lento

test-fallos: fase3_fallos
	bash tests/test_fallos_concurrente.sh ./fase3_fallos

clean:
	rm -f $(TARGET) $(CONCURRENT_TARGET) $(STRESS_TARGET) $(EXTRA_TARGETS) $(OBJECTS)
