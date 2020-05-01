
BIN:=patoggle
CFLAGS:=-Wall $(shell pkg-config --cflags libpulse)
LDFLAGS:=$(shell pkg-config --libs libpulse)

all: $(BIN)

install: all
	mkdir -p $(PREFIX)/bin
	cp -f $(BIN) $(PREFIX)/bin/$(BIN)
	chmod 755 $(PREFIX)/bin/$(BIN)

clean:
	$(RM) $(BIN)

.PHONY: all
