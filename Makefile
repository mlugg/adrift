.PHONY: all clean splitters

SDIR := src
ODIR := obj

CFLAGS := -Wall -Werror $(shell pkg-config --cflags vtk) -D_POSIX_C_SOURCE=200809L
LDFLAGS := $(shell pkg-config --libs vtk) -lpthread

SPLITTER_FLAGS := -D_POSIX_C_SOURCE=200809L

SRCS := $(wildcard $(SDIR)/*.c)
HDRS := $(wildcard $(SDIR)/*.h)
OBJS = $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(SRCS))

all: adrift splitters

clean:
	rm -rf adrift $(ODIR) splitters/sar_split

splitters: splitters/sar_split

adrift: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c
	@mkdir -p $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

splitters/%: splitters/%.c
	$(CC) -o $@ $^ $(SPLITTER_FLAGS)
