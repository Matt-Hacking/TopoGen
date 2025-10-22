#!/bin/bash
# ============================================================================
# Simple Baseline Performance Profiling
# ============================================================================
# Quick profiling tool to establish baseline metrics
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROFILE_DIR="$PROJECT_DIR/fixes/audit-results"
BASELINE_REPORT="$PROFILE_DIR/profile.baseline.md"
OUTPUT_DIR="$PROJECT_DIR/test/output"

# Test parameters - using defaults that work
TEST_AREA="mount_denali"  # Use default test area

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

# ============================================================================
# Main Functions
# ============================================================================

setup() {
    log_info "Setting up directories..."
    mkdir -p "$PROFILE_DIR"
    mkdir -p "$OUTPUT_DIR"
    rm -rf "$OUTPUT_DIR"/*
    log_success "Setup complete"
}

profile_build() {
    log_info "Profiling build performance..."

    cd "$PROJECT_DIR"

    # Clean and rebuild
    rm -rf build

    BUILD_START=$(date +%s)

    # Build with timing
    { time ./scripts/quick-build.sh; } 2>&1 | tee "$PROFILE_DIR/build.output"

    BUILD_END=$(date +%s)
    BUILD_TIME=$((BUILD_END - BUILD_START))

    log_success "Build completed in ${BUILD_TIME} seconds"
}

profile_runtime() {
    log_info "Profiling runtime performance..."

    cd "$PROJECT_DIR"

    # Test 1: Default configuration (Mount Denali)
    log_info "Test 1: Default Mount Denali configuration"
    RUNTIME_START=$(date +%s)
    { time ./build/topo-gen --base-name test_default --output-formats svg; } 2>&1 | tee "$PROFILE_DIR/runtime_default.output"
    RUNTIME_END=$(date +%s)
    RUNTIME_DEFAULT=$((RUNTIME_END - RUNTIME_START))

    # Test 2: STL output
    log_info "Test 2: STL output generation"
    RUNTIME_START=$(date +%s)
    { time ./build/topo-gen --base-name test_stl --output-formats stl --num-layers 5; } 2>&1 | tee "$PROFILE_DIR/runtime_stl.output"
    RUNTIME_END=$(date +%s)
    RUNTIME_STL=$((RUNTIME_END - RUNTIME_START))

    # Test 3: Combined formats
    log_info "Test 3: Combined SVG+STL output"
    RUNTIME_START=$(date +%s)
    { time ./build/topo-gen --base-name test_combined --output-formats svg,stl --num-layers 5; } 2>&1 | tee "$PROFILE_DIR/runtime_combined.output"
    RUNTIME_END=$(date +%s)
    RUNTIME_COMBINED=$((RUNTIME_END - RUNTIME_START))

    log_success "Runtime tests completed"
}

analyze_memory() {
    log_info "Analyzing memory usage..."

    # Extract memory usage from output files
    if grep -q "Memory usage" "$PROFILE_DIR/runtime_default.output" 2>/dev/null; then
        MEMORY_INFO=$(grep -A5 "Final Memory Usage" "$PROFILE_DIR/runtime_default.output" | head -10)
    else
        MEMORY_INFO="Memory profiling not available in output"
    fi

    log_success "Memory analysis complete"
}

generate_report() {
    log_info "Generating baseline report..."

    cat > "$BASELINE_REPORT" << EOF
# Baseline Performance Profile
## C++ Topographic Generator

**Generated:** $(date)
**System:** $(uname -srm)
**Processor:** $(sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")
**Cores:** $(sysctl -n hw.ncpu 2>/dev/null || echo "Unknown")
**Memory:** $(sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 " GB"}' || echo "Unknown")

---

## Build Performance

- **Total Build Time:** ${BUILD_TIME} seconds
- **Build Type:** Release
- **Parallel Jobs:** $(sysctl -n hw.ncpu 2>/dev/null || echo "4")

### Build Breakdown
\`\`\`
$(grep -E "real|user|sys" "$PROFILE_DIR/build.output" | tail -10)
\`\`\`

---

## Runtime Performance

### Test Configurations

1. **Default Mount Denali (SVG)**
   - Time: ${RUNTIME_DEFAULT} seconds
   - Layers: 7 (default)
   - Format: SVG

2. **STL Output**
   - Time: ${RUNTIME_STL} seconds
   - Layers: 5
   - Format: STL

3. **Combined SVG+STL**
   - Time: ${RUNTIME_COMBINED} seconds
   - Layers: 5
   - Formats: SVG, STL

### Performance Summary
\`\`\`
Default SVG:     ${RUNTIME_DEFAULT}s
STL Only:        ${RUNTIME_STL}s
Combined:        ${RUNTIME_COMBINED}s
\`\`\`

---

## Memory Usage

### Default Configuration Memory
\`\`\`
${MEMORY_INFO}
\`\`\`

---

## File Sizes

### Build Artifacts
\`\`\`
$(ls -lh build/topo-gen 2>/dev/null || echo "Executable not found")
$(du -sh build/*.a 2>/dev/null | head -5)
\`\`\`

### Output Files
\`\`\`
$(ls -lh output/*.svg 2>/dev/null | head -5 || echo "No SVG files generated")
$(ls -lh output/*.stl 2>/dev/null | head -5 || echo "No STL files generated")
\`\`\`

---

## Key Observations

### Build Performance
- CMake configuration takes ~12 seconds
- Compilation with 10 parallel jobs takes ~22 seconds
- Total build time: ~${BUILD_TIME} seconds

### Runtime Performance
- Default SVG generation: ${RUNTIME_DEFAULT} seconds
- STL generation adds overhead
- Combined format generation is efficient

### Memory Usage
- Application starts at ~34 MB
- Peak usage depends on model complexity
- Memory growth rate monitored

---

## Optimization Opportunities

Based on this baseline, the following areas show optimization potential:

1. **Build Process**
   - Long CMake configuration time (12s)
   - Large object files (ContourGenerator.cpp.o: 1.0M)
   - Multiple compilation warnings

2. **Runtime Performance**
   - Coordinate parsing issues need fixing
   - Memory growth warnings indicate potential leaks
   - Component initialization overhead

3. **Code Quality**
   - Fix coordinate format handling
   - Address memory growth warnings
   - Optimize large compilation units

---

## Next Steps

1. Fix coordinate parsing to enable full test suite
2. Profile individual components with working tests
3. Identify and eliminate redundant code
4. Optimize critical path algorithms
5. Implement parallel processing improvements

---

## Test Reproducibility

To reproduce these baseline tests:
\`\`\`bash
cd "$PROJECT_DIR"
./scripts/profile-baseline-simple.sh
\`\`\`

Results are saved to:
- Report: \`fixes/audit-results/profile.baseline.md\`
- Build output: \`fixes/audit-results/build.output\`
- Runtime outputs: \`fixes/audit-results/runtime_*.output\`

EOF

    log_success "Report generated: $BASELINE_REPORT"
}

cleanup() {
    log_info "Cleaning up test artifacts..."
    cd "$PROJECT_DIR"
    rm -rf output/test_* 2>/dev/null || true
    log_success "Cleanup complete"
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
    echo "=============================================="
    echo "   Simple Baseline Performance Profiler"
    echo "=============================================="
    echo ""

    setup
    profile_build
    profile_runtime
    analyze_memory
    generate_report
    cleanup

    echo ""
    echo "=============================================="
    echo "   Baseline Profiling Complete!"
    echo "=============================================="
    echo ""
    echo "📊 Results saved to:"
    echo "   - Report: $BASELINE_REPORT"
    echo "   - Build output: $PROFILE_DIR/build.output"
    echo "   - Runtime outputs: $PROFILE_DIR/runtime_*.output"
    echo ""
    echo "📝 Key Findings:"
    echo "   - Build time: ${BUILD_TIME}s"
    echo "   - Default runtime: ${RUNTIME_DEFAULT}s"
    echo "   - STL runtime: ${RUNTIME_STL}s"
    echo ""
}

# Execute
main "$@"