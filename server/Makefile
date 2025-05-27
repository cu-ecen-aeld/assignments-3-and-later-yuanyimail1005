CC=gcc
CFLAGS=-Wall -Werror
TARGET=aesdsocket

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) aesdsocket.c -o $(TARGET)

clean:
	rm -f $(TARGET)
