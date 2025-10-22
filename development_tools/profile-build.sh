#!/bin/bash
# ============================================================================
# Build Performance Profiler
# ============================================================================
# Measures build performance and identifies bottlenecks

set -e

BUILD_DIR="build-profile"
RESULTS_FILE="build-profile-results.txt"

echo "📊 Build Performance Profiler"
echo "=============================="
echo ""

# Clean start
echo "🧹 Cleaning previous build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Profile CMake configuration
echo "⚙️  Profiling CMake configuration..."
echo "=== CMake Configuration ===" > "../$RESULTS_FILE"
echo "Start time: $(date)" >> "../$RESULTS_FILE"

time_output=$({ time /opt/homebrew/Cellar/cmake/4.1.1/bin/cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -G "Unix Makefiles" \
    .. ; } 2>&1)

echo "CMake configuration completed at: $(date)" >> "../$RESULTS_FILE"
echo "Time output: $time_output" >> "../$RESULTS_FILE"
echo "" >> "../$RESULTS_FILE"

# Profile compilation
echo ""
echo "🔨 Profiling compilation..."
echo "=== Compilation Phase ===" >> "../$RESULTS_FILE"
echo "Start time: $(date)" >> "../$RESULTS_FILE"

compile_output=$({ time make -j4 VERBOSE=1 ; } 2>&1)

echo "Compilation completed at: $(date)" >> "../$RESULTS_FILE"
echo "Time output: $compile_output" >> "../$RESULTS_FILE"
echo "" >> "../$RESULTS_FILE"

# Analysis
echo ""
echo "📈 Performance Analysis"
echo "======================"

# Check for downloaded dependencies
echo "=== Dependency Analysis ===" >> "../$RESULTS_FILE"
if [ -d "_deps" ]; then
    echo "Dependencies downloaded:" >> "../$RESULTS_FILE"
    du -sh _deps/* >> "../$RESULTS_FILE" 2>/dev/null || echo "No _deps found" >> "../$RESULTS_FILE"
else
    echo "✅ No FetchContent downloads - using vendored dependencies!" >> "../$RESULTS_FILE"
fi

# Build artifacts size
echo "" >> "../$RESULTS_FILE"
echo "Build artifacts:" >> "../$RESULTS_FILE"
find . -name "*.o" -o -name "*.a" -o -name "topo*" | head -10 >> "../$RESULTS_FILE"

cd ..
echo ""
echo "📄 Results saved to: $RESULTS_FILE"
echo "📊 Review the file for detailed timing analysis"