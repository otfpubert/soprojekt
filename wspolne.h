#ifndef WSPOLNE_H
#define WSPOLNE_H

#define _XOPEN_SOURCE 700 // Wymagane dla usleep i sigaction

#include <sys/sem.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <unistd.h> 
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h> 
#include <signal.h>
#include <stdlib.h>

#define PRAWA 0600

/*
 * Funkcja pomocnicza do obsługi błędów systemowych.
 * Jeśli ret == -1, wypisuje komunikat błędu i kończy program.
 * Zgodne z wymaganiem 4.1.c - perror() i errno.
 */
static inline void check_error(int ret, const char *msg) {
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

/*
 * Wersja check_error która nie kończy programu, tylko loguje błąd.
 * Przydatna gdy chcemy kontynuować mimo błędu.
 */
static inline int check_error_warn(int ret, const char *msg) {
    if (ret == -1) {
        perror(msg);
        return -1;
    }
    return ret;
}
/* === STAŁE KONFIGURACYJNE === */
#define SEGMENTY 20
#define KOLORY 6
#define LADA_MIEJSC 9
#define STOLIKI 10
#define MAX_KLIENTOW 200
#define MAX_GRUP     100
#define MAX_ZAMOWIEN 50

/* Limity talerzyków na osobę (wymaganie: 3-10) */
#define MIN_TALERZYKI 3
#define MAX_TALERZYKI 10

/* Typy komunikatów w kolejce */
#define MSG_OBSLUGA    1      /* Zgłoszenie do obsługi */
#define MSG_KASA_REQ   1000   /* Żądanie rachunku (mtype = MSG_KASA_REQ + id_grupy) */
#define MSG_KASA_RESP  2000   /* Odpowiedź z rachunkiem (mtype = pid_klienta) */

/* Godziny otwarcia (w sekundach symulacji od startu) */
#define CZAS_OTWARCIA  0      /* Tp - start */
#define CZAS_ZAMKNIECIA 120   /* Tk - 120s = 2 minuty symulacji */

/* === STRUKTURY KOMUNIKATÓW === */

/*
 * Komunikat obsługi - zgłoszenie klienta i odpowiedź z miejscem.
 */
struct komunikat {
    long mtype;
    pid_t pid_nadawcy;
    int rozmiar_grupy;
    int numer_miejsca;
    int typ_miejsca;
    int group_priority;
};

/*
 * Komunikat kasy - żądanie rachunku od klienta.
 */
struct komunikat_kasa_req {
    long mtype;                  /* MSG_KASA_REQ + id_grupy */
    pid_t pid_lidera;            /* PID lidera grupy (płaci) */
    int id_grupy;
    int talerzyki[KOLORY];       /* Ile talerzyków każdego koloru zjadła grupa */
};

/*
 * Komunikat kasy - odpowiedź z rachunkiem.
 */
struct komunikat_kasa_resp {
    long mtype;                  /* PID lidera */
    int suma;                    /* Suma do zapłaty w zł */
    int talerzyki[KOLORY];       /* Podsumowanie talerzyków */
};

static const char *nazwy_kolorow[KOLORY] = {
    "niebieski", "czerwony", "zielony", 
    "BRAZOWY",   "SREBRNY",  "ZLOTY"
};

static const int ceny[KOLORY] = {10, 15, 20, 40, 50, 60};

struct talerzyk {
    int kolor;
    int cena;
    int ilosc_ryb;
    int id_odbiorcy;
};

struct segment_tasmy {
    int zajety;
    struct talerzyk t;
};

struct tasma {
    struct segment_tasmy seg[SEGMENTY];
};

/*
 * Informacje o kliencie w pamięci dzielonej.
 */
struct klient_info {
    pid_t pid;
    int id_grupy;
    int segment;
    int zjedzone;
    int limit;
    int aktywny;
    int czeka_na_specjalne;
    int is_vip;
    int is_child;
    int wiek;                    /* Wiek klienta (0 = nie ustawiony) */
    int talerzyki[KOLORY];       /* Zliczanie talerzyków per kolor (dla rachunku) */
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

struct zamowienie {
    pid_t pid_klienta;
    int typ_dania;
    int aktywne;
};

/*
 * Statystyki kasy (podsumowanie końcowe).
 */
struct kasa_stats {
    int transakcje;              /* Liczba obsłużonych grup */
    int suma_dzienna;            /* Całkowity utarg w zł */
    int sprzedane_talerzyki[KOLORY]; /* Sprzedane talerzyki per kolor */
};

/*
 * Główna struktura restauracji w pamięci dzielonej.
 */
struct restauracja {
    int otwarta;
    pid_t pid_kucharza;          /* PID kucharza (dla kierownika) */
    pid_t pid_kasy;              /* PID procesu kasy */

    /* Czas symulacji */
    time_t czas_startu;          /* Timestamp startu restauracji */
    int przyjmuje_klientow;      /* 1 = przyjmuje, 0 = zamknięte dla nowych */

    struct tasma tasma;
    struct miejsce_lada lada[LADA_MIEJSC];
    struct stolik stoliki[STOLIKI];
    struct klient_info klienci[MAX_KLIENTOW];

    int grupa_zjedzone_cnt[MAX_GRUP];
    int gdzie_siedzimy[MAX_GRUP];
    int typ_miejsca_grupy[MAX_GRUP];
    int grupa_talerzyki[MAX_GRUP][KOLORY]; /* Talerzyki per grupa per kolor */
    int grupa_zaplacila[MAX_GRUP];         /* Czy grupa już zapłaciła */

    struct zamowienie zamowienia[MAX_ZAMOWIEN];
    char info[200];

    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];

    struct kasa_stats kasa;      /* Statystyki kasy */
};

/*
 * Blokada semafora (mutex).
 * Używa semop() do dekrementacji semafora.
 */
static inline void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("lock: semop");
        }
    }
}

/*
 * Odblokowanie semafora (mutex).
 * Używa semop() do inkrementacji semafora.
 */
static inline void unlock(int sem) {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("unlock: semop");
        }
    }
}

static inline void zrzut_do_logu(const char *format, ...) {
    int fd = open("debug.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) return;
    
    char buffer[512];
    time_t now; time(&now);
    char *date = ctime(&now);
    if (date) date[strlen(date) - 1] = '\0';
    
    int len = sprintf(buffer, "[%s] ", date ? date : "UNKNOWN");
    
    va_list args;
    va_start(args, format);
    len += vsprintf(buffer + len, format, args);
    va_end(args);
    
    buffer[len] = '\n';
    write(fd, buffer, len + 1);
    close(fd);
}

#endif