CFLAGS = -Isrc -g -O3 -Wall -Wextra -Wno-unused-parameter $(shell pkg-config --cflags libxml-2.0)
LDFLAGS = $(shell pkg-config --libs libxml-2.0) -lm
PREFIX = /usr/local

LIBRARY_SRC=\
	src/EncoderRDS.o

# Library
%.o: %.cc
	$(CXX) $(CFLAGS) -c -o $@ $<

# Utilities
rdsencode: bin/rdsencode.cc $(LIBRARY_SRC)
	$(CXX) $^ $(CFLAGS) $(LDFLAGS) -o $@

# Housekeeping
clean:
	$(RM) src/*.o
	$(RM) rdsencode
install:
	install -m755 -d $(DESTDIR)$(PREFIX)/bin
	install -m755 jackpifm $(DESTDIR)$(PREFIX)/bin

all: rdsencode
