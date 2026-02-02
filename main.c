/*
 * main.c - Główny proces restauracji kaiten zushi
 *
 * Odpowiada za:
 * - Inicjalizację zasobów IPC (pamięć dzielona, semafory, kolejki)
 * - Uruchomienie procesów: kucharz, obsluga, tasma, klienci
 * - Obsługę sygnału SIGINT (CTRL+C) - sprzątanie zasobów
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "wspolne.h"

#define LICZBA_GRUP 50  /* Liczba grup do symulacji */

/* Flaga timera do tworzenia grup */
volatile sig_atomic_t utworz_grupe_flag = 0;

/* Handler SIGALRM - sygnalizuje że można utworzyć nową grupę */
void handler_nowa_grupa(int sig) {
    (void)sig;
    utworz_grupe_flag = 1;
}

/* Globalne ID zasobów IPC do sprzątania przy wyjściu */
int g_shm_id = -1;
int g_sem_id = -1;
int g_msg_id = -1;

/*
 * Handler sygnału SIGINT (CTRL+C).
 * Wysyła SIGTERM do wszystkich procesów potomnych,
 * usuwa zasoby IPC i kończy program.
 */
void sprzatanie(int sig) {
    printf("\n[MAIN] Otrzymano sygnal %d. Sprzatanie zasobow IPC...\n", sig);

    /* Wysyłamy SIGTERM do wszystkich procesów w grupie */
    if (kill(0, SIGTERM) == -1 && errno != ESRCH) {
        perror("[MAIN] kill(0, SIGTERM)");
    }

    /* Usuwamy pamięć dzieloną */
    if (g_shm_id != -1) {
        if (shmctl(g_shm_id, IPC_RMID, NULL) == -1) {
            perror("[MAIN] shmctl IPC_RMID");
        } else {
            printf("[MAIN] Pamiec dzielona (id=%d) usunieta.\n", g_shm_id);
        }
    }

    /* Usuwamy semafor */
    if (g_sem_id != -1) {
        if (semctl(g_sem_id, 0, IPC_RMID) == -1) {
            perror("[MAIN] semctl IPC_RMID");
        } else {
            printf("[MAIN] Semafor (id=%d) usuniety.\n", g_sem_id);
        }
    }

    /* Usuwamy kolejkę komunikatów */
    if (g_msg_id != -1) {
        if (msgctl(g_msg_id, IPC_RMID, NULL) == -1) {
            perror("[MAIN] msgctl IPC_RMID");
        } else {
            printf("[MAIN] Kolejka komunikatow (id=%d) usunieta.\n", g_msg_id);
        }
    }

    printf("[MAIN] Sprzatanie zakonczone. Koniec programu.\n");
    exit(0);
}

int main() {
    printf("[MAIN] Start systemu restauracji kaiten zushi\n");
    printf("[MAIN] Konfiguracja: MAX_GRUP=%d, SEGMENTY=%d, STOLIKI=%d, LADA=%d\n",
           MAX_GRUP, SEGMENTY, STOLIKI, LADA_MIEJSC);
    srand(time(NULL));

    /* Rejestracja handlera CTRL+C */
    if (signal(SIGINT, sprzatanie) == SIG_ERR) {
        perror("[MAIN] signal(SIGINT)");
        exit(EXIT_FAILURE);
    }

    /* Generowanie klucza IPC z pliku ipc_keyfile */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[MAIN] ftok - upewnij sie ze plik ipc_keyfile istnieje");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Klucz IPC wygenerowany: 0x%x\n", key);

    /* Tworzenie pamięci dzielonej */
    g_shm_id = shmget(key, sizeof(struct restauracja), IPC_CREAT | PRAWA);
    if (g_shm_id == -1) {
        perror("[MAIN] shmget - nie mozna utworzyc pamieci dzielonej");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Pamiec dzielona utworzona (id=%d, rozmiar=%zu bajtow)\n",
           g_shm_id, sizeof(struct restauracja));

    /* Tworzenie semafora */
    g_sem_id = semget(key, NUM_SEMS, IPC_CREAT | PRAWA);
    if (g_sem_id == -1) {
        perror("[MAIN] semget - nie mozna utworzyc semafora");
        shmctl(g_shm_id, IPC_RMID, NULL); /* Sprzątamy już utworzoną pamięć */
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Semafor utworzony (id=%d)\n", g_sem_id);

    /* Tworzenie kolejki komunikatów */
    g_msg_id = msgget(key, IPC_CREAT | PRAWA);
    if (g_msg_id == -1) {
        perror("[MAIN] msgget - nie mozna utworzyc kolejki komunikatow");
        shmctl(g_shm_id, IPC_RMID, NULL);
        semctl(g_sem_id, 0, IPC_RMID);
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Kolejka komunikatow utworzona (id=%d)\n", g_msg_id);

    /* Inicjalizacja semafora mutex na wartość 1 */
    if (semctl(g_sem_id, SEM_MUTEX, SETVAL, 1) == -1) {
        perror("[MAIN] semctl SETVAL SEM_MUTEX");
        sprzatanie(0);
    }

    /* Inicjalizacja semafora licznika aktywnych klientów na 0 */
    if (semctl(g_sem_id, SEM_ACTIVE, SETVAL, 0) == -1) {
        perror("[MAIN] semctl SETVAL SEM_ACTIVE");
        sprzatanie(0);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(g_shm_id, NULL, 0);
    if (r == (void *)-1) {
        perror("[MAIN] shmat - nie mozna dolaczyc pamieci dzielonej");
        sprzatanie(0);
    }
    printf("[MAIN] Dolaczono do pamieci dzielonej pod adresem %p\n", (void *)r);

    /* Inicjalizacja struktury restauracji */
    r->otwarta = 1;
    r->pid_kucharza = 0;
    r->czas_startu = time(NULL);
    r->przyjmuje_klientow = 1;  /* Restauracja przyjmuje nowych klientów */
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
        r->stoliki[i].rozmiar_grupy = 0;  /* Brak grupy na starcie */
        if (i < 3) r->stoliki[i].pojemnosc = 2;
        else if (i < 6) r->stoliki[i].pojemnosc = 3;
        else r->stoliki[i].pojemnosc = 4;
    }
    for (int i = 0; i < MAX_KLIENTOW; i++) r->klienci[i].aktywny = 0;

    /* Inicjalizacja liczników produkcji i sprzedaży */
    for (int i = 0; i < KOLORY; i++) {
        r->wyprodukowane[i] = 0;
        r->sprzedane[i] = 0;
    }

    /* Inicjalizacja statystyk kasy */
    r->kasa.transakcje = 0;
    r->kasa.suma_dzienna = 0;

    /* Inicjalizacja liczników grup */
    for (int i = 0; i < MAX_GRUP; i++) {
        r->grupa_zaplacila[i] = 0;
        for (int j = 0; j < KOLORY; j++) {
            r->grupa_talerzyki[i][j] = 0;
        }
    }

    printf("[MAIN] Uruchamianie procesow potomnych...\n");

    /* Uruchomienie procesu kucharza */
    pid_t pid_kucharz = fork();
    if (pid_kucharz == -1) {
        perror("[MAIN] fork(kucharz)");
        sprzatanie(0);
    } else if (pid_kucharz == 0) {
        execl("./kucharz", "kucharz", NULL);
        perror("[MAIN] execl(kucharz) - nie mozna uruchomic");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Kucharz uruchomiony (PID=%d)\n", pid_kucharz);

    /* Uruchomienie procesu obsługi */
    pid_t pid_obsluga = fork();
    if (pid_obsluga == -1) {
        perror("[MAIN] fork(obsluga)");
        sprzatanie(0);
    } else if (pid_obsluga == 0) {
        execl("./obsluga", "obsluga", NULL);
        perror("[MAIN] execl(obsluga) - nie mozna uruchomic");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Obsluga uruchomiona (PID=%d)\n", pid_obsluga);

    /* Uruchomienie procesu taśmy */
    pid_t pid_tasma = fork();
    if (pid_tasma == -1) {
        perror("[MAIN] fork(tasma)");
        sprzatanie(0);
    } else if (pid_tasma == 0) {
        execl("./tasma", "tasma", NULL);
        perror("[MAIN] execl(tasma) - nie mozna uruchomic");
        exit(EXIT_FAILURE);
    }
    printf("[MAIN] Tasma uruchomiona (PID=%d)\n", pid_tasma);

    printf("[MAIN] Uruchamianie grup klientow (max %d, czas pracy: %ds)...\n",
           LICZBA_GRUP, CZAS_ZAMKNIECIA);

    /* Rejestracja handlera SIGALRM dla timera grup */
    if (signal(SIGALRM, handler_nowa_grupa) == SIG_ERR) {
        perror("[MAIN] signal(SIGALRM)");
    }

    /* Funkcja pomocnicza do ustawienia losowego timera (1-3 sekundy) */
    struct itimerval timer_grup;
    timer_grup.it_interval.tv_sec = 0;  /* Jednorazowy */
    timer_grup.it_interval.tv_usec = 0;

    /* Ustawienie pierwszego timera (od razu pierwsza grupa) */
    utworz_grupe_flag = 1;

    int g = 0;
    while (g < LICZBA_GRUP && r->otwarta) {
        /* Czekaj na sygnał SIGALRM (nie busy-wait) */
        while (!utworz_grupe_flag && r->otwarta) {
            /* Sprawdź czy czas zamknięcia */
            time_t teraz = time(NULL);
            int czas_od_startu = (int)(teraz - r->czas_startu);
            if (czas_od_startu >= CZAS_ZAMKNIECIA) {
                break;
            }
            pause();  /* Blokuj do sygnału */
        }

        /* Sprawdź czy ewakuacja (kierownik zamknął restaurację) */
        if (!r->otwarta) {
            printf("[MAIN] Restauracja zamknieta przez kierownika. Przerywam tworzenie grup.\n");
            break;
        }

        /* Sprawdzenie czy restauracja nadal przyjmuje klientów */
        time_t teraz = time(NULL);
        int czas_od_startu = (int)(teraz - r->czas_startu);

        if (czas_od_startu >= CZAS_ZAMKNIECIA) {
            printf("[MAIN] Czas zamkniecia (Tk=%ds) osiagniety. Przestajemy przyjmowac nowych klientow.\n",
                   CZAS_ZAMKNIECIA);
            lock(g_sem_id);
            r->przyjmuje_klientow = 0;
            sprintf(r->info, "RESTAURACJA ZAMKNIETA dla nowych klientow! Obslugujemy pozostalych.");
            unlock(g_sem_id);
            break;  /* Przerywamy tworzenie nowych grup */
        }

        utworz_grupe_flag = 0;

        int rozmiar = 1;

        /* Tablice VLA dla statusu członków grupy */
        int member_vip[rozmiar];
        int member_child[rozmiar];
        int group_has_priority = 0;

        /* Losowanie statusu VIP i dziecka dla każdego członka */
        for (int k = 0; k < rozmiar; k++) {
            if (rand() % 100 < 0) {  /* 0% szans na VIP */
                member_vip[k] = 1;
                group_has_priority = 1;
            } else {
                member_vip[k] = 0;
            }

            /* Tylko nie-pierwsi członkowie mogą być dziećmi (10% szans) */
            if (k > 0 && rand() % 100 < 10) {
                member_child[k] = 1;
            } else {
                member_child[k] = 0;
            }
        }

        /* Walidacja: max 3 dzieci na 1 dorosłego (wymaganie z regulaminu) */
        int liczba_dzieci = 0;
        int liczba_doroslych = 0;
        for (int k = 0; k < rozmiar; k++) {
            if (member_child[k]) liczba_dzieci++;
            else liczba_doroslych++;
        }

        /* Korekta jeśli za dużo dzieci */
        if (liczba_doroslych > 0 && liczba_dzieci > 3 * liczba_doroslych) {
            int max_dzieci = 3 * liczba_doroslych;
            printf("[MAIN] Grupa %d: za duzo dzieci (%d), koryguje do %d\n",
                   g, liczba_dzieci, max_dzieci);

            /* Zamieniamy nadmiarowe dzieci na dorosłych */
            for (int k = rozmiar - 1; k > 0 && liczba_dzieci > max_dzieci; k--) {
                if (member_child[k]) {
                    member_child[k] = 0;
                    liczba_dzieci--;
                }
            }
        }

        /* Specjalny przypadek: grupa samych dzieci - niedozwolone */
        if (liczba_doroslych == 0) {
            printf("[MAIN] Grupa %d: brak doroslych, pierwszy czlonek staje sie doroslym\n", g);
            member_child[0] = 0;  /* Pierwszy zawsze dorosły */
        }

        /* Tworzenie procesów klientów w grupie */
        for (int i = 0; i < rozmiar; i++) {
            pid_t pid_klient = fork();
            if (pid_klient == -1) {
                perror("[MAIN] fork(klient)");
                /* Kontynuujemy mimo błędu - graceful degradation */
                continue;
            } else if (pid_klient == 0) {
                /* Proces potomny - uruchamiamy klienta */
                char gid[10], gsize[10];
                char my_vip[5], my_child[5], grp_prio[5];

                sprintf(gid, "%d", g);
                sprintf(gsize, "%d", rozmiar);
                sprintf(my_vip, "%d", member_vip[i]);
                sprintf(grp_prio, "%d", group_has_priority);
                sprintf(my_child, "%d", member_child[i]);

                execl("./klient", "klient", gid, gsize, my_vip, grp_prio, my_child, NULL);
                /* Jeśli execl się nie powiedzie, wypisujemy błąd */
                perror("[MAIN] execl(klient) - nie mozna uruchomic");
                exit(EXIT_FAILURE);
            }
            /* Proces macierzysty kontynuuje pętlę */
        }

        g++;

        /* Ustawienie losowego timera na następną grupę (1-3 sekundy) */
        int losowy_delay = 1 + rand() % 2;
        timer_grup.it_value.tv_sec = losowy_delay;
        timer_grup.it_value.tv_usec = 0;
        if (setitimer(ITIMER_REAL, &timer_grup, NULL) == -1) {
            perror("[MAIN] setitimer dla grup");
        }
    }

    /* Ustawienie statusu - nie przyjmujemy nowych klientów */
    lock(g_sem_id);
    r->przyjmuje_klientow = 0;
    sprintf(r->info, "Restauracja nie przyjmuje nowych klientow. Obslugujemy pozostalych.");
    unlock(g_sem_id);

    printf("[MAIN] Koniec przyjmowania nowych klientow.\n");

    /* Czekamy aż wszyscy klienci wyjdą (semafor licznikowy) */
    printf("[MAIN] Czekam az wszyscy klienci wyjda (sem_wait_zero)...\n");
    sem_wait_zero(g_sem_id, SEM_ACTIVE);
    printf("[MAIN] Wszyscy klienci wyszli. Zamykanie restauracji.\n");

    /* Zamknięcie restauracji - to zatrzyma pętle w innych procesach */
    lock(g_sem_id);
    r->otwarta = 0;
    sprintf(r->info, "RESTAURACJA ZAMKNIETA. Do zobaczenia jutro!");
    unlock(g_sem_id);

    /* Dajemy czas procesom na zakończenie i wygenerowanie raportów */
    // sleep(2);

    printf("[MAIN] Oczekiwanie na zakonczenie procesow potomnych...\n");

    /* Oczekiwanie na zakończenie wszystkich procesów potomnych */
    int status;
    pid_t child_pid;
    while ((child_pid = wait(&status)) > 0) {
        if (WIFEXITED(status)) {
            zrzut_do_logu("[MAIN] Proces %d zakonczyl sie z kodem %d",
                          child_pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            zrzut_do_logu("[MAIN] Proces %d zabity sygnalem %d",
                          child_pid, WTERMSIG(status));
        }
    }

    /* Sprawdzenie czy wait zakończyło się błędem (innym niż brak dzieci) */
    if (errno != ECHILD) {
        perror("[MAIN] wait");
    }

    printf("[MAIN] Wszystkie procesy potomne zakonczone.\n");
    sprzatanie(0);
    return 0;
}