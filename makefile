CC = gcc
CFLAGS = -Wall -Wextra -pthread
INCLUDES = -I./Librerie

# Targets
all: server client

server: server.c Librerie/Comunicazione.o Librerie/Strutture_server.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o server

client: client.c Librerie/Comunicazione.o
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o client

Librerie/%.o: Librerie/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f server client Librerie/*.o