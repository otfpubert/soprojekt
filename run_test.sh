#!/bin/bash
#
# =============================================================================
#                         SKRYPT TESTOWY - KAITEN ZUSHI
# =============================================================================
#
# Opis:
#   Skrypt do uruchamiania różnych scenariuszy testowych dla symulacji
#   restauracji kaiten zushi. Modyfikuje parametry w kodzie źródłowym,
#   kompiluje projekt i uruchamia wybrany test.
#
# Użycie:
#   ./run_test.sh <numer_testu>
#   ./run_test.sh list              - wyświetl listę dostępnych testów
#   ./run_test.sh restore           - przywróć domyślne wartości
#
# Przykład:
#   ./run_test.sh 1                 - uruchom test grup 1-osobowych
#   ./run_test.sh 5                 - uruchom stress test
#
# =============================================================================

# Kolory do wyświetlania
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
#                              DOMYŚLNE WARTOŚCI
# =============================================================================
# Te wartości zostaną przywrócone po wywołaniu ./run_test.sh restore

DEFAULT_LICZBA_GRUP=30
DEFAULT_CZAS_ZAMKNIECIA=60
DEFAULT_VIP_CHANCE=2
DEFAULT_DELAY_MIN=1
DEFAULT_DELAY_MAX=3
# Rozmiar grupy: 0 = losowy (1-4), 1-4 = wymuszony

# =============================================================================
#                           FUNKCJE POMOCNICZE
# =============================================================================

# Wyświetla nagłówek testu
print_header() {
    echo -e "${BLUE}"
    echo "============================================================================="
    echo "  $1"
    echo "============================================================================="
    echo -e "${NC}"
}

# Wyświetla parametry testu
print_params() {
    echo -e "${YELLOW}Parametry testu:${NC}"
    echo "  - LICZBA_GRUP:      $1"
    echo "  - CZAS_ZAMKNIECIA:  $2 sekund"
    echo "  - VIP_CHANCE:       $3%"
    echo "  - ROZMIAR_GRUPY:    $4 (0 = losowy)"
    echo "  - DELAY:            $5-$6 sekund między grupami"
    echo ""
}

# Modyfikuje wartości w plikach źródłowych
apply_config() {
    local LICZBA_GRUP=$1
    local CZAS_ZAMKNIECIA=$2
    local VIP_CHANCE=$3
    local ROZMIAR_GRUPY=$4  # 0 = losowy, 1-4 = wymuszony
    local DELAY_MIN=$5
    local DELAY_MAX=$6

    echo -e "${GREEN}Aplikowanie konfiguracji...${NC}"

    # ---------------------------------------------------------
    # main.c: LICZBA_GRUP (linia z #define LICZBA_GRUP)
    # ---------------------------------------------------------
    sed -i "s/#define LICZBA_GRUP [0-9]*/#define LICZBA_GRUP $LICZBA_GRUP/" main.c

    # ---------------------------------------------------------
    # wspolne.h: CZAS_ZAMKNIECIA
    # ---------------------------------------------------------
    sed -i "s/#define CZAS_ZAMKNIECIA [0-9]*/#define CZAS_ZAMKNIECIA $CZAS_ZAMKNIECIA/" wspolne.h

    # ---------------------------------------------------------
    # main.c: VIP_CHANCE (linia: if (rand() % 100 < X) /* VIP */)
    # UWAGA: Jest też linia z dziećmi (10%) - zmieniamy tylko VIP
    # ---------------------------------------------------------
    sed -i "s/rand() % 100 < [0-9]*) {  \/\* [0-9]*% szans na VIP/rand() % 100 < $VIP_CHANCE) {  \/\* $VIP_CHANCE% szans na VIP/" main.c

    # ---------------------------------------------------------
    # main.c: ROZMIAR_GRUPY
    # Oryginał: int rozmiar = 1 + rand() % 4;
    # Wymuszony: int rozmiar = X;
    # ---------------------------------------------------------
    if [ "$ROZMIAR_GRUPY" -eq 0 ]; then
        # Przywróć losowy rozmiar
        sed -i 's/int rozmiar = [0-9];/int rozmiar = 1 + rand() % 4;/' main.c
        sed -i 's/int rozmiar = 1 + rand() % 4 + rand() % 4;/int rozmiar = 1 + rand() % 4;/' main.c
    else
        # Wymuś konkretny rozmiar
        sed -i "s/int rozmiar = 1 + rand() % 4;/int rozmiar = $ROZMIAR_GRUPY;/" main.c
        sed -i "s/int rozmiar = [0-9];/int rozmiar = $ROZMIAR_GRUPY;/" main.c
    fi

    # ---------------------------------------------------------
    # main.c: DELAY między grupami
    # Oryginał: int losowy_delay = 1 + rand() % 3;
    # ---------------------------------------------------------
    local DELAY_RANGE=$((DELAY_MAX - DELAY_MIN + 1))
    if [ "$DELAY_MIN" -eq "$DELAY_MAX" ]; then
        # Stały delay
        sed -i "s/int losowy_delay = [0-9] + rand() % [0-9];/int losowy_delay = $DELAY_MIN;/" main.c
        sed -i "s/int losowy_delay = [0-9];/int losowy_delay = $DELAY_MIN;/" main.c
    else
        # Losowy delay w zakresie
        sed -i "s/int losowy_delay = [0-9] + rand() % [0-9];/int losowy_delay = $DELAY_MIN + rand() % $DELAY_RANGE;/" main.c
        sed -i "s/int losowy_delay = [0-9];/int losowy_delay = $DELAY_MIN + rand() % $DELAY_RANGE;/" main.c
    fi

    echo -e "${GREEN}Konfiguracja zastosowana.${NC}"
}

# Kompiluje projekt
compile_project() {
    echo -e "${GREEN}Kompilacja projektu...${NC}"
    make clean > /dev/null 2>&1
    if make; then
        echo -e "${GREEN}Kompilacja zakończona sukcesem.${NC}"
        return 0
    else
        echo -e "${RED}BŁĄD KOMPILACJI!${NC}"
        return 1
    fi
}

# Uruchamia symulację
run_simulation() {
    echo ""
    echo -e "${GREEN}Uruchamianie symulacji...${NC}"
    echo -e "${YELLOW}(Ctrl+C aby przerwać)${NC}"
    echo ""
    ./main
}

# Wyświetla listę testów
show_test_list() {
    print_header "DOSTĘPNE TESTY"
    echo -e "${YELLOW}Testy siadania przy stolikach:${NC}"
    echo "  1 - Grupy 1-osobowe (15 grup)"
    echo "      Sprawdza: najpierw lada, potem stoliki"
    echo ""
    echo "  2 - Grupy 2-osobowe (10 grup)"
    echo "      Sprawdza: pary przy ladzie, stoliki 2-os., dosiadanie na 4-os."
    echo ""
    echo "  3 - Dosiadanie równoliczne (8 grup: 2,2,2,2,1,1,1,1)"
    echo "      Sprawdza: tylko grupy tej samej wielkości dzielą stolik"
    echo ""
    echo -e "${YELLOW}Testy VIP:${NC}"
    echo "  4 - VIP 100% (20 grup, wszyscy VIP)"
    echo "      Sprawdza: VIP tylko przy stolikach, kolejka VIP"
    echo ""
    echo -e "${YELLOW}Testy obciążeniowe:${NC}"
    echo "  5 - Stress test (100 grup, szybkie tworzenie)"
    echo "      Sprawdza: brak deadlocków, wszystkie grupy obsłużone"
    echo ""
    echo "  6 - MEGA stress test (200 grup, bardzo szybkie tworzenie)"
    echo "      Sprawdza: granice wydajności systemu"
    echo ""
    echo -e "${YELLOW}Inne:${NC}"
    echo "  7 - Test sygnałów (30 grup, wolne tempo)"
    echo "      Do testowania z kierownikiem w drugim terminalu"
    echo ""
    echo -e "${BLUE}Użycie:${NC}"
    echo "  ./run_test.sh <numer>     - uruchom test"
    echo "  ./run_test.sh restore     - przywróć domyślne wartości"
    echo ""
}

# =============================================================================
#                            DEFINICJE TESTÓW
# =============================================================================

run_test_1() {
    # -------------------------------------------------------------------------
    # TEST 1: Grupy 1-osobowe
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić czy grupy 1-osobowe najpierw zajmują ladę (9 miejsc),
    #      a dopiero potem siadają przy stolikach.
    #
    # Oczekiwany wynik:
    # - Pierwsze 9 grup siada przy ladzie (LADA 0-8)
    # - Kolejne grupy siadają przy stolikach (zaczynając od 2-osobowych)
    # - Brak dosiadania (każda grupa 1-osobowa jest sama)
    # -------------------------------------------------------------------------

    print_header "TEST 1: Grupy 1-osobowe"

    local LICZBA_GRUP=50
    local CZAS_ZAMKNIECIA=60
    local VIP_CHANCE=0          # Brak VIP - czysty test siadania
    local ROZMIAR_GRUPY=1       # Wymuszony rozmiar 1
    local DELAY_MIN=1
    local DELAY_MAX=2

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - Lada powinna się zapełnić pierwsza (LADA 0-8)"
    echo "  - Gdy lada pełna, grupy idą na stoliki"
    echo "  - Każda grupa siedzi sama (brak dosiadania)"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_2() {
    # -------------------------------------------------------------------------
    # TEST 2: Grupy 2-osobowe
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić czy grupy 2-osobowe siadają poprawnie:
    #      1. Najpierw pary przy ladzie (max 4 pary = 8 osób)
    #      2. Potem stoliki 2-osobowe (3 sztuki)
    #      3. Potem dosiadanie na stolikach 4-osobowych
    #
    # Oczekiwany wynik:
    # - Lada: miejsca 0-1, 2-3, 4-5, 6-7 (4 pary)
    # - Stoliki 2-os: 0, 1, 2
    # - Stoliki 4-os: dosiadanie po 2 grupy
    # -------------------------------------------------------------------------

    print_header "TEST 2: Grupy 2-osobowe"

    local LICZBA_GRUP=50
    local CZAS_ZAMKNIECIA=60
    local VIP_CHANCE=0
    local ROZMIAR_GRUPY=2       # Wymuszony rozmiar 2
    local DELAY_MIN=1
    local DELAY_MAX=2

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - Lada: pary siedzą obok siebie (0-1, 2-3, itd.)"
    echo "  - Stoliki 2-os. (0-2) zapełniają się"
    echo "  - Stoliki 4-os.: dwie grupy 2-os. mogą siedzieć razem"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_3() {
    # -------------------------------------------------------------------------
    # TEST 3: Dosiadanie równoliczne
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić czy tylko grupy tej samej wielkości mogą dzielić stolik.
    #      Grupy 2-os. mogą siedzieć z grupami 2-os.
    #      Grupy 1-os. NIE mogą siedzieć z grupami 2-os.
    #
    # Scenariusz: naprzemiennie grupy 2-os i 1-os
    # Oczekiwany wynik:
    # - Grupy 2-os. siedzą razem na stolikach 4-os.
    # - Grupy 1-os. siedzą osobno (lada lub wolne stoliki)
    # -------------------------------------------------------------------------

    print_header "TEST 3: Dosiadanie równoliczne"

    local LICZBA_GRUP=50
    local CZAS_ZAMKNIECIA=60
    local VIP_CHANCE=0
    local ROZMIAR_GRUPY=0       # LOSOWY - ale skrypt nie obsługuje naprzemiennego
    local DELAY_MIN=2
    local DELAY_MAX=3

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - Grupy różnych rozmiarów NIE siedzą razem"
    echo "  - Tylko równoliczne grupy mogą dzielić stolik"
    echo "  - Sprawdź kolumnę 'KLIENCI' - grupy przy tym samym stoliku mają różne G="
    echo ""
    echo -e "${RED}UWAGA: Ten test używa losowych rozmiarów grup.${NC}"
    echo -e "${RED}Obserwuj czy grupy różnych rozmiarów NIE siedzą razem.${NC}"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_4() {
    # -------------------------------------------------------------------------
    # TEST 4: VIP 100%
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić czy system obsługuje sytuację gdy wszyscy są VIP.
    #      VIP-y MUSZĄ siadać tylko przy stolikach (nie przy ladzie).
   
    print_header "TEST 4: VIP 100%"

    local LICZBA_GRUP=20
    local CZAS_ZAMKNIECIA=40
    local VIP_CHANCE=100        
    local ROZMIAR_GRUPY=0       
    local DELAY_MIN=1
    local DELAY_MAX=2

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - LADA powinna być PUSTA (VIP nie siada przy ladzie)"
    echo "  - Wszyscy klienci oznaczeni złotym 'VIP'"
    echo "  - System nie powinien się zawiesić"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_5() {
    # -------------------------------------------------------------------------
    # TEST 5: Stress test
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić czy system radzi sobie z dużą liczbą grup.
    #      100 grup z szybkim tempem tworzenia.
   

    print_header "TEST 5: STRESS TEST (100 grup)"

    local LICZBA_GRUP=100
    local CZAS_ZAMKNIECIA=90
    local VIP_CHANCE=2          # Normalny procent VIP
    local ROZMIAR_GRUPY=0       # Losowy rozmiar
    local DELAY_MIN=1           # Szybko (1s minimum, bo 0 wyłącza timer)
    local DELAY_MAX=1

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - Program NIE powinien się zawiesić"
    echo "  - Kolejki mogą być długie - to normalne"
    echo "  - Na końcu: raport_dzienny.txt - sprawdź czy wszystkie grupy zapłaciły"
    echo ""
    echo -e "${RED}Ten test może trwać ~2 minuty${NC}"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_6() {
    # -------------------------------------------------------------------------
    # TEST 6: MEGA Stress test
    # -------------------------------------------------------------------------
    # Cel: Sprawdzić granice wydajności systemu.
    #      200 grup z bardzo szybkim tempem.
    

    print_header "TEST 6: MEGA STRESS TEST (200 grup)"

    local LICZBA_GRUP=2000
    local CZAS_ZAMKNIECIA=240
    local VIP_CHANCE=2
    local ROZMIAR_GRUPY=0
    local DELAY_MIN=1
    local DELAY_MAX=1           

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Co obserwować:${NC}"
    echo "  - Program NIE powinien crashować"
    echo "  - Może być wolny - to normalne przy 200 grupach"
    echo "  - Sprawdź czy kończy się poprawnie"
    echo ""
    echo -e "${RED}UWAGA: Ten test może trwać ~3-4 minuty!${NC}"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

run_test_7() {
    # -------------------------------------------------------------------------
    # TEST 7: Test sygnałów kierownika
    # -------------------------------------------------------------------------
    # Cel: Umożliwić testowanie sygnałów SIGUSR1/SIGUSR2/SIGTERM.
    #      Wolne tempo żeby było czas na interakcję.
   

    print_header "TEST 7: Test sygnałów kierownika"

    local LICZBA_GRUP=30
    local CZAS_ZAMKNIECIA=120   # Dużo czasu na testy
    local VIP_CHANCE=2
    local ROZMIAR_GRUPY=0
    local DELAY_MIN=3
    local DELAY_MAX=5           # Wolne tempo

    print_params $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX

    echo -e "${YELLOW}Procedura testowa:${NC}"
    echo "  1. Ten test uruchomi symulację z wolnym tempem"
    echo "  2. Otwórz DRUGI terminal"
    echo "  3. Uruchom: ./kierownik"
    echo "  4. Testuj opcje:"
    echo "     - 1: Przyspieszenie (SIGUSR1) - kucharz/taśma 2x szybciej"
    echo "     - 2: Spowolnienie (SIGUSR2) - kucharz/taśma 50% wolniej"
    echo "     - 3: Ewakuacja (SIGTERM) - wszyscy wychodzą"
    echo ""

    apply_config $LICZBA_GRUP $CZAS_ZAMKNIECIA $VIP_CHANCE $ROZMIAR_GRUPY $DELAY_MIN $DELAY_MAX
    compile_project && run_simulation
}

restore_defaults() {
    # -------------------------------------------------------------------------
    # Przywraca domyślne wartości w plikach źródłowych
    # -------------------------------------------------------------------------

    print_header "Przywracanie domyślnych wartości"

    apply_config $DEFAULT_LICZBA_GRUP $DEFAULT_CZAS_ZAMKNIECIA $DEFAULT_VIP_CHANCE 0 $DEFAULT_DELAY_MIN $DEFAULT_DELAY_MAX

    echo -e "${GREEN}Domyślne wartości przywrócone.${NC}"
    echo ""
    echo "Obecna konfiguracja:"
    echo "  - LICZBA_GRUP:      $DEFAULT_LICZBA_GRUP"
    echo "  - CZAS_ZAMKNIECIA:  $DEFAULT_CZAS_ZAMKNIECIA"
    echo "  - VIP_CHANCE:       $DEFAULT_VIP_CHANCE%"
    echo "  - ROZMIAR_GRUPY:    losowy (1-4)"
    echo "  - DELAY:            $DEFAULT_DELAY_MIN-$DEFAULT_DELAY_MAX sekund"
    echo ""
}

# =============================================================================
#                              MAIN
# =============================================================================

# Sprawdź czy jesteśmy w dobrym katalogu
if [ ! -f "main.c" ] || [ ! -f "wspolne.h" ]; then
    echo -e "${RED}BŁĄD: Nie znaleziono plików źródłowych!${NC}"
    echo "Upewnij się, że uruchamiasz skrypt z katalogu projektu."
    exit 1
fi

# Parsuj argumenty
case "$1" in
    1) run_test_1 ;;
    2) run_test_2 ;;
    3) run_test_3 ;;
    4) run_test_4 ;;
    5) run_test_5 ;;
    6) run_test_6 ;;
    7) run_test_7 ;;
    list|--list|-l)
        show_test_list
        ;;
    restore|--restore|-r)
        restore_defaults
        ;;
    *)
        echo -e "${RED}Nieznany test: $1${NC}"
        echo ""
        show_test_list
        exit 1
        ;;
esac
