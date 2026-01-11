#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#define SEGMENTY 20
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

void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    semop(sem, &sb, 1);
}

void unlock(int sem) {
    struct sembuf sb = {0, 1, 0};
    semop(sem, &sb, 1);
}

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
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
        printf("[PRACOWNIK] podglad tasmy:\n");
        for (int i = 0; i < SEGMENTY; i++) {
            if (r->tasma.seg[i].zajety)
                printf("  [%02d] TALERZ\n", i);
            else
                printf("  [%02d] ---\n", i);
        }
        unlock(sem);
    }

    return 0;
}
