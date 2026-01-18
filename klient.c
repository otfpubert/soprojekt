#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>

#include "wspolne.h"

int main(int argc, char *argv[]) {
    srand(getpid() ^ time(NULL));

    int moj_gid = 0;
    int moja_grupa_size = 1;
    if (argc >= 3) {
        moj_gid = atoi(argv[1]);
        moja_grupa_size = atoi(argv[2]);
    }
    if (moj_gid >= MAX_GRUP) exit(1);

    key_t key = ftok("ipc_keyfile", 'Z');
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    int msg = msgget(key, 0); 
    if (shm == -1 || sem == -1 || msg == -1) exit(1);
    struct restauracja *r = shmat(shm, NULL, 0);

    int moj_slot_idx = -1;
    int do_zjedzenia = 2 + rand() % 3;
    lock(sem);
    for (int i = 0; i < MAX_KLIENTOW; i++) {
        if (!r->klienci[i].aktywny) {
            r->klienci[i].aktywny = 1;
            r->klienci[i].pid = getpid();
            r->klienci[i].id_grupy = moj_gid;
            r->klienci[i].limit = do_zjedzenia;
            r->klienci[i].zjedzone = 0;
            r->klienci[i].segment = -1; 
            moj_slot_idx = i;
            break;
        }
    }
    unlock(sem);

    printf("[KLIENT %d] G=%d Osob=%d. Szukam miejsca...\n", getpid(), moj_gid, moja_grupa_size);

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
        struct komunikat zapytanie;
        zapytanie.mtype = 1; 
        zapytanie.pid_nadawcy = getpid();
        zapytanie.rozmiar_grupy = moja_grupa_size;
        msgsnd(msg, &zapytanie, sizeof(struct komunikat) - sizeof(long), 0);

        struct komunikat odpowiedz;
        msgrcv(msg, &odpowiedz, sizeof(struct komunikat) - sizeof(long), getpid(), 0);

        typ_miejsca = odpowiedz.typ_miejsca;
        idx_miejsca = odpowiedz.numer_miejsca;

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
                    if (moja_grupa_size == 2) {
                        idx_miejsca = baza + 1; 
                    } else {
                        idx_miejsca = baza;
                    }
                    moj_segment = r->lada[idx_miejsca].segment;
                } 
                else { // Stolik
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
                if (rand() % 5000 < moj_segment) { 
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
                if (moj_slot_idx != -1) r->klienci[moj_slot_idx].zjedzone = zjedzone;
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

    lock(sem);
    if (typ_miejsca == 0) {
        r->lada[idx_miejsca].zajete = 0;
        if (r->grupa_zjedzone_cnt[moj_gid] >= moja_grupa_size) {
             r->gdzie_siedzimy[moj_gid] = -1;
        }
    } else {
        r->stoliki[idx_miejsca].ile_osob--;
        if (r->stoliki[idx_miejsca].ile_osob == 0) {
            r->stoliki[idx_miejsca].id_grupy = -1;
            r->gdzie_siedzimy[moj_gid] = -1; 
        }
    }
    if (moj_slot_idx != -1) r->klienci[moj_slot_idx].aktywny = 0;
    unlock(sem);
    shmdt(r);
    return 0;
}