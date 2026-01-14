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

    printf("[PRACOWNIK %d] start procesu podgladu tasmy\n", getpid());

    while (r->otwarta) {
        sleep(2);

        lock(sem);

        printf("\n[PRACOWNIK] PODGLAD TASMY:\n");

        for (int i = 0; i < SEGMENTY; i++) {

            if (i == 0) {
                printf(" [%02d] KUCHARZ | ", i);
            } else if (i >= 1 && i <= 9) {
                printf(" [%02d] LADA 0/1 | ", i);
            } else {
                int nr_stolika = i - 10;
                printf(
                    " [%02d] STOLIK %d/%d | ",
                    i,
                    r->stoliki[nr_stolika].ile_osob,
                    POJEMNOSC_STOLIKA
                );
            }

            if (r->tasma.seg[i].zajety) {
                struct talerzyk t = r->tasma.seg[i].t;
                printf(
                    "%s ryby=%d cena=%d\n",
                    nazwy_kolorow[t.kolor],
                    t.ilosc_ryb,
                    t.cena
                );
            } else {
                printf("---\n");
            }
        }

        unlock(sem);
    }

    shmdt(r);
    return 0;
}
