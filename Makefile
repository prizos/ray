# Ray - Game Client Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I include
LDFLAGS = -lraylib -lm

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # Homebrew paths for raylib
    RAYLIB_PATH := $(shell brew --prefix raylib 2>/dev/null)
    ifneq ($(RAYLIB_PATH),)
        CFLAGS += -I$(RAYLIB_PATH)/include
        LDFLAGS := -L$(RAYLIB_PATH)/lib $(LDFLAGS)
    endif
    LDFLAGS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lGL -lpthread -ldl -lrt -lX11
endif

# Directories
SRC_DIR = src
BUILD_DIR = build
INCLUDE_DIR = include
TEST_DIR = tests

# Files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TARGET = game

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%,$(TEST_SRCS))

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the game
run: all
	./$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Test targets
test: $(BUILD_DIR) $(TEST_TARGETS)
	@echo "Running tests..."
	@for test in $(TEST_TARGETS); do \
		echo ""; \
		./$$test || exit 1; \
	done
	@echo ""
	@echo "All tests passed!"

# Compile test files (no window required, just uses raymath)
$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild
rebuild: clean all

.PHONY: all run debug clean rebuild test
