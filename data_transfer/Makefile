CC = gcc
LIBS = -lcurl -lsqlite3
COMMON = cJSON.c CircularBuffer.c config.c
SERVER = server.c db_transfer.c
SERVEROUT = server.out
CLIENT = client.c


server: $(COMMON) $(SERVER)
	$(CC) $(SERVER) $(COMMON) -o $(SERVEROUT) $(LIBS)