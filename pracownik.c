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
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    if (shm == -1 || sem == -1) {
        perror("pracownik ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    printf("[PRACOWNIK %d] start procesu pracownika\n", getpid());

    while (r->otwarta) {
        sleep(2);

        lock(sem);

        printf(
            "[PRACOWNIK %d] na tasmie %d/%d talerzykow:\n",
            getpid(),
            r->tasma.count,
            P
        );

        int idx = r->tasma.head;
        for (int i = 0; i < r->tasma.count; i++) {
            struct talerzyk t = r->tasma.buf[idx];
            printf(
                "  -> kolor=%s ryby=%d cena=%d\n",
                nazwy_kolorow[t.kolor],
                t.ilosc_ryb,
                t.cena
            );
            idx = (idx + 1) % P;
        }

        unlock(sem);
    }

    return 0;
}
