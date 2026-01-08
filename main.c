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
    int otwarta;        // 1 = otwarta, 0 = zamknieta
    int liczba_potraw;  // licznik potraw (debug)
};

void zablokuj_semafor(int semafor_id) {
    struct sembuf op = {0, -1, 0};
    if (semop(semafor_id, &op, 1) == -1) {
        perror("semop lock");
        exit(1);
    }
}

void odblokuj_semafor(int semafor_id) {
    struct sembuf op = {0, 1, 0};
    if (semop(semafor_id, &op, 1) == -1) {
        perror("semop unlock");
        exit(1);
    }
}

void wypisz_stan_restauracji(struct restauracja *rest) {
    printf(
        "[MAIN][STAN] otwarta = %d, liczba_potraw = %d\n",
        rest->otwarta,
        rest->liczba_potraw
    );
}

int main() {
    printf("[MAIN] start programu\n");

    key_t klucz = ftok("ipc_keyfile", 'R');
    if (klucz == -1) {
        perror("ftok");
        exit(1);
    }

    int pamiec_id = shmget(
        klucz,
        sizeof(struct restauracja),
        IPC_CREAT | PRAWA
    );
    if (pamiec_id == -1) {
        perror("shmget");
        exit(1);
    }

    int semafor_id = semget(klucz, 1, IPC_CREAT | PRAWA);
    if (semafor_id == -1) {
        perror("semget");
        exit(1);
    }

    if (semctl(semafor_id, 0, SETVAL, 1) == -1) {
        perror("semctl SETVAL");
        exit(1);
    }

    struct restauracja *rest = shmat(pamiec_id, NULL, 0);
    if (rest == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    zablokuj_semafor(semafor_id);
    rest->otwarta = 1;
    rest->liczba_potraw = 0;
    wypisz_stan_restauracji(rest);
    odblokuj_semafor(semafor_id);

    shmdt(rest);

    printf("[MAIN] IPC gotowe, uruchamiam proces pracownika\n");

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        execl("./pracownik", "pracownik", NULL);
        perror("execl");
        exit(1);
    }

    // Okresowy podglad stanu restauracji
    for (int i = 1; i <= 5; i++) {
        sleep(1);

        rest = shmat(pamiec_id, NULL, 0);
        if (rest == (void *)-1) {
            perror("shmat");
            exit(1);
        }

        zablokuj_semafor(semafor_id);
        wypisz_stan_restauracji(rest);
        odblokuj_semafor(semafor_id);

        shmdt(rest);
    }

    printf("[MAIN] czekam na zakonczenie pracownika\n");
    wait(NULL);

    printf("[MAIN] usuwam zasoby IPC\n");
    shmctl(pamiec_id, IPC_RMID, NULL);
    semctl(semafor_id, 0, IPC_RMID);

    printf("[MAIN] koniec programu\n");
    return 0;
}

