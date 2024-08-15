# Variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror
TARGET = writer
SRC = writer.c
OBJS = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Compile the application
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -f $(TARGET) $(OBJS)

# Phony targets
.PHONY: all clean
