#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <errno.h>

struct restauracja {
    int otwarta;
    int liczba_potraw;
};

volatile sig_atomic_t dzialaj = 1;

void obsluga_sygnalu(int sig) {
    printf("[KUCHARZ %d] odebralem SIGTERM\n", getpid());
    dzialaj = 0;
}

void lock(int sem) {
    struct sembuf op = {0, -1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop lock");
        _exit(1);
    }
}

void unlock(int sem) {
    struct sembuf op = {0, 1, 0};
    if (semop(sem, &op, 1) == -1) {
        perror("semop unlock");
        _exit(1);
    }
}

int main() {
    printf("[KUCHARZ %d] start procesu kucharza\n", getpid());
    signal(SIGTERM, obsluga_sygnalu);

    key_t klucz = ftok("ipc_keyfile", 'R');
    if (klucz == -1) {
        perror("ftok");
        return 1;
    }

    int pamiec_id = shmget(klucz, sizeof(struct restauracja), 0);
    if (pamiec_id == -1) {
        perror("shmget");
        return 1;
    }

    int semafor_id = semget(klucz, 1, 0);
    if (semafor_id == -1) {
        perror("semget");
        return 1;
    }

    struct restauracja *rest = shmat(pamiec_id, NULL, 0);
    if (rest == (void *)-1) {
        perror("shmat");
        return 1;
    }

    while (dzialaj) {
        sleep(2); // kucharz produkuje wolniej

        lock(semafor_id);
        rest->liczba_potraw++;
        printf(
            "[KUCHARZ %d] przygotowalem potrawe, razem = %d\n",
            getpid(),
            rest->liczba_potraw
        );
        unlock(semafor_id);
    }

    printf("[KUCHARZ %d] koncze prace\n", getpid());
    shmdt(rest);
    return 0;
}
