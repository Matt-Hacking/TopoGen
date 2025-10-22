#!/bin/bash
#
# create-appimage.sh - Create AppImage portable Linux package
#
# Creates a single-file portable Linux application with all dependencies bundled.
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Create AppImage portable package for Linux.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen)
    --gui PATH              GUI executable path (default: build/topo-gen-gui)
    --deps-dir PATH         Directory with bundled libraries (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/linux-x64/)
    --help                  Show this help message

Examples:
    $0 --gui build/topo-gen-gui --deps-dir dist/linux-deps

Notes:
    - Creates single-file portable executable
    - Bundles all dependencies
    - Requires appimagetool (downloaded automatically if needed)
    - Works on most Linux distributions

EOF
    exit 0
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

CLI_PATH="$PROJECT_ROOT/build/topo-gen"
GUI_PATH="$PROJECT_ROOT/build/topo-gen-gui"
DEPS_DIR=""
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/linux-x64"

while [[ $# -gt 0 ]]; do
    case $1 in
        --cli) CLI_PATH="$2"; shift 2 ;;
        --gui) GUI_PATH="$2"; shift 2 ;;
        --deps-dir) DEPS_DIR="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --help) usage ;;
        *) log_error "Unknown argument: $1"; usage ;;
    esac
done

if [[ -z "$DEPS_DIR" ]]; then
    log_error "Dependencies directory is required"
    exit 1
fi

if [[ ! -d "$DEPS_DIR" ]]; then
    log_error "Dependencies directory not found: $DEPS_DIR"
    exit 1
fi

if [[ ! -f "$GUI_PATH" && ! -f "$CLI_PATH" ]]; then
    log_error "No executables found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

log_info "AppImage Creation"
log_info "  Version: $VERSION"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Create AppDir structure
TEMP_DIR=$(mktemp -d)
APPDIR="$TEMP_DIR/TopoGen.AppDir"

mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

log_info "Staging AppImage contents..."

# Copy executable (prefer GUI)
if [[ -f "$GUI_PATH" ]]; then
    cp "$GUI_PATH" "$APPDIR/usr/bin/topo-gen-gui"
    chmod +x "$APPDIR/usr/bin/topo-gen-gui"
    MAIN_EXEC="topo-gen-gui"
else
    cp "$CLI_PATH" "$APPDIR/usr/bin/topo-gen"
    chmod +x "$APPDIR/usr/bin/topo-gen"
    MAIN_EXEC="topo-gen"
fi

# Copy libraries
log_info "Copying libraries..."
cp -P "$DEPS_DIR"/*.so* "$APPDIR/usr/lib/" 2>/dev/null || true

# Create AppRun script
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
APPDIR="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"
export GDAL_DATA="$APPDIR/usr/share/gdal"
export PATH="$APPDIR/usr/bin:$PATH"
exec "$APPDIR/usr/bin/MAIN_EXEC" "$@"
EOF

sed -i "s/MAIN_EXEC/$MAIN_EXEC/g" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

# Create desktop file
cat > "$APPDIR/topo-gen.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Topographic Generator
Exec=topo-gen-gui
Icon=topo-gen
Categories=Graphics;Science;
Terminal=false
EOF

# Create simple icon placeholder
echo "P6 256 256 255 $(printf '\xff\x00\x00%.0s' {1..196608})" > "$APPDIR/topo-gen.png" 2>/dev/null || touch "$APPDIR/topo-gen.png"

# Download appimagetool if not available
APPIMAGETOOL="appimagetool-x86_64.AppImage"
if [[ ! -f "$TEMP_DIR/$APPIMAGETOOL" ]]; then
    log_info "Downloading appimagetool..."
    curl -sL "https://github.com/AppImage/AppImageKit/releases/download/continuous/$APPIMAGETOOL" -o "$TEMP_DIR/$APPIMAGETOOL"
    chmod +x "$TEMP_DIR/$APPIMAGETOOL"
fi

# Build AppImage
log_info "Building AppImage..."
cd "$TEMP_DIR"
ARCH=x86_64 "./$APPIMAGETOOL" "$APPDIR" "$OUTPUT_DIR/TopoGen-${VERSION}-x86_64.AppImage"

# Clean up
rm -rf "$TEMP_DIR"

APPIMAGE_FILE="$OUTPUT_DIR/TopoGen-${VERSION}-x86_64.AppImage"

if [[ -f "$APPIMAGE_FILE" ]]; then
    chmod +x "$APPIMAGE_FILE"
    SIZE=$(du -h "$APPIMAGE_FILE" | awk '{print $1}')

    echo ""
    log_success "AppImage created successfully!"
    log_info "Location: $APPIMAGE_FILE"
    log_info "Size: $SIZE"
    echo ""
    log_info "To run: ./$APPIMAGE_FILE"
    log_info "Or: chmod +x $APPIMAGE_FILE && ./$APPIMAGE_FILE"
else
    log_error "Failed to create AppImage"
    exit 1
fi
