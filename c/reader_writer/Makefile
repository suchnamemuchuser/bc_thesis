CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lpthread -lrt

TARGET = writer-reader.out
SRCS = main.c
OBJS = $(SRCS:.c=.o)
HEADERS =

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
