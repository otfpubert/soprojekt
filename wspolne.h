#ifndef WSPOLNE_H
#define WSPOLNE_H

#define _XOPEN_SOURCE 700 

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


static inline void check_error(int ret, const char *msg) {
    if (ret == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}


static inline int check_error_warn(int ret, const char *msg) {
    if (ret == -1) {
        perror(msg);
        return -1;
    }
    return ret;
}
#define SEGMENTY 20
#define KOLORY 6 
#define LADA_MIEJSC 9
#define STOLIKI 10
#define MAX_KLIENTOW 200 
#define MAX_GRUP     100
#define MAX_ZAMOWIEN 50

struct komunikat {
    long mtype;
    pid_t pid_nadawcy;
    int rozmiar_grupy;
    int numer_miejsca;
    int typ_miejsca;
    int group_priority;     
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

struct restauracja {
    int otwarta;
    pid_t pid_kucharza; 

    struct tasma tasma;
    struct miejsce_lada lada[LADA_MIEJSC];
    struct stolik stoliki[STOLIKI];
    struct klient_info klienci[MAX_KLIENTOW];

    int grupa_zjedzone_cnt[MAX_GRUP];
    int gdzie_siedzimy[MAX_GRUP];
    int typ_miejsca_grupy[MAX_GRUP];

    struct zamowienie zamowienia[MAX_ZAMOWIEN];
    char info[200]; 

    int wyprodukowane[KOLORY];
    int sprzedane[KOLORY];
};


static inline void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("lock: semop");
        }
    }
}


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