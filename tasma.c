/*
 * tasma.c - Proces taśmy transportowej (conveyor belt)
 *
 * Odpowiada za:
 * - Cykliczne przesuwanie talerzyków na taśmie
 * - Ruch: segment[i] -> segment[(i+1) % SEGMENTY]
 * - Reagowanie na sygnały kierownika (przyspieszenie/spowolnienie)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

#include "wspolne.h"

/*
 * Flaga sygnału SIGALRM - ustawiana przez handler, czyszczona w pętli.
 * Używamy sig_atomic_t dla bezpieczeństwa sygnałów.
 */
volatile sig_atomic_t tick_flag = 0;

/*
 * Aktualny interwał timera w tickach (1-4).
 * Modyfikowany przez SIGUSR1/SIGUSR2.
 * Startuje od 2 (normalny), może spaść do 1 (2x szybciej) lub wzrosnąć do 4 (2x wolniej).
 */
volatile int current_delay = 2;

/* Handler SIGALRM - ustawia flagę tick */
void handler_alarm(int sig) {
    (void)sig;
    tick_flag = 1;
}

/* Handler SIGUSR1 - przyspieszenie taśmy 2x */
void handler_szybciej(int sig) {
    (void)sig;
    current_delay = (current_delay > 1) ? current_delay / 2 : 1;
    printf("[TASMA] Przyspieszenie! Nowy delay: %ds\n", current_delay);
}

/* Handler SIGUSR2 - spowolnienie taśmy 2x */
void handler_wolniej(int sig) {
    (void)sig;
    current_delay = (current_delay < 4) ? current_delay * 2 : 4;
    printf("[TASMA] Spowolnienie! Nowy delay: %ds\n", current_delay);
}

int main() {
    printf("[TASMA] Start procesu tasmy (PID=%d)\n", getpid());

    /* Rejestracja handlerów sygnałów */
    if (signal(SIGALRM, handler_alarm) == SIG_ERR) {
        perror("[TASMA] signal(SIGALRM)");
    }
    if (signal(SIGUSR1, handler_szybciej) == SIG_ERR) {
        perror("[TASMA] signal(SIGUSR1)");
    }
    if (signal(SIGUSR2, handler_wolniej) == SIG_ERR) {
        perror("[TASMA] signal(SIGUSR2)");
    }

    /* Konfiguracja timera SIGALRM (co 1 sekundę) */
    struct itimerval timer;
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("[TASMA] setitimer");
    }

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[TASMA] ftok");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z zasobami IPC */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[TASMA] shmget");
        exit(EXIT_FAILURE);
    }

    int sem = semget(key, NUM_SEMS, 0);
    if (sem == -1) {
        perror("[TASMA] semget");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[TASMA] shmat");
        exit(EXIT_FAILURE);
    }

    printf("[TASMA] Polaczono z zasobami IPC (shm=%d, sem=%d)\n", shm, sem);

    /* Zapisanie PID taśmy dla kierownika */
    lock(sem);
    r->pid_tasmy = getpid();
    unlock(sem);

    int tick_counter = 0;  /* Licznik ticków dla zmiennej prędkości */

    while (r->otwarta) {
        /* Czekaj na sygnał SIGALRM (pause() nie używa CPU) */
        while (!tick_flag && r->otwarta) {
            pause();  /* Blokuje do otrzymania sygnału - nie busy-wait! */
        }
        tick_flag = 0;
        tick_counter++;

        /* Sprawdź czy minęło wystarczająco ticków (dla zmiennej prędkości) */
        if (tick_counter < current_delay) {
            continue;  /* Jeszcze nie czas na obrót */
        }
        tick_counter = 0;

        lock(sem);

        struct segment_tasmy tmp[SEGMENTY];

        /* kopia stanu tasmy */
        for (int i = 0; i < SEGMENTY; i++) {
            tmp[i] = r->tasma.seg[i];
        }

        /* czysty ruch cykliczny 0..19 */
        for (int i = 0; i < SEGMENTY; i++) {
            r->tasma.seg[(i + 1) % SEGMENTY] = tmp[i];
        }

        unlock(sem);
    }

    printf("[TASMA] Restauracja zamknieta. Konczenie pracy.\n");

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[TASMA] shmdt");
    }

    return 0;
}
