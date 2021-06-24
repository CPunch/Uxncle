CC=clang
CFLAGS=-fPIE -Wall -O2 -Isrc -std=c89
LDFLAGS=-lm #-fsanitize=address
OUT=bin/uxncle

CHDR=\
	src/umem.h\
	src/ulex.h\
	src/uparse.h\
	src/uasm.h\

CSRC=\
	src/umem.c\
	src/ulex.c\
	src/uparse.c\
	src/uasm.c\
	src/main.c

COBJ=$(CSRC:.c=.o)

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

$(OUT): $(COBJ) $(CHDR)
	mkdir -p bin
	$(CC) $(COBJ) $(LDFLAGS) -o $(OUT)

clean:
	rm -rf $(COBJ) $(OUT)