#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#include <string.h>

#include "wspolne.h"

#define LICZBA_GRUP 40 

int main() {
    printf("[MAIN] Start systemu (MAX_GRUP=%d)\n", MAX_GRUP);
    srand(time(NULL));

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), IPC_CREAT | PRAWA);
    int sem = semget(key, 1, IPC_CREAT | PRAWA);
    int msg = msgget(key, IPC_CREAT | PRAWA);

    if (shm == -1 || sem == -1 || msg == -1) { perror("IPC init"); exit(1); }

    semctl(sem, 0, SETVAL, 1);

    struct restauracja *r = shmat(shm, NULL, 0);
    r->otwarta = 1;
    sprintf(r->info, "System uruchomiony. Czekam na gosci.");

    for (int i = 0; i < MAX_GRUP; i++) {
        r->grupa_zjedzone_cnt[i] = 0;
        r->gdzie_siedzimy[i] = -1; 
    }
    for (int i = 0; i < SEGMENTY; i++) r->tasma.seg[i].zajety = 0;
    for (int i = 0; i < LADA_MIEJSC; i++) {
        r->lada[i].zajete = 0; r->lada[i].segment = i + 1;
    }
    for (int i = 0; i < MAX_ZAMOWIEN; i++) r->zamowienia[i].aktywne = 0;
    
    for (int i = 0; i < STOLIKI; i++) {
        r->stoliki[i].ile_osob = 0;
        r->stoliki[i].segment = 10 + i;
        r->stoliki[i].id_grupy = -1;
        if (i < 3) r->stoliki[i].pojemnosc = 2;
        else if (i < 6) r->stoliki[i].pojemnosc = 3;
        else r->stoliki[i].pojemnosc = 4;
    }
    for (int i = 0; i < MAX_KLIENTOW; i++) r->klienci[i].aktywny = 0;

    printf("[MAIN] Start procesow...\n");
    if (fork() == 0) { execl("./kucharz", "kucharz", NULL); exit(0); }
    if (fork() == 0) { execl("./obsluga", "obsluga", NULL); exit(0); } // NOWA NAZWA
    if (fork() == 0) { execl("./tasma", "tasma", NULL); exit(0); }

    for (int g = 0; g < LICZBA_GRUP; g++) {
        int rozmiar = 1 + rand() % 4;
        for (int i = 0; i < rozmiar; i++) {
            if (fork() == 0) {
                char gid[10], gsize[10];
                sprintf(gid, "%d", g);
                sprintf(gsize, "%d", rozmiar);
                execl("./klient", "klient", gid, gsize, NULL);
                exit(1);
            }
        }
        sleep(1);
    }

    while (1) sleep(10);
}