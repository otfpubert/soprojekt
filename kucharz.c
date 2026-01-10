#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <time.h>

#define P 5

struct potrawa {
    int cena;
};

struct tasma {
    struct potrawa buf[P];
    int head;
    int tail;
    int count;
};

struct restauracja {
    int otwarta;
    struct tasma tasma;
};

volatile sig_atomic_t dzialaj = 1;

void stop(int s) {
    printf("[KUCHARZ %d] SIGTERM, koncze\n", getpid());
    dzialaj = 0;
}

void lock(int sem) {
    struct sembuf o = {0, -1, 0};
    semop(sem, &o, 1);
}

void unlock(int sem) {
    struct sembuf o = {0, 1, 0};
    semop(sem, &o, 1);
}

int main() {
    printf("[KUCHARZ %d] start\n", getpid());
    signal(SIGTERM, stop);
    srand(time(NULL) ^ getpid());

    key_t key = ftok("ipc_keyfile", 'R');
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    struct restauracja *r = shmat(shm, NULL, 0);

    int ceny[3] = {10, 15, 20};

    while (dzialaj) {
        sleep(2);

        lock(sem);
        if (r->tasma.count < P) {
            int cena = ceny[rand() % 3];
            r->tasma.buf[r->tasma.tail].cena = cena;
            r->tasma.tail = (r->tasma.tail + 1) % P;
            r->tasma.count++;

            printf(
                "[KUCHARZ %d] dodalem potrawe %d zl, na tasmie = %d\n",
                getpid(), cena, r->tasma.count
            );
        } else {
            printf("[KUCHARZ %d] tasma pelna, czekam\n", getpid());
        }
        unlock(sem);
    }
    return 0;
}
