/*
 * obsluga.c - Proces obsługi restauracji (manager sali)
 *
 * Odpowiada za:
 * - Przyjmowanie zgłoszeń klientów przez kolejkę komunikatów
 * - Przydzielanie miejsc przy ladzie i stolikach (algorytm "Tetris")
 * - Wyświetlanie monitora stanu sali w czasie rzeczywistym
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>

#include "wspolne.h"

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
    printf("[OBSLUGA] Start managera sali\n");

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

        lock(sem);
        
        // 2. LOGIKA TETRIS 
        // G4
        if (q4_cnt > 0) {
            for (int i=6; i<10; i++) { 
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 4; zapros_klienta(msg, queue_g4, &q4_cnt, 0, i, 1);
                    if(q4_cnt == 0) break;
                }
            }
        }
        // G3
        if (q3_cnt > 0) {
            for (int i=3; i<6; i++) { 
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 3; zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                    if(q3_cnt == 0) break;
                }
            }
            if (q3_cnt > 0 && q4_cnt == 0) { 
                for (int i=6; i<10; i++) {
                     if (r->stoliki[i].ile_osob == 0) {
                        r->stoliki[i].ile_osob = 3; zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                        if(q3_cnt == 0) break;
                    }
                }
            }
        }
        // G2
        if (q2_cnt > 0) {
            for (int i = 0; i < LADA_MIEJSC - 1; i++) { 
                if (r->lada[i].zajete == 0 && r->lada[i+1].zajete == 0) {
                    r->lada[i].zajete=1; r->lada[i+1].zajete=1;
                    zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 0); 
                    if(q2_cnt == 0) break;
                }
            }
            if (q2_cnt > 0) { 
                for (int i=0; i<3; i++) { 
                    if (r->stoliki[i].ile_osob==0) { r->stoliki[i].ile_osob=2; zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1); if(q2_cnt==0) break;}
                }
            }
            if (q2_cnt > 0 && q3_cnt == 0) { 
                for (int i=3; i<6; i++) {
                    if (r->stoliki[i].ile_osob==0) { r->stoliki[i].ile_osob=2; zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1); if(q2_cnt==0) break;}
                }
            }
            if (q2_cnt > 0) { 
                for (int i=6; i<10; i++) {
                    int wolne = r->stoliki[i].pojemnosc - r->stoliki[i].ile_osob;
                    int dosiadka = (r->stoliki[i].ile_osob > 0);
                    int pusty_ok = (r->stoliki[i].ile_osob == 0 && q4_cnt==0 && q3_cnt==0);
                    if (wolne >= 2 && (dosiadka || pusty_ok)) {
                        r->stoliki[i].ile_osob += 2; zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt == 0) break;
                    }
                }
            }
        }
        // G1
        if (q1_cnt > 0) {
            for (int i=0; i<LADA_MIEJSC; i++) {
                if (r->lada[i].zajete == 0) {
                    r->lada[i].zajete = 1; zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 0); 
                    if(q1_cnt == 0) break;
                }
            }
            if (q1_cnt > 0) {
                for (int i=0; i<STOLIKI; i++) {
                    if (r->stoliki[i].ile_osob < r->stoliki[i].pojemnosc) {
                        int pusty = (r->stoliki[i].ile_osob == 0);
                        int blokada = 0;
                        if (pusty && (q4_cnt>0 || q3_cnt>0 || q2_cnt>0)) blokada = 1; 
                        
                        if (!blokada) {
                            r->stoliki[i].ile_osob += 1; zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 1);
                            if(q1_cnt == 0) break;
                        }
                    }
                }
            }
        }
        
        unlock(sem);

        // 3. WYSWIETLANIE
        system("clear");
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

            char bufor_jedzenia[40];
            if (r->tasma.seg[seg].zajety) {
                struct talerzyk t = r->tasma.seg[seg].t;
                if(t.id_odbiorcy != -1) sprintf(bufor_jedzenia, "%s($%d)->%d", nazwy_kolorow[t.kolor], t.cena, t.id_odbiorcy);
                else sprintf(bufor_jedzenia, "%s ($%d)", nazwy_kolorow[t.kolor], t.cena);
            } else sprintf(bufor_jedzenia, "---");
            printf("%-20s | ", bufor_jedzenia);

            int found = 0;
            if (seg > 0) { 
                for (int i = 0; i < MAX_KLIENTOW; i++) {
                    struct klient_info *k = &r->klienci[i];
                    if (!k->aktywny || k->segment != seg) continue;
                    if (found) printf(", ");
                    
                    char status[15] = "";
                    if (k->is_vip) strcat(status, "VIP ");
                    if (k->is_child) strcat(status, "JUNIOR ");
                    
                    printf("PID=%d %sG=%d (%d/%d)", k->pid, status, k->id_grupy, k->zjedzone, k->limit);
                    found = 1;
                }
            }
            printf("\n");
        }
        printf("---------------------------------------------------------------------------------\n");
        printf("[INFO]: %s\n", r->info);

        sleep(1);
    }

    printf("[OBSLUGA] Restauracja zamknieta. Konczenie pracy.\n");

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[OBSLUGA] shmdt");
    }

    return 0;
}