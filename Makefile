.POSIX:
.PHONY: all clean splitters

CFLAGS := -Wall $(shell pkg-config --cflags vtk) $(shell pkg-config --cflags cairo-xlib) -D_POSIX_C_SOURCE=200809L
LDFLAGS := $(shell pkg-config --libs vtk) -lpthread

SPLITTER_FLAGS := -D_POSIX_C_SOURCE=200809L

HDRS := $(wildcard *.h)

all: adrift splitters

clean:
	rm -f adrift *.o splitters/sar_split

splitters: splitters/sar_split

adrift: main.o draw.o common.o io.o calc.o timer.o config.o
	$(CC) -o $@ $^ $(LDFLAGS)

splitters/%: splitters/%.c
	$(CC) -o $@ $^ $(SPLITTER_FLAGS)
