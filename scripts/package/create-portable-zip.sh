#!/bin/bash
#
# create-portable-zip.sh - Create portable Windows ZIP package
#
# Creates a self-contained ZIP archive with executables, DLLs, and launcher scripts.
# No installation required - can run from any directory or USB drive.
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
Usage: $0 [OPTIONS]

Create a portable Windows ZIP package for Topographic Generator.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen.exe)
    --gui PATH              GUI executable path (default: build/topo-gen-gui.exe)
    --deps-dir PATH         Directory with bundled DLLs (required)
    --data-dir PATH         GDAL data directory (optional)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/windows-x64/)
    --no-cli                Don't include CLI executable
    --no-gui                Don't include GUI application
    --help                  Show this help message

Package Contents:
    topo-gen.exe            CLI executable
    topo-gen-gui.exe        GUI executable
    *.dll                   Dependency libraries
    data/                   GDAL data files
    topo-gen.bat            Windows batch launcher
    topo-gen.ps1            PowerShell launcher
    README.txt              Usage instructions

Examples:
    # Basic portable package (CLI + GUI)
    $0 --deps-dir dist/windows-deps

    # CLI only
    $0 --cli build/topo-gen.exe --deps-dir dist/windows-deps --no-gui

    # With GDAL data
    $0 --deps-dir dist/windows-deps --data-dir /path/to/gdal/data

    # Custom output location
    $0 --deps-dir dist/windows-deps --output-dir /path/to/output

Notes:
    - No installation required - unzip and run
    - Launcher scripts set GDAL_DATA automatically
    - Can run from USB drive or any directory
    - No admin privileges needed

EOF
    exit 0
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Extract version from CMakeLists.txt
get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

# Default values
CLI_PATH="$PROJECT_ROOT/build/topo-gen.exe"
GUI_PATH="$PROJECT_ROOT/build/topo-gen-gui.exe"
DEPS_DIR=""
DATA_DIR=""
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/windows-x64"
INCLUDE_CLI=true
INCLUDE_GUI=true

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cli)
            CLI_PATH="$2"
            shift 2
            ;;
        --gui)
            GUI_PATH="$2"
            shift 2
            ;;
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --data-dir)
            DATA_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --no-cli)
            INCLUDE_CLI=false
            shift
            ;;
        --no-gui)
            INCLUDE_GUI=false
            shift
            ;;
        --help)
            usage
            ;;
        *)
            log_error "Unknown argument: $1"
            usage
            ;;
    esac
done

# Validate inputs
if [[ -z "$DEPS_DIR" ]]; then
    log_error "Dependencies directory is required (--deps-dir)"
    exit 1
fi

if [[ ! -d "$DEPS_DIR" ]]; then
    log_error "Dependencies directory not found: $DEPS_DIR"
    exit 1
fi

if [[ "$INCLUDE_CLI" == true && ! -f "$CLI_PATH" ]]; then
    log_warning "CLI executable not found: $CLI_PATH"
    log_info "Proceeding without CLI..."
    INCLUDE_CLI=false
fi

if [[ "$INCLUDE_GUI" == true && ! -f "$GUI_PATH" ]]; then
    log_warning "GUI executable not found: $GUI_PATH"
    log_info "Proceeding without GUI..."
    INCLUDE_GUI=false
fi

if [[ "$INCLUDE_CLI" == false && "$INCLUDE_GUI" == false ]]; then
    log_error "No components to package"
    exit 1
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "Portable Windows ZIP Package Creation"
log_info "  Version: $VERSION"
log_info "  Include CLI: $INCLUDE_CLI"
log_info "  Include GUI: $INCLUDE_GUI"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Create temporary staging directory
TEMP_DIR=$(mktemp -d)
STAGE_DIR="$TEMP_DIR/topo-gen-$VERSION-portable"
mkdir -p "$STAGE_DIR"

log_info "Staging package contents..."

# Copy executables
if [[ "$INCLUDE_CLI" == true ]]; then
    cp "$CLI_PATH" "$STAGE_DIR/topo-gen.exe"
    log_success "Copied CLI executable"
fi

if [[ "$INCLUDE_GUI" == true ]]; then
    cp "$GUI_PATH" "$STAGE_DIR/topo-gen-gui.exe"
    log_success "Copied GUI executable"
fi

# Copy DLLs
log_info "Copying dependency libraries..."
DLL_COUNT=0
for dll in "$DEPS_DIR"/*.dll; do
    if [[ -f "$dll" ]]; then
        cp "$dll" "$STAGE_DIR/"
        DLL_COUNT=$((DLL_COUNT + 1))
    fi
done
log_success "Copied $DLL_COUNT DLL files"

# Copy GDAL data if provided
if [[ -n "$DATA_DIR" && -d "$DATA_DIR" ]]; then
    log_info "Copying GDAL data files..."
    mkdir -p "$STAGE_DIR/data/gdal"
    cp -R "$DATA_DIR"/* "$STAGE_DIR/data/gdal/"
    log_success "Copied GDAL data"
elif command -v gdal-config &> /dev/null; then
    GDAL_DATA=$(gdal-config --datadir)
    if [[ -d "$GDAL_DATA" ]]; then
        log_info "Copying GDAL data from system installation..."
        mkdir -p "$STAGE_DIR/data/gdal"
        cp -R "$GDAL_DATA"/* "$STAGE_DIR/data/gdal/"
        log_success "Copied GDAL data"
    fi
else
    log_warning "No GDAL data directory provided"
    log_info "Application may need GDAL_DATA environment variable set manually"
fi

# Create batch launcher
log_info "Creating launcher scripts..."

cat > "$STAGE_DIR/topo-gen.bat" << 'EOF'
@echo off
REM Topographic Generator Launcher (Batch)
REM Portable package - no installation required

setlocal

REM Get directory where this script is located
set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

REM Set GDAL_DATA to bundled data
if exist "%SCRIPT_DIR%\data\gdal" (
    set GDAL_DATA=%SCRIPT_DIR%\data\gdal
)

REM Add current directory to PATH for DLL discovery
set PATH=%SCRIPT_DIR%;%PATH%

REM Launch CLI with arguments
"%SCRIPT_DIR%\topo-gen.exe" %*

endlocal
EOF

log_success "Created topo-gen.bat"

# Create PowerShell launcher
cat > "$STAGE_DIR/topo-gen.ps1" << 'EOF'
# Topographic Generator Launcher (PowerShell)
# Portable package - no installation required

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)

# Get directory where this script is located
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Set GDAL_DATA to bundled data
$GdalDataPath = Join-Path $ScriptDir "data\gdal"
if (Test-Path $GdalDataPath) {
    $env:GDAL_DATA = $GdalDataPath
}

# Add current directory to PATH for DLL discovery
$env:PATH = "$ScriptDir;$env:PATH"

# Launch CLI with arguments
$ExePath = Join-Path $ScriptDir "topo-gen.exe"

if (Test-Path $ExePath) {
    & $ExePath @Arguments
    exit $LASTEXITCODE
} else {
    Write-Error "topo-gen.exe not found in: $ScriptDir"
    exit 1
}
EOF

log_success "Created topo-gen.ps1"

# Create GUI launcher (if GUI included)
if [[ "$INCLUDE_GUI" == true ]]; then
    cat > "$STAGE_DIR/topo-gen-gui.bat" << 'EOF'
@echo off
REM Topographic Generator GUI Launcher (Batch)

setlocal

set SCRIPT_DIR=%~dp0
set SCRIPT_DIR=%SCRIPT_DIR:~0,-1%

if exist "%SCRIPT_DIR%\data\gdal" (
    set GDAL_DATA=%SCRIPT_DIR%\data\gdal
)

set PATH=%SCRIPT_DIR%;%PATH%

start "" "%SCRIPT_DIR%\topo-gen-gui.exe"

endlocal
EOF

    log_success "Created topo-gen-gui.bat"
fi

# Create README
log_info "Creating README..."

cat > "$STAGE_DIR/README.txt" << EOF
Topographic Generator v$VERSION - Portable Package
=================================================

This is a portable package that requires no installation.
Simply unzip and run from any directory or USB drive.

QUICK START
-----------

For GUI:
  1. Double-click: topo-gen-gui.exe
     (or run: topo-gen-gui.bat)

For CLI:
  1. Open Command Prompt in this directory
  2. Run: topo-gen.bat --help

PACKAGE CONTENTS
----------------

topo-gen.exe         - Command-line executable
topo-gen-gui.exe     - GUI application
*.dll                - Required libraries
data/gdal/           - GDAL data files
topo-gen.bat         - Windows batch launcher
topo-gen.ps1         - PowerShell launcher
README.txt           - This file

USAGE EXAMPLES
--------------

Using batch launcher:
  topo-gen.bat --upper-left 47.6062,-122.3321 --lower-right 47.6020,-122.3280

Using PowerShell launcher:
  powershell -File topo-gen.ps1 --upper-left 47.6062,-122.3321 --lower-right 47.6020,-122.3280

Direct execution:
  topo-gen.exe --help

ENVIRONMENT VARIABLES
---------------------

The launcher scripts automatically set:
  GDAL_DATA    - Points to bundled GDAL data files
  PATH         - Includes current directory for DLL discovery

If you run executables directly, you may need to set these manually:
  set GDAL_DATA=%CD%\data\gdal
  set PATH=%CD%;%PATH%

REQUIREMENTS
------------

- Windows 10 or later (x64)
- No installation needed
- No admin privileges required
- Can run from any directory or removable media

FEATURES
--------

- High-performance C++ implementation
- Automatic SRTM elevation data downloading
- Contour generation from elevation data
- OpenStreetMap feature integration
- Multi-format export: SVG, STL, GeoJSON, Shapefile
- Command-line and GUI interfaces

COMMON COMMANDS
---------------

Get help:
  topo-gen.bat --help

Show version:
  topo-gen.bat --version

Generate contours:
  topo-gen.bat --upper-left LAT,LON --lower-right LAT,LON

Create configuration file:
  topo-gen.bat --create-config config.json

Use configuration file:
  topo-gen.bat --config config.json

TROUBLESHOOTING
---------------

Error: "Missing DLL"
  Solution: Ensure all *.dll files are in the same directory as the executable
  Or use: topo-gen.bat (which sets PATH automatically)

Error: "GDAL_DATA not found"
  Solution: Use launcher scripts which set GDAL_DATA
  Or set manually: set GDAL_DATA=%CD%\data\gdal

Error: "Application failed to start"
  Solution: Ensure you have Visual C++ Redistributable installed
  Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe

Windows SmartScreen warning:
  Solution: Click "More info" then "Run anyway"
  (Package is not code-signed)

DOCUMENTATION
-------------

For complete documentation, visit:
  https://github.com/matthewblock/topo-gen

LICENSE
-------

MIT License - Copyright (c) 2025 Matthew Block

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

SUPPORT
-------

For issues, questions, or feature requests:
  https://github.com/matthewblock/topo-gen/issues

EOF

log_success "Created README.txt"

# Create LICENSE file
cat > "$STAGE_DIR/LICENSE.txt" << 'EOF'
MIT License

Copyright (c) 2025 Matthew Block

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF

log_success "Created LICENSE.txt"

# Create ZIP archive
log_info "Creating ZIP archive..."

ZIP_NAME="topo-gen-${VERSION}-windows-x64-portable.zip"
ZIP_PATH="$OUTPUT_DIR/$ZIP_NAME"

# Remove old ZIP if exists
rm -f "$ZIP_PATH"

# Create ZIP (compatible across platforms)
if command -v zip &> /dev/null; then
    cd "$TEMP_DIR"
    zip -r "$ZIP_PATH" "$(basename "$STAGE_DIR")" > /dev/null
    cd - > /dev/null
    log_success "Created ZIP with zip command"
elif command -v python3 &> /dev/null; then
    python3 << PYEOF
import zipfile
import os

def zip_directory(folder_path, output_path):
    with zipfile.ZipFile(output_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(folder_path):
            for file in files:
                file_path = os.path.join(root, file)
                arcname = os.path.relpath(file_path, os.path.dirname(folder_path))
                zipf.write(file_path, arcname)

zip_directory('$STAGE_DIR', '$ZIP_PATH')
PYEOF
    log_success "Created ZIP with python3"
else
    log_error "Cannot create ZIP (need zip or python3)"
    log_info "Staged files are in: $STAGE_DIR"
    exit 1
fi

# Clean up temporary directory
rm -rf "$TEMP_DIR"

# Get ZIP size
if [[ -f "$ZIP_PATH" ]]; then
    ZIP_SIZE=$(du -h "$ZIP_PATH" | awk '{print $1}')

    echo ""
    log_success "Portable ZIP package created successfully!"
    log_info "Location: $ZIP_PATH"
    log_info "Size: $ZIP_SIZE"
    echo ""

    log_info "Package contents:"
    if command -v unzip &> /dev/null; then
        unzip -l "$ZIP_PATH" | head -20
    fi

    echo ""
    log_info "Distribution instructions:"
    echo ""
    echo "  1. Upload ZIP to distribution server:"
    echo "     - GitHub releases"
    echo "     - Website downloads page"
    echo "     - File hosting service"
    echo ""
    echo "  2. Users download and extract:"
    echo "     - Unzip to any directory"
    echo "     - No installation required"
    echo "     - Run topo-gen.exe or topo-gen-gui.exe"
    echo ""
    echo "  3. Usage:"
    echo "     - GUI: Double-click topo-gen-gui.exe"
    echo "     - CLI: Run topo-gen.bat from Command Prompt"
    echo "     - PowerShell: Run topo-gen.ps1"
    echo ""

    log_info "Testing the package:"
    echo ""
    echo "  1. Extract ZIP to test directory"
    echo "  2. On Windows:"
    echo "     cd extracted-directory"
    echo "     topo-gen.bat --version"
    echo "     topo-gen-gui.exe"
    echo ""
    echo "  3. Verify:"
    echo "     - CLI shows version"
    echo "     - GUI launches without errors"
    echo "     - No missing DLL errors"
    echo ""

else
    log_error "Failed to create ZIP archive"
    exit 1
fi
