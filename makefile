# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra

# Linker flags
LDFLAGS = -lSDL2 -lSDL2_ttf

# Source files
SRC = ./src/main.c

# Output executable
OUT = katc

# Default target
all: $(OUT)

# Build target
$(OUT): $(SRC)
	$(CC) $(SRC) -o $(OUT) $(CFLAGS) $(LDFLAGS)

# Clean target
clean:
	rm -f $(OUT)

# Phony targets
.PHONY: all clean
