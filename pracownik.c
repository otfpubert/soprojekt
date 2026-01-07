#include <stdio.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

volatile sig_atomic_t dzialaj = 1;

void obsluga_sygnalu(int sig) {
    printf("[PRACOWNIK %d] dostalem sygnal SIGTERM\n", getpid());
    dzialaj = 0;
}

int main() {
    printf("[PRACOWNIK %d] start\n", getpid());

    signal(SIGTERM, obsluga_sygnalu);

    key_t klucz = ftok("ipc_keyfile", 'R');
    int pamiec_id = shmget(klucz, sizeof(int), 0);
    int semafor_id = semget(klucz, 1, 0);

    int *licznik = shmat(pamiec_id, NULL, 0);

    while (dzialaj) {
        struct sembuf operacja = {0, -1, 0};
        semop(semafor_id, &operacja, 1);

        (*licznik)++;
        printf("[PRACOWNIK %d] zwiekszam licznik -> %d\n",
               getpid(), *licznik);

        operacja.sem_op = 1;
        semop(semafor_id, &operacja, 1);

        sleep(1);
    }

    printf("[PRACOWNIK %d] koncze prace\n", getpid());
    shmdt(licznik);
    return 0;
}
