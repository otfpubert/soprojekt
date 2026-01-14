#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) {
        perror("[PRACOWNIK] ftok");
        exit(1);
    }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    if (shm == -1 || sem == -1) {
        perror("[PRACOWNIK] ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) {
        perror("[PRACOWNIK] shmat");
        exit(1);
    }

    printf("[PRACOWNIK %d] start procesu pracownika\n", getpid());

    while (r->otwarta) {
        sleep(2);

        lock(sem);

        printf("\n[PRACOWNIK] PODGLAD TASMY:\n");

        for (int seg = 0; seg < SEGMENTY; seg++) {

            if (seg == 0) {
                printf(" [%02d] KUCHARZ | ", seg);
            }
            else if (seg >= 1 && seg <= 9) {
                int idx = seg - 1; 
                printf(
                    " [%02d] LADA %d/1 | ",
                    seg,
                    r->lada[idx].zajete
                );
            }
            else {
                int idx = seg - 10;
                printf(
                    " [%02d] STOLIK %d/4 | ",
                    seg,
                    r->stoliki[idx].ile_osob
                );
            }

            if (r->tasma.seg[seg].zajety) {
                struct talerzyk t = r->tasma.seg[seg].t;
                printf(
                    "%s ryby=%d cena=%d",
                    nazwy_kolorow[t.kolor],
                    t.ilosc_ryb,
                    t.cena
                );
            } else {
                printf("---");
            }

            printf("\n");
        }

        unlock(sem);
    }

    shmdt(r);
    return 0;
}
