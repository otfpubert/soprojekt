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
    printf("[PRACOWNIK %d] odebralem sygnal SIGTERM\n", getpid());
    dzialaj = 0;
}

void zablokuj_semafor(int semafor_id) {
    struct sembuf op = {0, -1, 0};
    if (semop(semafor_id, &op, 1) == -1) {
        perror("semop lock");
        _exit(1);
    }
}

void odblokuj_semafor(int semafor_id) {
    struct sembuf op = {0, 1, 0};
    if (semop(semafor_id, &op, 1) == -1) {
        perror("semop unlock");
        _exit(1);
    }
}

int main() {
    printf("[PRACOWNIK %d] start procesu pracownika\n", getpid());

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
        zablokuj_semafor(semafor_id);

        rest->liczba_potraw++;
        printf(
            "[PRACOWNIK %d] dodalem potrawe, razem = %d\n",
            getpid(),
            rest->liczba_potraw
        );

        odblokuj_semafor(semafor_id);
        sleep(1);
    }

    printf("[PRACOWNIK %d] koncze prace i odlaczam pamiec\n", getpid());
    shmdt(rest);
    return 0;
}