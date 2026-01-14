#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>

#include "wspolne.h"

int main() {
    srand(getpid() ^ time(NULL));

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) {
        perror("[KLIENT] ftok");
        exit(1);
    }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        if (errno == ENOENT)
            printf("[KLIENT] brak polaczenia z serwerem\n");
        else
            perror("[KLIENT] shmget");
        exit(1);
    }

    int sem = semget(key, 1, 0);
    if (sem == -1) {
        perror("[KLIENT] semget");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) {
        perror("[KLIENT] shmat");
        exit(1);
    }

    printf("[KLIENT %d] start procesu klienta\n", getpid());

    int moj_segment;
    int typ_miejsca; /* 0 = lada, 1 = stolik */
    int idx_miejsca;

    lock(sem);

    if (rand() % 2 == 0) {
        idx_miejsca = rand() % LADA_MIEJSC;
        moj_segment = r->lada[idx_miejsca].segment;
        r->lada[idx_miejsca].zajete = 1;
        typ_miejsca = 0;

        printf(
            "[KLIENT %d] zajmuje miejsce przy ladzie (segment %d)\n",
            getpid(), moj_segment
        );
    } else {
        idx_miejsca = rand() % STOLIKI;
        moj_segment = r->stoliki[idx_miejsca].segment;
        r->stoliki[idx_miejsca].ile_osob = 1;
        typ_miejsca = 1;

        printf(
            "[KLIENT %d] zajmuje miejsce przy stoliku (segment %d)\n",
            getpid(), moj_segment
        );
    }

    unlock(sem);

    int zjedzone = 0;
    int do_zjedzenia = 3 + rand() % 8;

    printf(
        "[KLIENT %d] planuje zjesc %d potraw\n",
        getpid(), do_zjedzenia
    );

    while (r->otwarta && zjedzone < do_zjedzenia) {
        sleep(1);

        lock(sem);

        if (r->tasma.seg[moj_segment].zajety) {

            if (rand() % 100 < 20) { 
                struct talerzyk t = r->tasma.seg[moj_segment].t;
                r->tasma.seg[moj_segment].zajety = 0;
                r->sprzedane[t.kolor]++;
                zjedzone++;

                printf(
                    "[KLIENT %d] zjadlem: %s ryby=%d cena=%d (%d/%d)\n",
                    getpid(),
                    nazwy_kolorow[t.kolor],
                    t.ilosc_ryb,
                    t.cena,
                    zjedzone,
                    do_zjedzenia
                );

                unlock(sem);

                sleep(2 + rand() % 3);
                continue;
            }
        }

        unlock(sem);
    }

    printf(
        "[KLIENT %d] skonczylem jesc, opuszczam restauracje\n",
        getpid()
    );

    lock(sem);
    if (typ_miejsca == 0)
        r->lada[idx_miejsca].zajete = 0;
    else
        r->stoliki[idx_miejsca].ile_osob = 0;
    unlock(sem);

    shmdt(r);
    return 0;
}
