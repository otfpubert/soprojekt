#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("[KLIENT] ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        if (errno == ENOENT)
            printf("[KLIENT] brak polaczenia z serwerem\n");
        else
            perror("[KLIENT] shmget");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("[KLIENT] shmat"); exit(1); }

    printf("[KLIENT %d] start procesu klienta\n", getpid());

    while (r->otwarta)
        sleep(5);

    shmdt(r);
    return 0;
}
