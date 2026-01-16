#ifndef WSPOLNE_H
#define WSPOLNE_H

#include <sys/sem.h>
#include <sys/types.h>

#define PRAWA 0600

#define SEGMENTY 20
#define KOLORY 3

#define LADA_MIEJSC 9
#define STOLIKI 10

#define MAX_KLIENTOW 200
#define MAX_GRUP     100

static const char *nazwy_kolorow[KOLORY] = {
    "niebieski",
    "czerwony",
    "zielony"
};

static const int ceny[KOLORY] = {10, 15, 20};

struct talerzyk {
    int kolor;
    int cena;
    int ilosc_ryb;
};

struct segment_tasmy {
    int zajety;
    struct talerzyk t;
};

struct tasma {
    struct segment_tasmy seg[SEGMENTY];
};

struct klient_info {
    pid_t pid;
    int id_grupy;
    int segment;      
    int zjedzone;
    int limit;
    int aktywny;
};

struct miejsce_lada {
    int zajete;
    int segment;
};

struct stolik {
    int ile_osob;
    int pojemnosc;
    int segment;
    int id_grupy;
};

struct restauracja {
    int otwarta;

    struct tasma tasma;
    struct miejsce_lada lada[LADA_MIEJSC];
    struct stolik stoliki[STOLIKI];
    struct klient_info klienci[MAX_KLIENTOW];

    int grupa_zjedzone_cnt[MAX_GRUP];
    int gdzie_siedzimy[MAX_GRUP];
    int typ_miejsca_grupy[MAX_GRUP];

    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];
};

static inline void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    semop(sem, &sb, 1);
}

static inline void unlock(int sem) {
    struct sembuf sb = {0, 1, 0};
    semop(sem, &sb, 1);
}

#endif