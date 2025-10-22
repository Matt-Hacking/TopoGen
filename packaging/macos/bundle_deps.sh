#!/bin/bash
#
# macOS Dependency Bundling Script
# Bundles Homebrew libraries with topo-gen executables
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
Usage: $0 [OPTIONS] <executable_or_app_bundle>

Bundle macOS dependencies for Topographic Generator executables.

Options:
    --cli               Bundle CLI executable (topo-gen)
    --gui               Bundle GUI executable (topo-gen-gui or .app)
    --output-dir DIR    Output directory (default: current directory)
    --homebrew-prefix   Homebrew prefix (default: /opt/homebrew)
    --method METHOD     Bundling method: dylibbundler|manual (default: auto-detect)
    --help              Show this help message

Examples:
    $0 --cli build/topo-gen
    $0 --gui build/topo-gen-gui.app
    $0 --output-dir dist/macos-arm64 --cli build/topo-gen

EOF
    exit 0
}

# Default values
HOMEBREW_PREFIX="${HOMEBREW_PREFIX:-/opt/homebrew}"
OUTPUT_DIR="."
BUNDLE_METHOD="auto"
TARGET_TYPE=""
TARGET_PATH=""

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
        --homebrew-prefix)
            HOMEBREW_PREFIX="$2"
            shift 2
            ;;
        --method)
            BUNDLE_METHOD="$2"
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

if [[ ! -e "$TARGET_PATH" ]]; then
    log_error "Target does not exist: $TARGET_PATH"
    exit 1
fi

# Auto-detect target type if not specified
if [[ -z "$TARGET_TYPE" ]]; then
    if [[ "$TARGET_PATH" == *.app ]]; then
        TARGET_TYPE="gui"
    else
        TARGET_TYPE="cli"
    fi
fi

log_info "macOS Dependency Bundling"
log_info "Target: $TARGET_PATH"
log_info "Type: $TARGET_TYPE"
log_info "Homebrew: $HOMEBREW_PREFIX"

# Check if dylibbundler is available
if command -v dylibbundler &> /dev/null && [[ "$BUNDLE_METHOD" != "manual" ]]; then
    BUNDLE_METHOD="dylibbundler"
    log_info "Using dylibbundler for dependency bundling"
else
    BUNDLE_METHOD="manual"
    log_info "Using manual install_name_tool for dependency bundling"
fi

# Function to get executable path
get_executable_path() {
    if [[ "$TARGET_TYPE" == "gui" && "$TARGET_PATH" == *.app ]]; then
        echo "$TARGET_PATH/Contents/MacOS/topo-gen-gui"
    else
        echo "$TARGET_PATH"
    fi
}

# Function to get frameworks directory
get_frameworks_dir() {
    if [[ "$TARGET_TYPE" == "gui" && "$TARGET_PATH" == *.app ]]; then
        echo "$TARGET_PATH/Contents/Frameworks"
    else
        echo "$(dirname "$TARGET_PATH")/lib"
    fi
}

# Function to get resources directory
get_resources_dir() {
    if [[ "$TARGET_TYPE" == "gui" && "$TARGET_PATH" == *.app ]]; then
        echo "$TARGET_PATH/Contents/Resources"
    else
        echo "$(dirname "$TARGET_PATH")/share"
    fi
}

EXECUTABLE=$(get_executable_path)
FRAMEWORKS_DIR=$(get_frameworks_dir)
RESOURCES_DIR=$(get_resources_dir)

log_info "Executable: $EXECUTABLE"
log_info "Frameworks: $FRAMEWORKS_DIR"
log_info "Resources: $RESOURCES_DIR"

# Create directories
mkdir -p "$FRAMEWORKS_DIR"
mkdir -p "$RESOURCES_DIR"

# Bundle Qt dependencies for GUI
if [[ "$TARGET_TYPE" == "gui" ]]; then
    log_info "Bundling Qt dependencies with macdeployqt..."
    if command -v macdeployqt &> /dev/null; then
        macdeployqt "$TARGET_PATH" -always-overwrite
        log_success "Qt dependencies bundled"
    else
        log_warning "macdeployqt not found - Qt dependencies may not be bundled"
        log_warning "Install with: brew install qt@6 && export PATH=\"$(brew --prefix qt@6)/bin:\$PATH\""
    fi
fi

# Bundle Homebrew dependencies
log_info "Bundling Homebrew dependencies..."

if [[ "$BUNDLE_METHOD" == "dylibbundler" ]]; then
    # Use dylibbundler
    dylibbundler -od -b \
        -x "$EXECUTABLE" \
        -d "$FRAMEWORKS_DIR" \
        -p @executable_path/../Frameworks \
        -s "$HOMEBREW_PREFIX/lib"
    log_success "Dependencies bundled with dylibbundler"
else
    # Manual bundling with install_name_tool
    log_info "Using manual bundling method..."

    # Get list of Homebrew dependencies
    DEPS=$(otool -L "$EXECUTABLE" | grep "$HOMEBREW_PREFIX" | awk '{print $1}')

    for dep in $DEPS; do
        lib_name=$(basename "$dep")
        target_lib="$FRAMEWORKS_DIR/$lib_name"

        log_info "Bundling: $lib_name"

        # Copy library if not already present
        if [[ ! -f "$target_lib" ]]; then
            cp "$dep" "$target_lib"
            chmod u+w "$target_lib"
        fi

        # Fix install name
        install_name_tool -change "$dep" "@executable_path/../Frameworks/$lib_name" "$EXECUTABLE"

        # Recursively bundle dependencies of this library
        SUB_DEPS=$(otool -L "$target_lib" | grep "$HOMEBREW_PREFIX" | awk '{print $1}')
        for sub_dep in $SUB_DEPS; do
            sub_lib_name=$(basename "$sub_dep")
            sub_target_lib="$FRAMEWORKS_DIR/$sub_lib_name"

            if [[ ! -f "$sub_target_lib" ]]; then
                log_info "  Bundling sub-dependency: $sub_lib_name"
                cp "$sub_dep" "$sub_target_lib"
                chmod u+w "$sub_target_lib"
            fi

            # Fix install name in the library
            install_name_tool -change "$sub_dep" "@loader_path/$sub_lib_name" "$target_lib"
        done

        # Fix library's own install name
        install_name_tool -id "@loader_path/$lib_name" "$target_lib"
    done

    log_success "Dependencies bundled manually"
fi

# Bundle GDAL data files
log_info "Bundling GDAL data files..."

GDAL_DATA_SOURCE=""
if command -v gdal-config &> /dev/null; then
    GDAL_DATA_SOURCE=$(gdal-config --datadir)
elif [[ -d "$HOMEBREW_PREFIX/share/gdal" ]]; then
    GDAL_DATA_SOURCE="$HOMEBREW_PREFIX/share/gdal"
fi

if [[ -n "$GDAL_DATA_SOURCE" && -d "$GDAL_DATA_SOURCE" ]]; then
    GDAL_DATA_TARGET="$RESOURCES_DIR/gdal"
    mkdir -p "$GDAL_DATA_TARGET"
    cp -R "$GDAL_DATA_SOURCE/"* "$GDAL_DATA_TARGET/"
    log_success "GDAL data files bundled to $GDAL_DATA_TARGET"
else
    log_warning "GDAL data directory not found - coordinate transformations may fail"
fi

# Verify bundled dependencies
log_info "Verifying bundled dependencies..."

echo ""
echo "Library dependencies:"
otool -L "$EXECUTABLE" | grep -v "/usr/lib" | grep -v "/System/Library"

echo ""
echo "Bundled frameworks:"
ls -lh "$FRAMEWORKS_DIR" 2>/dev/null | tail -n +2 | awk '{print $9, $5}'

if [[ -d "$RESOURCES_DIR/gdal" ]]; then
    GDAL_FILE_COUNT=$(find "$RESOURCES_DIR/gdal" -type f | wc -l | tr -d ' ')
    log_success "GDAL data files: $GDAL_FILE_COUNT files"
fi

echo ""
log_success "macOS dependency bundling complete!"
log_info "Target: $TARGET_PATH"
log_info "All Homebrew dependencies have been bundled"

# Check for any remaining Homebrew dependencies
REMAINING=$(otool -L "$EXECUTABLE" | grep "$HOMEBREW_PREFIX" || true)
if [[ -n "$REMAINING" ]]; then
    log_warning "Some Homebrew dependencies still referenced:"
    echo "$REMAINING"
else
    log_success "No external Homebrew dependencies remaining"
fi
