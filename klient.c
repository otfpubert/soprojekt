#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define P 5
#define KOLORY 3

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

struct tasma {
    struct talerzyk buf[P];
    int head;
    int tail;
    int count;
};

struct restauracja {
    int otwarta;
    struct tasma tasma;
    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];
};

void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem, &sb, 1) == -1)
        perror("semop lock");
}

void unlock(int sem) {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem, &sb, 1) == -1)
        perror("semop unlock");
}

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    if (shm == -1 || sem == -1) {
        perror("klient ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    printf("[KLIENT %d] start procesu klienta\n", getpid());

    int do_zjedzenia = (rand() % 8) + 3; // 3..10
    int zjedzone = 0;
    int rachunek = 0;

    while (zjedzone < do_zjedzenia) {
        sleep(3);

        lock(sem);

        if (r->tasma.count > 0) {
            struct talerzyk t = r->tasma.buf[r->tasma.head];
            r->tasma.head = (r->tasma.head + 1) % P;
            r->tasma.count--;
            r->sprzedane[t.kolor]++;

            zjedzone++;
            rachunek += t.cena;

            printf(
                "[KLIENT %d] zjadlem talerzyk kolor=%s ryby=%d cena=%d\n",
                getpid(),
                nazwy_kolorow[t.kolor],
                t.ilosc_ryb,
                t.cena
            );
        }

        unlock(sem);
    }

    printf(
        "[KLIENT %d] koncze jedzenie, rachunek=%d\n",
        getpid(),
        rachunek
    );

    return 0;
}
