#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define SEGMENTY 20

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
        perror("tasma ipc");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    printf("[TASMA] start procesu tasmy\n");

    while (r->otwarta) {
        sleep(3);

        lock(sem);

        struct segment_tasmy tmp[SEGMENTY];

        for (int i = 0; i < SEGMENTY; i++) {
            tmp[i] = r->tasma.seg[i];
        }

        for (int i = 0; i < SEGMENTY; i++) {
            r->tasma.seg[(i + 1) % SEGMENTY] = tmp[i];
        }

        unlock(sem);
    }

    return 0;
}
