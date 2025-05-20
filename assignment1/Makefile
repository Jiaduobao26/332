# Makefile for IMS Project

CC = gcc
CFLAGS = -pthread -Wall
SRC_DIR = src
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
TARGETS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%, $(SOURCES))

all: $(BIN_DIR) $(TARGETS)

# Rule to compile each source file to binary
$(BIN_DIR)/%: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

# Ensure bin directory exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR)
