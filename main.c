#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#include "wspolne.h"

#define LICZBA_GRUP 15

int main() {
    printf("[MAIN] start programu\n");
    srand(time(NULL));

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), IPC_CREAT | PRAWA);
    if (shm == -1) { perror("shmget"); exit(1); }

    int sem = semget(key, 1, IPC_CREAT | PRAWA);
    if (sem == -1) { perror("semget"); exit(1); }

    semctl(sem, 0, SETVAL, 1);

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    r->otwarta = 1;

    for (int i = 0; i < SEGMENTY; i++)
        r->tasma.seg[i].zajety = 0;

    for (int i = 0; i < LADA_MIEJSC; i++) {
        r->lada[i].zajete = 0;
        r->lada[i].segment = i + 1;
    }

    for (int i = 0; i < STOLIKI; i++) {
        r->stoliki[i].ile_osob = 0;
        r->stoliki[i].segment = 10 + i;
    }

    for (int i = 0; i < MAX_KLIENTOW; i++)
        r->klienci[i].aktywny = 0;


    printf("[MAIN] IPC gotowe, uruchamiam procesy\n");

    if (fork() == 0) execl("./kucharz", "kucharz", NULL);
    if (fork() == 0) execl("./pracownik", "pracownik", NULL);
    if (fork() == 0) execl("./tasma", "tasma", NULL);

    for (int g = 0; g < LICZBA_GRUP; g++) {
        int rozmiar = 1 + rand() % 4;

        for (int i = 0; i < rozmiar; i++) {
            if (fork() == 0) {
                char gid[8], gsize[8], lider[8];
                sprintf(gid, "%d", g);
                sprintf(gsize, "%d", rozmiar);
                sprintf(lider, "%d", i == 0);

                execl("./klient", "klient", gid, gsize, lider, NULL);
                exit(1);
            }
        }
        sleep(1);
    }

    while (1) sleep(1);
}
