#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

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
    printf("[OBSLUGA %d] SIGTERM, koncze\n", getpid());
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
    printf("[OBSLUGA %d] start\n", getpid());
    signal(SIGTERM, stop);

    key_t key = ftok("ipc_keyfile", 'R');
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);

    struct restauracja *r = shmat(shm, NULL, 0);

    while (dzialaj) {
        sleep(1);

        lock(sem);
        printf("[OBSLUGA %d] tasma (%d/%d): ",
               getpid(), r->tasma.count, P);

        for (int i = 0; i < r->tasma.count; i++) {
            int idx = (r->tasma.head + i) % P;
            printf("%d ", r->tasma.buf[idx].cena);
        }
        printf("\n");
        unlock(sem);
    }
    return 0;
}
