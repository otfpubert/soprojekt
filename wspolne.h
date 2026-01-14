#ifndef WSPOLNE_H
#define WSPOLNE_H

#include <sys/sem.h>

#define PRAWA 0600

#define SEGMENTY 20
#define KOLORY 3

#define LADA_MIEJSC 9
#define STOLIKI 10
#define POJEMNOSC_STOLIKA 4

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

struct miejsce_lada {
    int zajete;
    int segment;
};

struct stolik {
    int zajete;
    int ile_osob;
    int segment;
};

struct restauracja {
    int otwarta;
    struct tasma tasma;

    struct miejsce_lada lada[LADA_MIEJSC];
    struct stolik stoliki[STOLIKI];

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