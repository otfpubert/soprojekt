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

    key_t key = ftok("ipc_keyfile", 'Z');
    if (key == -1) {
        perror("[KUCHARZ] ftok");
        exit(1);
    }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    if (shm == -1 || sem == -1) {
        perror("[KUCHARZ] ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) {
        perror("[KUCHARZ] shmat");
        exit(1);
    }

    printf("[KUCHARZ %d] start procesu kucharza\n", getpid());

    while (r->otwarta) {

        sleep(5);

        int k = rand() % KOLORY;
        struct talerzyk nowy;
        nowy.kolor = k;
        nowy.cena = ceny[k];
        nowy.ilosc_ryb = (rand() % 2) + 1;

        printf(
            "[KUCHARZ %d] przygotowalem talerzyk: kolor=%s ryby=%d cena=%d\n",
            getpid(),
            nazwy_kolorow[k],
            nowy.ilosc_ryb,
            nowy.cena
        );

        int polozony = 0;

        while (r->otwarta && !polozony) {
            lock(sem);

            if (!r->tasma.seg[0].zajety) {
                r->tasma.seg[0].zajety = 1;
                r->tasma.seg[0].t = nowy;
                r->wyprodukowane[k]++;

                printf(
                    "[KUCHARZ %d] polozylem talerzyk na segmencie 0\n",
                    getpid()
                );

                polozony = 1;
            }

            unlock(sem);

            if (!polozony) {
                printf(
                    "[KUCHARZ %d] segment 0 zajety, czekam\n",
                    getpid()
                );
                sleep(1);
            }
        }
    }

    shmdt(r);
    return 0;
}
