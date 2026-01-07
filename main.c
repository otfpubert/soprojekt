#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <errno.h>

#define PRAWA 0600

int main() {
    printf("[MAIN] start programu\n");

    key_t klucz = ftok("ipc_keyfile", 'R');
    if (klucz == -1) {
        perror("ftok");
        exit(1);
    }

    int pamiec_id = shmget(klucz, sizeof(int), IPC_CREAT | PRAWA);
    if (pamiec_id == -1) {
        perror("shmget");
        exit(1);
    }

    int semafor_id = semget(klucz, 1, IPC_CREAT | PRAWA);
    if (semafor_id == -1) {
        perror("semget");
        exit(1);
    }

    semctl(semafor_id, 0, SETVAL, 1);

    int *licznik = shmat(pamiec_id, NULL, 0);
    *licznik = 0;
    shmdt(licznik);

    printf("[MAIN] IPC gotowe, uruchamiam pracownika\n");

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

    for (int i = 1; i <= 5; i++) {
        sleep(1);
        printf("[MAIN] nadal dzialam (%d)\n", i);
    }

    printf("[MAIN] czekam na pracownika\n");
    wait(NULL);

    printf("[MAIN] sprzatam zasoby IPC\n");
    shmctl(pamiec_id, IPC_RMID, NULL);
    semctl(semafor_id, 0, IPC_RMID);

    printf("[MAIN] koniec programu\n");
    return 0;
}
