src = $(wildcard *.c)
obj = $(src:.c=.o)

CFLAGS = -std=c11 -Wall -Wextra -pedantic -Wunused

bfm:	$(obj)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) bfm
