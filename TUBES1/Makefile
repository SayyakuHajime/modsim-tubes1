CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic
LDFLAGS = -lm
LATEX   = lualatex
TEXFLAGS = -interaction=nonstopmode -halt-on-error

all: tubes1

.PHONY: all run test pdf clean

tubes1: tubes1.c ../SIMLIB/simlib.c ../SIMLIB/simlib.h ../SIMLIB/simlibdefs.h
	$(CC) $(CFLAGS) tubes1.c ../SIMLIB/simlib.c $(LDFLAGS) -o tubes1

run: tubes1
	./tubes1

test: tubes1
	./tubes1
	bash test_input.sh
	bash test_features.sh

pdf: tubes1
	./tubes1
	$(LATEX) $(TEXFLAGS) tubes1_report.tex
	$(LATEX) $(TEXFLAGS) tubes1_report.tex

clean:
	rm -f tubes1 tubes1.out
