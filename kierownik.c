/*
 * kierownik.c - Panel kontrolny kierownika restauracji
 *
 * Odpowiada za:
 * - Wysyłanie sygnałów do kucharza (SIGUSR1/SIGUSR2)
 * - Ewakuację klientów (SIGTERM)
 * - Zamykanie restauracji
 *
 * Uruchamiany jako osobny proces (w osobnym terminalu).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <errno.h>

#include "wspolne.h"

/*
 * Czyści bufor wejściowy po błędzie scanf.
 */
void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/*
 * Wyświetla menu kierownika.
 */
void wyswietl_menu(void) {
    printf("\n=== PANEL KIEROWNIKA ===\n");
    printf("1. Kucharz: Szybciej! (SIGUSR1) - 2x szybsza produkcja\n");
    printf("2. Kucharz: Wolniej! (SIGUSR2) - 50%% wolniejsza produkcja\n");
    printf("3. EWAKUACJA! (SIGTERM) - wyrzuc wszystkich klientow\n");
    printf("0. Wyjscie z panelu\n");
    printf("------------------------\n");
}

int main() {
    printf("[KIEROWNIK] Start panelu kierownika\n");

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'K');
    if (key == -1) {
        perror("[KIEROWNIK] ftok - upewnij sie ze main jest uruchomiony");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z pamięcią dzieloną */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[KIEROWNIK] shmget - czy main jest uruchomiony?");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[KIEROWNIK] shmat");
        exit(EXIT_FAILURE);
    }

    printf("[KIEROWNIK] Polaczono z pamiecia dzielona (shm=%d)\n", shm);

    wyswietl_menu();

    while (1) {
        printf("\nWybor (0-3): ");
        fflush(stdout);

        int wybor;
        int scan_result = scanf("%d", &wybor);

        /* Walidacja: czy scanf się powiódł */
        if (scan_result != 1) {
            if (scan_result == EOF) {
                printf("\n[KIEROWNIK] Koniec wejscia (EOF). Wychodzenie.\n");
                break;
            }
            printf("[BLAD] Niepoprawne dane wejsciowe. Podaj liczbe 0-3.\n");
            clear_input_buffer();
            continue;
        }

        /* Walidacja: czy wybór jest w zakresie */
        if (wybor < 0 || wybor > 3) {
            printf("[BLAD] Niepoprawna opcja %d. Dostepne opcje: 0, 1, 2, 3.\n", wybor);
            continue;
        }

        if (wybor == 0) {
            printf("[KIEROWNIK] Zamykanie panelu.\n");
            break;
        }

        /* Sprawdzenie czy restauracja jest otwarta */
        if (!r->otwarta && wybor != 0) {
            printf("[UWAGA] Restauracja jest juz zamknieta.\n");
            continue;
        }

        /* Pobieramy aktualny PID kucharza z pamięci */
        pid_t kucharz = r->pid_kucharza;

        switch (wybor) {
            case 1:
                /* Walidacja: czy kucharz jest zarejestrowany */
                if (kucharz <= 0) {
                    printf("[BLAD] Kucharz nie zglosil PIDu! (pid=%d)\n", kucharz);
                    break;
                }
                /* Wysłanie sygnału z obsługą błędów */
                if (kill(kucharz, SIGUSR1) == -1) {
                    if (errno == ESRCH) {
                        printf("[BLAD] Kucharz (PID=%d) nie istnieje!\n", kucharz);
                    } else {
                        perror("[KIEROWNIK] kill(SIGUSR1)");
                    }
                } else {
                    printf("-> Wyslano SIGUSR1 do Kucharza (PID=%d) - przyspieszenie 2x\n", kucharz);
                    zrzut_do_logu("KIEROWNIK: Sygnal SIGUSR1 do kucharza %d", kucharz);
                }
                break;

            case 2:
                /* Walidacja: czy kucharz jest zarejestrowany */
                if (kucharz <= 0) {
                    printf("[BLAD] Kucharz nie zglosil PIDu! (pid=%d)\n", kucharz);
                    break;
                }
                /* Wysłanie sygnału z obsługą błędów */
                if (kill(kucharz, SIGUSR2) == -1) {
                    if (errno == ESRCH) {
                        printf("[BLAD] Kucharz (PID=%d) nie istnieje!\n", kucharz);
                    } else {
                        perror("[KIEROWNIK] kill(SIGUSR2)");
                    }
                } else {
                    printf("-> Wyslano SIGUSR2 do Kucharza (PID=%d) - spowolnienie 50%%\n", kucharz);
                    zrzut_do_logu("KIEROWNIK: Sygnal SIGUSR2 do kucharza %d", kucharz);
                }
                break;

            case 3:
                printf("-> EWAKUACJA! Wysylanie SIGTERM do wszystkich klientow...\n");
                zrzut_do_logu("KIEROWNIK: Rozpoczeto ewakuacje!");

                int ewakuowani = 0;
                /* Iterujemy po klientach w SHM i wysyłamy sygnał */
                for (int i = 0; i < MAX_KLIENTOW; i++) {
                    if (r->klienci[i].aktywny && r->klienci[i].pid > 0) {
                        if (kill(r->klienci[i].pid, SIGTERM) == 0) {
                            ewakuowani++;
                        }
                        /* Ignorujemy błędy - proces mógł już się zakończyć */
                    }
                }

                r->otwarta = 0; /* Zamykamy lokal logicznie */
                printf("-> Ewakuowano %d klientow. Restauracja zamknieta.\n", ewakuowani);
                zrzut_do_logu("KIEROWNIK: Ewakuowano %d klientow", ewakuowani);
                break;
        }
    }

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[KIEROWNIK] shmdt");
    }

    printf("[KIEROWNIK] Koniec pracy.\n");
    return 0;
}