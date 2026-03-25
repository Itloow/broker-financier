CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread

all: broker client

broker: broker.c
	$(CC) $(CFLAGS) broker.c -o broker $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f broker client

.PHONY: all clean