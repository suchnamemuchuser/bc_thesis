CC = gcc
CFLAGS = -Wall

TARGET = writer-reader

SRCS = main.c

OBJS = $(SRCS:.c=.o)

HEADERS = 

LIBS = -lpthread -lrt

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET).out $(OBJS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET) $(OBJS)