CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lpthread

TARGET = writer-reader.out
SRCS = main.c CircularBuffer.c
OBJS = $(SRCS:.c=.o)
HEADERS = CircularBuffer.h

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
