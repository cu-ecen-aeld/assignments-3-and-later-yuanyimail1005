CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror
LDFLAGS ?= -lpthread -lrt
TARGET = aesdsocket
SRC = aesdsocket.c
OBJ = $(SRC:.c=.o)
EXTRA_CFLAGS ?= -DUSE_AESD_CHAR_DEVICE=1

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(EXTRA_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(EXTRA_CFLAGS) $(CFLAGS) $(LDFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ)
