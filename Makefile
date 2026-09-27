CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude
LDLIBS = -lm
PREFIX ?= $(HOME)/.local

.PHONY: all clean test install uninstall
all: pikupiku
pikupiku: src/main.c src/renderer.c src/tui.c include/pikupiku.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ src/main.c src/renderer.c src/tui.c $(LDLIBS)
test: pikupiku
	python3 tests/test_renderer.py
install: pikupiku
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 755 pikupiku "$(DESTDIR)$(PREFIX)/bin/pikupiku"
uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/pikupiku"
clean:
	rm -f pikupiku

.PHONY: desktop-install
desktop-install: install
	install -d "$(DESTDIR)$(PREFIX)/share/applications"
	printf '%s\n' '[Desktop Entry]' 'Type=Application' 'Name=pikupiku' 'Comment=Create animated pencil contours from an image' 'Exec="$(PREFIX)/bin/pikupiku" --tui' 'Terminal=true' 'Categories=Graphics;' > "$(DESTDIR)$(PREFIX)/share/applications/pikupiku.desktop"
