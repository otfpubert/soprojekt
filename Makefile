

# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -g

PROGRAMS = main klient obsluga kucharz tasma kierownik

all: $(PROGRAMS)
	@echo ""
	@echo "Kompilacja zakończona. Dostępne programy:"
	@echo "  ./main       - główny program (uruchamia symulację)"
	@echo "  ./kierownik  - panel kierownika (w osobnym terminalu)"
	@echo ""

# Kompilacja poszczególnych programów
main: main.c wspolne.h
	$(CC) $(CFLAGS) -o main main.c

klient: klient.c wspolne.h
	$(CC) $(CFLAGS) -o klient klient.c

obsluga: obsluga.c wspolne.h
	$(CC) $(CFLAGS) -o obsluga obsluga.c

kucharz: kucharz.c wspolne.h
	$(CC) $(CFLAGS) -o kucharz kucharz.c

tasma: tasma.c wspolne.h
	$(CC) $(CFLAGS) -o tasma tasma.c

kierownik: kierownik.c wspolne.h
	$(CC) $(CFLAGS) -o kierownik kierownik.c


clean:
	rm -f $(PROGRAMS)
	rm -f *.log
	@echo "Wyczyszczono pliki wykonywalne i logi."

# Usuwa wszystko łącznie z raportami
cleanall: clean
	rm -f raport_*.txt
	@echo "Wyczyszczono również raporty."

# Kompiluje i uruchamia
run: all
	@echo "Uruchamianie symulacji..."
	@echo "(Ctrl+C aby przerwać)"
	@echo ""
	./main

# Tworzy plik klucza IPC jeśli nie istnieje
init:
	@touch ipc_keyfile
	@echo "Utworzono plik ipc_keyfile"


.PHONY: all clean cleanall run init
