/*
 * obsluga.c - Proces obsługi restauracji (manager sali + kasa)
 *
 * Odpowiada za:
 * - Przyjmowanie zgłoszeń klientów przez kolejkę komunikatów
 * - Przydzielanie miejsc przy ladzie i stolikach (algorytm "Tetris")
 * - Wyświetlanie monitora stanu sali w czasie rzeczywistym
 * - KASA: Przyjmowanie płatności i wystawianie rachunków
 * - Generowanie podsumowania końcowego
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>

#include "wspolne.h"

/* Kolory ANSI dla terminala */
#define ANSI_RESET   "\033[0m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_BROWN   "\033[38;5;130m" /* prawdziwy brązowy (256 kolorów) */
#define ANSI_SILVER  "\033[37m"       /* jasny szary */
#define ANSI_GOLD    "\033[33;1m"     /* jasny żółty = złoty */
#define ANSI_CYAN    "\033[36m"

/* Tablica kolorów ANSI dla talerzyków (indeksowana jak KOLORY) */
static const char *kolory_ansi[KOLORY] = {
    ANSI_BLUE,   /* niebieski */
    ANSI_RED,    /* czerwony */
    ANSI_GREEN,  /* zielony */
    ANSI_BROWN,  /* BRAZOWY */
    ANSI_SILVER, /* SREBRNY */
    ANSI_GOLD    /* ZLOTY */
};


/*
 * Oblicza sumę rachunku na podstawie talerzyków.
 */
int oblicz_rachunek(int talerzyki[KOLORY]) {
    int suma = 0;
    for (int i = 0; i < KOLORY; i++) {
        suma += talerzyki[i] * ceny[i];
    }
    return suma;
}

/*
 * Generuje podsumowanie końcowe do pliku.
 */
void generuj_podsumowanie(struct restauracja *r) {
    FILE *f = fopen("raport_dzienny.txt", "w");
    if (!f) {
        perror("[OBSLUGA] fopen raport_dzienny.txt");
        return;
    }

    fprintf(f, "========================================\n");
    fprintf(f, "    RAPORT DZIENNY - KAITEN ZUSHI\n");
    fprintf(f, "========================================\n\n");

    /* Sekcja KASA */
    fprintf(f, "--- KASA (transakcje) ---\n");
    fprintf(f, "Liczba transakcji: %d\n", r->kasa.transakcje);
    fprintf(f, "Utarg dzienny: %d zl\n\n", r->kasa.suma_dzienna);

    /* Sekcja SPRZEDAZ (faktycznie zjedzone talerzyki) */
    fprintf(f, "--- SPRZEDAZ (zjedzone talerzyki) ---\n");
    fprintf(f, "+---------------+------+--------+\n");
    fprintf(f, "| Kolor         | Szt. | Kwota  |\n");
    fprintf(f, "+---------------+------+--------+\n");
    int suma_szt = 0;
    int suma_kwota = 0;
    for (int i = 0; i < KOLORY; i++) {
        int kwota = r->sprzedane[i] * ceny[i];
        fprintf(f, "| %-13s | %4d | %6d |\n",
                nazwy_kolorow[i], r->sprzedane[i], kwota);
        suma_szt += r->sprzedane[i];
        suma_kwota += kwota;
    }
    fprintf(f, "+---------------+------+--------+\n");
    fprintf(f, "| RAZEM         | %4d | %6d |\n", suma_szt, suma_kwota);
    fprintf(f, "+---------------+------+--------+\n\n");

    /* Sekcja POZOSTALE NA TASMIE */
    fprintf(f, "--- POZOSTALE NA TASMIE ---\n");
    int pozostale[KOLORY] = {0};
    int suma_pozostale = 0;
    for (int i = 0; i < SEGMENTY; i++) {
        if (r->tasma.seg[i].zajety) {
            pozostale[r->tasma.seg[i].t.kolor]++;
            suma_pozostale++;
        }
    }
    fprintf(f, "+---------------+------+\n");
    fprintf(f, "| Kolor         | Szt. |\n");
    fprintf(f, "+---------------+------+\n");
    for (int i = 0; i < KOLORY; i++) {
        if (pozostale[i] > 0) {
            fprintf(f, "| %-13s | %4d |\n", nazwy_kolorow[i], pozostale[i]);
        }
    }
    fprintf(f, "+---------------+------+\n");
    fprintf(f, "| RAZEM         | %4d |\n", suma_pozostale);
    fprintf(f, "+---------------+------+\n\n");

    /* Walidacja spójności danych */
    fprintf(f, "--- WALIDACJA ---\n");
    int suma_prod = 0;
    for (int i = 0; i < KOLORY; i++) suma_prod += r->wyprodukowane[i];
    int oczekiwane = suma_szt + suma_pozostale;
    if (suma_prod == oczekiwane) {
        fprintf(f, "OK: Wyprodukowano %d = Sprzedano %d + Pozostalo %d\n\n",
                suma_prod, suma_szt, suma_pozostale);
    } else {
        fprintf(f, "UWAGA: Wyprodukowano %d != Sprzedano %d + Pozostalo %d (=%d)\n",
                suma_prod, suma_szt, suma_pozostale, oczekiwane);
        fprintf(f, "Roznica: %d (moga byc talerzyki w tranzycie)\n\n",
                suma_prod - oczekiwane);
    }

    fprintf(f, "========================================\n");
    fprintf(f, "     KONIEC RAPORTU OBSLUGI\n");
    fprintf(f, "========================================\n");

    fclose(f);
    printf("[OBSLUGA] Raport dzienny zapisany do raport_dzienny.txt\n");
    zrzut_do_logu("OBSLUGA: Raport - %d transakcji, %d zl utargu, sprzedano %d, pozostalo %d",
                  r->kasa.transakcje, r->kasa.suma_dzienna, suma_szt, suma_pozostale);
}

/* --- STRUKTURA LOKALNEJ KOLEJKI --- */
typedef struct {
    pid_t pid;
    int group_priority; // Czy grupa ma priorytet (VIP)
} wpis_kolejki;

#define Q_SIZE 100

// Kolejki
wpis_kolejki queue_g1[Q_SIZE]; int q1_cnt = 0;
wpis_kolejki queue_g2[Q_SIZE]; int q2_cnt = 0;
wpis_kolejki queue_g3[Q_SIZE]; int q3_cnt = 0;
wpis_kolejki queue_g4[Q_SIZE]; int q4_cnt = 0;

void wstaw_do_kolejki(wpis_kolejki *q, int *cnt, pid_t pid, int priority) {
    if (*cnt >= Q_SIZE) return;

    int idx_wstawienia = *cnt; 

    if (priority) {
        // Jesli grupa ma priorytet, szukamy pierwszego BEZ priorytetu
        for (int i = 0; i < *cnt; i++) {
            if (q[i].group_priority == 0) {
                idx_wstawienia = i;
                break;
            }
        }
    }

    for (int i = *cnt; i > idx_wstawienia; i--) {
        q[i] = q[i-1];
    }

    q[idx_wstawienia].pid = pid;
    q[idx_wstawienia].group_priority = priority;
    (*cnt)++;
}

void dodaj_do_kolejki(int rozmiar, int priority, pid_t pid) {
    if (rozmiar == 1) wstaw_do_kolejki(queue_g1, &q1_cnt, pid, priority);
    else if (rozmiar == 2) wstaw_do_kolejki(queue_g2, &q2_cnt, pid, priority);
    else if (rozmiar == 3) wstaw_do_kolejki(queue_g3, &q3_cnt, pid, priority);
    else if (rozmiar >= 4) wstaw_do_kolejki(queue_g4, &q4_cnt, pid, priority);
}

/*
 * Wysyła zaproszenie do klienta z przydzielonym miejscem.
 * Używa msgsnd() do wysłania komunikatu na PID klienta.
 */
void zapros_klienta(int msg_id, wpis_kolejki *kolejka, int *cnt, int idx_w_kolejce, int nr_miejsca, int typ) {
    pid_t pid = kolejka[idx_w_kolejce].pid;

    struct komunikat zaproszenie;
    zaproszenie.mtype = pid;
    zaproszenie.numer_miejsca = nr_miejsca;
    zaproszenie.typ_miejsca = typ;

    /* Wysłanie komunikatu z obsługą błędów */
    if (msgsnd(msg_id, &zaproszenie, sizeof(struct komunikat) - sizeof(long), 0) == -1) {
        if (errno == EIDRM) {
            /* Kolejka została usunięta - restauracja zamykana */
            return;
        }
        perror("[OBSLUGA] msgsnd - blad wysylania zaproszenia");
        return;
    }

    if (typ == 0) {
        zrzut_do_logu("OBSLUGA: Przydzielono LADE %d dla PID %d", nr_miejsca, pid);
    } else {
        zrzut_do_logu("OBSLUGA: Przydzielono STOLIK %d dla PID %d", nr_miejsca, pid);
    }

    /* Usunięcie klienta z kolejki */
    for (int i = idx_w_kolejce; i < *cnt - 1; i++) {
        kolejka[i] = kolejka[i + 1];
    }
    (*cnt)--;
}

int main() {
    printf("[OBSLUGA] Start managera sali + kasy\n");

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[OBSLUGA] ftok");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z istniejącymi zasobami IPC */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[OBSLUGA] shmget - nie mozna polaczyc z pamiecia dzielona");
        exit(EXIT_FAILURE);
    }

    int sem = semget(key, 1, 0);
    if (sem == -1) {
        perror("[OBSLUGA] semget - nie mozna polaczyc z semaforem");
        exit(EXIT_FAILURE);
    }

    int msg = msgget(key, 0);
    if (msg == -1) {
        perror("[OBSLUGA] msgget - nie mozna polaczyc z kolejka komunikatow");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[OBSLUGA] shmat - nie mozna dolaczyc pamieci dzielonej");
        exit(EXIT_FAILURE);
    }

    printf("[OBSLUGA] Polaczono z zasobami IPC (shm=%d, sem=%d, msg=%d)\n", shm, sem, msg);

    /* Inicjalizacja statystyk kasy */
    lock(sem);
    r->kasa.transakcje = 0;
    r->kasa.suma_dzienna = 0;
    for (int i = 0; i < MAX_GRUP; i++) {
        r->grupa_zaplacila[i] = 0;
        for (int j = 0; j < KOLORY; j++) {
            r->grupa_talerzyki[i][j] = 0;
        }
    }
    unlock(sem);

    while (r->otwarta) {
        /* 1. Odbierz nowe zgłoszenia klientów (non-blocking) */
        struct komunikat buf;
        ssize_t recv_result;
        while ((recv_result = msgrcv(msg, &buf, sizeof(struct komunikat) - sizeof(long), 1, IPC_NOWAIT)) != -1) {
            /* Walidacja danych przed dodaniem do kolejki */
            if (buf.rozmiar_grupy >= 1 && buf.rozmiar_grupy <= 4 && buf.pid_nadawcy > 0) {
                dodaj_do_kolejki(buf.rozmiar_grupy, buf.group_priority, buf.pid_nadawcy);
            } else {
                zrzut_do_logu("[OBSLUGA] Odrzucono niepoprawne zgloszenie: rozmiar=%d, pid=%d",
                              buf.rozmiar_grupy, buf.pid_nadawcy);
            }
        }
        /* Sprawdzenie błędu msgrcv (ENOMSG to normalne przy IPC_NOWAIT) */
        if (recv_result == -1 && errno != ENOMSG && errno != EINTR) {
            if (errno == EIDRM) {
                /* Kolejka usunięta - restauracja zamykana */
                break;
            }
            perror("[OBSLUGA] msgrcv");
        }

        /* 1b. Odbierz żądania rachunków (KASA) */
        struct komunikat_kasa_req kasa_req;
        while (msgrcv(msg, &kasa_req, sizeof(kasa_req) - sizeof(long),
                      -(MSG_KASA_REQ + MAX_GRUP), IPC_NOWAIT) != -1) {

            int id_grupy = kasa_req.id_grupy;
            pid_t pid_lidera = kasa_req.pid_lidera;

            /* Obliczenie sumy rachunku */
            int suma = oblicz_rachunek(kasa_req.talerzyki);

            /* Aktualizacja statystyk kasy */
            lock(sem);
            r->kasa.transakcje++;
            r->kasa.suma_dzienna += suma;
            r->grupa_zaplacila[id_grupy] = 1;
            sprintf(r->info, "KASA: Grupa %d zaplacila %d zl", id_grupy, suma);
            unlock(sem);

            /* Wysłanie odpowiedzi z rachunkiem */
            struct komunikat_kasa_resp kasa_resp;
            kasa_resp.mtype = pid_lidera;
            kasa_resp.suma = suma;
            for (int i = 0; i < KOLORY; i++) {
                kasa_resp.talerzyki[i] = kasa_req.talerzyki[i];
            }

            if (msgsnd(msg, &kasa_resp, sizeof(kasa_resp) - sizeof(long), 0) == -1) {
                if (errno != EIDRM) {
                    perror("[OBSLUGA/KASA] msgsnd odpowiedz");
                }
            } else {
                zrzut_do_logu("KASA: Grupa %d (lider PID=%d) zaplacila %d zl",
                              id_grupy, pid_lidera, suma);
            }
        }

        lock(sem);
        
        // 2. LOGIKA TETRIS 
        // G4
        if (q4_cnt > 0) {
            for (int i=6; i<10; i++) {
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 4;
                    r->stoliki[i].rozmiar_grupy = 4;  /* Zapamiętaj rozmiar grupy */
                    zapros_klienta(msg, queue_g4, &q4_cnt, 0, i, 1);
                    if(q4_cnt == 0) break;
                }
            }
        }
        // G3
        if (q3_cnt > 0) {
            for (int i=3; i<6; i++) {
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 3;
                    r->stoliki[i].rozmiar_grupy = 3;
                    zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                    if(q3_cnt == 0) break;
                }
            }
            if (q3_cnt > 0 && q4_cnt == 0) {
                for (int i=6; i<10; i++) {
                     if (r->stoliki[i].ile_osob == 0) {
                        r->stoliki[i].ile_osob = 3;
                        r->stoliki[i].rozmiar_grupy = 3;
                        zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                        if(q3_cnt == 0) break;
                    }
                }
            }
        }
        // G2
        if (q2_cnt > 0) {
            /* VIP G2 nie siada przy ladzie - sprawdź czy pierwszy w kolejce ma priorytet */
            int g2_vip = (q2_cnt > 0 && queue_g2[0].group_priority);

            /* Lada tylko dla nie-VIP */
            if (!g2_vip) {
                for (int i = 0; i < LADA_MIEJSC - 1; i++) {
                    if (r->lada[i].zajete == 0 && r->lada[i+1].zajete == 0) {
                        r->lada[i].zajete=1; r->lada[i+1].zajete=1;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 0);
                        if(q2_cnt == 0) break;
                    }
                }
            }
            if (q2_cnt > 0) {
                for (int i=0; i<3; i++) {
                    if (r->stoliki[i].ile_osob==0) {
                        r->stoliki[i].ile_osob=2;
                        r->stoliki[i].rozmiar_grupy = 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt==0) break;
                    }
                }
            }
            if (q2_cnt > 0 && q3_cnt == 0) {
                for (int i=3; i<6; i++) {
                    if (r->stoliki[i].ile_osob==0) {
                        r->stoliki[i].ile_osob=2;
                        r->stoliki[i].rozmiar_grupy = 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt==0) break;
                    }
                }
            }
            if (q2_cnt > 0) {
                for (int i=6; i<10; i++) {
                    int wolne = r->stoliki[i].pojemnosc - r->stoliki[i].ile_osob;
                    /* DOSIADANIE RÓWNOLICZNE: tylko G2 do G2 */
                    int dosiadka_ok = (r->stoliki[i].ile_osob > 0 && r->stoliki[i].rozmiar_grupy == 2);
                    int pusty_ok = (r->stoliki[i].ile_osob == 0 && q4_cnt==0 && q3_cnt==0);
                    if (wolne >= 2 && (dosiadka_ok || pusty_ok)) {
                        if (pusty_ok) r->stoliki[i].rozmiar_grupy = 2;
                        r->stoliki[i].ile_osob += 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt == 0) break;
                    }
                }
            }
        }
        // G1
        if (q1_cnt > 0) {
            /* VIP G1 nie siada przy ladzie - sprawdź czy pierwszy w kolejce ma priorytet */
            int g1_vip = (q1_cnt > 0 && queue_g1[0].group_priority);

            /* Lada tylko dla nie-VIP */
            if (!g1_vip) {
                for (int i=0; i<LADA_MIEJSC; i++) {
                    if (r->lada[i].zajete == 0) {
                        r->lada[i].zajete = 1; zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 0);
                        if(q1_cnt == 0) break;
                    }
                }
            }
            if (q1_cnt > 0) {
                for (int i=0; i<STOLIKI; i++) {
                    if (r->stoliki[i].ile_osob < r->stoliki[i].pojemnosc) {
                        int pusty = (r->stoliki[i].ile_osob == 0);
                        int blokada = 0;
                        if (pusty && (q4_cnt>0 || q3_cnt>0 || q2_cnt>0)) blokada = 1;
                        /* DOSIADANIE RÓWNOLICZNE: tylko G1 do G1 */
                        int dosiadka_ok = (!pusty && r->stoliki[i].rozmiar_grupy == 1);

                        if (!blokada && (pusty || dosiadka_ok)) {
                            if (pusty) r->stoliki[i].rozmiar_grupy = 1;
                            r->stoliki[i].ile_osob += 1; zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 1);
                            if(q1_cnt == 0) break;
                        }
                    }
                }
            }
        }
        
        unlock(sem);

        // 3. WYSWIETLANIE
        printf("\033[H\033[J"); /* ANSI: kursor home + clear (szybsze niż system("clear")) */
        printf("\n=== MONITOR SALI I KOLEJEK ===\n");
        printf("Kolejki: G4:[%d:%d] G3:[%d:%d] G2:[%d:%d] G1:[%d:%d]\n", 
               q4_cnt, queue_g4[0].group_priority, 
               q3_cnt, queue_g3[0].group_priority, 
               q2_cnt, queue_g2[0].group_priority, 
               q1_cnt, queue_g1[0].group_priority);
        printf("---------------------------------------------------------------------------------\n");
        printf("| SEG |       MIEJSCE       |          NA TASMIE           |      KLIENCI       \n");
        printf("---------------------------------------------------------------------------------\n");

        for (int seg = 0; seg < SEGMENTY; seg++) {
            printf("| %02d  | ", seg);
            char bufor_miejsca[30];
            if (seg == 0) sprintf(bufor_miejsca, "KUCHARZ");
            else if (seg >= 1 && seg <= 9) {
                int idx = seg - 1;
                sprintf(bufor_miejsca, "LADA %d %s", idx, r->lada[idx].zajete ? "(ZAJ)" : "(wol)");
            } else {
                int idx = seg - 10;
                sprintf(bufor_miejsca, "STOL %d (%d/%d)", idx, r->stoliki[idx].ile_osob, r->stoliki[idx].pojemnosc);
            }
            printf("%-18s | ", bufor_miejsca);

            /* Wyświetlanie talerzyków z kolorami */
            if (r->tasma.seg[seg].zajety) {
                struct talerzyk t = r->tasma.seg[seg].t;
                const char *kolor = kolory_ansi[t.kolor];
                if(t.id_odbiorcy != -1)
                    printf("%s%-10s%s($%d)->%-5d | ", kolor, nazwy_kolorow[t.kolor], ANSI_RESET, t.cena, t.id_odbiorcy);
                else
                    printf("%s%-10s%s ($%d)       | ", kolor, nazwy_kolorow[t.kolor], ANSI_RESET, t.cena);
            } else {
                printf("---                  | ");
            }

            /* Wyświetlanie klientów z kolorami VIP */
            int found = 0;
            if (seg > 0) {
                for (int i = 0; i < MAX_KLIENTOW; i++) {
                    struct klient_info *k = &r->klienci[i];
                    if (!k->aktywny || k->segment != seg) continue;
                    if (found) printf(", ");

                    if (k->is_vip) {
                        printf("%sVIP%s PID=%d G=%d (%d/%d)", ANSI_GOLD, ANSI_RESET, k->pid, k->id_grupy, k->zjedzone, k->limit);
                    } else if (k->is_child) {
                        printf("%sJUNIOR%s PID=%d G=%d (%d/%d)", ANSI_CYAN, ANSI_RESET, k->pid, k->id_grupy, k->zjedzone, k->limit);
                    } else {
                        printf("PID=%d G=%d (%d/%d)", k->pid, k->id_grupy, k->zjedzone, k->limit);
                    }
                    found = 1;
                }
            }
            printf("\n");
        }
        printf("---------------------------------------------------------------------------------\n");
        printf("[KASA] Transakcje: %d | Utarg: %d zl\n", r->kasa.transakcje, r->kasa.suma_dzienna);
        printf("[INFO]: %s\n", r->info);
        fflush(stdout);

        sleep(1); /* Odświeżanie co 1s */
    }

    printf("[OBSLUGA] Restauracja zamknieta. Generowanie raportu...\n");

    /* Generowanie podsumowania końcowego */
    generuj_podsumowanie(r);

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[OBSLUGA] shmdt");
    }

    printf("[OBSLUGA] Koniec pracy.\n");
    return 0;
}