#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define PRAWA 0600
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

int main() {
    printf("[MAIN] start programu\n");

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja),
                     IPC_CREAT | PRAWA);
    if (shm == -1) { perror("shmget"); exit(1); }

    int sem = semget(key, 1, IPC_CREAT | PRAWA);
    if (sem == -1) { perror("semget"); exit(1); }

    semctl(sem, 0, SETVAL, 1);

    int msg = msgget(key, IPC_CREAT | PRAWA);
    if (msg == -1) { perror("msgget"); exit(1); }

    struct restauracja *r = shmat(shm, NULL, 0);
    r->otwarta = 1;
    r->tasma.head = 0;
    r->tasma.tail = 0;
    r->tasma.count = 0;
    shmdt(r);

    printf("[MAIN] uruchamiam obsluge\n");
    if (fork() == 0) {
        execl("./pracownik", "pracownik", NULL);
        perror("execl obsluga");
        exit(1);
    }

    printf("[MAIN] uruchamiam kucharza\n");
    if (fork() == 0) {
        execl("./kucharz", "kucharz", NULL);
        perror("execl kucharz");
        exit(1);
    }

    wait(NULL);
    wait(NULL);

    printf("[MAIN] sprzatam IPC\n");
    shmctl(shm, IPC_RMID, NULL);
    semctl(sem, 0, IPC_RMID);
    msgctl(msg, IPC_RMID, NULL);

    printf("[MAIN] koniec programu\n");
    return 0;
}
