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
#include <time.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#include "wspolne.h"

#define LICZBA_GRUP 40

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
    g_sem_id = semget(key, 1, IPC_CREAT | PRAWA);
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

    /* Inicjalizacja semafora na wartość 1 (mutex) */
    if (semctl(g_sem_id, 0, SETVAL, 1) == -1) {
        perror("[MAIN] semctl SETVAL - nie mozna zainicjalizowac semafora");
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

    printf("[MAIN] Uruchamianie %d grup klientow...\n", LICZBA_GRUP);

    for (int g = 0; g < LICZBA_GRUP; g++) {
        int rozmiar = 1 + rand() % 4;

        /* Tablice VLA dla statusu członków grupy */
        int member_vip[rozmiar];
        int member_child[rozmiar];
        int group_has_priority = 0;

        /* Losowanie statusu VIP i dziecka dla każdego członka */
        for (int k = 0; k < rozmiar; k++) {
            if (rand() % 100 < 2) {  /* 2% szans na VIP */
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

        /* Przerwa między grupami */
        usleep(500000); /* 0.5 sekundy zamiast 1s - szybsza symulacja */
    }

    printf("[MAIN] Wszystkie grupy klientow utworzone. Oczekiwanie na zakonczenie...\n");

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