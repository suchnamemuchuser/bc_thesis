CC = gcc
LIBS = -lcurl -lsqlite3
COMMON = cJSON.c CircularBuffer.c config.c
SERVER = server.c db_transfer.c server_producer.c server_file_consumer.c server_network_consumer.c
SERVEROUT = server.out
CLIENT = client.c


server: $(COMMON) $(SERVER)
	$(CC) $(SERVER) $(COMMON) -o $(SERVEROUT) $(LIBS)