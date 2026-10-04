CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -pedantic -D_POSIX_C_SOURCE=200809L
CPPFLAGS := -Iinclude
TARGET := mi_shell
SOURCES := src/main.c src/procesos.c src/archivos.c src/senales.c
OBJECTS := $(SOURCES:.c=.o)
CONCURRENT_TARGET := fase2_concurrente
CONCURRENT_SOURCES := src/main_concurrente.c src/hilos.c src/sincronizacion.c

.PHONY: all run concurrent test test-concurrent clean

all: $(TARGET) $(CONCURRENT_TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(CONCURRENT_TARGET): $(CONCURRENT_SOURCES) include/concurrencia.h include/sincronizacion.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(CONCURRENT_SOURCES) -o $@

concurrent: $(CONCURRENT_TARGET)
	./$(CONCURRENT_TARGET)

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	printf 'contenido de prueba\n' > /tmp/mi_shell_entrada.txt
	printf 'cat < /tmp/mi_shell_entrada.txt > /tmp/mi_shell_salida.txt\nexit\n' | ./$(TARGET)
	cmp /tmp/mi_shell_entrada.txt /tmp/mi_shell_salida.txt
	@echo "Prueba de redireccion: OK"

test-concurrent: $(CONCURRENT_TARGET)
	./$(CONCURRENT_TARGET)
	@echo "Prueba productor-consumidor: OK"

clean:
	rm -f $(TARGET) $(CONCURRENT_TARGET) $(OBJECTS)