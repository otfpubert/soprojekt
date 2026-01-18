#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>

#include "wspolne.h"

#define Q_SIZE 100
pid_t queue_g1[Q_SIZE]; int q1_cnt = 0;
pid_t queue_g2[Q_SIZE]; int q2_cnt = 0;
pid_t queue_g3[Q_SIZE]; int q3_cnt = 0;
pid_t queue_g4[Q_SIZE]; int q4_cnt = 0;

void dodaj_do_kolejki(int rozmiar, pid_t pid) {
    if (rozmiar == 1 && q1_cnt < Q_SIZE) queue_g1[q1_cnt++] = pid;
    else if (rozmiar == 2 && q2_cnt < Q_SIZE) queue_g2[q2_cnt++] = pid;
    else if (rozmiar == 3 && q3_cnt < Q_SIZE) queue_g3[q3_cnt++] = pid;
    else if (rozmiar >= 4 && q4_cnt < Q_SIZE) queue_g4[q4_cnt++] = pid;
}

void zapros_klienta(int msg_id, pid_t *kolejka, int *cnt, int idx_w_kolejce, int nr_miejsca, int typ) {
    pid_t pid = kolejka[idx_w_kolejce];
    
    struct komunikat zaproszenie;
    zaproszenie.mtype = pid; 
    zaproszenie.numer_miejsca = nr_miejsca;
    zaproszenie.typ_miejsca = typ;
    msgsnd(msg_id, &zaproszenie, sizeof(struct komunikat) - sizeof(long), 0);

    for (int i = idx_w_kolejce; i < *cnt - 1; i++) {
        kolejka[i] = kolejka[i+1];
    }
    (*cnt)--;
}

int main() {
    key_t key = ftok("ipc_keyfile", 'Z');
    int shm = shmget(key, sizeof(struct restauracja), 0);
    int sem = semget(key, 1, 0);
    int msg = msgget(key, 0);
    if (shm == -1 || sem == -1 || msg == -1) exit(1);
    struct restauracja *r = shmat(shm, NULL, 0);

    printf("[OBSLUGA] Start managera sali\n");

    while (r->otwarta) {
        struct komunikat buf;
        while (msgrcv(msg, &buf, sizeof(struct komunikat)-sizeof(long), 1, IPC_NOWAIT) != -1) {
            dodaj_do_kolejki(buf.rozmiar_grupy, buf.pid_nadawcy);
        }

        lock(sem);
        
        
        if (q4_cnt > 0) {
            for (int i=6; i<10; i++) { 
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 4; 
                    zapros_klienta(msg, queue_g4, &q4_cnt, 0, i, 1);
                    if(q4_cnt == 0) break;
                }
            }
        }

        if (q3_cnt > 0) {
            for (int i=3; i<6; i++) { 
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 3;
                    zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                    if(q3_cnt == 0) break;
                }
            }
            if (q3_cnt > 0 && q4_cnt == 0) {
                for (int i=6; i<10; i++) {
                     if (r->stoliki[i].ile_osob == 0) {
                        r->stoliki[i].ile_osob = 3;
                        zapros_klienta(msg, queue_g3, &q3_cnt, 0, i, 1);
                        if(q3_cnt == 0) break;
                    }
                }
            }
        }

        if (q2_cnt > 0) {
            for (int i = 0; i < LADA_MIEJSC - 1; i++) {
                if (r->lada[i].zajete == 0 && r->lada[i+1].zajete == 0) {
                    r->lada[i].zajete = 1;
                    r->lada[i+1].zajete = 1;
                    zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 0); 
                    if(q2_cnt == 0) break;
                }
            }

            if (q2_cnt > 0) {
                for (int i=0; i<3; i++) {
                    if (r->stoliki[i].ile_osob == 0) {
                        r->stoliki[i].ile_osob = 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt == 0) break;
                    }
                }
            }
            if (q2_cnt > 0 && q3_cnt == 0) {
                for (int i=3; i<6; i++) {
                    if (r->stoliki[i].ile_osob == 0) {
                        r->stoliki[i].ile_osob = 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt == 0) break;
                    }
                }
            }
            if (q2_cnt > 0) {
                for (int i=6; i<10; i++) {
                    int wolne = r->stoliki[i].pojemnosc - r->stoliki[i].ile_osob;
                    int dosiadka = (r->stoliki[i].ile_osob > 0);
                    int pusty_ale_wolno = (r->stoliki[i].ile_osob == 0 && q4_cnt == 0 && q3_cnt == 0);

                    if (wolne >= 2 && (dosiadka || pusty_ale_wolno)) {
                        r->stoliki[i].ile_osob += 2;
                        zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
                        if(q2_cnt == 0) break;
                    }
                }
            }
        }

        if (q1_cnt > 0) {
            for (int i=0; i<LADA_MIEJSC; i++) {
                if (r->lada[i].zajete == 0) {
                    r->lada[i].zajete = 1;
                    zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 0); 
                    if(q1_cnt == 0) break;
                }
            }
            if (q1_cnt > 0) {
                for (int i=0; i<STOLIKI; i++) {
                    if (r->stoliki[i].ile_osob < r->stoliki[i].pojemnosc) {
                        int pusty = (r->stoliki[i].ile_osob == 0);
                        int blokada = 0;
                        if (pusty) {
                            if (r->stoliki[i].pojemnosc == 4 && q4_cnt > 0) blokada = 1;
                            if (r->stoliki[i].pojemnosc == 3 && q3_cnt > 0) blokada = 1;
                            if (r->stoliki[i].pojemnosc == 2 && q2_cnt > 0) blokada = 1;
                        }
                        if (!blokada) {
                            r->stoliki[i].ile_osob += 1;
                            zapros_klienta(msg, queue_g1, &q1_cnt, 0, i, 1);
                            if(q1_cnt == 0) break;
                        }
                    }
                }
            }
        }
        
        unlock(sem);

        system("clear");
        printf("\n=== MONITOR SALI I KOLEJEK ===\n");
        printf("Kolejki: G4:[%d] G3:[%d] G2:[%d] G1:[%d]\n", q4_cnt, q3_cnt, q2_cnt, q1_cnt);
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
                    printf("PID=%d%s G=%d (%d/%d)", k->pid, k->czeka_na_specjalne ? "*" : "", k->id_grupy, k->zjedzone, k->limit);
                    found = 1;
                }
            }
            printf("\n");
        }
        printf("---------------------------------------------------------------------------------\n");
        printf("[INFO]: %s\n", r->info);

        sleep(1); 
    }
    shmdt(r);
    return 0;
}