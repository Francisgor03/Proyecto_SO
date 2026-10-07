#include "sincronizacion.h"

int inicializar_sincronizacion(buffer_t *b) {
    if (sem_init(&b->sem_vacios, 0, BUFFER_SIZE) != 0) {
        return -1;
    }

    if (sem_init(&b->sem_llenos, 0, 0) != 0) {
        sem_destroy(&b->sem_vacios);
        return -1;
    }

    if (pthread_mutex_init(&b->mutex_buffer, NULL) != 0) {
        sem_destroy(&b->sem_llenos);
        sem_destroy(&b->sem_vacios);
        return -1;
    }

    if (monitor_inicializar(&b->monitor) != 0) {
        pthread_mutex_destroy(&b->mutex_buffer);
        sem_destroy(&b->sem_llenos);
        sem_destroy(&b->sem_vacios);
        return -1;
    }
    return 0;
}

void destruir_sincronizacion(buffer_t *b) {
    monitor_destruir(&b->monitor);
    pthread_mutex_destroy(&b->mutex_buffer);
    sem_destroy(&b->sem_llenos);
    sem_destroy(&b->sem_vacios);
}