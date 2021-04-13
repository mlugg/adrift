.POSIX:
.PHONY: all clean splitters

CFLAGS := -Wall -Werror $(shell pkg-config --cflags vtk) -D_POSIX_C_SOURCE=200809L
LDFLAGS := $(shell pkg-config --libs vtk) -lpthread

HDRS := $(wildcard *.h)

all: adrift splitters

clean:
	rm -f adrift *.o splitters/*.o splitters/sar_split

splitters: splitters/sar_split

adrift: main.o draw.o common.o io.o calc.o timer.o config.o
	$(CC) -o $@ $^ $(LDFLAGS)

splitters/sar_split: splitters/sar_split.o
	$(CC) -o $@ $^ $(LDFLAGS)
	sudo setcap cap_sys_ptrace=eip $@

%.o: %.c $(HDRS)
	$(CC) -c -o $@ $< $(CFLAGS)
