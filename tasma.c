/*
 * tasma.c - Proces taśmy transportowej (conveyor belt)
 *
 * Odpowiada za:
 * - Cykliczne przesuwanie talerzyków na taśmie co 1 sekundę
 * - Ruch: segment[i] -> segment[(i+1) % SEGMENTY]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    printf("[TASMA] Start procesu tasmy\n");

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

    while (r->otwarta) {
        sleep(1);

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
