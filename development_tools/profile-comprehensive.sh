#!/bin/bash
# ============================================================================
# Comprehensive Profiling Tool for C++ Topographic Generator
# ============================================================================
# Establishes baseline performance metrics for optimization strategy
# Author: Claude (Anthropic AI Assistant)
# Date: October 2025
# ============================================================================

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TEST_DIR="$PROJECT_DIR/test"
OUTPUT_DIR="$TEST_DIR/output"
PROFILE_DIR="$PROJECT_DIR/fixes/audit-results"
PROFILE_REPORT="$PROFILE_DIR/profile.before.md"
BUILD_LOG="$PROFILE_DIR/build-profile.log"
RUNTIME_LOG="$PROFILE_DIR/runtime-profile.log"

# Test parameters for consistent profiling
TEST_UPPER_LEFT="47.6062,-122.3321"
TEST_LOWER_RIGHT="47.6020,-122.3280"
TEST_NUM_LAYERS="10"
TEST_SUBSTRATE="200,200"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ============================================================================
# Utility Functions
# ============================================================================

log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

get_timestamp() {
    date "+%Y-%m-%d %H:%M:%S"
}

get_epoch_ms() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        echo $(($(date +%s) * 1000))
    else
        # Linux
        date +%s%3N
    fi
}

format_duration() {
    local duration=$1
    local hours=$((duration / 3600))
    local minutes=$(((duration % 3600) / 60))
    local seconds=$((duration % 60))
    printf "%02d:%02d:%02d" $hours $minutes $seconds
}

# ============================================================================
# Setup Functions
# ============================================================================

setup_directories() {
    log_info "Setting up directories..."
    mkdir -p "$PROFILE_DIR"
    mkdir -p "$OUTPUT_DIR"
    mkdir -p "$TEST_DIR/cache"

    # Clean test output directory
    rm -rf "$OUTPUT_DIR"/*

    log_success "Directories ready"
}

check_dependencies() {
    log_info "Checking dependencies..."

    local missing_deps=()

    # Check for time command (GNU time preferred)
    if command -v gtime &> /dev/null; then
        TIME_CMD="gtime"
    elif command -v time &> /dev/null; then
        TIME_CMD="time"
    else
        missing_deps+=("time")
    fi

    # Check for memory profiling tools
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS specific tools
        if ! command -v vm_stat &> /dev/null; then
            log_warning "vm_stat not found (should be available on macOS)"
        fi

        # Check for instruments (optional)
        if command -v xcrun &> /dev/null && xcrun instruments -s &> /dev/null; then
            INSTRUMENTS_AVAILABLE=1
        else
            log_warning "Xcode Instruments not available (optional)"
            INSTRUMENTS_AVAILABLE=0
        fi
    fi

    # Check for dtrace (optional but useful)
    if command -v dtrace &> /dev/null; then
        DTRACE_AVAILABLE=1
    else
        log_warning "dtrace not available (optional)"
        DTRACE_AVAILABLE=0
    fi

    if [ ${#missing_deps[@]} -gt 0 ]; then
        log_error "Missing required dependencies: ${missing_deps[*]}"
        exit 1
    fi

    log_success "Dependencies checked"
}

# ============================================================================
# Build Profiling Functions
# ============================================================================

profile_build() {
    log_info "Starting build profiling..."

    local build_start=$(get_epoch_ms)

    # Clean previous build
    cd "$PROJECT_DIR"
    if [ -d "build" ]; then
        log_info "Cleaning previous build..."
        rm -rf build
    fi

    # Create build log header
    cat > "$BUILD_LOG" << EOF
# Build Profile Log
Generated: $(get_timestamp)
System: $(uname -srm)
Processor: $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")
Cores: $(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo "Unknown")
Memory: $(sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 " GB"}' || echo "Unknown")

## Build Configuration
- Build Type: Release
- Parallel Jobs: $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4")

## Build Timeline
EOF

    # Profile CMake configuration
    log_info "Profiling CMake configuration..."
    local cmake_start=$(get_epoch_ms)

    mkdir -p build
    cd build

    if [[ "$TIME_CMD" == "gtime" ]]; then
        $TIME_CMD -v cmake -DCMAKE_BUILD_TYPE=Release .. >> "$BUILD_LOG" 2>&1
    else
        $TIME_CMD cmake -DCMAKE_BUILD_TYPE=Release .. >> "$BUILD_LOG" 2>&1
    fi

    local cmake_end=$(get_epoch_ms)
    local cmake_duration=$((cmake_end - cmake_start))

    echo "### CMake Configuration" >> "$BUILD_LOG"
    echo "- Start: $(date -r $((cmake_start/1000)) '+%H:%M:%S')" >> "$BUILD_LOG"
    echo "- End: $(date -r $((cmake_end/1000)) '+%H:%M:%S')" >> "$BUILD_LOG"
    echo "- Duration: ${cmake_duration}ms" >> "$BUILD_LOG"
    echo "" >> "$BUILD_LOG"

    # Profile compilation
    log_info "Profiling compilation..."
    local compile_start=$(get_epoch_ms)

    # Use time command with verbose output
    if [[ "$TIME_CMD" == "gtime" ]]; then
        $TIME_CMD -v make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4") >> "$BUILD_LOG" 2>&1
    else
        $TIME_CMD make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "4") >> "$BUILD_LOG" 2>&1
    fi

    local compile_end=$(get_epoch_ms)
    local compile_duration=$((compile_end - compile_start))

    echo "### Compilation" >> "$BUILD_LOG"
    echo "- Start: $(date -r $((compile_start/1000)) '+%H:%M:%S')" >> "$BUILD_LOG"
    echo "- End: $(date -r $((compile_end/1000)) '+%H:%M:%S')" >> "$BUILD_LOG"
    echo "- Duration: ${compile_duration}ms" >> "$BUILD_LOG"
    echo "" >> "$BUILD_LOG"

    # Analyze build artifacts
    echo "## Build Artifacts" >> "$BUILD_LOG"
    echo "### Object Files" >> "$BUILD_LOG"
    find . -name "*.o" -exec ls -lh {} \; | awk '{print $9 ": " $5}' >> "$BUILD_LOG"
    echo "" >> "$BUILD_LOG"

    echo "### Libraries" >> "$BUILD_LOG"
    find . -name "*.a" -exec ls -lh {} \; | awk '{print $9 ": " $5}' >> "$BUILD_LOG"
    echo "" >> "$BUILD_LOG"

    echo "### Executable" >> "$BUILD_LOG"
    ls -lh topo-gen 2>/dev/null >> "$BUILD_LOG" || echo "Executable not found!" >> "$BUILD_LOG"
    echo "" >> "$BUILD_LOG"

    local build_end=$(get_epoch_ms)
    local total_build_duration=$((build_end - build_start))

    echo "## Total Build Statistics" >> "$BUILD_LOG"
    echo "- Total Duration: ${total_build_duration}ms ($(format_duration $((total_build_duration/1000))))" >> "$BUILD_LOG"
    echo "- CMake: ${cmake_duration}ms ($(echo "scale=1; $cmake_duration * 100 / $total_build_duration" | bc)%)" >> "$BUILD_LOG"
    echo "- Compilation: ${compile_duration}ms ($(echo "scale=1; $compile_duration * 100 / $total_build_duration" | bc)%)" >> "$BUILD_LOG"

    cd "$PROJECT_DIR"
    log_success "Build profiling complete"

    return 0
}

# ============================================================================
# Runtime Profiling Functions
# ============================================================================

profile_runtime_format() {
    local format=$1
    local format_name=$2
    local output_base="test_${format//,/_}"

    log_info "Profiling $format_name output generation..."

    # Clear output directory for this test
    rm -rf "$OUTPUT_DIR/${output_base}"*

    # Prepare command based on format
    local cmd="./build/topo-gen"
    cmd="$cmd --upper-left $TEST_UPPER_LEFT"
    cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
    cmd="$cmd --num-layers $TEST_NUM_LAYERS"
    cmd="$cmd --substrate-size $TEST_SUBSTRATE"
    cmd="$cmd --base-name $output_base"
    cmd="$cmd --output-layers"
    cmd="$cmd --output-stacked"
    cmd="$cmd --output-formats $format"

    echo "### $format_name Format" >> "$RUNTIME_LOG"
    echo "Command: $cmd" >> "$RUNTIME_LOG"
    echo "" >> "$RUNTIME_LOG"

    local start_time=$(get_epoch_ms)

    # Run with memory tracking on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # Get initial memory stats
        local mem_before=$(vm_stat | grep "Pages free" | awk '{print $3}' | sed 's/\.//')

        # Run the command with time
        if [[ "$TIME_CMD" == "gtime" ]]; then
            $TIME_CMD -v $cmd >> "$RUNTIME_LOG" 2>&1
        else
            $TIME_CMD $cmd >> "$RUNTIME_LOG" 2>&1
        fi
        local exit_code=$?

        # Get final memory stats
        local mem_after=$(vm_stat | grep "Pages free" | awk '{print $3}' | sed 's/\.//')
        local mem_used=$((mem_before - mem_after))
        echo "Memory pages used: $mem_used" >> "$RUNTIME_LOG"
    else
        # Linux or other
        if [[ "$TIME_CMD" == "gtime" ]]; then
            $TIME_CMD -v $cmd >> "$RUNTIME_LOG" 2>&1
        else
            $TIME_CMD $cmd >> "$RUNTIME_LOG" 2>&1
        fi
        local exit_code=$?
    fi

    local end_time=$(get_epoch_ms)
    local duration=$((end_time - start_time))

    echo "Duration: ${duration}ms ($(format_duration $((duration/1000))))" >> "$RUNTIME_LOG"
    echo "Exit code: $exit_code" >> "$RUNTIME_LOG"

    # Check output files
    echo "Output files generated:" >> "$RUNTIME_LOG"
    ls -lh "${output_base}"* 2>/dev/null >> "$RUNTIME_LOG" || echo "No output files found" >> "$RUNTIME_LOG"
    echo "" >> "$RUNTIME_LOG"

    return $exit_code
}

profile_runtime() {
    log_info "Starting runtime profiling..."

    # Create runtime log header
    cat > "$RUNTIME_LOG" << EOF
# Runtime Profile Log
Generated: $(get_timestamp)

## Test Configuration
- Upper Left: $TEST_UPPER_LEFT
- Lower Right: $TEST_LOWER_RIGHT
- Layers: $TEST_NUM_LAYERS
- Substrate: $TEST_SUBSTRATE
- Cache Directory: $TEST_DIR/cache

## Runtime Performance

EOF

    # Check executable exists
    if [ ! -f "build/topo-gen" ]; then
        log_error "Executable not found. Building first..."
        profile_build
    fi

    # Profile each output format
    profile_runtime_format "svg" "SVG"
    profile_runtime_format "stl" "STL"
    profile_runtime_format "svg,stl" "SVG+STL"

    log_success "Runtime profiling complete"
}

# ============================================================================
# Component Profiling Functions
# ============================================================================

profile_components() {
    log_info "Profiling individual components..."

    # This would require instrumenting the code with timing points
    # For now, we'll parse the debug output if available

    echo "## Component Analysis" >> "$RUNTIME_LOG"
    echo "" >> "$RUNTIME_LOG"

    # Check if debug output contains component timings
    if grep -q "ElevationProcessor" "$RUNTIME_LOG" 2>/dev/null; then
        echo "### Component Timings (from debug output)" >> "$RUNTIME_LOG"
        grep -E "(ElevationProcessor|ContourGenerator|TrianglePlane|Export|OSM|SRTM)" "$RUNTIME_LOG" | tail -20 >> "$RUNTIME_LOG"
    else
        echo "No component timing information available in output." >> "$RUNTIME_LOG"
        echo "Consider enabling debug logging for detailed component analysis." >> "$RUNTIME_LOG"
    fi
    echo "" >> "$RUNTIME_LOG"
}

# ============================================================================
# Report Generation Functions
# ============================================================================

generate_report() {
    log_info "Generating comprehensive profile report..."

    cat > "$PROFILE_REPORT" << 'EOF'
# Comprehensive Performance Profile - Baseline
## C++ Topographic Generator Optimization Strategy

**Generated:** TIMESTAMP
**System:** SYSTEM_INFO
**Purpose:** Establish baseline metrics for optimization strategy

---

## Executive Summary

This report establishes baseline performance metrics for the C++ topographic generator before optimization. These metrics will be used to measure the effectiveness of each optimization stage.

### Key Metrics
- **Total Build Time:** BUILD_TIME
- **SVG Generation:** SVG_TIME
- **STL Generation:** STL_TIME
- **ZIP-SVG Generation:** ZIPSVG_TIME
- **Peak Memory Usage:** PEAK_MEMORY

---

## Build Performance Analysis

### Build Timeline
BUILD_TIMELINE

### Build Bottlenecks
BUILD_BOTTLENECKS

### Compilation Statistics
COMPILATION_STATS

---

## Runtime Performance Analysis

### Test Configuration
- **Test Area:** Seattle Space Needle vicinity
- **Coordinates:** Upper-left (47.6062, -122.3321) to Lower-right (47.6020, -122.3280)
- **Layers:** 10
- **Substrate Size:** 200mm x 200mm

### Performance by Output Format

#### SVG Output
SVG_PERFORMANCE

#### STL Output
STL_PERFORMANCE

#### ZIP-SVG Output
ZIPSVG_PERFORMANCE

### Component Breakdown
COMPONENT_ANALYSIS

---

## Memory Usage Analysis

### Peak Memory Usage
MEMORY_PEAK

### Memory Allocation Patterns
MEMORY_PATTERNS

---

## I/O Performance

### File Operations
FILE_OPS

### Network Operations (SRTM Downloads)
NETWORK_OPS

---

## Threading and Parallelization

### Thread Utilization
THREAD_UTIL

### Parallel Efficiency
PARALLEL_EFF

---

## Optimization Opportunities Identified

Based on this baseline profiling, the following areas show potential for optimization:

1. **Build Process**
   BUILD_OPPORTUNITIES

2. **Runtime Performance**
   RUNTIME_OPPORTUNITIES

3. **Memory Management**
   MEMORY_OPPORTUNITIES

4. **I/O Operations**
   IO_OPPORTUNITIES

5. **Parallelization**
   PARALLEL_OPPORTUNITIES

---

## Profiling Methodology

### Tools Used
- **Build Profiling:** GNU time, make verbose output
- **Runtime Profiling:** time command, vm_stat (macOS)
- **Memory Analysis:** vm_stat, process monitoring
- **Component Analysis:** Debug output parsing

### Test Reproducibility
To reproduce these tests, run:
```bash
./scripts/profile-comprehensive.sh
```

### Data Collection Process
1. Clean build from scratch
2. Full compilation with timing
3. Runtime tests for each output format
4. Memory monitoring during execution
5. Component timing extraction from logs

---

## Next Steps

### Optimization Stages
1. **Stage 1:** Library and Open Source Integration
   - Profile after: `profile.stage1.md`

2. **Stage 2:** Code Redundancy Elimination
   - Profile after: `profile.stage2.md`

3. **Stage 3:** Logic and Algorithm Optimization
   - Profile after: `profile.stage3.md`

4. **Stage 4:** Memory and Cache Optimization
   - Profile after: `profile.stage4.md`

5. **Stage 5:** Parallelization Enhancement
   - Profile after: `profile.stage5.md`

### Performance Targets
- **Build Time:** Reduce by 30%
- **Runtime:** Improve by 50%
- **Memory Usage:** Reduce peak by 40%
- **Parallel Efficiency:** Achieve 80%+ on 8 cores

---

## Appendices

### A. Raw Build Log
See: `fixes/audit-results/build-profile.log`

### B. Raw Runtime Log
See: `fixes/audit-results/runtime-profile.log`

### C. Test Commands
TEST_COMMANDS

### D. System Configuration
SYSTEM_CONFIG

---

**Note:** This baseline profile will be compared against future optimization profiles to measure improvement. Each optimization stage should generate a new profile report for comparison.
EOF

    # Now fill in the template with actual data
    local timestamp=$(get_timestamp)
    local system_info="$(uname -srm)"

    # Extract metrics from logs
    local build_time=$(grep "Total Duration" "$BUILD_LOG" 2>/dev/null | cut -d: -f2 | xargs || echo "N/A")
    local svg_time=$(grep -A5 "SVG Format" "$RUNTIME_LOG" 2>/dev/null | grep "Duration:" | cut -d: -f2 | xargs || echo "N/A")
    local stl_time=$(grep -A5 "STL Format" "$RUNTIME_LOG" 2>/dev/null | grep "Duration:" | cut -d: -f2 | xargs || echo "N/A")
    local zipsvg_time=$(grep -A5 "ZIP-SVG Format" "$RUNTIME_LOG" 2>/dev/null | grep "Duration:" | cut -d: -f2 | xargs || echo "N/A")

    # Update the report with actual values
    sed -i '' "s/TIMESTAMP/$timestamp/g" "$PROFILE_REPORT"
    sed -i '' "s/SYSTEM_INFO/$system_info/g" "$PROFILE_REPORT"
    sed -i '' "s/BUILD_TIME/$build_time/g" "$PROFILE_REPORT"
    sed -i '' "s/SVG_TIME/$svg_time/g" "$PROFILE_REPORT"
    sed -i '' "s/STL_TIME/$stl_time/g" "$PROFILE_REPORT"
    sed -i '' "s/ZIPSVG_TIME/$zipsvg_time/g" "$PROFILE_REPORT"

    # Add sections from logs
    echo "" >> "$PROFILE_REPORT"
    echo "### Build Timeline Details" >> "$PROFILE_REPORT"
    grep -A10 "Build Timeline" "$BUILD_LOG" 2>/dev/null >> "$PROFILE_REPORT" || echo "No build timeline data available" >> "$PROFILE_REPORT"

    echo "" >> "$PROFILE_REPORT"
    echo "### Runtime Performance Details" >> "$PROFILE_REPORT"
    grep -A20 "Runtime Performance" "$RUNTIME_LOG" 2>/dev/null >> "$PROFILE_REPORT" || echo "No runtime performance data available" >> "$PROFILE_REPORT"

    log_success "Profile report generated: $PROFILE_REPORT"
}

# ============================================================================
# Cleanup Functions
# ============================================================================

cleanup() {
    log_info "Cleaning up test artifacts..."

    # Clean test output but keep cache
    rm -rf "$OUTPUT_DIR"/*

    log_success "Cleanup complete"
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
    echo "=============================================="
    echo "   Comprehensive Performance Profiler"
    echo "   C++ Topographic Generator"
    echo "=============================================="
    echo ""

    # Setup
    setup_directories
    check_dependencies

    # Profiling
    profile_build
    profile_runtime
    profile_components

    # Report generation
    generate_report

    # Cleanup
    cleanup

    echo ""
    echo "=============================================="
    echo "   Profiling Complete!"
    echo "=============================================="
    echo ""
    echo "📊 Results saved to:"
    echo "   - Profile Report: $PROFILE_REPORT"
    echo "   - Build Log: $BUILD_LOG"
    echo "   - Runtime Log: $RUNTIME_LOG"
    echo ""
    echo "📝 Next steps:"
    echo "   1. Review the baseline metrics in $PROFILE_REPORT"
    echo "   2. Begin optimization implementation"
    echo "   3. Re-run profiling after each optimization stage"
    echo "   4. Compare results to measure improvement"
    echo ""

    return 0
}

# Execute main function
main "$@"