CC := zig cc
CFLAGS := --std=c17 -O3 -Wall -Wextra
LDLIBS := -lncurses
#INCDIR := -Iinclude

target := lifegame

$(target): main.c
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@
	@ du -b $(target)

run: $(target)
	@ ./$(target)

all: clean $(target) run

clean:
	$(RM) $(target)
