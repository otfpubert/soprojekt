/*
 * tasma.c - Proces taśmy transportowej (conveyor belt)
 *
 * Odpowiada za:
 * - Cykliczne przesuwanie talerzyków na taśmie
 * - Ruch: segment[i] -> segment[(i+1) % SEGMENTY]
 * - Reagowanie na sygnały kierownika (przyspieszenie/spowolnienie)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <signal.h>

#include "wspolne.h"

/*
 * Zmienna globalna kontrolująca prędkość taśmy (w mikrosekundach).
 * Domyślnie 1 sekunda = 1000000 us
 */
volatile int delay = 1000000;

/* Handler SIGUSR1 - przyspieszenie taśmy 2x */
void handler_szybciej(int sig) {
    (void)sig;
    delay = 500000; /* 0.5s (2x szybciej) */
}

/* Handler SIGUSR2 - spowolnienie taśmy o 50% */
void handler_wolniej(int sig) {
    (void)sig;
    delay = 2000000; /* 2.0s (50% wolniej) */
}

int main() {
    printf("[TASMA] Start procesu tasmy (PID=%d)\n", getpid());

    /* Rejestracja handlerów sygnałów */
    if (signal(SIGUSR1, handler_szybciej) == SIG_ERR) {
        perror("[TASMA] signal(SIGUSR1)");
    }
    if (signal(SIGUSR2, handler_wolniej) == SIG_ERR) {
        perror("[TASMA] signal(SIGUSR2)");
    }

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[TASMA] ftok");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z zasobami IPC */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[TASMA] shmget");
        exit(EXIT_FAILURE);
    }

    int sem = semget(key, 1, 0);
    if (sem == -1) {
        perror("[TASMA] semget");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[TASMA] shmat");
        exit(EXIT_FAILURE);
    }

    printf("[TASMA] Polaczono z zasobami IPC (shm=%d, sem=%d)\n", shm, sem);

    /* Zapisanie PID taśmy dla kierownika */
    lock(sem);
    r->pid_tasmy = getpid();
    unlock(sem);

    while (r->otwarta) {
        usleep(delay); /* Sterowane sygnałami SIGUSR1/SIGUSR2 */

        lock(sem);

        struct segment_tasmy tmp[SEGMENTY];

        /* kopia stanu tasmy */
        for (int i = 0; i < SEGMENTY; i++) {
            tmp[i] = r->tasma.seg[i];
        }

        /* czysty ruch cykliczny 0..19 */
        for (int i = 0; i < SEGMENTY; i++) {
            r->tasma.seg[(i + 1) % SEGMENTY] = tmp[i];
        }

        unlock(sem);
    }

    printf("[TASMA] Restauracja zamknieta. Konczenie pracy.\n");

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[TASMA] shmdt");
    }

    return 0;
}
