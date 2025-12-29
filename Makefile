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

# Files (exclude test files, standalone tools, and old matter system from main build)
SRCS = $(filter-out $(SRC_DIR)/test_%.c $(SRC_DIR)/terrain_tune.c $(SRC_DIR)/matter.c,$(wildcard $(SRC_DIR)/*.c))
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

# Specific rules for tests that need source file dependencies

# Matter system test (needs matter.c, noise.c)
$(BUILD_DIR)/test_matter_system: $(TEST_DIR)/test_matter_system.c $(SRC_DIR)/matter.c $(SRC_DIR)/noise.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Matter integration test (needs matter.c, noise.c)
$(BUILD_DIR)/test_matter_integration: $(TEST_DIR)/test_matter_integration.c $(SRC_DIR)/matter.c $(SRC_DIR)/noise.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Noise test (needs noise.c)
$(BUILD_DIR)/test_noise: $(TEST_DIR)/test_noise.c $(SRC_DIR)/noise.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Octree test (needs octree.c)
$(BUILD_DIR)/test_octree: $(TEST_DIR)/test_octree.c $(SRC_DIR)/octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Terrain test (needs terrain.c, tree.c)
$(BUILD_DIR)/test_terrain: $(TEST_DIR)/test_terrain.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Tree test (needs tree.c)
$(BUILD_DIR)/test_tree: $(TEST_DIR)/test_tree.c $(SRC_DIR)/tree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

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

# ============ MATTER SYSTEM TESTS ============
# Tiered testing: unit -> integration -> system

# Matter unit tests (isolated function testing)
test-matter-unit: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/test_matter_unit.c -o $(BUILD_DIR)/test_matter_unit $(LDFLAGS)
	./$(BUILD_DIR)/test_matter_unit

# Matter integration tests (grid simulation without engine)
test-matter-integration: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/test_matter_integration.c -o $(BUILD_DIR)/test_matter_integration $(LDFLAGS)
	./$(BUILD_DIR)/test_matter_integration

# Matter system tests (full engine with designed maps)
# Requires linking with actual matter.c and dependencies
MATTER_TEST_DEPS = $(SRC_DIR)/matter.c $(SRC_DIR)/noise.c
test-matter-system: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_DIR)/test_matter_system.c $(MATTER_TEST_DEPS) -o $(BUILD_DIR)/test_matter_system $(LDFLAGS)
	./$(BUILD_DIR)/test_matter_system

# Run all matter tests in order
test-matter: test-matter-unit test-matter-integration test-matter-system
	@echo ""
	@echo "All matter tests passed!"

# ============ CONSERVATION LAW TESTS ============
# Tests for mass and energy conservation

# Conservation tests (includes matter.c and noise.c directly)
$(BUILD_DIR)/test_conservation: $(TEST_DIR)/test_conservation.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-conservation: $(BUILD_DIR)/test_conservation
	./$(BUILD_DIR)/test_conservation

# Run ALL thermodynamics tests
test-thermo: test-matter test-physics
	@echo ""
	@echo "All thermodynamics tests passed!"

# ============ FLOW BEHAVIOR TESTS ============
# Tests for matter/water flow physics

$(BUILD_DIR)/test_flow_behavior: $(TEST_DIR)/test_flow_behavior.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-flow: $(BUILD_DIR)/test_flow_behavior
	./$(BUILD_DIR)/test_flow_behavior

# ============ PHASE TRANSITION TESTS ============
$(BUILD_DIR)/test_phase_transitions: $(TEST_DIR)/test_phase_transitions.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

test-phase: $(BUILD_DIR)/test_phase_transitions
	./$(BUILD_DIR)/test_phase_transitions

# ============ SVO UNIT TESTS ============
$(BUILD_DIR)/test_svo_unit: $(TEST_DIR)/test_svo_unit.c $(SRC_DIR)/svo.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-svo-unit: $(BUILD_DIR)/test_svo_unit
	./$(BUILD_DIR)/test_svo_unit

# ============ SVO MEMORY TESTS ============
$(BUILD_DIR)/test_svo_memory: $(TEST_DIR)/test_svo_memory.c $(SRC_DIR)/svo.c $(SRC_DIR)/svo_physics.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-svo-memory: $(BUILD_DIR)/test_svo_memory
	./$(BUILD_DIR)/test_svo_memory

# ============ SVO PHYSICS TESTS ============
$(BUILD_DIR)/test_svo_physics: $(TEST_DIR)/test_svo_physics.c $(SRC_DIR)/svo.c $(SRC_DIR)/svo_physics.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-svo-physics: $(BUILD_DIR)/test_svo_physics
	./$(BUILD_DIR)/test_svo_physics

# Run all SVO tests
test-svo: test-svo-unit test-svo-memory test-svo-physics
	@echo ""
	@echo "All SVO tests passed!"

# ============ TOOL INTEGRATION TESTS ============
$(BUILD_DIR)/test_tool_integration: $(TEST_DIR)/test_tool_integration.c $(SRC_DIR)/svo.c $(SRC_DIR)/svo_physics.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-tools: $(BUILD_DIR)/test_tool_integration
	./$(BUILD_DIR)/test_tool_integration

# ============ 3D PHYSICS TESTS ============
$(BUILD_DIR)/test_3d_physics: $(TEST_DIR)/test_3d_physics.c $(SRC_DIR)/svo.c $(SRC_DIR)/svo_physics.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-3d-physics: $(BUILD_DIR)/test_3d_physics
	./$(BUILD_DIR)/test_3d_physics

# ============ ENERGY CONSERVATION TESTS ============
$(BUILD_DIR)/test_energy_conservation: $(TEST_DIR)/test_energy_conservation.c $(SRC_DIR)/svo.c $(SRC_DIR)/svo_physics.c $(SRC_DIR)/terrain.c $(SRC_DIR)/tree.c $(SRC_DIR)/noise.c $(SRC_DIR)/octree.c $(SRC_DIR)/attractor_octree.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test-energy: $(BUILD_DIR)/test_energy_conservation
	./$(BUILD_DIR)/test_energy_conservation

# ============ ALL PHYSICS TESTS ============
test-physics: test-conservation test-flow test-phase
	@echo ""
	@echo "All physics tests passed!"

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

.PHONY: all run debug clean rebuild test test-growth test-unit test-matter test-matter-unit test-matter-integration test-matter-system test-thermo test-conservation test-flow test-phase test-physics test-tools tune-terrain tune-init tune-single tune-dry-run tune-clean
