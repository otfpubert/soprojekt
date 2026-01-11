#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define KOLORY 3
#define SEGMENTY 20

const char *nazwy_kolorow[KOLORY] = {
    "niebieski",
    "czerwony",
    "zielony"
};

struct talerzyk {
    int kolor;
    int cena;
    int ilosc_ryb;
};

struct segment_tasmy {
    int zajety;
    struct talerzyk t;
};

struct tasma {
    struct segment_tasmy seg[SEGMENTY];
};

struct restauracja {
    int otwarta;
    struct tasma tasma;
    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];
};

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
    int shm = shmget(key, sizeof(struct restauracja), 0);

    if (shm == -1) {
        perror("klient shmget");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    printf("[KLIENT %d] start procesu klienta (brak interakcji z tasma)\n", getpid());

    while (r->otwarta) {
        sleep(5);
    }

    return 0;
}
