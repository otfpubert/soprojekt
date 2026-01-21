#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>

#include "wspolne.h"

typedef struct {
    pid_t pid;
    int group_priority; 
} wpis_kolejki;

#define Q_SIZE 100

wpis_kolejki queue_g1[Q_SIZE]; int q1_cnt = 0;
wpis_kolejki queue_g2[Q_SIZE]; int q2_cnt = 0;
wpis_kolejki queue_g3[Q_SIZE]; int q3_cnt = 0;
wpis_kolejki queue_g4[Q_SIZE]; int q4_cnt = 0;

void wstaw_do_kolejki(wpis_kolejki *q, int *cnt, pid_t pid, int priority) {
    if (*cnt >= Q_SIZE) return;

    int idx_wstawienia = *cnt; 

    if (priority) {
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

void zapros_klienta(int msg_id, wpis_kolejki *kolejka, int *cnt, int idx_w_kolejce, int nr_miejsca, int typ) {
    pid_t pid = kolejka[idx_w_kolejce].pid;
    
    struct komunikat zaproszenie;
    zaproszenie.mtype = pid; 
    zaproszenie.numer_miejsca = nr_miejsca;
    zaproszenie.typ_miejsca = typ;
    msgsnd(msg_id, &zaproszenie, sizeof(struct komunikat) - sizeof(long), 0);
    
    if (typ == 0) zrzut_do_logu("OBSLUGA: Przydzielono LADE %d dla PID %d", nr_miejsca, pid);
    else zrzut_do_logu("OBSLUGA: Przydzielono STOLIK %d dla PID %d", nr_miejsca, pid);

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
            dodaj_do_kolejki(buf.rozmiar_grupy, buf.group_priority, buf.pid_nadawcy);
        }

        lock(sem);
        if (q4_cnt > 0) {
            for (int i=6; i<10; i++) { 
                if (r->stoliki[i].ile_osob == 0) {
                    r->stoliki[i].ile_osob = 4; zapros_klienta(msg, queue_g4, &q4_cnt, 0, i, 1);
                    if(q4_cnt == 0) break;
                }
            }
        }
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
    shmdt(r);
    return 0;
}