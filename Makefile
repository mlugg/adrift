.POSIX:
.PHONY: all clean

CFLAGS := -Wall -Werror $(shell pkg-config --cflags vtk) -D_POSIX_C_SOURCE=200809L
LDFLAGS := $(shell pkg-config --libs vtk) -lpthread

HDRS := $(wildcard *.h)

all: adrift

adrift: main.o draw.o common.o io.o calc.o timer.o
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f adrift *.o

%.o: %.c $(HDRS)
	$(CC) -c -o $@ $< $(CFLAGS)
