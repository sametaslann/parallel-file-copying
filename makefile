# Compiler
CC = gcc

# Compiler flags
CFLAGS = -D_DEFAULT_SOURCE -Wall  -pedantic -lrt -pthread -std=c11 -Iinclude
# Source files
SRCS = main.c src/file_transfer.c src/common.c

# Object files
OBJS = obj/common.o obj/file_transfer.o obj/main.o

# Header files
HEADERS = include/file_transfer.h include/common.h

# Target executable
TARGET = hw5

# Default target
all: $(TARGET)

# Compile source files into object files
obj/%.o: src/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

obj/main.o: main.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files into the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

valgrind: $(hw5)
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --gen-suppressions=all -s ./hw5


# Clean up the generated files
clean:
	rm -f $(OBJS) $(TARGET)
