CC=c++
CFLAGS=-std=c++17 -Wall -Ievent_catbus
LDFLAGS=-lpthread

test:
	$(CC) -o test CatbusLib.cpp $< $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm test
