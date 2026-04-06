CC = gcc
LIBS = -lcurl -lsqlite3
COMMON = cJSON.c CircularBuffer.c config.c db_transfer.c file_consumer.c
SERVER = server.c server_producer.c server_network_consumer.c
SERVEROUT = server.out
CLIENT = client.c client_producer.c
CLIENTOUT = client.out


server: $(COMMON) $(SERVER)
	$(CC) $(SERVER) $(COMMON) -o $(SERVEROUT) $(LIBS)

client: $(COMMON) $(CLIENT)
	$(CC) $(CLIENT) $(COMMON) -o $(CLIENTOUT) $(LIBS)