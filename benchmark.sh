#!/bin/bash

# Benchmark runner for Tree Growth Simulator
# Runs multiple benchmark tests with varying tree counts

set -e

# Configuration
DURATION=${DURATION:-10}
TREE_COUNTS=${TREE_COUNTS:-"1 5 10 20 30 50"}
OUTPUT_DIR="benchmarks"
SUMMARY_FILE="$OUTPUT_DIR/summary.csv"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Tree Growth Simulator Benchmark Suite ===${NC}"
echo "Duration per test: ${DURATION}s"
echo "Tree counts: ${TREE_COUNTS}"
echo ""

# Build the game
echo -e "${YELLOW}Building game...${NC}"
make clean && make
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Create/reset summary file with header
echo "timestamp,trees,total_voxels,duration,samples,fps_min,fps_max,fps_avg,fps_p1,fps_p5,fps_p50,fps_p95,fps_p99" > "$SUMMARY_FILE"

# Run benchmarks
for trees in $TREE_COUNTS; do
    echo -e "${YELLOW}Running benchmark with ${trees} trees...${NC}"

    timestamp=$(date +%Y%m%d_%H%M%S)
    output_file="$OUTPUT_DIR/benchmark_${timestamp}_trees${trees}.txt"

    # Run the benchmark
    ./game -t "$trees" -b -d "$DURATION" -o "$output_file"

    # Parse results and add to summary
    if [ -f "$output_file" ]; then
        # Extract values from the output file
        trees_actual=$(grep "^trees=" "$output_file" | cut -d= -f2)
        total_voxels=$(grep "^total_voxels=" "$output_file" | cut -d= -f2)
        duration_actual=$(grep "^duration_seconds=" "$output_file" | cut -d= -f2)
        samples=$(grep "^sample_count=" "$output_file" | cut -d= -f2)
        fps_min=$(grep "^fps_min=" "$output_file" | cut -d= -f2)
        fps_max=$(grep "^fps_max=" "$output_file" | cut -d= -f2)
        fps_avg=$(grep "^fps_avg=" "$output_file" | cut -d= -f2)
        fps_p1=$(grep "^fps_p1=" "$output_file" | cut -d= -f2)
        fps_p5=$(grep "^fps_p5=" "$output_file" | cut -d= -f2)
        fps_p50=$(grep "^fps_p50=" "$output_file" | cut -d= -f2)
        fps_p95=$(grep "^fps_p95=" "$output_file" | cut -d= -f2)
        fps_p99=$(grep "^fps_p99=" "$output_file" | cut -d= -f2)

        # Append to summary
        echo "$timestamp,$trees_actual,$total_voxels,$duration_actual,$samples,$fps_min,$fps_max,$fps_avg,$fps_p1,$fps_p5,$fps_p50,$fps_p95,$fps_p99" >> "$SUMMARY_FILE"

        echo -e "${GREEN}  Avg FPS: ${fps_avg}, Voxels: ${total_voxels}${NC}"
    else
        echo -e "${RED}  Failed to generate output file${NC}"
    fi

    echo ""
done

# Print summary
echo -e "${GREEN}=== Benchmark Complete ===${NC}"
echo ""
echo "Summary saved to: $SUMMARY_FILE"
echo ""
echo "Results:"
echo "--------"
column -t -s',' "$SUMMARY_FILE" | head -20

echo ""
echo "Individual benchmark files saved in: $OUTPUT_DIR/"
ls -la "$OUTPUT_DIR"/*.txt 2>/dev/null || echo "No individual files found"
