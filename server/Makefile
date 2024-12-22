CC=$(CROSS_COMPILE)gcc
CFLAGS=-Wall -Wextra -O2 -pthread -Iexamples/threading
TARGET=aesdsocket

all: $(TARGET)

$(TARGET): aesdsocket.o threading.o # Link both object files
	$(CC) $(CFLAGS) -o $(TARGET) aesdsocket.o threading.o -lrt

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

threading.o: ../examples/threading/threading.c
	$(CC) $(CFLAGS) -c ../examples/threading/threading.c

clean:
	rm -f $(TARGET)

.PHONY: all clean
