#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "wspolne.h"

// Zmienna globalna kontrolujaca predkosc (w mikrosekundach)
// Domyslnie 1 sekunda = 1000000 us
volatile int delay = 1000000;

void handler_szybciej(int sig) {
    delay = 500000; // 0.5s (2x szybciej)
    // Nie uzywamy printf w handlerach (unsafe), ale w symulacji ujdzie
}

void handler_wolniej(int sig) {
    delay = 2000000; // 2.0s (50% wolniej / ilosci)
}

int main() {
    srand(getpid() ^ time(NULL));

    // Rejestracja sygnalow
    signal(SIGUSR1, handler_szybciej);
    signal(SIGUSR2, handler_wolniej);

    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) exit(1);
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    if (shm == -1 || sem == -1) exit(1);
    struct restauracja *r = shmat(shm, NULL, 0);

    // Rejestracja PID dla Kierownika
    lock(sem);
    r->pid_kucharza = getpid();
    sprintf(r->info, "KUCHARZ: Gotowy (PID=%d)!", getpid());
    unlock(sem);

    while (r->otwarta) {
        // Uzywamy usleep zamiast sleep dla precyzji
        usleep(delay);

        int k = -1;
        int dla_kogo = -1;
        int idx_zamowienia = -1;

        lock(sem);
        for(int i=0; i<MAX_ZAMOWIEN; i++) {
            if (r->zamowienia[i].aktywne) {
                k = r->zamowienia[i].typ_dania;
                dla_kogo = r->zamowienia[i].pid_klienta;
                idx_zamowienia = i;
                
                sprintf(r->info, "KUCHARZ: Specjalne %s dla %d...", nazwy_kolorow[k], dla_kogo);
                break; 
            }
        }
        unlock(sem);

        if (k == -1) {
            k = rand() % 3;
            dla_kogo = -1;
        }

        struct talerzyk nowy;
        nowy.kolor = k;
        nowy.cena = ceny[k];
        nowy.ilosc_ryb = (rand() % 2) + 1;
        nowy.id_odbiorcy = dla_kogo;

        int polozony = 0;
        while (r->otwarta && !polozony) {
            lock(sem);
            if (!r->tasma.seg[0].zajety) {
                r->tasma.seg[0].zajety = 1;
                r->tasma.seg[0].t = nowy;
                r->wyprodukowane[k]++;
                
                if (idx_zamowienia != -1) {
                    r->zamowienia[idx_zamowienia].aktywne = 0;
                    sprintf(r->info, "KUCHARZ: Wydano %s dla %d!", nazwy_kolorow[k], dla_kogo);
                }
                
                polozony = 1;
            }
            unlock(sem);
            if (!polozony) usleep(500000); // 0.5s czekania na miejsce na tasmie
        }
    }
    return 0;
}