CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Wextra -Werror -O2
TARGET=aesdsocket

all: $(TARGET)

$(TARGET): aesdsocket.c
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
