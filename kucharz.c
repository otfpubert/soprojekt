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
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "wspolne.h"

/*
 * Flaga sygnału SIGALRM - ustawiana przez handler, czyszczona w pętli.
 */
volatile sig_atomic_t tick_flag = 0;

/*
 * Interwał produkcji w tickach (1-4).
 * Startuje od 2 (normalny), może spaść do 1 (2x szybciej) lub wzrosnąć do 4 (2x wolniej).
 */
volatile int production_delay = 2;

/*
 * Handler sygnału SIGALRM - ustawia flagę tick.
 */
void handler_alarm(int sig) {
    (void)sig;
    tick_flag = 1;
}

/*
 * Handler sygnału SIGUSR1 - przyspieszenie produkcji 2x.
 * Zgodne z wymaganiem: "Na polecenie kierownika (sygnał 1) obsługa
 * przyśpiesza dwukrotnie wydawanie dań z kuchni."
 */
void handler_szybciej(int sig) {
    (void)sig;
    production_delay = (production_delay > 1) ? production_delay / 2 : 1;
    printf("[KUCHARZ] Przyspieszenie! Nowy delay: %d tickow\n", production_delay);
}

/*
 * Handler sygnału SIGUSR2 - spowolnienie produkcji o 50%.
 * Zgodne z wymaganiem: "Na polecenie kierownika (sygnał 2) obsługa
 * zmniejsza o 50% ilość wydawanych dań z kuchni."
 */
void handler_wolniej(int sig) {
    (void)sig;
    production_delay = (production_delay < 4) ? production_delay * 2 : 4;
    printf("[KUCHARZ] Spowolnienie! Nowy delay: %d tickow\n", production_delay);
}

int main() {
    printf("[KUCHARZ] Start procesu kucharza (PID=%d)\n", getpid());
    srand(getpid() ^ time(NULL));

    /* Rejestracja handlerów sygnałów */
    if (signal(SIGALRM, handler_alarm) == SIG_ERR) {
        perror("[KUCHARZ] signal(SIGALRM)");
    }
    if (signal(SIGUSR1, handler_szybciej) == SIG_ERR) {
        perror("[KUCHARZ] signal(SIGUSR1)");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGUSR2, handler_wolniej) == SIG_ERR) {
        perror("[KUCHARZ] signal(SIGUSR2)");
        exit(EXIT_FAILURE);
    }

    /* Konfiguracja timera SIGALRM (co 1 sekundę) */
    struct itimerval timer;
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("[KUCHARZ] setitimer");
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

    int sem = semget(key, NUM_SEMS, 0);
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

    int tick_counter = 0;  /* Licznik ticków dla zmiennej prędkości */

    while (r->otwarta) {
        /* Czekaj na sygnał SIGALRM (pause() nie używa CPU) */
        while (!tick_flag && r->otwarta) {
            pause();
        }
        tick_flag = 0;
        tick_counter++;

        /* Sprawdź czy minęło wystarczająco ticków (dla zmiennej prędkości) */
        if (tick_counter < production_delay) {
            continue;  /* Jeszcze nie czas na produkcję */
        }
        tick_counter = 0;

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
        nowy.id_odbiorcy = dla_kogo;

        lock(sem);
        if (!r->tasma.seg[0].zajety) {
            /* Sprawdź czy zamówienie specjalne jest jeszcze aktywne */
            if (idx_zamowienia != -1) {
                if (!r->zamowienia[idx_zamowienia].aktywne) {
                    /* Klient anulował - zmień na zwykły talerzyk */
                    nowy.id_odbiorcy = -1;
                    idx_zamowienia = -1;
                    sprintf(r->info, "KUCHARZ: Zamowienie anulowane, %s -> zwykly", nazwy_kolorow[k]);
                }
            }

            r->tasma.seg[0].zajety = 1;
            r->tasma.seg[0].t = nowy;
            r->wyprodukowane[k]++;

            if (idx_zamowienia != -1) {
                r->zamowienia[idx_zamowienia].aktywne = 0;
                sprintf(r->info, "KUCHARZ: Wydano %s dla %d!", nazwy_kolorow[k], dla_kogo);
            }
        }
        unlock(sem);
    }

    printf("[KUCHARZ] Restauracja zamknieta. Generowanie raportu produkcji...\n");

    /* === RAPORT KUCHARZA - podsumowanie produkcji === */
    lock(sem);
    FILE *f = fopen("raport_kucharz.txt", "w");
    if (f) {
        fprintf(f, "========================================\n");
        fprintf(f, "   RAPORT KUCHARZA - PRODUKCJA\n");
        fprintf(f, "========================================\n\n");

        fprintf(f, "+---------------+------+--------+\n");
        fprintf(f, "| Kolor         | Szt. | Kwota  |\n");
        fprintf(f, "+---------------+------+--------+\n");

        int razem_szt = 0;
        int razem_kwota = 0;
        for (int i = 0; i < KOLORY; i++) {
            int szt = r->wyprodukowane[i];
            int kwota = szt * ceny[i];
            razem_szt += szt;
            razem_kwota += kwota;
            fprintf(f, "| %-13s | %4d | %6d |\n", nazwy_kolorow[i], szt, kwota);
        }
        fprintf(f, "+---------------+------+--------+\n");
        fprintf(f, "| RAZEM         | %4d | %6d |\n", razem_szt, razem_kwota);
        fprintf(f, "+---------------+------+--------+\n");

        fprintf(f, "\n========================================\n");
        fprintf(f, "      KONIEC RAPORTU KUCHARZA\n");
        fprintf(f, "========================================\n");

        fclose(f);
        printf("[KUCHARZ] Raport zapisany do raport_kucharz.txt\n");
    } else {
        perror("[KUCHARZ] fopen raport_kucharz.txt");
    }
    unlock(sem);

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[KUCHARZ] shmdt");
    }

    return 0;
}