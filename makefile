
# Compiler
CC = gcc

# Compiler flags
CFLAGS = -g -D_DEFAULT_SOURCE -Wall  -pedantic -lrt
# Source files
SRCS = main.c file_transfer.c common.c

# Object files
OBJS = $(SRCS:.c=.o)

# Header files
HEADERS = file_transfer.h common.h

# Target executable
TARGET = hw5

# Default target
all: $(TARGET)

# Compile source files into object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Link object files into the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

valgrind: $(hw5)
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all --gen-suppressions=all -s ./hw5


# Clean up the generated files
clean:
	rm -f $(OBJS) $(TARGET)
