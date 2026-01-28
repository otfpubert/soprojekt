/*
 * klient.c - Proces klienta restauracji
 *
 * Odpowiada za:
 * - Rejestrację w systemie i oczekiwanie na miejsce
 * - Jedzenie potraw z taśmy
 * - Składanie zamówień specjalnych
 * - Synchronizację z innymi członkami grupy
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "wspolne.h"

/* Zmienne globalne do sprzątania w handlerze sygnału */
struct restauracja *r_global = NULL;
int moj_slot_idx_global = -1;
int sem_global = -1;

/*
 * Handler sygnału SIGTERM - ewakuacja klienta.
 * Zwalnia slot w tablicy klientów i kończy proces.
 */
void handler_ewakuacja(int sig) {
    (void)sig; /* Suppress unused parameter warning */

    if (r_global != NULL && moj_slot_idx_global != -1 && sem_global != -1) {
        lock(sem_global);
        r_global->klienci[moj_slot_idx_global].aktywny = 0;
        unlock(sem_global);
    }
    zrzut_do_logu("KLIENT %d: EWAKUACJA! Uciekam!", getpid());
    exit(0);
}

int main(int argc, char *argv[]) {
    srand(getpid() ^ time(NULL));

    /* Rejestracja handlera ewakuacji */
    if (signal(SIGTERM, handler_ewakuacja) == SIG_ERR) {
        perror("[KLIENT] signal(SIGTERM)");
        exit(EXIT_FAILURE);
    }

    /* Parsowanie argumentów */
    int moj_gid = 0;
    int moja_grupa_size = 1;
    int ja_jestem_vip = 0;
    int moja_grupa_ma_priorytet = 0;
    int ja_jestem_dziecko = 0;

    if (argc >= 6) {
        moj_gid = atoi(argv[1]);
        moja_grupa_size = atoi(argv[2]);
        ja_jestem_vip = atoi(argv[3]);
        moja_grupa_ma_priorytet = atoi(argv[4]);
        ja_jestem_dziecko = atoi(argv[5]);
    } else {
        fprintf(stderr, "[KLIENT %d] Blad: za malo argumentow (oczekiwano 5)\n", getpid());
        exit(EXIT_FAILURE);
    }

    /* Walidacja ID grupy */
    if (moj_gid < 0 || moj_gid >= MAX_GRUP) {
        fprintf(stderr, "[KLIENT %d] Blad: niepoprawny ID grupy %d (max %d)\n",
                getpid(), moj_gid, MAX_GRUP - 1);
        exit(EXIT_FAILURE);
    }

    /* Walidacja rozmiaru grupy */
    if (moja_grupa_size < 1 || moja_grupa_size > 4) {
        fprintf(stderr, "[KLIENT %d] Blad: niepoprawny rozmiar grupy %d (1-4)\n",
                getpid(), moja_grupa_size);
        exit(EXIT_FAILURE);
    }

    /* Generowanie klucza IPC */
    key_t key = ftok("ipc_keyfile", 'C');
    if (key == -1) {
        perror("[KLIENT] ftok");
        exit(EXIT_FAILURE);
    }

    /* Połączenie z zasobami IPC */
    int shm = shmget(key, sizeof(struct restauracja), 0);
    if (shm == -1) {
        perror("[KLIENT] shmget");
        exit(EXIT_FAILURE);
    }

    int sem = semget(key, 1, 0);
    if (sem == -1) {
        perror("[KLIENT] semget");
        exit(EXIT_FAILURE);
    }

    int msg = msgget(key, 0);
    if (msg == -1) {
        perror("[KLIENT] msgget");
        exit(EXIT_FAILURE);
    }

    /* Dołączenie do pamięci dzielonej */
    struct restauracja *r = shmat(shm, NULL, 0);
    if (r == (void *)-1) {
        perror("[KLIENT] shmat");
        exit(EXIT_FAILURE);
    }

    // Ustawienie zmiennych globalnych dla handlera
    r_global = r;
    sem_global = sem;

    int moj_slot_idx = -1;
    /* Losowy limit talerzyków: MIN_TALERZYKI do MAX_TALERZYKI (wymaganie: 3-10) */
    int do_zjedzenia = MIN_TALERZYKI + rand() % (MAX_TALERZYKI - MIN_TALERZYKI + 1);

    /* Lokalne zliczanie zjedzonych talerzyków per kolor (do rachunku) */
    int moje_talerzyki[KOLORY] = {0};

    lock(sem);
    for (int i = 0; i < MAX_KLIENTOW; i++) {
        if (!r->klienci[i].aktywny) {
            r->klienci[i].aktywny = 1;
            r->klienci[i].pid = getpid();
            r->klienci[i].id_grupy = moj_gid;
            r->klienci[i].limit = do_zjedzenia;
            r->klienci[i].zjedzone = 0;
            r->klienci[i].segment = -1; 
            r->klienci[i].is_vip = ja_jestem_vip;
            r->klienci[i].is_child = ja_jestem_dziecko;
            /* Inicjalizacja liczników talerzyków */
            for (int j = 0; j < KOLORY; j++) {
                r->klienci[i].talerzyki[j] = 0;
            }
            moj_slot_idx = i;
            break;
        }
    }
    unlock(sem);
    
    // Aktualizacja globalnego indeksu
    moj_slot_idx_global = moj_slot_idx;

    if(ja_jestem_vip) printf("[KLIENT %d] VIP z G=%d wchodzi!\n", getpid(), moj_gid);
    else if(ja_jestem_dziecko) printf("[KLIENT %d] Dziecko z G=%d wchodzi!\n", getpid(), moj_gid);
    else printf("[KLIENT %d] Klient z G=%d wchodzi.\n", getpid(), moj_gid);

    int moj_segment = -1;
    int typ_miejsca = -1;
    int idx_miejsca = -1;
    int jestem_liderem = 0;

    lock(sem);
    if (r->gdzie_siedzimy[moj_gid] == -1) {
        r->gdzie_siedzimy[moj_gid] = -2;
        jestem_liderem = 1;
    }
    unlock(sem);

    if (jestem_liderem) {
        /* Lider wysyła zgłoszenie do obsługi */
        struct komunikat zapytanie;
        zapytanie.mtype = 1;
        zapytanie.pid_nadawcy = getpid();
        zapytanie.rozmiar_grupy = moja_grupa_size;
        zapytanie.group_priority = moja_grupa_ma_priorytet;

        if (msgsnd(msg, &zapytanie, sizeof(struct komunikat) - sizeof(long), 0) == -1) {
            if (errno == EIDRM) {
                /* Kolejka usunięta - restauracja zamykana */
                zrzut_do_logu("KLIENT %d: Restauracja zamknieta podczas rejestracji", getpid());
                shmdt(r);
                exit(0);
            }
            perror("[KLIENT] msgsnd - blad wysylania zgloszenia");
            shmdt(r);
            exit(EXIT_FAILURE);
        }

        /* Oczekiwanie na odpowiedź z przydziałem miejsca */
        struct komunikat odpowiedz;
        if (msgrcv(msg, &odpowiedz, sizeof(struct komunikat) - sizeof(long), getpid(), 0) == -1) {
            if (errno == EIDRM || errno == EINTR) {
                /* Kolejka usunięta lub przerwane sygnałem */
                zrzut_do_logu("KLIENT %d: Przerywam oczekiwanie na miejsce", getpid());
                shmdt(r);
                exit(0);
            }
            perror("[KLIENT] msgrcv - blad odbierania odpowiedzi");
            shmdt(r);
            exit(EXIT_FAILURE);
        }

        typ_miejsca = odpowiedz.typ_miejsca;
        idx_miejsca = odpowiedz.numer_miejsca;

        zrzut_do_logu("KLIENT PID=%d (G=%d): Otrzymalem miejsce nr %d (Typ: %d)",
                      getpid(), moj_gid, idx_miejsca, typ_miejsca);

        lock(sem);
        r->typ_miejsca_grupy[moj_gid] = typ_miejsca;
        r->gdzie_siedzimy[moj_gid] = idx_miejsca; 
        
        if (typ_miejsca == 0) {
            moj_segment = r->lada[idx_miejsca].segment;
        } else {
            moj_segment = r->stoliki[idx_miejsca].segment;
            r->stoliki[idx_miejsca].id_grupy = moj_gid;
        }
        unlock(sem);
    } 
    else {
        while (1) {
            lock(sem);
            int status = r->gdzie_siedzimy[moj_gid];
            if (status >= 0) {
                int baza = status;
                typ_miejsca = r->typ_miejsca_grupy[moj_gid];
                
                if (typ_miejsca == 0) { 
                    if (moja_grupa_size == 2) idx_miejsca = baza + 1;
                    else idx_miejsca = baza;
                    moj_segment = r->lada[idx_miejsca].segment;
                } 
                else { 
                    idx_miejsca = baza;
                    moj_segment = r->stoliki[idx_miejsca].segment;
                }
                unlock(sem);
                break; 
            }
            unlock(sem);
            usleep(100000); 
        }
    }
    
    lock(sem);
    if (moj_slot_idx != -1) r->klienci[moj_slot_idx].segment = moj_segment;
    unlock(sem);

    int zjedzone = 0;
    int oczekuje_na_specjalne = 0;
    
    while (r->otwarta && zjedzone < do_zjedzenia) {
        if (!oczekuje_na_specjalne) {
            if (moj_segment > 15) {
                if (!ja_jestem_dziecko && rand() % 5000 < moj_segment) {
                    lock(sem);
                    for(int i=0; i<MAX_ZAMOWIEN; i++) {
                        if(!r->zamowienia[i].aktywne) {
                            int zamowiony_typ = 3 + (rand() % 3);
                            r->zamowienia[i].pid_klienta = getpid();
                            r->zamowienia[i].typ_dania = zamowiony_typ;
                            r->zamowienia[i].aktywne = 1;
                            oczekuje_na_specjalne = 1;
                            if(moj_slot_idx != -1) r->klienci[moj_slot_idx].czeka_na_specjalne = 1;
                            sprintf(r->info, "KLIENT %d (G%d Seg%d) zamowil %s!", getpid(), moj_gid, moj_segment, nazwy_kolorow[zamowiony_typ]);
                            break;
                        }
                    }
                    unlock(sem);
                }
            }
        }

        lock(sem);
        if (r->tasma.seg[moj_segment].zajety) {
            struct talerzyk t = r->tasma.seg[moj_segment].t;
            int czy_jem = 0;

            if (oczekuje_na_specjalne) {
                if (t.id_odbiorcy == getpid()) {
                    sprintf(r->info, ">>> KLIENT %d ODEBRAL %s!", getpid(), nazwy_kolorow[t.kolor]);
                    czy_jem = 1; oczekuje_na_specjalne = 0;
                    if(moj_slot_idx != -1) r->klienci[moj_slot_idx].czeka_na_specjalne = 0;
                }
            } else {
                if (t.id_odbiorcy == -1 && rand() % 100 < 60) czy_jem = 1;
            }

            if (czy_jem) {
                r->tasma.seg[moj_segment].zajety = 0;
                r->sprzedane[t.kolor]++;
                zjedzone++;
                /* Zliczanie talerzyków per kolor - lokalnie i w SHM */
                moje_talerzyki[t.kolor]++;
                if (moj_slot_idx != -1) {
                    r->klienci[moj_slot_idx].zjedzone = zjedzone;
                    r->klienci[moj_slot_idx].talerzyki[t.kolor]++;
                }
                /* Aktualizacja licznika grupy */
                r->grupa_talerzyki[moj_gid][t.kolor]++;
                printf("[KLIENT %d] Zjadl %s\n", getpid(), nazwy_kolorow[t.kolor]);
            }
        }
        unlock(sem);
        sleep(1); 
    }

    lock(sem);
    r->grupa_zjedzone_cnt[moj_gid]++;
    unlock(sem);

    int moge_wyjsc = 0;
    while (r->otwarta && !moge_wyjsc) {
        lock(sem);
        if (r->grupa_zjedzone_cnt[moj_gid] >= moja_grupa_size) moge_wyjsc = 1;
        unlock(sem);
        if (!moge_wyjsc) sleep(1);
    }

    /* === PŁATNOŚĆ - tylko lider płaci za całą grupę === */
    if (jestem_liderem && r->otwarta) {
        lock(sem);
        int czy_zaplacona = r->grupa_zaplacila[moj_gid];
        unlock(sem);

        if (!czy_zaplacona) {
            /* Przygotowanie żądania rachunku */
            struct komunikat_kasa_req req;
            req.mtype = MSG_KASA_REQ + moj_gid;
            req.pid_lidera = getpid();
            req.id_grupy = moj_gid;

            /* Kopiowanie talerzyków grupy */
            lock(sem);
            for (int i = 0; i < KOLORY; i++) {
                req.talerzyki[i] = r->grupa_talerzyki[moj_gid][i];
            }
            unlock(sem);

            /* Wysłanie żądania do kasy */
            if (msgsnd(msg, &req, sizeof(struct komunikat_kasa_req) - sizeof(long), 0) == -1) {
                if (errno != EIDRM) {
                    perror("[KLIENT] msgsnd - blad wysylania zadania rachunku");
                }
            } else {
                /* Oczekiwanie na rachunek */
                struct komunikat_kasa_resp resp;
                if (msgrcv(msg, &resp, sizeof(struct komunikat_kasa_resp) - sizeof(long), getpid(), 0) == -1) {
                    if (errno != EIDRM && errno != EINTR) {
                        perror("[KLIENT] msgrcv - blad odbierania rachunku");
                    }
                } else {
                    printf("[KLIENT %d] Rachunek dla grupy %d: %d zl\n", getpid(), moj_gid, resp.suma);
                    zrzut_do_logu("KLIENT %d (G=%d): Zaplacil rachunek %d zl", getpid(), moj_gid, resp.suma);

                    lock(sem);
                    r->grupa_zaplacila[moj_gid] = 1;
                    unlock(sem);
                }
            }
        }
    }

    lock(sem);
    if (typ_miejsca == 0) {
        r->lada[idx_miejsca].zajete = 0;
        zrzut_do_logu("KLIENT PID=%d (G=%d): Zwolnilem LADE nr %d", getpid(), moj_gid, idx_miejsca);
        if (r->grupa_zjedzone_cnt[moj_gid] >= moja_grupa_size) {
             r->gdzie_siedzimy[moj_gid] = -1;
        }
    } else {
        r->stoliki[idx_miejsca].ile_osob--;
        if (r->stoliki[idx_miejsca].ile_osob == 0) {
            r->stoliki[idx_miejsca].id_grupy = -1;
            r->gdzie_siedzimy[moj_gid] = -1; 
            zrzut_do_logu("KLIENT PID=%d (G=%d): Zwolnilem STOLIK nr %d (Typ: 1)", getpid(), moj_gid, idx_miejsca);
        }
    }
    if (moj_slot_idx != -1) {
        r->klienci[moj_slot_idx].aktywny = 0;
    }
    unlock(sem);

    /* Odłączenie od pamięci dzielonej */
    if (shmdt(r) == -1) {
        perror("[KLIENT] shmdt");
    }

    zrzut_do_logu("KLIENT %d (G=%d): Wychodzi z restauracji", getpid(), moj_gid);
    return 0;
}