#!/bin/bash
# ============================================================================
# Memory Profiling Tool for C++ Topographic Generator
# ============================================================================
# Tracks memory usage patterns, allocations, and potential leaks
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROFILE_DIR="$PROJECT_DIR/fixes/audit-results"
MEMORY_LOG="$PROFILE_DIR/memory-profile.log"
EXECUTABLE="$PROJECT_DIR/build/topo-gen"
TEST_OUTPUT="/tmp/memory_profile_test"

# Test parameters (small for memory analysis)
TEST_UPPER_LEFT="47.6062,-122.3321"
TEST_LOWER_RIGHT="47.6040,-122.3300"
TEST_NUM_LAYERS="5"
TEST_SUBSTRATE="100,100"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# ============================================================================
# Memory Monitoring Functions
# ============================================================================

monitor_memory_macos() {
    local pid=$1
    local output_file=$2

    echo "Time,RSS(KB),VSZ(KB),CPU%" > "$output_file"

    while kill -0 $pid 2>/dev/null; do
        # Get memory info using ps
        local mem_info=$(ps -o rss,vsz,%cpu -p $pid | tail -1)
        local timestamp=$(date +%s)
        echo "$timestamp,$mem_info" | tr -s ' ' ',' >> "$output_file"
        sleep 0.5
    done
}

profile_with_leaks() {
    log_info "Running with macOS leaks tool..."

    local cmd="$EXECUTABLE"
    cmd="$cmd --upper-left $TEST_UPPER_LEFT"
    cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
    cmd="$cmd --num-layers $TEST_NUM_LAYERS"
    cmd="$cmd --substrate-size $TEST_SUBSTRATE"
    cmd="$cmd --output-base $TEST_OUTPUT"
    cmd="$cmd --output-format svg"

    # Enable malloc debugging
    export MallocStackLogging=1
    export MallocScribble=1
    export MallocPreScribble=1
    export MallocGuardEdges=1

    log_info "Starting process with leak detection..."

    # Run the process
    $cmd &
    local pid=$!

    log_info "Process PID: $pid"
    log_info "Monitoring memory usage..."

    # Start memory monitoring
    monitor_memory_macos $pid "$PROFILE_DIR/memory-timeline.csv" &
    local monitor_pid=$!

    # Wait for process to complete
    wait $pid
    local exit_code=$?

    # Stop monitoring
    kill $monitor_pid 2>/dev/null || true

    # Run leaks check
    log_info "Checking for memory leaks..."
    leaks $pid > "$PROFILE_DIR/leaks-report.txt" 2>&1 || true

    # Unset malloc debugging
    unset MallocStackLogging
    unset MallocScribble
    unset MallocPreScribble
    unset MallocGuardEdges

    return $exit_code
}

profile_with_vmmap() {
    log_info "Profiling with vmmap..."

    local cmd="$EXECUTABLE"
    cmd="$cmd --upper-left $TEST_UPPER_LEFT"
    cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
    cmd="$cmd --num-layers $TEST_NUM_LAYERS"
    cmd="$cmd --substrate-size $TEST_SUBSTRATE"
    cmd="$cmd --output-base $TEST_OUTPUT"
    cmd="$cmd --output-format stl"  # STL often uses more memory

    log_info "Starting process for vmmap analysis..."

    # Run in background
    $cmd &
    local pid=$!

    log_info "Process PID: $pid"

    # Capture vmmap at different stages
    sleep 1  # Let it start
    vmmap $pid > "$PROFILE_DIR/vmmap-start.txt" 2>&1 || true

    sleep 3  # Mid-execution
    vmmap $pid > "$PROFILE_DIR/vmmap-middle.txt" 2>&1 || true

    # Wait for completion
    wait $pid

    # Final vmmap (might not work if process ended)
    vmmap $pid > "$PROFILE_DIR/vmmap-end.txt" 2>&1 || true

    log_success "vmmap profiles saved"
}

profile_with_heap() {
    log_info "Profiling heap allocations..."

    local cmd="$EXECUTABLE"
    cmd="$cmd --upper-left $TEST_UPPER_LEFT"
    cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
    cmd="$cmd --num-layers $TEST_NUM_LAYERS"
    cmd="$cmd --substrate-size $TEST_SUBSTRATE"
    cmd="$cmd --output-base $TEST_OUTPUT"
    cmd="$cmd --output-format zipsvg"

    # Use heap command
    if command -v heap &> /dev/null; then
        log_info "Running with heap profiling..."

        # Enable malloc stack logging
        export MallocStackLogging=1

        $cmd &
        local pid=$!

        # Wait a bit then capture heap
        sleep 2
        heap $pid > "$PROFILE_DIR/heap-report.txt" 2>&1 || true

        wait $pid

        unset MallocStackLogging

        log_success "Heap profile saved"
    else
        log_warning "heap command not available"
    fi
}

analyze_memory_timeline() {
    log_info "Analyzing memory timeline..."

    if [ -f "$PROFILE_DIR/memory-timeline.csv" ]; then
        # Create analysis report
        cat > "$PROFILE_DIR/memory-analysis.txt" << 'EOF'
# Memory Usage Analysis

## Timeline Statistics
EOF

        # Use awk to analyze the CSV
        awk -F',' 'NR>1 {
            if (NR==2) { min_rss=$2; max_rss=$2; min_vsz=$3; max_vsz=$3; }
            if ($2 < min_rss) min_rss=$2;
            if ($2 > max_rss) max_rss=$2;
            if ($3 < min_vsz) min_vsz=$3;
            if ($3 > max_vsz) max_vsz=$3;
            sum_rss+=$2; sum_vsz+=$3; count++;
        }
        END {
            print "RSS Memory (KB):";
            print "  Min: " min_rss;
            print "  Max: " max_rss;
            print "  Avg: " sum_rss/count;
            print "  Growth: " (max_rss-min_rss);
            print "";
            print "Virtual Memory (KB):";
            print "  Min: " min_vsz;
            print "  Max: " max_vsz;
            print "  Avg: " sum_vsz/count;
            print "  Growth: " (max_vsz-min_vsz);
        }' "$PROFILE_DIR/memory-timeline.csv" >> "$PROFILE_DIR/memory-analysis.txt"

        log_success "Memory analysis complete"
    fi
}

# ============================================================================
# Generate Memory Report
# ============================================================================

generate_memory_report() {
    log_info "Generating memory profile report..."

    cat > "$MEMORY_LOG" << EOF
# Memory Profile Report
Generated: $(date)
System: $(uname -srm)
Memory: $(sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 " GB"}')

## Test Configuration
- Area: ($TEST_UPPER_LEFT) to ($TEST_LOWER_RIGHT)
- Layers: $TEST_NUM_LAYERS
- Substrate: $TEST_SUBSTRATE mm

## Memory Usage Summary

EOF

    # Add timeline analysis
    if [ -f "$PROFILE_DIR/memory-analysis.txt" ]; then
        cat "$PROFILE_DIR/memory-analysis.txt" >> "$MEMORY_LOG"
        echo "" >> "$MEMORY_LOG"
    fi

    # Add leak detection results
    echo "## Leak Detection Results" >> "$MEMORY_LOG"
    if [ -f "$PROFILE_DIR/leaks-report.txt" ]; then
        echo '```' >> "$MEMORY_LOG"
        tail -20 "$PROFILE_DIR/leaks-report.txt" >> "$MEMORY_LOG"
        echo '```' >> "$MEMORY_LOG"
    else
        echo "No leak detection data available" >> "$MEMORY_LOG"
    fi
    echo "" >> "$MEMORY_LOG"

    # Add vmmap summary
    echo "## Virtual Memory Map" >> "$MEMORY_LOG"
    if [ -f "$PROFILE_DIR/vmmap-middle.txt" ]; then
        echo "### Mid-execution vmmap summary:" >> "$MEMORY_LOG"
        echo '```' >> "$MEMORY_LOG"
        grep -A 10 "Summary" "$PROFILE_DIR/vmmap-middle.txt" 2>/dev/null >> "$MEMORY_LOG" || echo "No summary found" >> "$MEMORY_LOG"
        echo '```' >> "$MEMORY_LOG"
    else
        echo "No vmmap data available" >> "$MEMORY_LOG"
    fi
    echo "" >> "$MEMORY_LOG"

    # Add heap analysis
    echo "## Heap Analysis" >> "$MEMORY_LOG"
    if [ -f "$PROFILE_DIR/heap-report.txt" ]; then
        echo '```' >> "$MEMORY_LOG"
        head -30 "$PROFILE_DIR/heap-report.txt" >> "$MEMORY_LOG"
        echo '```' >> "$MEMORY_LOG"
    else
        echo "No heap analysis available" >> "$MEMORY_LOG"
    fi

    log_success "Memory report generated: $MEMORY_LOG"
}

# ============================================================================
# Cleanup
# ============================================================================

cleanup() {
    log_info "Cleaning up..."
    rm -rf $TEST_OUTPUT*
    rm -rf /tmp/memory_profile_test*
}

# ============================================================================
# Main
# ============================================================================

main() {
    echo "=============================================="
    echo "   Memory Profiler"
    echo "   C++ Topographic Generator"
    echo "=============================================="
    echo ""

    mkdir -p "$PROFILE_DIR"

    # Check executable
    if [ ! -f "$EXECUTABLE" ]; then
        log_warning "Executable not found. Build first with:"
        echo "  cd $PROJECT_DIR && ./scripts/quick-build.sh"
        exit 1
    fi

    # Run profiling
    profile_with_leaks
    profile_with_vmmap
    profile_with_heap

    # Analyze results
    analyze_memory_timeline

    # Generate report
    generate_memory_report

    # Cleanup
    cleanup

    echo ""
    echo "=============================================="
    echo "   Memory Profiling Complete!"
    echo "=============================================="
    echo ""
    echo "📊 Results saved to:"
    echo "   - Report: $MEMORY_LOG"
    echo "   - Timeline: $PROFILE_DIR/memory-timeline.csv"
    echo "   - Leaks: $PROFILE_DIR/leaks-report.txt"
    echo "   - VMMap: $PROFILE_DIR/vmmap-*.txt"
    echo "   - Heap: $PROFILE_DIR/heap-report.txt"
    echo ""

    return 0
}

# Execute
main "$@"