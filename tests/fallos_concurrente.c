/* Inyección exclusiva de pruebas; no se enlaza con los binarios normales. */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

int __real_pthread_create(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
int __real_sem_wait(sem_t *);

int __wrap_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start)(void *), void *arg) {
    static int llamadas;
    llamadas++;
    const char *modo = getenv("FALLO");
    if (modo && strcmp(modo, "crear") == 0 && llamadas == 4) return EAGAIN;
    return __real_pthread_create(thread, attr, start, arg);
}

int __wrap_sem_wait(sem_t *sem) {
    static _Thread_local int primera = 1;
    const char *modo = getenv("FALLO");
    if (primera && modo) {
        primera = 0;
        if (strcmp(modo, "eintr") == 0 || strcmp(modo, "esperar") == 0) {
            errno = strcmp(modo, "eintr") == 0 ? EINTR : EIO;
            return -1;
        }
    }
    return __real_sem_wait(sem);
}
