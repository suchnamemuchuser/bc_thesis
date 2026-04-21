CC = gcc
LIBS = -lcurl -lsqlite3
FTDI_LIB = -lftdi1
COMMON = cJSON.c CircularBuffer.c config.c db_transfer.c file_consumer.c
SERVER = server.c server_producer.c server_network_consumer.c
SERVEROUT = server.out
CLIENT = client.c client_producer.c
CLIENTOUT = client.out

DB_FILE = plan.db
DB_SCHEMA = telescope_db_schema.sql

all: server client

server: $(COMMON) $(SERVER) | $(DB_FILE)
	$(CC) $(SERVER) $(COMMON) -o $(SERVEROUT) $(LIBS) $(FTDI_LIB)

client: $(COMMON) $(CLIENT)
	$(CC) $(CLIENT) $(COMMON) -o $(CLIENTOUT) $(LIBS)

$(DB_FILE):
	sqlite3 $(DB_FILE) < $(DB_SCHEMA)

clean:
	rm -f $(SERVEROUT) $(CLIENTOUT)