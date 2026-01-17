#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
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

    key_t key = ftok("ipc_keyfile", 'R');
    if (key == -1) exit(1);
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    if (shm == -1 || sem == -1) exit(1);
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
            r->klienci[i].czeka_na_specjalne = 0;
            moj_slot_idx = i;
            break;
        }
    }
    unlock(sem);

    printf("[KLIENT %d] G=%d Szukam miejsca.\n", getpid(), moj_gid);

    int moj_segment = -1;
    int typ_miejsca = -1; 
    int idx_miejsca = -1;

    while (r->otwarta && moj_segment == -1) {
        lock(sem);
        int wybrany_idx = r->gdzie_siedzimy[moj_gid];
        int wybrany_typ = r->typ_miejsca_grupy[moj_gid];

        if (wybrany_idx != -1) {
            if (wybrany_typ == 0) { 
                int moje_krzeslo = wybrany_idx + 1; 
                if (moje_krzeslo < LADA_MIEJSC && r->lada[moje_krzeslo].zajete == 1) {
                     moj_segment = r->lada[moje_krzeslo].segment; idx_miejsca = moje_krzeslo; typ_miejsca = 0;
                }
            } else if (wybrany_typ == 1) {
                if (r->stoliki[wybrany_idx].ile_osob < r->stoliki[wybrany_idx].pojemnosc) {
                    r->stoliki[wybrany_idx].ile_osob++;
                    moj_segment = r->stoliki[wybrany_idx].segment; idx_miejsca = wybrany_idx; typ_miejsca = 1;
                }
            }
        } else {
            int znaleziono = 0;
            if (moja_grupa_size <= 2) {
                if (moja_grupa_size == 1) {
                   for (int i = 0; i < LADA_MIEJSC; i++) if (r->lada[i].zajete == 0) {
                        r->lada[i].zajete=1; r->gdzie_siedzimy[moj_gid]=i; r->typ_miejsca_grupy[moj_gid]=0;
                        moj_segment=r->lada[i].segment; idx_miejsca=i; typ_miejsca=0; znaleziono=1; break;
                   }
                } else if (moja_grupa_size == 2) {
                   for (int i = 0; i < LADA_MIEJSC-1; i++) if (r->lada[i].zajete==0 && r->lada[i+1].zajete==0) {
                        r->lada[i].zajete=1; r->lada[i+1].zajete=1; r->gdzie_siedzimy[moj_gid]=i; r->typ_miejsca_grupy[moj_gid]=0;
                        moj_segment=r->lada[i].segment; idx_miejsca=i; typ_miejsca=0; znaleziono=1; break;
                   }
                }
            }
            if (!znaleziono) {
                for (int i = 0; i < STOLIKI; i++) {
                    if (r->stoliki[i].id_grupy == -1 && r->stoliki[i].pojemnosc >= moja_grupa_size) {
                        r->stoliki[i].id_grupy = moj_gid; r->stoliki[i].ile_osob = 1; r->gdzie_siedzimy[moj_gid]=i; r->typ_miejsca_grupy[moj_gid]=1;
                        moj_segment = r->stoliki[i].segment; idx_miejsca = i; typ_miejsca = 1; break;
                    }
                }
            }
        }
        if (moj_segment != -1 && moj_slot_idx != -1) r->klienci[moj_slot_idx].segment = moj_segment;
        unlock(sem);
        if (moj_segment == -1) usleep(100000); 
    }

    if (moj_segment == -1) {
        if (moj_slot_idx != -1) { lock(sem); r->klienci[moj_slot_idx].aktywny = 0; unlock(sem); }
        shmdt(r); return 0;
    }

    int zjedzone = 0;
    int oczekuje_na_specjalne = 0;
    int zamowiony_typ = -1;

    while (r->otwarta && zjedzone < do_zjedzenia) {
        
        if (!oczekuje_na_specjalne) {
            
            // Losujemy liczbe z zakresu 0-4999.
            // Segment 1: 1/5000 = 0.02% szans
            // Segment 19: 19/5000 = 0.38% szans
            
            if (rand() % 5000 < moj_segment) {
                lock(sem);
                for(int i=0; i<MAX_ZAMOWIEN; i++) {
                    if(!r->zamowienia[i].aktywne) {
                        zamowiony_typ = 3 + (rand() % 3); // Kolory 3,4,5
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

        lock(sem);
        if (r->tasma.seg[moj_segment].zajety) {
            struct talerzyk t = r->tasma.seg[moj_segment].t;
            int czy_jem = 0;

            if (oczekuje_na_specjalne) {
                if (t.id_odbiorcy == getpid()) {
                    sprintf(r->info, ">>> KLIENT %d ODEBRAL %s!", getpid(), nazwy_kolorow[t.kolor]);
                    czy_jem = 1;
                    oczekuje_na_specjalne = 0;
                    if(moj_slot_idx != -1) r->klienci[moj_slot_idx].czeka_na_specjalne = 0;
                }
            } else {
                if (t.id_odbiorcy == -1) {
                    if (rand() % 100 < 60) { 
                        czy_jem = 1;
                    }
                }
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
        if (r->gdzie_siedzimy[moj_gid] == idx_miejsca) { 
             r->gdzie_siedzimy[moj_gid] = -1;
             r->grupa_zjedzone_cnt[moj_gid] = 0;
        }
    } else {
        r->stoliki[idx_miejsca].ile_osob--;
        if (r->stoliki[idx_miejsca].ile_osob == 0) {
            r->stoliki[idx_miejsca].id_grupy = -1;
            r->gdzie_siedzimy[moj_gid] = -1; 
            r->grupa_zjedzone_cnt[moj_gid] = 0;
        }
    }
    if (moj_slot_idx != -1) r->klienci[moj_slot_idx].aktywny = 0;
    unlock(sem);
    shmdt(r);
    return 0;
}