#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define PRAWA 0600
#define P 5          // maksymalna liczba talerzykow na tasmie
#define KOLORY 3     

struct talerzyk {
    int kolor;       
    int cena;        
    int ilosc_ryb;   
};

struct tasma {
    struct talerzyk buf[P];
    int head;
    int tail;
    int count;
};

struct restauracja {
    int otwarta;
    struct tasma tasma;
    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];
};


int main() {
    printf("[MAIN] start programu\n");
    srand(time(NULL));

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), IPC_CREAT | PRAWA);
    if (shm == -1) { perror("shmget"); exit(1); }

    int sem = semget(key, 1, IPC_CREAT | PRAWA);
    if (sem == -1) { perror("semget"); exit(1); }

    if (semctl(sem, 0, SETVAL, 1) == -1) {
        perror("semctl");
        exit(1);
    }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    r->otwarta = 1;
    r->tasma.head = 0;
    r->tasma.tail = 0;
    r->tasma.count = 0;

    for (int i = 0; i < KOLORY; i++) {
        r->wyprodukowane[i] = 0;
        r->sprzedane[i] = 0;
    }

    printf(
        "[MAIN][STAN] restauracja otwarta, liczba potraw = %d\n",
        r->tasma.count
    );

    printf("[MAIN] IPC gotowe, uruchamiam procesy\n");

    if (fork() == 0) execl("./pracownik", "pracownik", NULL);
    if (fork() == 0) execl("./kucharz", "kucharz", NULL);
    if (fork() == 0) execl("./klient", "klient", NULL);

    sleep(10);

    printf("[MAIN] czekam na zakonczenie procesow\n");
    while (wait(NULL) > 0);

    shmdt(r);
    shmctl(shm, IPC_RMID, NULL);
    semctl(sem, 0, IPC_RMID);

    return 0;
}
