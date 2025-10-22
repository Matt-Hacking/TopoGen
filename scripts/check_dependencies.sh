#!/bin/bash

# ============================================================================
# Comprehensive Dependency Checker
# C++ Topographic Generator - Build Environment Validation
# ============================================================================
# Checks all required and optional dependencies with version validation
# Provides detailed status report and installation instructions

# Note: Do NOT use 'set -e' here - we want to continue checking all dependencies
# even if some are missing, so we can report everything at the end

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Status counters
REQUIRED_MISSING=0
OPTIONAL_MISSING=0
VERSION_WARNINGS=0

# Detect platform
PLATFORM="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
fi

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}C++ Topographic Generator - Dependency Check${NC}               ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "Platform: ${BLUE}$PLATFORM${NC} (${OSTYPE})"
echo ""

# ============================================================================
# Helper Functions
# ============================================================================

check_command() {
    local cmd=$1
    local version_flag=$2
    local required=$3
    local min_version=$4

    echo -n "  Checking ${cmd}... "

    if command -v "$cmd" &> /dev/null; then
        local cmd_path=$(command -v "$cmd")

        if [[ -n "$version_flag" ]]; then
            local version=$($cmd $version_flag 2>&1 | head -1)
            echo -e "${GREEN}✓ found${NC}"
            echo "    Location: $cmd_path"
            echo "    Version: $version"

            # Version checking (if min_version provided)
            if [[ -n "$min_version" ]]; then
                local current_version=$($cmd $version_flag 2>&1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -1)
                if [[ -n "$current_version" ]]; then
                    if version_compare "$current_version" "$min_version"; then
                        echo -e "    ${GREEN}Version OK${NC} (≥ $min_version)"
                    else
                        echo -e "    ${YELLOW}⚠ Version too old${NC} (need ≥ $min_version)"
                        VERSION_WARNINGS=$((VERSION_WARNINGS + 1))
                    fi
                fi
            fi
        else
            echo -e "${GREEN}✓ found${NC} at $cmd_path"
        fi
        return 0
    else
        if [[ "$required" == "true" ]]; then
            echo -e "${RED}✗ missing (REQUIRED)${NC}"
            REQUIRED_MISSING=$((REQUIRED_MISSING + 1))
        else
            echo -e "${YELLOW}- not found (optional)${NC}"
            OPTIONAL_MISSING=$((OPTIONAL_MISSING + 1))
        fi
        return 1
    fi
}

check_library() {
    local lib_name=$1
    local pkg_config_name=$2
    local required=$3
    local min_version=$4

    echo -n "  Checking ${lib_name}... "

    # Try pkg-config first
    if [[ -n "$pkg_config_name" ]] && command -v pkg-config &> /dev/null && pkg-config --exists "$pkg_config_name" 2>/dev/null; then
        local version=$(pkg-config --modversion "$pkg_config_name" 2>/dev/null)
        echo -e "${GREEN}✓ found${NC} (via pkg-config)"
        echo "    Version: $version"

        if [[ -n "$min_version" ]] && [[ -n "$version" ]]; then
            if version_compare "$version" "$min_version"; then
                echo -e "    ${GREEN}Version OK${NC} (≥ $min_version)"
            else
                echo -e "    ${YELLOW}⚠ Version too old${NC} (need ≥ $min_version)"
                VERSION_WARNINGS=$((VERSION_WARNINGS + 1))
            fi
        fi
        return 0
    fi

    # Fallback: search for library files
    local found=false
    local search_paths=("/usr/lib" "/usr/local/lib" "/opt/homebrew/lib")

    if [[ -n "$CONDA_PREFIX" ]]; then
        search_paths+=("$CONDA_PREFIX/lib")
    fi

    for path in "${search_paths[@]}"; do
        if [[ -d "$path" ]] && find "$path" -name "*${lib_name}*" -type f 2>/dev/null | grep -q .; then
            echo -e "${GREEN}✓ found${NC} at $path"
            found=true
            break
        fi
    done

    if [[ "$found" == "false" ]]; then
        if [[ "$required" == "true" ]]; then
            echo -e "${RED}✗ missing (REQUIRED)${NC}"
            REQUIRED_MISSING=$((REQUIRED_MISSING + 1))
        else
            echo -e "${YELLOW}- not found (optional)${NC}"
            OPTIONAL_MISSING=$((OPTIONAL_MISSING + 1))
        fi
        return 1
    fi

    return 0
}

check_header() {
    local header=$1
    local lib_name=$2
    local required=$3

    echo -n "  Checking ${lib_name} headers... "

    local search_paths=("/usr/include" "/usr/local/include" "/opt/homebrew/include")

    if [[ -n "$CONDA_PREFIX" ]]; then
        search_paths+=("$CONDA_PREFIX/include")
    fi

    for path in "${search_paths[@]}"; do
        if [[ -f "$path/$header" ]]; then
            echo -e "${GREEN}✓ found${NC} at $path/$header"
            return 0
        fi
    done

    if [[ "$required" == "true" ]]; then
        echo -e "${RED}✗ missing (REQUIRED)${NC}"
        REQUIRED_MISSING=$((REQUIRED_MISSING + 1))
    else
        echo -e "${YELLOW}- not found (optional)${NC}"
        OPTIONAL_MISSING=$((OPTIONAL_MISSING + 1))
    fi
    return 1
}

version_compare() {
    local version1=$1
    local version2=$2

    # Convert versions to comparable numbers (e.g., "3.4.1" -> 3004001)
    local v1=$(echo "$version1" | awk -F. '{ printf("%d%03d%03d\n", $1,$2,$3); }')
    local v2=$(echo "$version2" | awk -F. '{ printf("%d%03d%03d\n", $1,$2,$3); }')

    [[ "$v1" -ge "$v2" ]]
}

test_cpp20_support() {
    echo -n "  Testing C++20 support... "

    # mktemp syntax differs between Linux and macOS
    local test_file=$(mktemp /tmp/test_cpp20.XXXXXX)
    mv "$test_file" "${test_file}.cpp"
    test_file="${test_file}.cpp"
    cat > "$test_file" << 'EOF'
#include <concepts>
#include <ranges>
template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;
int main() { return 0; }
EOF

    local compiler=""
    if command -v g++ &> /dev/null; then
        compiler="g++"
    elif command -v clang++ &> /dev/null; then
        compiler="clang++"
    else
        echo -e "${RED}✗ no C++ compiler found${NC}"
        rm -f "$test_file"
        return 1
    fi

    if $compiler -std=c++20 -c "$test_file" -o /tmp/test_cpp20.o &> /dev/null; then
        echo -e "${GREEN}✓ working${NC} ($compiler)"
        rm -f "$test_file" /tmp/test_cpp20.o
        return 0
    else
        echo -e "${RED}✗ failed${NC} ($compiler cannot compile C++20 code)"
        rm -f "$test_file"
        REQUIRED_MISSING=$((REQUIRED_MISSING + 1))
        return 1
    fi
}

# ============================================================================
# Core Build Tools
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}1. Core Build Tools${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

check_command "cmake" "--version" "true" "3.20"
check_command "make" "--version" "true"
check_command "pkg-config" "--version" "true"

if [[ "$PLATFORM" == "macos" ]]; then
    check_command "brew" "--version" "false"
fi

echo ""

# ============================================================================
# Compilers
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}2. C++ Compiler (C++20 support required)${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

check_command "g++" "--version" "false" "10.0"
check_command "clang++" "--version" "false"

test_cpp20_support

echo ""

# ============================================================================
# Required Libraries
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}3. Required Libraries${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# CGAL
check_header "CGAL/version.h" "CGAL" "true"
if command -v pkg-config &> /dev/null && pkg-config --exists CGAL 2>/dev/null; then
    CGAL_VERSION=$(pkg-config --modversion CGAL)
    echo "    CGAL version: $CGAL_VERSION"
fi

# Eigen3
check_header "eigen3/Eigen/Core" "Eigen3" "true"
if ! check_library "eigen3" "eigen3" "true" "3.4"; then
    check_header "Eigen/Core" "Eigen3 (alternate)" "true"
fi

# GDAL
check_library "libgdal" "gdal" "true" "3.0"
check_command "gdal-config" "--version" "true" "3.0"

# libcurl
check_library "libcurl" "libcurl" "true"

# OpenSSL
check_library "libssl" "openssl" "true"
check_library "libcrypto" "libcrypto" "true"

# zlib
check_library "libz" "zlib" "true"

echo ""

# ============================================================================
# Parallel Processing Libraries (Highly Recommended)
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}4. Parallel Processing (Highly Recommended)${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# Intel TBB
check_library "libtbb" "tbb" "false"

# OpenMP
echo -n "  Checking OpenMP... "
OPENMP_FOUND=false

if [[ "$PLATFORM" == "macos" ]]; then
    # macOS-specific OpenMP detection (libomp)
    if [[ -f "/opt/homebrew/lib/libomp.dylib" ]] && [[ -f "/opt/homebrew/include/omp.h" ]]; then
        echo -e "${GREEN}✓ found${NC} (libomp via Homebrew)"
        echo "    Library: /opt/homebrew/lib/libomp.dylib"
        echo "    Headers: /opt/homebrew/include/omp.h"
        OPENMP_FOUND=true
    elif find /opt/homebrew/Cellar/libomp -name "libomp.dylib" 2>/dev/null | grep -q .; then
        LIBOMP_PATH=$(find /opt/homebrew/Cellar/libomp -name "libomp.dylib" 2>/dev/null | head -1)
        echo -e "${GREEN}✓ found${NC} (libomp in Cellar)"
        echo "    Library: $LIBOMP_PATH"
        OPENMP_FOUND=true
    fi
elif command -v gcc &> /dev/null && gcc -fopenmp -E - </dev/null &>/dev/null; then
    echo -e "${GREEN}✓ found${NC} (GCC built-in)"
    OPENMP_FOUND=true
elif [[ -f "/usr/lib/x86_64-linux-gnu/libgomp.so" ]]; then
    echo -e "${GREEN}✓ found${NC} (libgomp)"
    OPENMP_FOUND=true
fi

if [[ "$OPENMP_FOUND" == "false" ]]; then
    echo -e "${YELLOW}- not found (optional but recommended)${NC}"
    OPTIONAL_MISSING=$((OPTIONAL_MISSING + 1))
fi

echo ""

# ============================================================================
# Optional Advanced Features
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}5. Optional Advanced Features${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

# libigl
check_header "igl/igl_inline.h" "libigl" "false"

# Boost
check_library "libboost" "boost" "false"

# GMP (CGAL dependency)
check_library "libgmp" "gmp" "false"

# MPFR (CGAL dependency)
check_library "libmpfr" "mpfr" "false"

# nlohmann/json (can be auto-downloaded)
check_header "nlohmann/json.hpp" "nlohmann_json" "false"

echo ""

# ============================================================================
# Build Optimization Tools
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}6. Build Optimization Tools${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

check_command "ccache" "--version" "false"
check_command "ninja" "--version" "false"

echo ""

# ============================================================================
# Environment Variables
# ============================================================================

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}7. Environment Configuration${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

[[ -n "$CMAKE_PREFIX_PATH" ]] && echo "  CMAKE_PREFIX_PATH: $CMAKE_PREFIX_PATH"
[[ -n "$PKG_CONFIG_PATH" ]] && echo "  PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
[[ -n "$CGAL_DIR" ]] && echo "  CGAL_DIR: $CGAL_DIR"
[[ -n "$CONDA_PREFIX" ]] && echo "  Conda environment: $CONDA_PREFIX"

if command -v brew &> /dev/null; then
    echo "  Homebrew prefix: $(brew --prefix)"
fi

echo ""

# ============================================================================
# Summary
# ============================================================================

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}Summary${NC}                                                          ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [[ $REQUIRED_MISSING -eq 0 ]] && [[ $VERSION_WARNINGS -eq 0 ]]; then
    echo -e "${GREEN}✓ All required dependencies are installed and up-to-date!${NC}"
    echo ""

    if [[ $OPTIONAL_MISSING -gt 0 ]]; then
        echo -e "${YELLOW}ℹ  $OPTIONAL_MISSING optional dependencies are missing${NC}"
        echo "   These provide performance improvements and advanced features."
        echo ""
    fi

    echo -e "${GREEN}✓ Your environment is ready for building!${NC}"
    echo ""
    echo "Next steps:"
    echo "  cd cpp-version"
    echo "  ./scripts/quick-build.sh"
    echo ""

    exit 0

elif [[ $REQUIRED_MISSING -gt 0 ]]; then
    echo -e "${RED}✗ $REQUIRED_MISSING required dependencies are missing${NC}"

    if [[ $VERSION_WARNINGS -gt 0 ]]; then
        echo -e "${YELLOW}⚠  $VERSION_WARNINGS dependencies have version warnings${NC}"
    fi

    if [[ $OPTIONAL_MISSING -gt 0 ]]; then
        echo -e "${YELLOW}ℹ  $OPTIONAL_MISSING optional dependencies are missing${NC}"
    fi

    echo ""
    echo -e "${YELLOW}Installation commands:${NC}"
    echo ""

    if [[ "$PLATFORM" == "macos" ]]; then
        echo "  macOS (Homebrew):"
        echo "    brew install cmake cgal eigen gdal tbb libomp boost gmp mpfr"
        echo ""
        echo "  Or run the automatic installer:"
        echo "    ./scripts/install_dependencies.sh"
    elif [[ "$PLATFORM" == "linux" ]]; then
        echo "  Ubuntu/Debian:"
        echo "    sudo apt update"
        echo "    sudo apt install build-essential cmake libcgal-dev"
        echo "    sudo apt install libeigen3-dev libgdal-dev libtbb-dev"
        echo "    sudo apt install libboost-all-dev libgmp-dev libmpfr-dev"
        echo ""
        echo "  Or run the automatic installer:"
        echo "    ./scripts/install_dependencies.sh"
    fi

    echo ""
    exit 1

else
    echo -e "${YELLOW}⚠  All required dependencies are present but some have version warnings${NC}"
    echo -e "    $VERSION_WARNINGS version warnings found"
    echo ""
    echo "Build may succeed but some features might not work optimally."
    echo "Consider updating the outdated packages."
    echo ""
    exit 0
fi
