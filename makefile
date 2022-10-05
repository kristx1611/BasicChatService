CFLAGS = -g -std=gnu11 -Wall -Wextra
SERVER = upush_server.o send_packet.o
CLIENT = upush_client.o send_packet.o
BIN = upush_server upush_client

all: $(BIN)

upush_client: $(CLIENT)
	gcc $(CFLAGS) $(CLIENT) -o upush_client

upush_client.o: upush_client.c
	gcc $(CFLAGS) -c upush_client.c

send_packet.o:
	gcc $(CFLAGS) -c send_packet.c -o send_packet.o

upush_server: $(SERVER)
	gcc $(CFLAGS) $(SERVER) -o upush_server

upush_server.o: upush_server.c
	gcc $(CFLAGS) -c upush_server.c

clean:
	rm $(BIN)
	rm send_packet.o
	rm upush_server.o
	rm upush_client.o
