#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'Z');
    if (key == -1) exit(1);

    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    if (shm == -1 || sem == -1) exit(1);

    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void*)-1) exit(1);

    while (r->otwarta) {
        system("clear");
        lock(sem);

        printf("\n=== PODGLAD RESTAURACJI (Odswiezanie co 2s) ===\n");
        printf("---------------------------------------------------------------------------------\n");
        printf("| SEG |       MIEJSCE       |          NA TASMIE           |      KLIENCI       \n");
        printf("---------------------------------------------------------------------------------\n");

        for (int seg = 0; seg < SEGMENTY; seg++) {
            printf("| %02d  | ", seg);

            char bufor_miejsca[30];
            
            if (seg == 0) {
                sprintf(bufor_miejsca, "KUCHARZ");
            }
            else if (seg >= 1 && seg <= 9) {
                int idx = seg - 1;
                sprintf(bufor_miejsca, "LADA %d %s", idx, r->lada[idx].zajete ? "(ZAJ)" : "(wol)");
            }
            else {
                int idx = seg - 10;
                sprintf(bufor_miejsca, "STOL %d (%d/%d)", idx, r->stoliki[idx].ile_osob, r->stoliki[idx].pojemnosc);
            }
            printf("%-18s | ", bufor_miejsca);

            char bufor_jedzenia[30];
            if (r->tasma.seg[seg].zajety) {
                struct talerzyk t = r->tasma.seg[seg].t;
                sprintf(bufor_jedzenia, "%s ($%d)", nazwy_kolorow[t.kolor], t.cena);
            } else {
                sprintf(bufor_jedzenia, "---");
            }
            printf("%-20s | ", bufor_jedzenia);

            int found = 0;
            if (seg > 0) { 
                for (int i = 0; i < MAX_KLIENTOW; i++) {
                    struct klient_info *k = &r->klienci[i];
                    if (!k->aktywny) continue;
                    if (k->segment != seg) continue;

                    if (found) printf(", ");
                    printf("PID=%d G=%d (%d/%d)", k->pid, k->id_grupy, k->zjedzone, k->limit);
                    found = 1;
                }
            }
            printf("\n");
        }
        
        printf("---------------------------------------------------------------------------------\n");
        printf("Statystyki sprzedazy: Niebieski: %d, Czerwony: %d, Zielony: %d\n",
            r->sprzedane[0], r->sprzedane[1], r->sprzedane[2]);

        unlock(sem);
        sleep(5); 
    }

    shmdt(r);
    return 0;
}