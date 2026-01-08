#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>

#define PRAWA 0600

struct restauracja {
    int otwarta;
    int liczba_potraw;
};

void lock(int sem) {
    struct sembuf op = {0, -1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop lock");
        exit(1);
    }
}

void unlock(int sem) {
    struct sembuf op = {0, 1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop unlock");
        exit(1);
    }
}

void wypisz_stan(struct restauracja *r) {
    printf("[MAIN][STAN] otwarta=%d, liczba_potraw=%d\n",
           r->otwarta, r->liczba_potraw);
}

int main() {
    printf("[MAIN] start programu\n");

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja),
                     IPC_CREAT | PRAWA);
    if (shm == -1) { perror("shmget"); exit(1); }

    int sem = semget(key, 1, IPC_CREAT | PRAWA);
    if (sem == -1) { perror("semget"); exit(1); }

    if (semctl(sem, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) { perror("shmat"); exit(1); }

    lock(sem);
    r->otwarta = 1;
    r->liczba_potraw = 0;
    wypisz_stan(r);
    unlock(sem);

    shmdt(r);

    printf("[MAIN] uruchamiam proces obslugi\n");
    if (fork() == 0) {
        execl("./pracownik", "pracownik", NULL);
        perror("execl obsluga");
        exit(1);
    }

    printf("[MAIN] uruchamiam proces kucharza\n");
    if (fork() == 0) {
        execl("./kucharz", "kucharz", NULL);
        perror("execl kucharz");
        exit(1);
    }

    for (int i = 0; i < 5; i++) {
        sleep(1);
        r = shmat(shm, NULL, 0);
        lock(sem);
        wypisz_stan(r);
        unlock(sem);
        shmdt(r);
    }

    printf("[MAIN] czekam na zakonczenie procesow\n");
    wait(NULL);
    wait(NULL);

    printf("[MAIN] usuwam IPC\n");
    shmctl(shm, IPC_RMID, NULL);
    semctl(sem, 0, IPC_RMID);

    printf("[MAIN] koniec programu\n");
    return 0;
}
