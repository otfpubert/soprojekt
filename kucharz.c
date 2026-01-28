/*
 * kucharz.c - Proces kucharza restauracji
 *
 * Odpowiada za:
 * - Produkcję dań podstawowych (losowych) i specjalnych (na zamówienie)
 * - Umieszczanie talerzyków na taśmie (segment 0)
 * - Reagowanie na sygnały kierownika (SIGUSR1/SIGUSR2)
 */

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

/*
 * Zmienna globalna kontrolująca prędkość produkcji (w mikrosekundach).
 * volatile - może być zmieniana przez handlery sygnałów.
 * Domyślnie 1 sekunda = 1000000 us
 */
volatile int delay = 1000000;

/*
 * Handler sygnału SIGUSR1 - przyspieszenie produkcji 2x.
 * Zgodne z wymaganiem: "Na polecenie kierownika (sygnał 1) obsługa
 * przyśpiesza dwukrotnie wydawanie dań z kuchni."
 */
void handler_szybciej(int sig) {
    (void)sig;
    delay = 500000; /* 0.5s (2x szybciej) */
}

/*
 * Handler sygnału SIGUSR2 - spowolnienie produkcji o 50%.
 * Zgodne z wymaganiem: "Na polecenie kierownika (sygnał 2) obsługa
 * zmniejsza o 50% ilość wydawanych dań z kuchni."
 */
void handler_wolniej(int sig) {
    (void)sig; /* Suppress unused parameter warning */
    delay = 2000000; /* 2.0s (50% wolniej) */
}

int main() {
    printf("[KUCHARZ] Start procesu kucharza (PID=%d)\n", getpid());
    srand(getpid() ^ time(NULL));

    /* Rejestracja handlerów sygnałów */
    if (signal(SIGUSR1, handler_szybciej) == SIG_ERR) {
        perror("[KUCHARZ] signal(SIGUSR1)");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGUSR2, handler_wolniej) == SIG_ERR) {
        perror("[KUCHARZ] signal(SIGUSR2)");
        exit(EXIT_FAILURE);
    }

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[KUCHARZ] ftok");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z zasobami IPC */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[KUCHARZ] shmget");
        exit(EXIT_FAILURE);
    }

    int sem = semget(key, 1, 0);
    if (sem == -1) {
        perror("[KUCHARZ] semget");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[KUCHARZ] shmat");
        exit(EXIT_FAILURE);
    }

    printf("[KUCHARZ] Polaczono z zasobami IPC\n");

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
            if (!polozony) usleep(500000); /* 0.5s czekania na miejsce na taśmie */
        }
    }

    printf("[KUCHARZ] Restauracja zamknieta. Konczenie pracy.\n");

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[KUCHARZ] shmdt");
    }

    return 0;
}