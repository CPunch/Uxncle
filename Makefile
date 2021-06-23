CC=clang
CFLAGS=-fPIE -Wall -O3 -Isrc -std=c99
LDFLAGS=-lm #-fsanitize=address
OUT=bin/uxnscr

CHDR=\
	src/ulex.h\

CSRC=\
	src/ulex.c\
	src/main.c

COBJ=$(CSRC:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT): $(COBJ) $(CHDR)
	mkdir -p bin
	$(CC) $(COBJ) $(LDFLAGS) -o $(OUT)

clean:
	rm -rf $(COBJ) $(OUT)