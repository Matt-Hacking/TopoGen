#!/bin/bash
# ============================================================================
# Quick Build Script - Optimized for Development
# ============================================================================
# Uses vendored dependencies and intelligent incremental builds for maximum speed
#
# Usage:
#   ./quick-build.sh          # Debug build (default)
#   ./quick-build.sh -r       # Release build
#   ./quick-build.sh --release # Release build

#set -e

# Parse command line arguments
CMAKE_BUILD_TYPE="Debug"  # Default to Debug for development
while [[ $# -gt 0 ]]; do
    case $1 in
        -r|--release)
            CMAKE_BUILD_TYPE="Release"
            shift
            ;;
        -d|--debug)
            CMAKE_BUILD_TYPE="Debug"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-r|--release] [-d|--debug]"
            exit 1
            ;;
    esac
done

# Configuration
BUILD_DIR="build"
# Limit parallel jobs to prevent "Resource busy" errors on mounted volumes
MAX_JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
JOBS="${JOBS:-$((MAX_JOBS > 9 ? 9 : MAX_JOBS))}"

echo "🚀 Quick Build: Topographic Generator"
echo "   Build type: $CMAKE_BUILD_TYPE"
echo "   Jobs: $JOBS"
echo "   Build dir: $BUILD_DIR"
echo ""

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Smart CMake configuration - only run if needed
NEED_CMAKE=0

if [[ ! -f "Makefile" ]]; then
    NEED_CMAKE=1
    echo "🆕 No Makefile found, running CMake configure..."
elif [[ "../CMakeLists.txt" -nt "Makefile" ]]; then
    NEED_CMAKE=1
    echo "📝 CMakeLists.txt updated, running CMake configure..."
elif [[ ! -f "compile_commands.json" ]]; then
    NEED_CMAKE=1
    echo "🔧 Missing compile_commands.json, running CMake configure..."
else
    echo "✅ Build files up-to-date, skipping CMake configure"
fi

if [[ $NEED_CMAKE -eq 1 ]]; then
    echo "⚙️  Configuring with vendored dependencies..."
    time cmake \
        -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -G "Unix Makefiles" \
        ..
fi

# Intelligent incremental build using Make's dependency tracking
echo ""
echo "🔍 Let Make determine what needs to be built..."

# Use Make's dry-run to see what would be built
# Skip dependency check as it can unnecessarily update timestamps

# Check for header changes more precisely - only compared to executable time
# Only force rebuild if headers are actually newer than the executable
if [[ -f "topo-gen" ]]; then
    EXECUTABLE_TIME=$(stat -f %m "topo-gen" 2>/dev/null || echo 0)
    NEWER_HEADERS=$(find ../src -name "*.hpp" -newer "topo-gen" 2>/dev/null | wc -l | tr -d ' ')
    if [[ $NEWER_HEADERS -gt 0 ]]; then
        echo "🔄 Headers newer than executable detected, forcing targeted rebuild..."
        # Only touch .cpp files that include the changed headers, not all files
        find ../src -name "*.hpp" -newer "topo-gen" -exec basename {} \; 2>/dev/null | while read header; do
            grep -l "#include.*$header" ../src/**/*.cpp 2>/dev/null | xargs touch 2>/dev/null || true
        done
    else
        echo "✅ No header changes since last build"
    fi
else
    echo "🆕 No executable found, full build required"
fi

MAKE_DRY_RUN=$(make -n 2>/dev/null)

# Helper function to run make with retry logic for mounted volumes
run_make() {
    local jobs=$1
    local attempt=1
    local max_attempts=3

    while [ $attempt -le $max_attempts ]; do
        echo "🚀 Building with $jobs parallel jobs (attempt $attempt/$max_attempts)..."
        if time make -j"$jobs"; then
            return 0
        elif [ $attempt -lt $max_attempts ] && grep -q "Resource busy" <<< "$(make -j"$jobs" 2>&1)"; then
            echo "⚠️  Resource busy error detected, retrying with fewer jobs..."
            jobs=1  # Fall back to sequential build
            sleep 2
        else
            echo "❌ Build failed"
            exit 1
        fi
        attempt=$((attempt + 1))
    done
    return 1
}

# Check if Make has real work to do by looking for compilation/linking
if echo "$MAKE_DRY_RUN" | grep -q "Building CXX object"; then
    COMPILE_COUNT=$(echo "$MAKE_DRY_RUN" | grep "Building CXX object" | wc -l | tr -d ' ')
    echo "🔨 Incremental build: $COMPILE_COUNT source file(s) changed"
    run_make "$JOBS"
elif echo "$MAKE_DRY_RUN" | grep -q "Linking CXX executable"; then
    echo "🔗 Link-only build required (no source changes)"
    run_make "$JOBS"
elif echo "$MAKE_DRY_RUN" | grep -q "Built target" && ! echo "$MAKE_DRY_RUN" | grep -q "Building\|Linking"; then
    echo "✨ All targets up-to-date, no build needed!"
elif [[ -z "$MAKE_DRY_RUN" ]] || echo "$MAKE_DRY_RUN" | grep -q "Nothing to be done"; then
    echo "✨ Nothing to build - all targets up-to-date!"
else
    echo "🔧 Build required (Make detected changes)"
    echo "Debug: Make dry-run output:"
    echo "$MAKE_DRY_RUN" | head -10
    run_make "$JOBS"
fi

echo ""
echo "✅ Build complete! Executable ready for testing."
