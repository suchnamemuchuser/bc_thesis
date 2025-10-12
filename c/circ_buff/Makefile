CC = gcc
CFLAGS = -Wall -Wextra
TARGET = test.out
SRC = main.c CircularBuffer.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)