

**Autor:** Krzysztof Cichocki (155168)

### 1.1 Opis Tematu

Temat 1 - Restauracja „kaiten zushi”.
W restauracji znajduje się taśma, po której krążą talerzyki z jednym - dwoma kawałkami sushi. Kolor
talerzyka zależał od ceny, którą należy za niego zapłacić (np.: 10, 15, 20 zł). Przy taśmie znajduje się
odpowiednio X1, X2, X3, X4 ponumerowanych miejsc/stolików 1,2,3 lub 4-osobowych. Restauracja
może pomieścić maksymalnie N osób (N = X1 + 2*X2 + 3*X3 + 4*X4). Klienci przychodzą do lokalu
sami lub w grupach 2, 3 i 4 osobowych. Z uwagi na liczne przypadki kłótni przy stolikach o wasabi i
sosy (darmowe), różne grupy klientów nie mogą siadać przy tym samym stoliku, chyba że są
równoliczne. Klienci przychodzą do restauracji w losowych momentach czasu. Po wejściu do
restauracji klient udaje się do terminala wydającego numerki (podobnego do tych, które znamy np.:
w urzędzie miasta). Do wyboru mamy dwie opcje – stolik lub miejsce przy ladzie oraz jednocześnie
podajemy liczbę osób w grupie. Na miejsca przy ladzie czeka się zdecydowanie krócej, ale nie mają
większego sensu, jeśli w grupie jest więcej niż jedna - dwie osoby. Numer stolika/miejsca przy ladzie
przydziela obsługa w ramach dostępnych wolnych miejsc.
Zasady działania restauracji ustalone przez kierownika są następujące:
• Restauracja jest czynna w godzinach od Tp do Tk,. Wszystkie osoby, które otrzymały numer
stolika/miejsca przy ladzie muszą zostać obsłużone.
• Dzieci w wieku do 10 lat siadają przy stoliku pod opieką osoby dorosłej. Osoba dorosła może
opiekować się jednocześnie co najwyżej trzema dziećmi w wieku do 10 lat;
• Obsługa serwuje dania podstawowe (losowo) w cenie 10zł, 15zł, 20zł i umieszcza je na
taśmie;
• Taśma może pomieścić maksymalnie P talerzyków;
• Klienci pobierają losową ilość potraw podstawowych z taśmy (sumarycznie nie mniej niż 3 i
nie więcej niż 10 szt.) w momencie, gdy talerzyk znajdzie się przy danym stoliku/miejscu;
• Przy każdym stoliku znajduje się tablet, przy jego użyciu klient może złożyć specjalne
zamówienie z dostępnego menu (potrawy bardziej wyszukane, droższe (40, 50, 60zł). Jest
to rozwiązanie szczególnie przydatne, kiedy siedzimy “na końcu łańcucha pokarmowego” i
najlepsze kąski do nas nie dojeżdżają zabrane przez klientów siedzących bliżej kuchni .
Obsługa przygotowuje daną potrawę i umieszcza na taśmie (kolor talerzyka zależy od ceny)
z adnotacją do którego stolika/klienta ma trafić.
• Po zakończeniu konsumpcji zgłaszamy chęć zapłaty obsłudze. Obsługa wystawia rachunek
z którym idziemy do kasy (sumowane są talerzyki różnych rodzajów i mnożone przez koszt
jednego talerzyka). W kasie za posiłek płaci jedna osoba z danej grupy. Po zapłaceniu
rachunku grupa wychodzi z restauracji.
• Osoby uprawnione VIP (max. 2% klientów) wchodzą do restauracji bez kolejki (na terminalu
przycisk VIP) i zajmują miejsce tylko przy stolikach;
• Po zamknięciu restauracji, kucharz robi podsumowanie dla wytworzonych/umieszczonych na
taśmie produktów (rodzaj - liczba szt. – kwota, kwota sumaryczna, kasa robi podsumowanie
6
sprzedanych produktów, a obsługa tworzy takie samo podsumowanie dla produktów
pozostałych na taśmie.
Na polecenie kierownika (sygnał 1) obsługa przyśpiesza dwukrotnie wydawanie dań z kuchni.
Na polecenie kierownika (sygnał 2) obsługa zmniejsza o 50% ilość wydawanych dań z kuchni.
Na polecenie kierownika (sygnał 3) wszyscy klienci (ci którzy siedzą przy ladzie, stolikach i z
poczekalni) natychmiast opuszczają restaurację.
Napisz procedury Kierownik, Obsługa, Kucharz i Klient symulujące działanie restauracji. Raport z
przebiegu symulacji zapisać w pliku (plikach) tekstowym.

### 1.2 Pliki Źródłowe

| Plik | Opis |
|------|------|
| `wspolne.h` | Struktury danych, stałe, funkcje pomocnicze |
| `main.c` | Proces główny, inicjalizacja IPC, tworzenie procesów |
| `klient.c` | Proces klienta - jedzenie, zamówienia, płatność |
| `obsluga.c` | Manager sali + kasa + monitor w czasie rzeczywistym |
| `kucharz.c` | Produkcja dań podstawowych i specjalnych |
| `tasma.c` | Rotacja talerzyków na taśmie |
| `kierownik.c` | Panel kontrolny - sygnały |

---

## 2. Architektura Systemu i Działanie Procesów

### 2.1 Schemat Komunikacji Między Procesami

```
                            ┌─────────────────┐
                            │    KIEROWNIK    │
                            │  (osobny term.) │
                            └────────┬────────┘
                                     │ SIGUSR1/SIGUSR2/SIGTERM
                                     ▼
┌─────────────┐  fork/exec   ┌─────────────┐  fork/exec   ┌─────────────┐
│    MAIN     │─────────────▶│   KUCHARZ   │◀────────────▶│    TASMA    │
│  (zarządca) │              │ (producent) │   sygnały    │  (rotacja)  │
└──────┬──────┘              └──────┬──────┘              └──────┬──────┘
       │                            │                            │
       │ fork/exec                  │ SHM (talerzyki)            │ SHM
       ▼                            ▼                            ▼
┌─────────────┐  msg queue   ┌─────────────────────────────────────────┐
│   KLIENCI   │◀────────────▶│              PAMIĘĆ DZIELONA           │
│ (grupy 1-4) │              │  - struct restauracja                   │
└──────┬──────┘              │  - taśma, stoliki, lada, klienci        │
       │                     │  - zamówienia specjalne                 │
       │ msg queue           │  - statystyki kasy                      │
       ▼                     └─────────────────────────────────────────┘
┌─────────────┐                              ▲
│   OBSLUGA   │──────────────────────────────┘
│ (manager)   │              SHM + semafory
└─────────────┘
```

### 2.2 Opis Poszczególnych Procesów

#### MAIN (main.c)
**Rola:** Zarządca całego systemu - inicjalizuje zasoby i uruchamia pozostałe procesy.

**Działanie:**
1. Tworzy zasoby IPC (pamięć dzielona, semafory, kolejka komunikatów)
2. Inicjalizuje strukturę restauracji (stoliki, lada, taśma)
3. Uruchamia procesy: kucharz, obsluga, tasma (fork + exec)
4. W pętli tworzy grupy klientów co 1-3 sekundy (timer SIGALRM)
5. Po czasie zamknięcia (Tk) czeka aż wszyscy klienci wyjdą (sem_wait_zero)
6. Sprząta zasoby IPC przy zakończeniu lub SIGINT (Ctrl+C)

```c
// Główna pętla tworzenia klientów
while (g < LICZBA_GRUP && r->otwarta) {
    // Czekaj na timer (SIGALRM)
    while (!utworz_grupe_flag && r->otwarta) {
        pause();
    }

    // Twórz grupę klientów
    int rozmiar = 1 + rand() % 4;
    for (int i = 0; i < rozmiar; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("./klient", "klient", ...);
        }
    }
}
```

#### KLIENT (klient.c)
**Rola:** Symuluje pojedynczego klienta - siada, je, płaci, wychodzi.

**Działanie:**
1. Rejestruje się w pamięci dzielonej (zajmuje slot w tablicy klientów)
2. **Lider grupy** wysyła zgłoszenie do obsługi przez kolejkę komunikatów
3. Czeka na odpowiedź z przydzielonym miejscem (msgrcv blokujący)
4. **Followerzy** czekają aż lider ustawi `gdzie_siedzimy[gid]` w SHM
5. Siada przy przydzielonym miejscu i zaczyna jeść
6. System głodu: je tylko gdy `glod >= PROG_GLODU` (rośnie co sekundę)
7. Może złożyć zamówienie specjalne (talerzyk z `id_odbiorcy = PID`)
8. Po zjedzeniu limitu czeka na resztę grupy
9. **Lider** wysyła żądanie rachunku do kasy, płaci
10. Zwalnia miejsce i wychodzi

```c
// Lider wysyła zgłoszenie
struct komunikat zapytanie;
zapytanie.mtype = 1;  // Typ dla obsługi
zapytanie.pid_nadawcy = getpid();
zapytanie.rozmiar_grupy = moja_grupa_size;
msgsnd(msg, &zapytanie, ...);

// Czeka na odpowiedź
msgrcv(msg, &odpowiedz, ..., getpid(), 0);  // mtype = mój PID
```

#### OBSLUGA (obsluga.c)
**Rola:** Manager sali + kasa + wyświetlanie monitora.

**Działanie:**
1. W pętli odbiera zgłoszenia klientów (msgrcv z IPC_NOWAIT)
2. Dodaje grupy do lokalnych kolejek (queue_g1, queue_g2, queue_g3, queue_g4)
3. **VIP-y** są wstawiane na początek kolejki (przed zwykłymi klientami)
4. Wykonuje algorytm "Tetris" - przydziela miejsca według priorytetu:
   - G4 → stoliki 4-os. (indeksy 6-9)
   - G3 → stoliki 3-os. (3-5), overflow na 4-os.
   - G2 → lada (pary), stoliki 2-os. (0-2), dosiadanie na 4-os.
   - G1 → lada, potem stoliki z wolnymi miejscami
5. **Dosiadanie równoliczne:** tylko grupy tego samego rozmiaru mogą dzielić stolik
6. Wysyła odpowiedź z miejscem do klienta (msgsnd z mtype = PID klienta)
7. **KASA:** Odbiera żądania rachunków, oblicza sumę, wysyła odpowiedź
8. Co sekundę wyświetla monitor stanu sali (ANSI colors)
9. Na końcu generuje raport_dzienny.txt

```c
// Algorytm Tetris - fragment dla G2
if (q2_cnt > 0) {
    // Najpierw lada (pary)
    for (int i = 0; i < LADA_MIEJSC - 1; i++) {
        if (r->lada[i].zajete == 0 && r->lada[i+1].zajete == 0) {
            r->lada[i].zajete = 1;
            r->lada[i+1].zajete = 1;
            zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 0);
        }
    }
    // Potem stoliki 2-os.
    // Potem dosiadanie na 4-os. (tylko do innych G2!)
}
```

#### KUCHARZ (kucharz.c)
**Rola:** Produkuje talerzyki i umieszcza na taśmie.

**Działanie:**
1. Rejestruje swój PID w pamięci dzielonej (dla kierownika)
2. Co `production_delay` ticków (1-4 sekundy) produkuje talerzyk
3. Losuje kolor (0-2 podstawowe, 3-5 specjalne tylko na zamówienie)
4. Sprawdza zamówienia specjalne - jeśli jest, produkuje dla konkretnego klienta
5. Umieszcza talerzyk na segmencie 0 taśmy (jeśli wolny)
6. **SIGUSR1:** przyspieszenie 2x (`production_delay /= 2`)
7. **SIGUSR2:** spowolnienie 50% (`production_delay *= 2`)
8. Na końcu generuje raport_kucharz.txt

```c
// Produkcja talerzyka
if (r->tasma.seg[0].zajety == 0) {
    struct talerzyk t;
    t.kolor = rand() % 3;  // Podstawowe: 0, 1, 2
    t.cena = ceny[t.kolor];
    t.id_odbiorcy = -1;    // Ogólny (nie zamówienie)

    r->tasma.seg[0].zajety = 1;
    r->tasma.seg[0].t = t;
    r->wyprodukowane[t.kolor]++;
}
```

#### TASMA (tasma.c)
**Rola:** Przesuwa talerzyki po taśmie w cyklu.

**Działanie:**
1. Co `current_delay` sekund przesuwa wszystkie talerzyki o 1 segment
2. Segment 19 → segment 0 (cyklicznie, jeśli 0 wolny)
3. **SIGUSR1/SIGUSR2:** analogicznie jak kucharz (synchronizacja prędkości)

```c
// Przesuwanie talerzyków
struct segment_tasmy temp = r->tasma.seg[SEGMENTY - 1];
for (int i = SEGMENTY - 1; i > 0; i--) {
    r->tasma.seg[i] = r->tasma.seg[i - 1];
}
if (temp.zajety && !r->tasma.seg[0].zajety) {
    r->tasma.seg[0] = temp;  // Cykliczne zawijanie
} else {
    r->tasma.seg[0].zajety = 0;
}
```

#### KIEROWNIK (kierownik.c)
**Rola:** Panel kontrolny uruchamiany w osobnym terminalu.

**Działanie:**
1. Łączy się z pamięcią dzieloną (odczytuje PID kucharza i taśmy)
2. Wyświetla menu z opcjami
3. **Opcja 1:** Wysyła SIGUSR1 do kucharza i taśmy (przyspieszenie)
4. **Opcja 2:** Wysyła SIGUSR2 do kucharza i taśmy (spowolnienie)
5. **Opcja 3:** Ewakuacja - wysyła SIGTERM do wszystkich klientów, zamyka restaurację

```c
// Ewakuacja
for (int i = 0; i < MAX_KLIENTOW; i++) {
    if (r->klienci[i].aktywny && r->klienci[i].pid > 0) {
        kill(r->klienci[i].pid, SIGTERM);
    }
}
r->przyjmuje_klientow = 0;
r->otwarta = 0;
```

### 2.3 Obsługa Kolejki Komunikatów

Kolejka komunikatów służy do komunikacji między klientami a obsługą:

| Typ komunikatu | mtype | Kierunek | Opis |
|----------------|-------|----------|------|
| Zgłoszenie | 1 | Klient → Obsługa | Nowa grupa prosi o miejsce |
| Odpowiedź | PID klienta | Obsługa → Klient | Przydzielone miejsce |
| Żądanie rachunku | 1000 + id_grupy | Klient → Kasa | Lider prosi o rachunek |
| Rachunek | PID lidera | Kasa → Klient | Suma do zapłaty |

**Dlaczego mtype = PID?**
Pozwala klientowi odebrać tylko swoją odpowiedź:
```c
msgrcv(msg, &odpowiedz, ..., getpid(), 0);  // Odbierz tylko dla mojego PID
```

### 2.4 Synchronizacja - Semafory

| Semafor | Indeks | Wartość początkowa | Zastosowanie |
|---------|--------|-------------------|--------------|
| SEM_MUTEX | 0 | 1 | Mutex do sekcji krytycznych (lock/unlock) |
| SEM_ACTIVE | 1 | 0 | Licznik aktywnych klientów |

**SEM_ACTIVE:**
- Klient wchodzi: `sem_inc(sem, SEM_ACTIVE)`
- Klient wychodzi: `sem_dec(sem, SEM_ACTIVE)`
- Main czeka na wszystkich: `sem_wait_zero(sem, SEM_ACTIVE)`

### 2.5 System Głodu Klienta

Klient nie je ciągle - musi "zgłodnieć":

```
PROG_GLODU = 15

┌─────────────────────────────────────────────────────────────┐
│  głód = 15 (start)                                          │
│     │                                                       │
│     ▼ [talerzyk dostępny && głód >= 15]                    │
│  ZJADA → głód = 0                                          │
│     │                                                       │
│     ▼ [co 1 sekundę]                                       │
│  głód++ (max 30)                                           │
│     │                                                       │
│     ▼ [głód >= 15]                                         │
│  Może znowu jeść                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Wymagane Konstrukcje Systemowe - Snippety Kodu

### 2.1 Tworzenie i obsługa plików

#### [open() - wspolne.h:214](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L214)

Funkcja `open()` otwiera plik do zapisu. Używamy flag:
- `O_WRONLY` - tylko do zapisu
- `O_CREAT` - utwórz plik jeśli nie istnieje
- `O_APPEND` - dopisuj na końcu pliku (nie nadpisuj)

Trzeci argument `0644` to prawa dostępu (rw-r--r--).

```c
int fd = open("debug.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
if (fd == -1) return;
```

#### [write() - wspolne.h:230](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L230)

Funkcja `write()` zapisuje dane do pliku. Przyjmuje:
- `fd` - deskryptor pliku (zwrócony przez open)
- `buffer` - wskaźnik na dane do zapisania
- `len + 1` - liczba bajtów do zapisania

Zwraca liczbę zapisanych bajtów lub -1 przy błędzie.

```c
buffer[len] = '\n';
write(fd, buffer, len + 1);
```

#### [close() - wspolne.h:231](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L231)

Funkcja `close()` zamyka deskryptor pliku i zwalnia zasoby systemowe.
Każdy otwarty plik powinien być zamknięty po zakończeniu pracy.

```c
close(fd);
```

#### [fopen() - obsluga.c:72](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L72)

Funkcja `fopen()` otwiera plik w trybie wysokopoziomowym (biblioteka stdio).
Tryb `"w"` oznacza zapis z nadpisaniem (jeśli plik istnieje, zostanie wyczyszczony).
Zwraca wskaźnik FILE* lub NULL przy błędzie.

```c
FILE *f = fopen("raport_dzienny.txt", "w");
if (!f) {
    perror("[OBSLUGA] fopen raport_dzienny.txt");
    return;
}
```

#### [fprintf() - obsluga.c:78-80](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L78-L80)

Funkcja `fprintf()` zapisuje sformatowany tekst do pliku.
Działa jak `printf()`, ale pierwszy argument to wskaźnik na plik.
Pozwala na formatowanie z użyciem specyfikatorów jak %d, %s, %f.

```c
fprintf(f, "========================================\n");
fprintf(f, "    RAPORT DZIENNY - KAITEN ZUSHI\n");
fprintf(f, "========================================\n\n");
```

#### [fclose() - obsluga.c:202](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L202)

Funkcja `fclose()` zamyka plik otwarty przez fopen().
Automatycznie opróżnia bufor (flush) przed zamknięciem.

```c
fclose(f);
```

---

### 2.2 Tworzenie procesów

#### [fork() - main.c:203](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L203)

Funkcja `fork()` tworzy nowy proces (potomny) będący kopią procesu macierzystego.
Zwraca:
- W procesie macierzystym: PID potomka (>0)
- W procesie potomnym: 0
- Przy błędzie: -1

Po fork() oba procesy wykonują ten sam kod, ale mają osobne przestrzenie pamięci.

```c
pid_t pid_kucharz = fork();
if (pid_kucharz == -1) {
    perror("[MAIN] fork(kucharz)");
    sprzatanie(0);
}
```

#### [fork() - main.c:338](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L338) (tworzenie klientów w pętli)

Tutaj fork() jest wywoływany w pętli, tworząc wiele procesów klientów.
Każdy klient to osobny proces z własnym PID.
`continue` przy błędzie pozwala kontynuować tworzenie pozostałych klientów.

```c
pid_t pid_klient = fork();
if (pid_klient == -1) {
    perror("[MAIN] fork(klient)");
    continue;
}
```

#### [execl() - main.c:208](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L208)

Funkcja `execl()` zastępuje bieżący proces nowym programem.
Argumenty:
- Ścieżka do programu ("./kucharz")
- argv[0] - nazwa programu ("kucharz")
- NULL - znacznik końca listy argumentów

Po udanym execl() kod poniżej NIE zostanie wykonany - proces został zastąpiony.
Jeśli execl() zwróci, oznacza to błąd.

```c
} else if (pid_kucharz == 0) {
    execl("./kucharz", "kucharz", NULL);
    perror("[MAIN] execl(kucharz) - nie mozna uruchomic");
    exit(EXIT_FAILURE);
}
```

#### [execl() - main.c:354](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L354) (z argumentami)

Ta wersja execl() przekazuje argumenty do procesu klienta:
- gid - ID grupy
- gsize - rozmiar grupy
- my_vip - czy klient jest VIP
- grp_prio - czy grupa ma priorytet
- my_child - czy klient jest dzieckiem

Argumenty są przekazywane jako stringi (sprintf konwertuje int na string).

```c
} else if (pid_klient == 0) {
    char gid[10], gsize[10];
    char my_vip[5], my_child[5], grp_prio[5];

    sprintf(gid, "%d", g);
    sprintf(gsize, "%d", rozmiar);
    sprintf(my_vip, "%d", member_vip[i]);
    sprintf(grp_prio, "%d", group_has_priority);
    sprintf(my_child, "%d", member_child[i]);

    execl("./klient", "klient", gid, gsize, my_vip, grp_prio, my_child, NULL);
    perror("[MAIN] execl(klient) - nie mozna uruchomic");
    exit(EXIT_FAILURE);
}
```

#### [exit() - main.c:210](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L210)

Funkcja `exit()` kończy proces z podanym kodem wyjścia.
`EXIT_FAILURE` (wartość 1) oznacza błąd.
`EXIT_SUCCESS` lub 0 oznacza poprawne zakończenie.

exit() automatycznie zamyka otwarte pliki i zwalnia pamięć.

```c
perror("[MAIN] execl(kucharz) - nie mozna uruchomic");
exit(EXIT_FAILURE);
```

#### [exit() - main.c:83](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L83) (w handlerze sprzątania)

Tutaj exit(0) kończy program po sprzątaniu zasobów IPC.
Kod 0 oznacza sukces - sprzątanie zakończyło się poprawnie.

```c
printf("[MAIN] Sprzatanie zakonczone. Koniec programu.\n");
exit(0);
```

#### [wait() - main.c:400](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L400)

Funkcja `wait()` blokuje proces macierzysty do zakończenia dowolnego potomka.
Zwraca PID zakończonego potomka lub -1 gdy nie ma więcej potomków.

Makra do analizy statusu:
- `WIFEXITED(status)` - czy proces zakończył się normalnie (przez exit)
- `WEXITSTATUS(status)` - kod wyjścia
- `WIFSIGNALED(status)` - czy proces został zabity sygnałem
- `WTERMSIG(status)` - numer sygnału który zabił proces

```c
int status;
pid_t child_pid;
while ((child_pid = wait(&status)) > 0) {
    if (WIFEXITED(status)) {
        zrzut_do_logu("[MAIN] Proces %d zakonczyl sie z kodem %d",
                      child_pid, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        zrzut_do_logu("[MAIN] Proces %d zabity sygnalem %d",
                      child_pid, WTERMSIG(status));
    }
}
```

---

### 2.3 Obsługa sygnałów

#### [signal() - main.c:93](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L93) (SIGINT)

Funkcja `signal()` rejestruje handler dla danego sygnału.
SIGINT jest wysyłany gdy użytkownik naciśnie Ctrl+C.
`sprzatanie` to nazwa funkcji handlera, która zostanie wywołana.

Zwraca poprzedni handler lub SIG_ERR przy błędzie.

```c
if (signal(SIGINT, sprzatanie) == SIG_ERR) {
    perror("[MAIN] signal(SIGINT)");
    exit(EXIT_FAILURE);
}
```

#### [signal() - kucharz.c:69](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L69) (SIGALRM)

SIGALRM jest wysyłany przez timer systemowy (setitimer).
Handler `handler_alarm` ustawia flagę, która jest sprawdzana w pętli głównej.
Dzięki temu kucharz produkuje dania w regularnych odstępach czasu.

```c
if (signal(SIGALRM, handler_alarm) == SIG_ERR) {
    perror("[KUCHARZ] signal(SIGALRM)");
}
```

#### [signal() - kucharz.c:72](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L72) (SIGUSR1)

SIGUSR1 to sygnał użytkownika nr 1 - używamy go do przyspieszenia produkcji.
Kierownik wysyła ten sygnał gdy chce żeby kucharz pracował szybciej.
Handler zmniejsza opóźnienie między produkcją dań.

```c
if (signal(SIGUSR1, handler_szybciej) == SIG_ERR) {
    perror("[KUCHARZ] signal(SIGUSR1)");
    exit(EXIT_FAILURE);
}
```

#### [signal() - kucharz.c:76](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L76) (SIGUSR2)

SIGUSR2 to sygnał użytkownika nr 2 - używamy go do spowolnienia produkcji.
Kierownik wysyła ten sygnał gdy chce żeby kucharz pracował wolniej.
Handler zwiększa opóźnienie między produkcją dań.

```c
if (signal(SIGUSR2, handler_wolniej) == SIG_ERR) {
    perror("[KUCHARZ] signal(SIGUSR2)");
    exit(EXIT_FAILURE);
}
```

#### [signal() - klient.c:63](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L63) (SIGTERM)

SIGTERM to sygnał zakończenia - używamy go do ewakuacji klientów.
Handler zwalnia zajmowane miejsce i kończy proces klienta.
Jest to "grzeczne" zakończenie - klient sprząta po sobie przed wyjściem.

```c
if (signal(SIGTERM, handler_ewakuacja) == SIG_ERR) {
    perror("[KLIENT] signal(SIGTERM)");
    exit(EXIT_FAILURE);
}
```

#### [kill() - main.c:51](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L51) (wysłanie do grupy procesów)

Funkcja `kill()` wysyła sygnał do procesu lub grupy procesów.
Gdy PID = 0, sygnał jest wysyłany do wszystkich procesów w tej samej grupie.
SIGTERM prosi procesy o zakończenie - pozwala im posprzątać przed wyjściem.

ESRCH oznacza że proces nie istnieje - ignorujemy ten błąd.

```c
if (kill(0, SIGTERM) == -1 && errno != ESRCH) {
    perror("[MAIN] kill(0, SIGTERM)");
}
```

#### [kill() - kierownik.c:117](https://github.com/otfpubert/SOPROJEKT/blob/main/kierownik.c#L117) (SIGUSR1 do kucharza)

Kierownik wysyła SIGUSR1 do konkretnego procesu kucharza.
Najpierw sprawdzamy czy PID jest poprawny (>0).
ESRCH oznacza że proces nie istnieje - być może kucharz już się zakończył.

```c
if (kill(kucharz, SIGUSR1) == -1) {
    if (errno == ESRCH) {
        printf("[BLAD] Kucharz (PID=%d) nie istnieje!\n", kucharz);
    } else {
        perror("[KIEROWNIK] kill(SIGUSR1) kucharz");
    }
} else {
    printf("-> Wyslano SIGUSR1 do Kucharza (PID=%d)\n", kucharz);
}
```

#### [kill() - kierownik.c:128](https://github.com/otfpubert/SOPROJEKT/blob/main/kierownik.c#L128) (SIGUSR1 do taśmy)

Wysyłamy ten sam sygnał do procesu taśmy, żeby też przyspieszył.
Sprawdzamy czy PID taśmy jest poprawny przed wysłaniem.
Dzięki temu kucharz i taśma są zsynchronizowane.

```c
if (tasma > 0 && kill(tasma, SIGUSR1) == 0) {
    printf("-> Wyslano SIGUSR1 do Tasmy (PID=%d)\n", tasma);
}
```

#### [kill() - kierownik.c:141](https://github.com/otfpubert/SOPROJEKT/blob/main/kierownik.c#L141) (SIGUSR2 do kucharza)

Analogicznie jak SIGUSR1, ale SIGUSR2 spowalnia produkcję.
Kierownik używa tej opcji gdy na taśmie jest za dużo jedzenia.

```c
if (kill(kucharz, SIGUSR2) == -1) {
    if (errno == ESRCH) {
        printf("[BLAD] Kucharz (PID=%d) nie istnieje!\n", kucharz);
    } else {
        perror("[KIEROWNIK] kill(SIGUSR2) kucharz");
    }
}
```

#### [kill() - kierownik.c:166](https://github.com/otfpubert/SOPROJEKT/blob/main/kierownik.c#L166) (SIGTERM - ewakuacja)

Pętla iteruje po wszystkich klientach w pamięci dzielonej.
Dla każdego aktywnego klienta wysyłamy SIGTERM.
Liczymy ile klientów udało się ewakuować.

```c
for (int i = 0; i < MAX_KLIENTOW; i++) {
    if (r->klienci[i].aktywny && r->klienci[i].pid > 0) {
        if (kill(r->klienci[i].pid, SIGTERM) == 0) {
            ewakuowani++;
        }
    }
}
```

---

### 2.4 Synchronizacja procesów - semafory

#### [ftok() - main.c:99](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L99)

Funkcja `ftok()` generuje unikalny klucz IPC na podstawie:
- Ścieżki do istniejącego pliku ("ipc_keyfile")
- Identyfikatora projektu ('K')

Ten sam klucz pozwala różnym procesom połączyć się z tymi samymi zasobami IPC.
Plik musi istnieć - dlatego przed uruchomieniem tworzymy `touch ipc_keyfile`.

```c
key_t key = ftok("ipc_keyfile", 'K');
if (key == -1) {
    perror("[MAIN] ftok - upewnij sie ze plik ipc_keyfile istnieje");
    exit(EXIT_FAILURE);
}
```

#### [semget() - main.c:116](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L116)

Funkcja `semget()` tworzy nowy zestaw semaforów lub łączy się z istniejącym.
Argumenty:
- `key` - klucz IPC (z ftok)
- `NUM_SEMS` - liczba semaforów w zestawie (2: mutex + licznik)
- `IPC_CREAT | PRAWA` - utwórz jeśli nie istnieje, z prawami 0600

Zwraca identyfikator zestawu semaforów.

```c
g_sem_id = semget(key, NUM_SEMS, IPC_CREAT | PRAWA);
if (g_sem_id == -1) {
    perror("[MAIN] semget - nie mozna utworzyc semafora");
    shmctl(g_shm_id, IPC_RMID, NULL);
    exit(EXIT_FAILURE);
}
```

#### [semget() - klient.c:129](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L129) (połączenie z istniejącym)

Proces klienta łączy się z istniejącym semaforem (stworzonym przez main).
Flaga 0 oznacza "tylko połącz, nie twórz".
Jeśli semafor nie istnieje, funkcja zwróci błąd.

```c
int sem = semget(key, NUM_SEMS, 0);
if (sem == -1) {
    perror("[KLIENT] semget");
    exit(EXIT_FAILURE);
}
```

#### [semctl() - main.c:135](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L135) (inicjalizacja mutex)

Funkcja `semctl()` wykonuje operacje kontrolne na semaforze.
`SETVAL` ustawia wartość semafora.
Semafor SEM_MUTEX = 1 oznacza "odblokowany" (jeden proces może wejść).

```c
if (semctl(g_sem_id, SEM_MUTEX, SETVAL, 1) == -1) {
    perror("[MAIN] semctl SETVAL SEM_MUTEX");
    sprzatanie(0);
}
```

#### [semctl() - main.c:141](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L141) (inicjalizacja licznika)

SEM_ACTIVE to semafor licznikowy - zlicza aktywnych klientów.
Wartość 0 na starcie oznacza "brak klientów".
Każdy klient przy wejściu zwiększa, przy wyjściu zmniejsza wartość.

```c
if (semctl(g_sem_id, SEM_ACTIVE, SETVAL, 0) == -1) {
    perror("[MAIN] semctl SETVAL SEM_ACTIVE");
    sprzatanie(0);
}
```

#### [semctl() - main.c:66](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L66) (usuwanie)

`IPC_RMID` usuwa zestaw semaforów z systemu.
Po usunięciu wszystkie procesy czekające na semaforze dostaną błąd EIDRM.
Zawsze usuwamy zasoby IPC po zakończeniu programu.

```c
if (semctl(g_sem_id, 0, IPC_RMID) == -1) {
    perror("[MAIN] semctl IPC_RMID");
} else {
    printf("[MAIN] Semafor (id=%d) usuniety.\n", g_sem_id);
}
```

#### [semop() - wspolne.h:192](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L192) (lock - blokada)

Funkcja `semop()` wykonuje atomową operację na semaforze.
Struktura sembuf:
- Indeks semafora (0 = mutex)
- Operacja (-1 = dekrementuj, czyli "weź blokadę")
- Flagi (0 = blokuj jeśli niedostępny)

Jeśli wartość semafora = 0, proces czeka aż inny proces zrobi unlock.
EINTR oznacza przerwanie przez sygnał - ignorujemy.
EIDRM oznacza usunięcie semafora - ignorujemy (restauracja zamykana).

```c
static inline void lock(int sem) {
    struct sembuf sb = {0, -1, 0};
    if (semop(sem, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("lock: semop");
        }
    }
}
```

#### [semop() - wspolne.h:205](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L205) (unlock - odblokowanie)

Operacja +1 zwiększa wartość semafora, pozwalając innemu procesowi wejść.
Para lock/unlock tworzy sekcję krytyczną - tylko jeden proces może być w środku.

```c
static inline void unlock(int sem) {
    struct sembuf sb = {0, 1, 0};
    if (semop(sem, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("unlock: semop");
        }
    }
}
```

#### [semop() - wspolne.h:243](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L243) (inkrementacja licznika)

Zwiększa wartość semafora SEM_ACTIVE o 1.
Wywoływane gdy nowy klient wchodzi do restauracji.
Używamy indeksu `idx` zamiast 0, bo to semafor licznikowy, nie mutex.

```c
static inline void sem_inc(int sem_id, int idx) {
    struct sembuf sb = {idx, 1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("sem_inc: semop");
        }
    }
}
```

#### [semop() - wspolne.h:254](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L254) (dekrementacja licznika)

Zmniejsza wartość semafora SEM_ACTIVE o 1.
Wywoływane gdy klient wychodzi z restauracji.

```c
static inline void sem_dec(int sem_id, int idx) {
    struct sembuf sb = {idx, -1, 0};
    if (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR && errno != EIDRM) {
            perror("sem_dec: semop");
        }
    }
}
```

#### [semop() - wspolne.h:267](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L267) (czekanie na zero)

Operacja 0 blokuje proces dopóki wartość semafora nie spadnie do 0.
Używane przez main do czekania aż wszyscy klienci wyjdą.
Pętla while obsługuje przerwania przez sygnały (EINTR).

```c
static inline void sem_wait_zero(int sem_id, int idx) {
    struct sembuf sb = {idx, 0, 0};
    while (semop(sem_id, &sb, 1) == -1) {
        if (errno != EINTR) break;
    }
}
```

---

### 2.5 Segmenty pamięci dzielonej

#### [shmget() - main.c:107](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L107) (tworzenie)

Funkcja `shmget()` tworzy nowy segment pamięci dzielonej.
Argumenty:
- `key` - klucz IPC (z ftok)
- `sizeof(struct restauracja)` - rozmiar segmentu w bajtach
- `IPC_CREAT | PRAWA` - utwórz z prawami 0600

Zwraca identyfikator segmentu. Pamięć dzielona pozwala wielu procesom
widzieć te same dane bez kopiowania.

```c
g_shm_id = shmget(key, sizeof(struct restauracja), IPC_CREAT | PRAWA);
if (g_shm_id == -1) {
    perror("[MAIN] shmget - nie mozna utworzyc pamieci dzielonej");
    exit(EXIT_FAILURE);
}
```

#### [shmget() - klient.c:123](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L123) (połączenie z istniejącą)

Klient łączy się z pamięcią stworzoną przez main.
Flaga 0 oznacza "tylko połącz".
Wszyscy klienci widzą tę samą strukturę restauracji.

```c
int shm = shmget(key, sizeof(struct restauracja), 0);
if (shm == -1) {
    perror("[KLIENT] shmget");
    exit(EXIT_FAILURE);
}
```

#### [shmat() - main.c:147](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L147)

Funkcja `shmat()` dołącza segment pamięci do przestrzeni adresowej procesu.
Argumenty:
- `g_shm_id` - identyfikator segmentu
- `NULL` - system wybiera adres
- `0` - brak flag (odczyt i zapis)

Zwraca wskaźnik do pamięci lub (void*)-1 przy błędzie.
Od tego momentu możemy używać `r->` jak zwykłej struktury.

```c
struct restauracja *r = shmat(g_shm_id, NULL, 0);
if (r == (void *)-1) {
    perror("[MAIN] shmat - nie mozna dolaczyc pamieci dzielonej");
    sprzatanie(0);
}
```

#### [shmat() - klient.c:142](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L142)

Każdy proces klienta też dołącza pamięć do swojej przestrzeni.
Wskaźnik `r` w każdym procesie wskazuje na tę samą fizyczną pamięć.
Zmiany dokonane przez jeden proces są widoczne dla wszystkich.

```c
struct restauracja *r = shmat(shm, NULL, 0);
if (r == (void *)-1) {
    perror("[KLIENT] shmat");
    exit(EXIT_FAILURE);
}
```

#### [shmdt() - klient.c:461](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L461)

Funkcja `shmdt()` odłącza segment od przestrzeni adresowej procesu.
Nie usuwa segmentu - tylko odłącza ten proces.
Powinno być wywołane przed zakończeniem procesu.

```c
if (shmdt(r) == -1) {
    perror("[KLIENT] shmdt");
}
```

#### [shmdt() - obsluga.c:659](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L659)

Analogicznie proces obsługi odłącza się przed zakończeniem.

```c
if (shmdt(r) == -1) {
    perror("[OBSLUGA] shmdt");
}
```

#### [shmctl() - main.c:57](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L57) (usuwanie)

Funkcja `shmctl()` z flagą `IPC_RMID` oznacza segment do usunięcia.
Segment zostanie faktycznie usunięty gdy ostatni proces się odłączy.
Zawsze usuwamy zasoby IPC po zakończeniu programu.

```c
if (shmctl(g_shm_id, IPC_RMID, NULL) == -1) {
    perror("[MAIN] shmctl IPC_RMID");
} else {
    printf("[MAIN] Pamiec dzielona (id=%d) usunieta.\n", g_shm_id);
}
```

---

### 2.6 Kolejki komunikatów

#### [msgget() - main.c:125](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L125) (tworzenie)

Funkcja `msgget()` tworzy nową kolejkę komunikatów.
Kolejka działa jak bufor FIFO - procesy mogą wysyłać i odbierać wiadomości.
Każda wiadomość ma typ (mtype) pozwalający na selektywne odbieranie.

```c
g_msg_id = msgget(key, IPC_CREAT | PRAWA);
if (g_msg_id == -1) {
    perror("[MAIN] msgget - nie mozna utworzyc kolejki komunikatow");
    shmctl(g_shm_id, IPC_RMID, NULL);
    semctl(g_sem_id, 0, IPC_RMID);
    exit(EXIT_FAILURE);
}
```

#### [msgget() - klient.c:135](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L135) (połączenie z istniejącą)

Klient łączy się z kolejką stworzoną przez main.
Używa tej samej kolejki do:
- Wysyłania zgłoszeń do obsługi
- Odbierania przydzielonych miejsc
- Wysyłania żądań rachunku do kasy

```c
int msg = msgget(key, 0);
if (msg == -1) {
    perror("[KLIENT] msgget");
    exit(EXIT_FAILURE);
}
```

#### [msgsnd() - klient.c:210](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L210) (wysłanie zgłoszenia)

Funkcja `msgsnd()` wysyła komunikat do kolejki.
Struktura komunikatu:
- `mtype = 1` - typ komunikatu (obsługa odbiera wiadomości typu 1)
- `pid_nadawcy` - PID klienta (do odpowiedzi)
- `rozmiar_grupy` - ile osób w grupie
- `group_priority` - czy grupa ma VIP

Rozmiar to wielkość struktury BEZ pola mtype (sizeof - sizeof(long)).
Pętla while obsługuje przerwania przez sygnały (EINTR).

```c
struct komunikat zapytanie;
zapytanie.mtype = 1;
zapytanie.pid_nadawcy = getpid();
zapytanie.rozmiar_grupy = moja_grupa_size;
zapytanie.group_priority = moja_grupa_ma_priorytet;

while (msgsnd(msg, &zapytanie, sizeof(struct komunikat) - sizeof(long), 0) == -1) {
    if (errno == EINTR) continue;
    if (errno == EIDRM) {
        shmdt(r);
        exit(0);
    }
    perror("[KLIENT] msgsnd");
    exit(EXIT_FAILURE);
}
```

#### [msgsnd() - obsluga.c:269](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L269) (wysłanie zaproszenia)

Obsługa wysyła odpowiedź z przydzielonym miejscem.
`mtype = pid` - typ to PID klienta, więc tylko ten klient odbierze tę wiadomość.
Zawiera numer miejsca i typ (0 = lada, 1 = stolik).

```c
struct komunikat zaproszenie;
zaproszenie.mtype = pid;
zaproszenie.numer_miejsca = nr_miejsca;
zaproszenie.typ_miejsca = typ;

if (msgsnd(msg_id, &zaproszenie, sizeof(struct komunikat) - sizeof(long), 0) == -1) {
    if (errno == EIDRM) return;
    perror("[OBSLUGA] msgsnd - blad wysylania zaproszenia");
    return;
}
```

#### [msgsnd() - klient.c:397](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L397) (żądanie rachunku)

Lider grupy wysyła żądanie rachunku do kasy.
`mtype = MSG_KASA_REQ + moj_gid` - unikalny typ dla żądań rachunku.
Zawiera informacje o zjedzonych talerzykach per kolor.

```c
struct komunikat_kasa_req req;
req.mtype = MSG_KASA_REQ + moj_gid;
req.pid_lidera = getpid();
req.id_grupy = moj_gid;

while (msgsnd(msg, &req, sizeof(struct komunikat_kasa_req) - sizeof(long), 0) == -1) {
    if (errno == EINTR) continue;
    if (errno == EIDRM) break;
    perror("[KLIENT] msgsnd - blad wysylania zadania rachunku");
    break;
}
```

#### [msgsnd() - obsluga.c:405](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L405) (odpowiedź z rachunkiem)

Kasa wysyła odpowiedź z obliczoną sumą do zapłaty.
`mtype = pid_lidera` - tylko lider grupy odbierze rachunek.

```c
struct komunikat_kasa_resp kasa_resp;
kasa_resp.mtype = pid_lidera;
kasa_resp.suma = suma;

if (msgsnd(msg, &kasa_resp, sizeof(kasa_resp) - sizeof(long), 0) == -1) {
    if (errno != EIDRM) {
        perror("[OBSLUGA/KASA] msgsnd odpowiedz");
    }
}
```

#### [msgrcv() - klient.c:225](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L225) (oczekiwanie na miejsce)

Funkcja `msgrcv()` odbiera komunikat z kolejki.
Argumenty:
- `msg` - identyfikator kolejki
- `&odpowiedz` - bufor na komunikat
- rozmiar danych (bez mtype)
- `getpid()` - odbierz tylko komunikat z mtype = mój PID
- `0` - blokuj aż przyjdzie wiadomość

Klient czeka na odpowiedź od obsługi z przydzielonym miejscem.

```c
struct komunikat odpowiedz;
while (1) {
    if (msgrcv(msg, &odpowiedz, sizeof(struct komunikat) - sizeof(long), getpid(), 0) == -1) {
        if (errno == EINTR) continue;
        if (errno == EIDRM) {
            shmdt(r);
            exit(0);
        }
        perror("[KLIENT] msgrcv");
        exit(EXIT_FAILURE);
    }
    break;
}
```

#### [msgrcv() - obsluga.c:360](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L360) (odbieranie zgłoszeń - non-blocking)

Flaga `IPC_NOWAIT` sprawia że funkcja nie blokuje - zwraca od razu.
Jeśli nie ma wiadomości, zwraca -1 z errno = ENOMSG.
Typ 1 to zgłoszenia nowych klientów.
Pętla odbiera wszystkie oczekujące zgłoszenia.

```c
struct komunikat buf;
ssize_t recv_result;
while ((recv_result = msgrcv(msg, &buf, sizeof(struct komunikat) - sizeof(long),
                              1, IPC_NOWAIT)) != -1) {
    if (buf.rozmiar_grupy >= 1 && buf.rozmiar_grupy <= 4 && buf.pid_nadawcy > 0) {
        dodaj_do_kolejki(buf.rozmiar_grupy, buf.group_priority, buf.pid_nadawcy);
    }
}
```

#### [msgrcv() - obsluga.c:380](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L380) (odbieranie żądań rachunków)

Ujemny typ `-X` oznacza "odbierz dowolny komunikat z typem <= X".
Pozwala to odebrać żądania rachunków od różnych grup.
MSG_KASA_REQ = 1000, więc typy 1001-1099 to żądania rachunków.

```c
struct komunikat_kasa_req kasa_req;
while (msgrcv(msg, &kasa_req, sizeof(kasa_req) - sizeof(long),
              -(MSG_KASA_REQ + MAX_GRUP), IPC_NOWAIT) != -1) {
    int suma = oblicz_rachunek(kasa_req.talerzyki);
    // ... obsługa rachunku ...
}
```

#### [msgrcv() - klient.c:407](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L407) (odbieranie rachunku)

Lider czeka na rachunek od kasy.
Typ = getpid() zapewnia że tylko ten lider odbierze swoją odpowiedź.

```c
struct komunikat_kasa_resp resp;
while (msgrcv(msg, &resp, sizeof(struct komunikat_kasa_resp) - sizeof(long), getpid(), 0) == -1) {
    if (errno == EINTR) continue;
    if (errno == EIDRM) break;
    perror("[KLIENT] msgrcv - blad odbierania rachunku");
    break;
}
```

#### [msgctl() - main.c:75](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L75) (usuwanie)

Funkcja `msgctl()` z flagą `IPC_RMID` usuwa kolejkę komunikatów.
Wszystkie oczekujące wiadomości są tracone.
Procesy czekające na msgrcv dostaną błąd EIDRM.

```c
if (msgctl(g_msg_id, IPC_RMID, NULL) == -1) {
    perror("[MAIN] msgctl IPC_RMID");
} else {
    printf("[MAIN] Kolejka komunikatow (id=%d) usunieta.\n", g_msg_id);
}
```

---

## 3. Struktury Danych

### 3.1 [Główna struktura restauracji (wspolne.h:158-185)](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L158-L185)

Struktura `restauracja` jest przechowywana w pamięci dzielonej i zawiera
cały stan symulacji. Wszystkie procesy mają dostęp do tych samych danych.

```c
struct restauracja {
    int otwarta;                  // Flaga czy restauracja działa (1=tak, 0=zamknięta)
    pid_t pid_kucharza;           // PID procesu kucharza (do wysyłania sygnałów)
    pid_t pid_tasmy;              // PID procesu taśmy (do wysyłania sygnałów)

    time_t czas_startu;           // Timestamp uruchomienia (do obliczania czasu pracy)
    int przyjmuje_klientow;       // Czy przyjmuje nowych klientów (0 po czasie Tk)

    struct tasma tasma;           // 20 segmentów taśmy z talerzykami
    struct miejsce_lada lada[LADA_MIEJSC];  // 9 miejsc przy ladzie
    struct stolik stoliki[STOLIKI];         // 10 stolików różnej pojemności
    struct klient_info klienci[MAX_KLIENTOW]; // Tablica wszystkich klientów

    int grupa_zjedzone_cnt[MAX_GRUP];  // Ile osób z grupy skończyło jeść
    int gdzie_siedzimy[MAX_GRUP];      // Indeks miejsca grupy (-1 = nie przydzielone)
    int typ_miejsca_grupy[MAX_GRUP];   // 0 = lada, 1 = stolik
    int grupa_talerzyki[MAX_GRUP][KOLORY]; // Zliczanie talerzyków per grupa per kolor
    int grupa_zaplacila[MAX_GRUP];     // Czy grupa już zapłaciła rachunek

    struct zamowienie zamowienia[MAX_ZAMOWIEN]; // Aktywne zamówienia specjalne
    char info[200];               // Tekst statusu wyświetlany na monitorze

    int wyprodukowane[KOLORY];    // Licznik wyprodukowanych talerzyków per kolor
    int sprzedane[KOLORY];        // Licznik zjedzonych talerzyków per kolor
    struct kasa_stats kasa;       // Statystyki kasy (transakcje, utarg)
};
```

### 3.2 [Struktura talerzyka (wspolne.h:96-100)](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L96-L100)

Reprezentuje jeden talerzyk sushi na taśmie.

```c
struct talerzyk {
    int kolor;       // Indeks koloru 0-5 (określa cenę)
    int cena;        // Cena w złotych (10, 15, 20, 40, 50, 60)
    int id_odbiorcy; // PID klienta dla zamówień specjalnych, -1 dla ogólnych
};
```

### 3.3 [Struktury komunikatów (wspolne.h:61-87)](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L61-L87)

Komunikaty używane w kolejce komunikatów do wymiany informacji między procesami.

```c
// Zgłoszenie klienta do obsługi i odpowiedź z miejscem
struct komunikat {
    long mtype;           // Typ: 1 dla zgłoszeń, PID dla odpowiedzi
    pid_t pid_nadawcy;    // PID klienta (do odpowiedzi)
    int rozmiar_grupy;    // Ile osób w grupie (1-4)
    int numer_miejsca;    // Przydzielony indeks miejsca
    int typ_miejsca;      // 0 = lada, 1 = stolik
    int group_priority;   // Czy grupa ma VIP (priorytet w kolejce)
};

// Żądanie rachunku od klienta do kasy
struct komunikat_kasa_req {
    long mtype;           // MSG_KASA_REQ + id_grupy
    pid_t pid_lidera;     // PID lidera (on płaci)
    int id_grupy;         // ID grupy
    int talerzyki[KOLORY]; // Ile talerzyków każdego koloru zjadła grupa
};

// Odpowiedź kasy z rachunkiem
struct komunikat_kasa_resp {
    long mtype;           // PID lidera (tylko on odbiera)
    int suma;             // Suma do zapłaty w złotych
    int talerzyki[KOLORY]; // Podsumowanie talerzyków (do wyświetlenia)
};
```

---

## 4. Handlery Sygnałów

### 4.1 [Handler SIGALRM - timer (kucharz.c:37-40)](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L37-L40)

Wywoływany co sekundę przez timer systemowy.
Ustawia flagę `tick_flag`, która jest sprawdzana w pętli głównej.
Używamy `volatile sig_atomic_t` dla bezpieczeństwa w handlerze.

```c
void handler_alarm(int sig) {
    (void)sig;       // Ignorujemy numer sygnału (zawsze SIGALRM)
    tick_flag = 1;   // Ustawiamy flagę - pętla główna ją odczyta
}
```

### 4.2 [Handler SIGUSR1 - przyspieszenie (kucharz.c:47-51)](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L47-L51)

Zmniejsza opóźnienie produkcji o połowę (2x szybciej).
Minimum to 1 tick - nie można zejść niżej.
Wywoływany przez kierownika.

```c
void handler_szybciej(int sig) {
    (void)sig;
    production_delay = (production_delay > 1) ? production_delay / 2 : 1;
    printf("[KUCHARZ] Przyspieszenie! Nowy delay: %d tickow\n", production_delay);
}
```

### 4.3 [Handler SIGUSR2 - spowolnienie (kucharz.c:58-62)](https://github.com/otfpubert/SOPROJEKT/blob/main/kucharz.c#L58-L62)

Zwiększa opóźnienie produkcji dwukrotnie (2x wolniej).
Maximum to 4 ticki - nie można iść wyżej.
Wywoływany przez kierownika.

```c
void handler_wolniej(int sig) {
    (void)sig;
    production_delay = (production_delay < 4) ? production_delay * 2 : 4;
    printf("[KUCHARZ] Spowolnienie! Nowy delay: %d tickow\n", production_delay);
}
```

### 4.4 [Handler SIGTERM - ewakuacja (klient.c:45-57)](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L45-L57)

Obsługuje polecenie ewakuacji od kierownika.
Zwalnia zajmowane miejsce w pamięci dzielonej.
Zmniejsza licznik aktywnych klientów.
Loguje ewakuację i kończy proces.

```c
void handler_ewakuacja(int sig) {
    (void)sig;

    // Sprzątanie tylko jeśli mamy dostęp do zasobów
    if (r_global != NULL && moj_slot_idx_global != -1 && sem_global != -1) {
        lock(sem_global);
        r_global->klienci[moj_slot_idx_global].aktywny = 0;  // Zwolnij slot
        unlock(sem_global);
        sem_dec(sem_global, SEM_ACTIVE);  // Zmniejsz licznik klientów
    }
    zrzut_do_logu("KLIENT %d: EWAKUACJA! Uciekam!", getpid());
    exit(0);  // Zakończ proces
}
```

### 4.5 [Handler SIGINT - sprzątanie (main.c:47-84)](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L47-L84)

Obsługuje Ctrl+C - czyste zamknięcie programu.
Wysyła SIGTERM do wszystkich procesów potomnych.
Usuwa wszystkie zasoby IPC (pamięć, semafory, kolejki).

```c
void sprzatanie(int sig) {
    printf("\n[MAIN] Otrzymano sygnal %d. Sprzatanie zasobow IPC...\n", sig);

    // Wyślij SIGTERM do wszystkich procesów w grupie
    if (kill(0, SIGTERM) == -1 && errno != ESRCH) {
        perror("[MAIN] kill(0, SIGTERM)");
    }

    // Usuń pamięć dzieloną
    if (g_shm_id != -1) {
        if (shmctl(g_shm_id, IPC_RMID, NULL) == -1) {
            perror("[MAIN] shmctl IPC_RMID");
        }
    }

    // Usuń semafory
    if (g_sem_id != -1) {
        if (semctl(g_sem_id, 0, IPC_RMID) == -1) {
            perror("[MAIN] semctl IPC_RMID");
        }
    }

    // Usuń kolejkę komunikatów
    if (g_msg_id != -1) {
        if (msgctl(g_msg_id, IPC_RMID, NULL) == -1) {
            perror("[MAIN] msgctl IPC_RMID");
        }
    }

    exit(0);
}
```

---

## 5. Algorytm Przydzielania Miejsc ("Tetris")

### 5.1 [Priorytet VIP (obsluga.c:222-244)](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L222-L244)

Grupy VIP są wstawiane na początek kolejki (przed zwykłymi klientami).
Szukamy pierwszej pozycji bez priorytetu i wstawiamy tam VIP-a.
Jeśli wszyscy w kolejce to VIP-y, nowy VIP idzie na koniec.

```c
void wstaw_do_kolejki(wpis_kolejki *q, int *cnt, pid_t pid, int priority) {
    if (*cnt >= Q_SIZE) return;  // Kolejka pełna

    int idx_wstawienia = *cnt;   // Domyślnie na koniec

    if (priority) {
        // VIP: szukaj pierwszego nie-VIP-a
        for (int i = 0; i < *cnt; i++) {
            if (q[i].group_priority == 0) {
                idx_wstawienia = i;
                break;
            }
        }
    }

    // Przesuń elementy żeby zrobić miejsce
    for (int i = *cnt; i > idx_wstawienia; i--) {
        q[i] = q[i-1];
    }

    // Wstaw nowy element
    q[idx_wstawienia].pid = pid;
    q[idx_wstawienia].group_priority = priority;
    (*cnt)++;
}
```

### 5.2 [Dosiadanie równoliczne (obsluga.c:485-498)](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L485-L498)

Grupy mogą dzielić stolik tylko z grupami o tym samym rozmiarze.
Np. dwie grupy 2-osobowe mogą siedzieć przy stoliku 4-osobowym.
Ale grupa 1-osobowa nie może dołączyć do grupy 2-osobowej.

```c
if (q2_cnt > 0) {
    for (int i=6; i<10; i++) {  // Stoliki 4-osobowe
        int wolne = r->stoliki[i].pojemnosc - r->stoliki[i].ile_osob;

        // Dosiadanie: stolik zajęty przez grupę 2-osobową
        int dosiadka_ok = (r->stoliki[i].ile_osob > 0 &&
                           r->stoliki[i].rozmiar_grupy == 2);

        // Pusty stolik: tylko gdy nie ma większych grup w kolejce
        int pusty_ok = (r->stoliki[i].ile_osob == 0 &&
                        q4_cnt==0 && q3_cnt==0);

        if (wolne >= 2 && (dosiadka_ok || pusty_ok)) {
            if (pusty_ok) r->stoliki[i].rozmiar_grupy = 2;  // Zapamiętaj rozmiar
            r->stoliki[i].ile_osob += 2;
            zapros_klienta(msg, queue_g2, &q2_cnt, 0, i, 1);
            if(q2_cnt == 0) break;
        }
    }
}
```

---

## 6. [System Głodu Klienta (klient.c:31-38)](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L31-L38)

Klient nie je ciągle - musi najpierw "zgłodnieć".
Głód rośnie co sekundę (SIGALRM). Gdy osiągnie próg, klient może jeść.
Po zjedzeniu głód spada do 0 i znów musi rosnąć.

```c
#define PROG_GLODU 15              // Próg głodu - poniżej tego klient nie je
volatile sig_atomic_t glod_tick_flag = 0;  // Flaga od timera
volatile int glod = PROG_GLODU;    // Start z pełnym głodem - może od razu jeść

// Handler wywoływany co sekundę
void handler_glod(int sig) {
    (void)sig;
    glod_tick_flag = 1;  // Sygnał że minęła sekunda
}
```

Użycie w [pętli głównej (klient.c:301-329)](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L301-L329):

```c
// Inkrementacja głodu przy każdym SIGALRM
if (glod_tick_flag) {
    glod_tick_flag = 0;
    if (glod < PROG_GLODU * 2) glod++;  // Maksymalny głód to 2x próg
}

// ... sprawdzanie taśmy ...

// Jedzenie tylko gdy głód >= PROG_GLODU i talerzyk ogólny
if (t.id_odbiorcy == -1 && glod >= PROG_GLODU) {
    czy_jem = 1;
}

if (czy_jem) {
    r->tasma.seg[moj_segment].zajety = 0;  // Zabierz talerzyk
    r->sprzedane[t.kolor]++;               // Statystyki
    zjedzone++;
    glod = 0;  // Reset głodu po zjedzeniu
    // ... reszta logiki ...
}
```

---

## 7. Obsługa Błędów (perror i errno)

### 7.1 [Funkcja check_error (wspolne.h:27-32)](https://github.com/otfpubert/SOPROJEKT/blob/main/wspolne.h#L27-L32)

Funkcja pomocnicza do sprawdzania błędów systemowych.
Jeśli funkcja zwróciła -1 (błąd), wypisuje komunikat i kończy program.

```c
static inline void check_error(int ret, const char *msg) {
    if (ret == -1) {
        perror(msg);          // Wypisz komunikat + opis błędu z errno
        exit(EXIT_FAILURE);   // Zakończ program z kodem błędu
    }
}
```

### 7.2 [Obsługa EINTR (klient.c:210-220)](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L210-L220)

EINTR oznacza że wywołanie systemowe zostało przerwane przez sygnał.
Nie jest to prawdziwy błąd - po prostu powtarzamy operację.
Typowe przy msgsnd/msgrcv gdy proces dostaje SIGALRM.

```c
while (msgsnd(msg, &zapytanie, sizeof(struct komunikat) - sizeof(long), 0) == -1) {
    if (errno == EINTR) continue;  // Przerwane przez sygnał - powtórz
    if (errno == EIDRM) {          // Kolejka usunięta - restauracja zamknięta
        shmdt(r);
        exit(0);
    }
    perror("[KLIENT] msgsnd");     // Prawdziwy błąd
    exit(EXIT_FAILURE);
}
```

### 7.3 [Obsługa EIDRM (obsluga.c:370-375)](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L370-L375)

EIDRM oznacza że zasób IPC został usunięty (np. przy zamykaniu restauracji).
Nie jest to błąd - to normalna sytuacja przy shutdown.
ENOMSG przy IPC_NOWAIT oznacza "nie ma wiadomości" - też normalne.

```c
if (recv_result == -1 && errno != ENOMSG && errno != EINTR) {
    if (errno == EIDRM) {
        break;  // Kolejka usunięta - zakończ pętlę
    }
    perror("[OBSLUGA] msgrcv");  // Prawdziwy błąd
}
```

---

## 8. Walidacja Danych Wejściowych

### 8.1 [Walidacja argumentów klienta (klient.c:101-113)](https://github.com/otfpubert/SOPROJEKT/blob/main/klient.c#L101-L113)

Sprawdzamy czy argumenty przekazane do procesu klienta są poprawne.
ID grupy musi być w zakresie 0 do MAX_GRUP-1.
Rozmiar grupy musi być 1-4 (zgodnie z tematem projektu).

```c
// Sprawdź zakres ID grupy
if (moj_gid < 0 || moj_gid >= MAX_GRUP) {
    fprintf(stderr, "[KLIENT %d] Blad: niepoprawny ID grupy %d (max %d)\n",
            getpid(), moj_gid, MAX_GRUP - 1);
    exit(EXIT_FAILURE);
}

// Sprawdź rozmiar grupy
if (moja_grupa_size < 1 || moja_grupa_size > 4) {
    fprintf(stderr, "[KLIENT %d] Blad: niepoprawny rozmiar grupy %d (1-4)\n",
            getpid(), moja_grupa_size);
    exit(EXIT_FAILURE);
}
```

### 8.2 [Walidacja w kierowniku (kierownik.c:78-92)](https://github.com/otfpubert/SOPROJEKT/blob/main/kierownik.c#L78-L92)

Sprawdzamy czy użytkownik wpisał poprawną liczbę.
scanf zwraca liczbę poprawnie odczytanych elementów.
EOF oznacza koniec wejścia (np. Ctrl+D).
Czyścimy bufor przy błędnych danych.

```c
int scan_result = scanf("%d", &wybor);

// scanf nie udało się odczytać liczby
if (scan_result != 1) {
    if (scan_result == EOF) {
        printf("\n[KIEROWNIK] Koniec wejscia (EOF). Wychodzenie.\n");
        break;
    }
    printf("[BLAD] Niepoprawne dane wejsciowe. Podaj liczbe 0-3.\n");
    clear_input_buffer();  // Wyczyść śmieci z bufora
    continue;
}

// Sprawdź zakres
if (wybor < 0 || wybor > 3) {
    printf("[BLAD] Niepoprawna opcja %d. Dostepne opcje: 0, 1, 2, 3.\n", wybor);
    continue;
}
```

### 8.3 [Walidacja zgłoszeń (obsluga.c:361-367)](https://github.com/otfpubert/SOPROJEKT/blob/main/obsluga.c#L361-L367)

Sprawdzamy dane otrzymane przez kolejkę komunikatów.
Rozmiar grupy musi być 1-4, PID musi być dodatni.
Niepoprawne zgłoszenia są logowane i ignorowane.

```c
if (buf.rozmiar_grupy >= 1 && buf.rozmiar_grupy <= 4 && buf.pid_nadawcy > 0) {
    dodaj_do_kolejki(buf.rozmiar_grupy, buf.group_priority, buf.pid_nadawcy);
} else {
    zrzut_do_logu("[OBSLUGA] Odrzucono niepoprawne zgloszenie: rozmiar=%d, pid=%d",
                  buf.rozmiar_grupy, buf.pid_nadawcy);
}
```

---

## 9. [Walidacja dzieci (main.c:307-334)](https://github.com/otfpubert/SOPROJEKT/blob/main/main.c#L307-L334)

Zgodnie z tematem: "Osoba dorosła może opiekować się jednocześnie
co najwyżej trzema dziećmi w wieku do 10 lat."

Sprawdzamy stosunek dzieci do dorosłych i korygujemy jeśli potrzeba.
Grupa nie może składać się z samych dzieci.

```c
// Policz dorosłych i dzieci
int liczba_dzieci = 0;
int liczba_doroslych = 0;
for (int k = 0; k < rozmiar; k++) {
    if (member_child[k]) liczba_dzieci++;
    else liczba_doroslych++;
}

// Max 3 dzieci na 1 dorosłego
if (liczba_doroslych > 0 && liczba_dzieci > 3 * liczba_doroslych) {
    int max_dzieci = 3 * liczba_doroslych;
    printf("[MAIN] Grupa %d: za duzo dzieci (%d), koryguje do %d\n",
           g, liczba_dzieci, max_dzieci);

    // Zamień nadmiarowe dzieci na dorosłych (od końca)
    for (int k = rozmiar - 1; k > 0 && liczba_dzieci > max_dzieci; k--) {
        if (member_child[k]) {
            member_child[k] = 0;
            liczba_dzieci--;
        }
    }
}

// Grupa nie może składać się z samych dzieci
if (liczba_doroslych == 0) {
    printf("[MAIN] Grupa %d: brak doroslych, pierwszy czlonek staje sie doroslym\n", g);
    member_child[0] = 0;  // Pierwszy zawsze dorosły
}
```

---

## 10. Testy

### 10.1 System Testowy

Projekt zawiera skrypt `run_test.sh` do uruchamiania różnych scenariuszy testowych.
Skrypt modyfikuje parametry w kodzie źródłowym, kompiluje i uruchamia symulację.

**Użycie:**
```bash
./run_test.sh list      # Wyświetl dostępne testy
./run_test.sh <numer>   # Uruchom test
./run_test.sh restore   # Przywróć domyślne wartości
```

### 10.2 Dostępne Testy

| Test | Nazwa | Parametry | Cel |
|------|-------|-----------|-----|
| 1 | Grupy 1-osobowe | 50 grup, rozmiar=1, VIP=0% | Sprawdzenie siadania: lada → stoliki |
| 2 | Grupy 2-osobowe | 50 grup, rozmiar=2, VIP=0% | Sprawdzenie par przy ladzie i dosiadania |
| 3 | Dosiadanie równoliczne | 50 grup, losowy rozmiar | Tylko te same rozmiary dzielą stolik |
| 4 | VIP 100% | 20 grup, VIP=100% | VIP tylko stoliki, brak crashy |
| 5 | Stress test | 100 grup, delay=1s | Brak deadlocków |
| 6 | MEGA stress | 2000 grup, delay=1s | Granice systemu |
| 7 | Test sygnałów | 30 grup, delay=3-5s | Do testowania z kierownikiem |

### 10.3 TEST 1: Grupy 1-osobowe

**Cel:** Sprawdzić czy grupy 1-osobowe najpierw zajmują ladę (9 miejsc), a dopiero potem siadają przy stolikach.

**Konfiguracja:**
```bash
LICZBA_GRUP=50
ROZMIAR_GRUPY=1  # Wymuszony
VIP_CHANCE=0%
CZAS_ZAMKNIECIA=60s
```

### 10.4 TEST 2: Grupy 2-osobowe

**Cel:** Sprawdzić czy grupy 2-osobowe siadają poprawnie jako pary.

**Konfiguracja:**
```bash
LICZBA_GRUP=50
ROZMIAR_GRUPY=2  # Wymuszony
VIP_CHANCE=0%
```

### 10.5 TEST 3: Dosiadanie równoliczne

**Cel:** Sprawdzić czy tylko grupy tej samej wielkości mogą dzielić stolik.

**Konfiguracja:**
```bash
LICZBA_GRUP=50
ROZMIAR_GRUPY=losowy
VIP_CHANCE=0%
```

### 10.6 TEST 4: VIP 100%

**Cel:** Sprawdzić czy system obsługuje sytuację gdy wszyscy są VIP.

**Konfiguracja:**
```bash
LICZBA_GRUP=20
VIP_CHANCE=100%
```

### 10.7 TEST 5: Stress test (100 grup)

**Cel:** Sprawdzić czy system radzi sobie z dużą liczbą grup.

**Konfiguracja:**
```bash
LICZBA_GRUP=100
DELAY=1s
CZAS_ZAMKNIECIA=90s
```

### 10.8 TEST 6: MEGA Stress test (2000 grup)

**Cel:** Sprawdzić granice wydajności systemu.

**Konfiguracja:**
```bash
LICZBA_GRUP=2000
DELAY=1s
CZAS_ZAMKNIECIA=240s (4 minuty)
```


### 10.9 TEST 7: Test sygnałów kierownika

**Cel:** Przetestować działanie sygnałów SIGUSR1/SIGUSR2/SIGTERM.

**Procedura:**
1. Uruchom test: `./run_test.sh 7`
2. Otwórz drugi terminal
3. Uruchom: `./kierownik`
4. Testuj opcje menu:

| Opcja | Sygnał | Efekt |
|-------|--------|-------|
| 1 | SIGUSR1 | Kucharz i taśma 2x szybciej |
| 2 | SIGUSR2 | Kucharz i taśma 50% wolniej |
| 3 | SIGTERM | Ewakuacja - wszyscy wychodzą |

