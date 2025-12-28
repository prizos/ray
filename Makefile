# Ray - Game Client Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I include
LDFLAGS = -lraylib -lm

# Enable dependency generation for proper incremental builds
DEPFLAGS = -MMD -MP

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

# Files (exclude test files and standalone tools from main build)
SRCS = $(filter-out $(SRC_DIR)/test_%.c $(SRC_DIR)/terrain_tune.c,$(wildcard $(SRC_DIR)/*.c))
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)
TARGET = game

# Terrain tuning tool (needs tree.c for terrain.c dependencies)
TUNE_TARGET = terrain_tune
TUNE_SRCS = $(SRC_DIR)/terrain_tune.c $(SRC_DIR)/terrain.c $(SRC_DIR)/noise.c \
            $(SRC_DIR)/tree.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c

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

# Compile with dependency generation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

# Include generated dependency files (if they exist)
-include $(DEPS)

# Run the game
# Usage: make run [SEED=12345] [TREES=50]
SEED ?=
TREES ?=
RUN_ARGS := $(if $(SEED),-s $(SEED)) $(if $(TREES),-t $(TREES))

run: all
	./$(TARGET) $(RUN_ARGS)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: clean all

# Test targets
test: $(BUILD_DIR) $(TEST_TARGETS) test-growth
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

# Growth distribution test (uses tree module)
test-growth: $(BUILD_DIR)
	$(CC) $(CFLAGS) -DTEST_BUILD $(SRC_DIR)/test_growth.c $(SRC_DIR)/tree.c -o $(BUILD_DIR)/test_growth $(LDFLAGS)
	./$(BUILD_DIR)/test_growth

# Unit tests for modules
test-unit: $(BUILD_DIR)
	$(CC) $(CFLAGS) -DTEST_BUILD $(TEST_DIR)/test_tree.c $(SRC_DIR)/tree.c -o $(BUILD_DIR)/test_tree $(LDFLAGS)
	$(CC) $(CFLAGS) -DTEST_BUILD $(TEST_DIR)/test_terrain.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c -o $(BUILD_DIR)/test_terrain $(LDFLAGS)
	./$(BUILD_DIR)/test_tree
	./$(BUILD_DIR)/test_terrain

# Clean (removes build dir with all .o and .d files)
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TUNE_TARGET)

# Rebuild
rebuild: clean all

# ============ TERRAIN TUNING ============

# Build the terrain tuning tool
$(TUNE_TARGET): $(BUILD_DIR) $(TUNE_SRCS)
	$(CC) $(CFLAGS) $(TUNE_SRCS) -o $(TUNE_TARGET) $(LDFLAGS)

# Generate terrain preview images
# Usage: make tune-terrain [SEED=12345]
tune-terrain: $(TUNE_TARGET)
	@mkdir -p terrain_output
	./$(TUNE_TARGET) $(if $(SEED),-s $(SEED))

# Create template config file
tune-init: $(TUNE_TARGET)
	./$(TUNE_TARGET) --create-template

# Generate single center terrain (no variations)
tune-single: $(TUNE_TARGET)
	@mkdir -p terrain_output
	./$(TUNE_TARGET) --single $(if $(SEED),-s $(SEED))

# Show what would be generated
tune-dry-run: $(TUNE_TARGET)
	./$(TUNE_TARGET) --dry-run $(if $(SEED),-s $(SEED))

# Clean terrain tuning output
tune-clean:
	rm -rf terrain_output $(TUNE_TARGET)

.PHONY: all run debug clean rebuild test test-growth tune-terrain tune-init tune-single tune-dry-run tune-clean
