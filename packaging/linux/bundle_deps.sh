#!/bin/bash
#
# Linux Dependency Bundling Script
# Bundles shared libraries with topo-gen executables for Linux portable distribution
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

Bundle Linux shared library dependencies for Topographic Generator executables.

Options:
    --cli               Bundle CLI executable (topo-gen)
    --gui               Bundle GUI executable (topo-gen-gui)
    --output-dir DIR    Output directory (default: same as executable)
    --lib-dir NAME      Library subdirectory name (default: lib)
    --use-rpath         Set RPATH in executable (recommended)
    --use-runpath       Set RUNPATH in executable
    --portable          Create fully portable bundle (bundle all non-system libs)
    --help              Show this help message

Packaging Modes:
    Default:  Bundle only project-specific libraries, rely on system packages for common libs
    --portable: Bundle all dependencies except glibc/system libraries (for AppImage/Flatpak)

Examples:
    # Standard bundling (for DEB/RPM packages)
    $0 --cli build/topo-gen

    # Portable bundle with RPATH (for AppImage)
    $0 --portable --use-rpath --cli build/topo-gen

    # GUI with Qt bundling
    $0 --gui --use-rpath build/topo-gen-gui

EOF
    exit 0
}

# Default values
OUTPUT_DIR=""
LIB_DIR_NAME="lib"
TARGET_TYPE=""
TARGET_PATH=""
USE_RPATH=false
USE_RUNPATH=false
PORTABLE_MODE=false

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
        --lib-dir)
            LIB_DIR_NAME="$2"
            shift 2
            ;;
        --use-rpath)
            USE_RPATH=true
            shift
            ;;
        --use-runpath)
            USE_RUNPATH=true
            shift
            ;;
        --portable)
            PORTABLE_MODE=true
            shift
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

LIB_DIR="${OUTPUT_DIR}/${LIB_DIR_NAME}"
SHARE_DIR="${OUTPUT_DIR}/share"

log_info "Linux Dependency Bundling"
log_info "Target: $TARGET_PATH"
log_info "Type: $TARGET_TYPE"
log_info "Output: $OUTPUT_DIR"
log_info "Libraries: $LIB_DIR"
log_info "Portable mode: $PORTABLE_MODE"

# Create directories
mkdir -p "$LIB_DIR"
mkdir -p "$SHARE_DIR"

# Function to check if library should be bundled
should_bundle_lib() {
    local lib="$1"

    # Never bundle these system libraries
    if [[ "$lib" == /lib/* ]] || \
       [[ "$lib" == /lib64/* ]] || \
       [[ "$lib" == */ld-linux* ]] || \
       [[ "$lib" == */libc.so* ]] || \
       [[ "$lib" == */libm.so* ]] || \
       [[ "$lib" == */libpthread.so* ]] || \
       [[ "$lib" == */libdl.so* ]] || \
       [[ "$lib" == */librt.so* ]]; then
        return 1
    fi

    # In portable mode, bundle everything except system libs
    if [[ "$PORTABLE_MODE" == true ]]; then
        return 0
    fi

    # In standard mode, only bundle specific libraries
    if [[ "$lib" == *libgdal* ]] || \
       [[ "$lib" == *libCGAL* ]] || \
       [[ "$lib" == *libtbb* ]] || \
       [[ "$lib" == *libQt* ]]; then
        return 0
    fi

    return 1
}

# Function to copy library and its dependencies recursively
bundle_library() {
    local lib="$1"
    local lib_name=$(basename "$lib")
    local target="${LIB_DIR}/${lib_name}"

    # Skip if already bundled or shouldn't be bundled
    if [[ -f "$target" ]]; then
        return 0
    fi

    if ! should_bundle_lib "$lib"; then
        return 0
    fi

    log_info "Bundling: $lib_name"

    # Copy library
    cp "$lib" "$target"
    chmod 755 "$target"

    # Strip debug symbols to reduce size (optional)
    if command -v strip &> /dev/null; then
        strip --strip-unneeded "$target" 2>/dev/null || true
    fi

    # Recursively bundle dependencies
    local deps=$(ldd "$lib" 2>/dev/null | grep "=>" | awk '{print $3}' | grep -v "^$")
    for dep in $deps; do
        if [[ -f "$dep" ]]; then
            bundle_library "$dep"
        fi
    done
}

# Get all dependencies of the executable
log_info "Analyzing dependencies..."
DEPS=$(ldd "$TARGET_PATH" 2>/dev/null | grep "=>" | awk '{print $3}' | grep -v "^$")

# Bundle each dependency
for dep in $DEPS; do
    if [[ -f "$dep" ]]; then
        bundle_library "$dep"
    fi
done

# Bundle Qt dependencies for GUI (if linuxdeployqt is available)
if [[ "$TARGET_TYPE" == "gui" ]]; then
    log_info "Checking for Qt deployment tools..."

    if command -v linuxdeployqt &> /dev/null; then
        log_info "Running linuxdeployqt..."
        linuxdeployqt "$TARGET_PATH" -bundle-non-qt-libs -verbose=1
        log_success "Qt dependencies bundled with linuxdeployqt"
    else
        log_warning "linuxdeployqt not found - Qt dependencies may need manual bundling"
        log_info "Download from: https://github.com/probonopd/linuxdeployqt"
    fi
fi

# Set RPATH/RUNPATH if requested
if [[ "$USE_RPATH" == true || "$USE_RUNPATH" == true ]]; then
    if command -v patchelf &> /dev/null; then
        log_info "Setting RPATH in executable..."

        if [[ "$USE_RUNPATH" == true ]]; then
            patchelf --set-rpath "\$ORIGIN/${LIB_DIR_NAME}" --force-rpath "$TARGET_PATH"
            log_success "RUNPATH set to \$ORIGIN/${LIB_DIR_NAME}"
        else
            patchelf --set-rpath "\$ORIGIN/${LIB_DIR_NAME}" "$TARGET_PATH"
            log_success "RPATH set to \$ORIGIN/${LIB_DIR_NAME}"
        fi

        # Also set RPATH for bundled libraries
        for lib in "$LIB_DIR"/*.so*; do
            if [[ -f "$lib" ]]; then
                patchelf --set-rpath "\$ORIGIN" "$lib" 2>/dev/null || true
            fi
        done
    else
        log_warning "patchelf not found - RPATH not set"
        log_warning "Install with: sudo apt install patchelf"
        log_info "You can set LD_LIBRARY_PATH manually: export LD_LIBRARY_PATH=\$PWD/$LIB_DIR_NAME"
    fi
fi

# Bundle GDAL data files
log_info "Bundling GDAL data files..."

GDAL_DATA_SOURCE=""
if command -v gdal-config &> /dev/null; then
    GDAL_DATA_SOURCE=$(gdal-config --datadir)
elif [[ -d "/usr/share/gdal" ]]; then
    GDAL_DATA_SOURCE="/usr/share/gdal"
elif [[ -d "/usr/local/share/gdal" ]]; then
    GDAL_DATA_SOURCE="/usr/local/share/gdal"
fi

if [[ -n "$GDAL_DATA_SOURCE" && -d "$GDAL_DATA_SOURCE" ]]; then
    GDAL_DATA_TARGET="${SHARE_DIR}/gdal"
    mkdir -p "$GDAL_DATA_TARGET"
    cp -R "$GDAL_DATA_SOURCE/"* "$GDAL_DATA_TARGET/"
    log_success "GDAL data files bundled to $GDAL_DATA_TARGET"

    # Create launcher script that sets GDAL_DATA
    cat > "${OUTPUT_DIR}/topo-gen-launcher.sh" << 'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export GDAL_DATA="${SCRIPT_DIR}/share/gdal"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
exec "${SCRIPT_DIR}/topo-gen" "$@"
EOF
    chmod +x "${OUTPUT_DIR}/topo-gen-launcher.sh"
    log_success "Created launcher script: topo-gen-launcher.sh"
else
    log_warning "GDAL data directory not found - coordinate transformations may fail"
fi

# Verify bundled dependencies
log_info "Verifying bundled dependencies..."

echo ""
echo "Executable dependencies:"
ldd "$TARGET_PATH" | grep -v "not found" || log_error "Missing dependencies detected!"

echo ""
echo "Bundled libraries:"
ls -lh "$LIB_DIR"/*.so* 2>/dev/null | awk '{print $9, $5}' || log_warning "No libraries bundled"

if [[ -d "${SHARE_DIR}/gdal" ]]; then
    GDAL_FILE_COUNT=$(find "${SHARE_DIR}/gdal" -type f | wc -l | tr -d ' ')
    log_success "GDAL data files: $GDAL_FILE_COUNT files"
fi

# Check for missing dependencies
MISSING=$(ldd "$TARGET_PATH" 2>/dev/null | grep "not found" || true)
if [[ -n "$MISSING" ]]; then
    log_error "Missing dependencies detected:"
    echo "$MISSING"
    exit 1
fi

echo ""
log_success "Linux dependency bundling complete!"
log_info "Target: $TARGET_PATH"
log_info "Output: $OUTPUT_DIR"

if [[ "$USE_RPATH" == true || "$USE_RUNPATH" == true ]]; then
    log_success "RPATH configured - executable should find bundled libraries automatically"
else
    log_info "To run, set LD_LIBRARY_PATH:"
    log_info "  export LD_LIBRARY_PATH=\$(pwd)/${LIB_DIR_NAME}"
    log_info "  ./$(basename "$TARGET_PATH") --help"
    log_info "Or use the launcher script: ./topo-gen-launcher.sh"
fi
