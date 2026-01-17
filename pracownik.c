#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>

#include "wspolne.h"

int main() {
    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) exit(1);
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    if (shm == -1 || sem == -1) exit(1);
    struct restauracja *r = shmat(shm, NULL, 0);

    printf("[PRACOWNIK] Start monitora\n");

    while (r->otwarta) {
        system("clear");
        lock(sem);

        printf("\n=== PODGLAD RESTAURACJI (Odswiezanie co 2s) ===\n");
        printf("------------------------------------------------------------------------------------------------\n");
        printf("| SEG |       MIEJSCE       |          NA TASMIE           |             KLIENCI                \n");
        printf("------------------------------------------------------------------------------------------------\n");

        for (int seg = 0; seg < SEGMENTY; seg++) {
            printf("| %02d  | ", seg);

            char bufor_miejsca[30];
            if (seg == 0) sprintf(bufor_miejsca, "KUCHARZ");
            else if (seg >= 1 && seg <= 9) {
                int idx = seg - 1;
                sprintf(bufor_miejsca, "LADA %d %s", idx, r->lada[idx].zajete ? "(ZAJ)" : "(wol)");
            }
            else {
                int idx = seg - 10;
                sprintf(bufor_miejsca, "STOL %d (%d/%d)", idx, r->stoliki[idx].ile_osob, r->stoliki[idx].pojemnosc);
            }
            printf("%-18s | ", bufor_miejsca);

            char bufor_jedzenia[40];
            if (r->tasma.seg[seg].zajety) {
                struct talerzyk t = r->tasma.seg[seg].t;
                if (t.id_odbiorcy != -1) {
                    sprintf(bufor_jedzenia, "%s($%d)->%d", nazwy_kolorow[t.kolor], t.cena, t.id_odbiorcy);
                } else {
                    sprintf(bufor_jedzenia, "%s ($%d)", nazwy_kolorow[t.kolor], t.cena);
                }
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
                    // Zmienione: Gwiazdka (*) zamiast [VIP] jesli czeka na specjalne
                    printf("PID=%d%s G=%d (%d/%d)", k->pid, k->czeka_na_specjalne ? "*" : "", k->id_grupy, k->zjedzone, k->limit);
                    found = 1;
                }
            }
            printf("\n");
        }
        
        printf("------------------------------------------------------------------------------------------------\n");
        printf("SPRZEDAZ: Nieb:%d Czer:%d Ziel:%d | BRAZ:%d SREB:%d ZLOT:%d\n",
            r->sprzedane[0], r->sprzedane[1], r->sprzedane[2],
            r->sprzedane[3], r->sprzedane[4], r->sprzedane[5]);
        
        printf("\n[INFO]: %s\n", r->info);
        printf("====================================================\n");

        unlock(sem);
        sleep(2); 
    }

    shmdt(r);
    return 0;
}