CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined

test: test_capu_logic
	./test_capu_logic

test_capu_logic: test_capu_logic.c ../capu_logic.c ../capu_logic.h
	$(CC) $(CFLAGS) -I.. test_capu_logic.c ../capu_logic.c -o $@

clean:
	rm -f test_capu_logic

.PHONY: test clean
