#!/bin/bash
#
# Windows Dependency Bundling Script
# Bundles DLLs with topo-gen executables for Windows portable distribution
#
# Note: This script is designed to run on Linux/macOS for cross-compilation
#       or on Windows via MSYS2/Git Bash
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS] <executable_path>

Bundle Windows DLL dependencies for Topographic Generator executables.

Options:
    --cli               Bundle CLI executable (topo-gen.exe)
    --gui               Bundle GUI executable (topo-gen-gui.exe)
    --output-dir DIR    Output directory (default: same as executable)
    --deps-dir DIR      Dependencies search directory (vcpkg/msys2/mingw)
    --windeployqt PATH  Path to windeployqt.exe (for GUI Qt bundling)
    --help              Show this help message

Environment Variables:
    VCPKG_ROOT          vcpkg installation directory
    MINGW_PREFIX        MinGW/MSYS2 prefix (e.g., /mingw64)

Examples:
    # Using vcpkg
    export VCPKG_ROOT=/path/to/vcpkg
    $0 --cli build/topo-gen.exe

    # Using MSYS2
    export MINGW_PREFIX=/mingw64
    $0 --gui build/topo-gen-gui.exe

    # Manual dependency directory
    $0 --deps-dir /c/dependencies/bin --cli build/topo-gen.exe

EOF
    exit 0
}

# Default values
OUTPUT_DIR=""
DEPS_DIR=""
TARGET_TYPE=""
TARGET_PATH=""
WINDEPLOYQT=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cli)
            TARGET_TYPE="cli"
            shift
            ;;
        --gui)
            TARGET_TYPE="gui"
            shift
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --windeployqt)
            WINDEPLOYQT="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            if [[ -z "$TARGET_PATH" ]]; then
                TARGET_PATH="$1"
            else
                log_error "Unknown argument: $1"
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate inputs
if [[ -z "$TARGET_PATH" ]]; then
    log_error "No target specified"
    usage
fi

if [[ ! -f "$TARGET_PATH" ]]; then
    log_error "Target executable does not exist: $TARGET_PATH"
    exit 1
fi

# Auto-detect target type
if [[ -z "$TARGET_TYPE" ]]; then
    if [[ "$TARGET_PATH" == *gui* ]]; then
        TARGET_TYPE="gui"
    else
        TARGET_TYPE="cli"
    fi
fi

# Set output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="$(dirname "$TARGET_PATH")"
fi

log_info "Windows Dependency Bundling"
log_info "Target: $TARGET_PATH"
log_info "Type: $TARGET_TYPE"
log_info "Output: $OUTPUT_DIR"

# Auto-detect dependency directory
if [[ -z "$DEPS_DIR" ]]; then
    if [[ -n "$VCPKG_ROOT" ]]; then
        # vcpkg detection
        if [[ -d "$VCPKG_ROOT/installed/x64-windows/bin" ]]; then
            DEPS_DIR="$VCPKG_ROOT/installed/x64-windows/bin"
            log_info "Detected vcpkg dependencies: $DEPS_DIR"
        fi
    elif [[ -n "$MINGW_PREFIX" ]]; then
        # MSYS2/MinGW detection
        if [[ -d "$MINGW_PREFIX/bin" ]]; then
            DEPS_DIR="$MINGW_PREFIX/bin"
            log_info "Detected MinGW dependencies: $DEPS_DIR"
        fi
    elif [[ -d "/mingw64/bin" ]]; then
        # Default MSYS2 location
        DEPS_DIR="/mingw64/bin"
        log_info "Detected MSYS2 dependencies: $DEPS_DIR"
    fi
fi

if [[ -z "$DEPS_DIR" ]]; then
    log_error "Cannot detect dependency directory"
    log_error "Please specify --deps-dir or set VCPKG_ROOT/MINGW_PREFIX"
    exit 1
fi

if [[ ! -d "$DEPS_DIR" ]]; then
    log_error "Dependency directory does not exist: $DEPS_DIR"
    exit 1
fi

# Function to copy DLL if it exists
copy_dll_if_exists() {
    local dll_name="$1"
    local source="${DEPS_DIR}/${dll_name}"
    local target="${OUTPUT_DIR}/${dll_name}"

    if [[ -f "$source" ]]; then
        if [[ ! -f "$target" ]]; then
            cp "$source" "$target"
            log_info "Bundled: $dll_name"
            return 0
        fi
    fi
    return 1
}

# Bundle Qt dependencies for GUI
if [[ "$TARGET_TYPE" == "gui" ]]; then
    log_info "Bundling Qt dependencies..."

    # Try to find windeployqt
    if [[ -z "$WINDEPLOYQT" ]]; then
        if command -v windeployqt &> /dev/null; then
            WINDEPLOYQT="windeployqt"
        elif [[ -f "${DEPS_DIR}/windeployqt.exe" ]]; then
            WINDEPLOYQT="${DEPS_DIR}/windeployqt.exe"
        elif [[ -n "$VCPKG_ROOT" ]] && [[ -f "$VCPKG_ROOT/installed/x64-windows/tools/qt6/bin/windeployqt.exe" ]]; then
            WINDEPLOYQT="$VCPKG_ROOT/installed/x64-windows/tools/qt6/bin/windeployqt.exe"
        fi
    fi

    if [[ -n "$WINDEPLOYQT" ]] && [[ -x "$WINDEPLOYQT" || -f "$WINDEPLOYQT" ]]; then
        log_info "Running windeployqt: $WINDEPLOYQT"
        "$WINDEPLOYQT" --release --no-translations "$TARGET_PATH"
        log_success "Qt dependencies bundled with windeployqt"
    else
        log_warning "windeployqt not found - Qt dependencies must be bundled manually"
        log_warning "Install Qt6 development tools to get windeployqt"
    fi
fi

# Bundle core dependencies
log_info "Bundling core dependencies..."

# GDAL dependencies
copy_dll_if_exists "gdal.dll" || copy_dll_if_exists "libgdal.dll" || copy_dll_if_exists "gdal300.dll"
copy_dll_if_exists "geos.dll" || copy_dll_if_exists "libgeos.dll"
copy_dll_if_exists "geos_c.dll" || copy_dll_if_exists "libgeos_c.dll"
copy_dll_if_exists "proj.dll" || copy_dll_if_exists "libproj.dll" || copy_dll_if_exists "proj_9_4.dll"

# CGAL and dependencies
copy_dll_if_exists "gmp.dll" || copy_dll_if_exists "libgmp-10.dll"
copy_dll_if_exists "mpfr.dll" || copy_dll_if_exists "libmpfr-6.dll"

# TBB
copy_dll_if_exists "tbb.dll" || copy_dll_if_exists "tbb12.dll"
copy_dll_if_exists "tbbmalloc.dll"

# OpenMP (MinGW)
copy_dll_if_exists "libgomp-1.dll"
copy_dll_if_exists "libwinpthread-1.dll"

# MinGW runtime (if using MinGW)
copy_dll_if_exists "libgcc_s_seh-1.dll"
copy_dll_if_exists "libstdc++-6.dll"

# Additional common dependencies
copy_dll_if_exists "zlib1.dll" || copy_dll_if_exists "zlib.dll"
copy_dll_if_exists "libjpeg-8.dll" || copy_dll_if_exists "jpeg62.dll"
copy_dll_if_exists "libpng16.dll" || copy_dll_if_exists "libpng16-16.dll"
copy_dll_if_exists "libtiff-5.dll" || copy_dll_if_exists "tiff.dll"
copy_dll_if_exists "libcurl-4.dll" || copy_dll_if_exists "libcurl.dll"
copy_dll_if_exists "libexpat-1.dll" || copy_dll_if_exists "expat.dll"
copy_dll_if_exists "sqlite3.dll"
copy_dll_if_exists "libxml2-2.dll" || copy_dll_if_exists "libxml2.dll"

# Use ldd or objdump to find remaining dependencies (if available)
if command -v ldd &> /dev/null; then
    log_info "Analyzing dependencies with ldd..."
    MISSING_DEPS=$(ldd "$TARGET_PATH" 2>/dev/null | grep "not found" || true)
    if [[ -n "$MISSING_DEPS" ]]; then
        log_warning "Missing dependencies detected:"
        echo "$MISSING_DEPS"
    fi
elif command -v objdump &> /dev/null; then
    log_info "Analyzing dependencies with objdump..."
    ALL_DEPS=$(objdump -p "$TARGET_PATH" | grep "DLL Name:" | awk '{print $3}')
    for dep in $ALL_DEPS; do
        if [[ ! -f "${OUTPUT_DIR}/${dep}" ]]; then
            copy_dll_if_exists "$dep" || log_warning "Could not find: $dep"
        fi
    done
fi

# Bundle GDAL data files
log_info "Bundling GDAL data files..."

GDAL_DATA_SOURCE=""
if [[ -d "${DEPS_DIR}/../share/gdal" ]]; then
    GDAL_DATA_SOURCE="${DEPS_DIR}/../share/gdal"
elif [[ -d "/mingw64/share/gdal" ]]; then
    GDAL_DATA_SOURCE="/mingw64/share/gdal"
fi

if [[ -n "$GDAL_DATA_SOURCE" && -d "$GDAL_DATA_SOURCE" ]]; then
    GDAL_DATA_TARGET="${OUTPUT_DIR}/gdal-data"
    mkdir -p "$GDAL_DATA_TARGET"
    cp -R "$GDAL_DATA_SOURCE/"* "$GDAL_DATA_TARGET/"
    log_success "GDAL data files bundled to $GDAL_DATA_TARGET"

    # Create launcher script that sets GDAL_DATA
    cat > "${OUTPUT_DIR}/topo-gen-launcher.bat" << 'EOF'
@echo off
set "SCRIPT_DIR=%~dp0"
set "GDAL_DATA=%SCRIPT_DIR%gdal-data"
"%SCRIPT_DIR%topo-gen.exe" %*
EOF
    log_success "Created launcher script: topo-gen-launcher.bat"
else
    log_warning "GDAL data directory not found - coordinate transformations may fail"
fi

# List bundled files
echo ""
log_info "Bundled DLLs:"
ls -lh "$OUTPUT_DIR"/*.dll 2>/dev/null | awk '{print $9, $5}' || log_warning "No DLLs found in output directory"

if [[ -d "${OUTPUT_DIR}/gdal-data" ]]; then
    GDAL_FILE_COUNT=$(find "${OUTPUT_DIR}/gdal-data" -type f | wc -l | tr -d ' ')
    log_success "GDAL data files: $GDAL_FILE_COUNT files"
fi

echo ""
log_success "Windows dependency bundling complete!"
log_info "Target: $TARGET_PATH"
log_info "Output: $OUTPUT_DIR"
log_info "All dependencies should be bundled for portable distribution"

echo ""
log_info "To test portability, copy $OUTPUT_DIR to a clean Windows system and run:"
log_info "  topo-gen.exe --help"
log_info "  (or use topo-gen-launcher.bat for GDAL data support)"
