#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[TASMA] ftok");
        exit(1);
    }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    if (shm == -1 || sem == -1) {
        perror("[TASMA] ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) {
        perror("[TASMA] shmat");
        exit(1);
    }

    printf("[TASMA] start procesu tasmy\n");

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

    shmdt(r);
    return 0;
}
