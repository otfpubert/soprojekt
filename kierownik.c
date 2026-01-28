#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) { perror("ftok"); exit(1); }

    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) { perror("shmget (Czy main dziala?)"); exit(1); }

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) { perror("shmat"); exit(1); }

    printf("=== PANEL KIEROWNIKA ===\n");
    printf("1. Kucharz: Szybciej! (SIGUSR1)\n");
    printf("2. Kucharz: Wolniej! (SIGUSR2)\n");
    printf("3. EWAKUACJA! (Wyrzuc klientow)\n");
    printf("0. Wyjscie\n");

    while (1) {
        printf("\nWybor: ");
        int wybor;
        if (scanf("%d", &wybor) != 1) break;

        if (wybor == 0) break;
        
        
        pid_t kucharz = r->pid_kucharza;

        switch(wybor) {
            case 1:
                if (kucharz > 0) {
                    kill(kucharz, SIGUSR1);
                    printf("-> Wyslano SIGUSR1 do Kucharza (%d)\n", kucharz);
                } else printf("Blad: Kucharz nie zglosil PIDu!\n");
                break;
            case 2:
                if (kucharz > 0) {
                    kill(kucharz, SIGUSR2);
                    printf("-> Wyslano SIGUSR2 do Kucharza (%d)\n", kucharz);
                } else printf("Blad: Kucharz nie zglosil PIDu!\n");
                break;
            case 3:
                printf("-> EWAKUACJA!\n");
                
                for (int i=0; i<MAX_KLIENTOW; i++) {
                    if (r->klienci[i].aktywny) {
                        kill(r->klienci[i].pid, SIGTERM);
                    }
                }
                r->otwarta = 0; 
                break;
            default:
                printf("Nieznana opcja.\n");
        }
    }

    shmdt(r);
    return 0;
}