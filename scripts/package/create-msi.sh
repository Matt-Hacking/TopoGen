#!/bin/bash
#
# create-msi.sh - Create Windows MSI installer
#
# Creates a professional Windows MSI installer package using WiX Toolset.
# Can generate WiX source files on any platform, but requires Windows + WiX to compile.
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

Create a Windows MSI installer for Topographic Generator.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen.exe)
    --gui PATH              GUI executable path (default: build/topo-gen-gui.exe)
    --deps-dir PATH         Directory with bundled DLLs (required)
    --identifier GUID       Product GUID (auto-generated if not provided)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/windows-x64/)
    --manufacturer NAME     Manufacturer name (default: Matthew Block)
    --product-name NAME     Product name (default: Topographic Generator)
    --no-cli                Don't include CLI executable
    --no-gui                Don't include GUI application
    --sign-identity CERT    Code signing certificate thumbprint (optional)
    --compile               Attempt to compile with WiX (requires Windows + WiX)
    --help                  Show this help message

Installation Locations:
    CLI:  C:\Program Files\Topographic Generator\topo-gen.exe
    GUI:  C:\Program Files\Topographic Generator\topo-gen-gui.exe
    DLLs: C:\Program Files\Topographic Generator\*.dll
    Data: C:\Program Files\Topographic Generator\data\

Examples:
    # Generate WiX source files (cross-platform)
    $0 --cli build/topo-gen.exe --gui build/topo-gen-gui.exe --deps-dir dist/windows-deps

    # Generate and compile (requires Windows + WiX)
    $0 --cli build/topo-gen.exe --deps-dir dist/windows-deps --compile

    # With code signing
    $0 --deps-dir dist/windows-deps --sign-identity "abc123..."

Notes:
    - This script generates WiX XML source files on any platform
    - To compile MSI, you need Windows with WiX Toolset installed
    - Use --deps-dir to specify directory containing bundled DLLs
    - Code signing requires signtool.exe and a valid certificate

EOF
    exit 0
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGING_DIR="$PROJECT_ROOT/packaging"

# Extract version from CMakeLists.txt
get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

# Generate GUID
generate_guid() {
    if command -v uuidgen &> /dev/null; then
        uuidgen | tr '[:lower:]' '[:upper:]'
    elif command -v python3 &> /dev/null; then
        python3 -c "import uuid; print(str(uuid.uuid4()).upper())"
    else
        log_error "Cannot generate GUID (need uuidgen or python3)"
        exit 1
    fi
}

# Default values
CLI_PATH="$PROJECT_ROOT/build/topo-gen.exe"
GUI_PATH="$PROJECT_ROOT/build/topo-gen-gui.exe"
DEPS_DIR=""
PRODUCT_GUID=""
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/windows-x64"
MANUFACTURER="Matthew Block"
PRODUCT_NAME="Topographic Generator"
INCLUDE_CLI=true
INCLUDE_GUI=true
SIGN_IDENTITY=""
COMPILE=false

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
        --identifier)
            PRODUCT_GUID="$2"
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
        --manufacturer)
            MANUFACTURER="$2"
            shift 2
            ;;
        --product-name)
            PRODUCT_NAME="$2"
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
        --sign-identity)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        --compile)
            COMPILE=true
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
    log_error "No components to install"
    exit 1
fi

# Generate Product GUID if not provided
if [[ -z "$PRODUCT_GUID" ]]; then
    PRODUCT_GUID=$(generate_guid)
    log_info "Generated Product GUID: $PRODUCT_GUID"
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "Windows MSI Installer Creation"
log_info "  Product: $PRODUCT_NAME"
log_info "  Version: $VERSION"
log_info "  GUID: $PRODUCT_GUID"
log_info "  Include CLI: $INCLUDE_CLI"
log_info "  Include GUI: $INCLUDE_GUI"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Create temporary directory for WiX sources
TEMP_DIR=$(mktemp -d)
WIX_DIR="$TEMP_DIR/wix"
mkdir -p "$WIX_DIR"

log_info "Generating WiX source files..."

# Generate main Product.wxs
cat > "$WIX_DIR/Product.wxs" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">
  <Product Id="$PRODUCT_GUID"
           Name="$PRODUCT_NAME"
           Language="1033"
           Version="$VERSION"
           Manufacturer="$MANUFACTURER"
           UpgradeCode="$(generate_guid)">

    <Package InstallerVersion="200"
             Compressed="yes"
             InstallScope="perMachine"
             Description="$PRODUCT_NAME Installer"
             Comments="High-performance topographic model generator for laser cutting and 3D printing" />

    <MajorUpgrade DowngradeErrorMessage="A newer version of [$PRODUCT_NAME] is already installed." />
    <MediaTemplate EmbedCab="yes" />

    <!-- Installation directory -->
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLFOLDER" Name="$PRODUCT_NAME">

          <!-- Executables -->
EOF

if [[ "$INCLUDE_CLI" == true ]]; then
    cat >> "$WIX_DIR/Product.wxs" << EOF
          <Component Id="CLI" Guid="$(generate_guid)" Win64="yes">
            <File Id="TopoGenExe" Source="$(basename "$CLI_PATH")" KeyPath="yes" />
            <Environment Id="PATH" Name="PATH" Value="[INSTALLFOLDER]" Permanent="no" Part="last" Action="set" System="yes" />
          </Component>
EOF
fi

if [[ "$INCLUDE_GUI" == true ]]; then
    cat >> "$WIX_DIR/Product.wxs" << EOF
          <Component Id="GUI" Guid="$(generate_guid)" Win64="yes">
            <File Id="TopoGenGuiExe" Source="$(basename "$GUI_PATH")" KeyPath="yes" />
          </Component>
EOF
fi

# Add DLL components
cat >> "$WIX_DIR/Product.wxs" << EOF

          <!-- Dependencies -->
          <Component Id="Dependencies" Guid="$(generate_guid)" Win64="yes">
            <Environment Id="GDAL_DATA" Name="GDAL_DATA" Value="[INSTALLFOLDER]data\gdal" Permanent="no" Action="set" System="yes" />
EOF

# Add all DLL files
DLL_COUNT=0
for dll in "$DEPS_DIR"/*.dll; do
    if [[ -f "$dll" ]]; then
        DLL_NAME=$(basename "$dll")
        DLL_ID="Dll_$(printf '%03d' $DLL_COUNT)"
        cat >> "$WIX_DIR/Product.wxs" << EOF
            <File Id="$DLL_ID" Source="$DLL_NAME" />
EOF
        DLL_COUNT=$((DLL_COUNT + 1))
    fi
done

cat >> "$WIX_DIR/Product.wxs" << EOF
          </Component>

          <!-- Data directory -->
          <Directory Id="DataFolder" Name="data">
            <Directory Id="GdalDataFolder" Name="gdal">
              <Component Id="GdalData" Guid="$(generate_guid)" Win64="yes">
                <CreateFolder />
              </Component>
            </Directory>
          </Directory>

        </Directory>
      </Directory>

      <!-- Start Menu -->
      <Directory Id="ProgramMenuFolder">
        <Directory Id="ApplicationProgramsFolder" Name="$PRODUCT_NAME">
EOF

if [[ "$INCLUDE_GUI" == true ]]; then
    cat >> "$WIX_DIR/Product.wxs" << EOF
          <Component Id="ApplicationShortcut" Guid="$(generate_guid)">
            <Shortcut Id="ApplicationStartMenuShortcut"
                      Name="$PRODUCT_NAME"
                      Description="Generate topographic models"
                      Target="[INSTALLFOLDER]$(basename "$GUI_PATH")"
                      WorkingDirectory="INSTALLFOLDER" />
            <RemoveFolder Id="CleanUpShortCut" Directory="ApplicationProgramsFolder" On="uninstall" />
            <RegistryValue Root="HKCU" Key="Software\\$MANUFACTURER\\$PRODUCT_NAME" Name="installed" Type="integer" Value="1" KeyPath="yes" />
          </Component>
EOF
fi

cat >> "$WIX_DIR/Product.wxs" << EOF
        </Directory>
      </Directory>
    </Directory>

    <!-- Features -->
    <Feature Id="ProductFeature" Title="$PRODUCT_NAME" Level="1">
EOF

if [[ "$INCLUDE_CLI" == true ]]; then
    echo "      <ComponentRef Id=\"CLI\" />" >> "$WIX_DIR/Product.wxs"
fi

if [[ "$INCLUDE_GUI" == true ]]; then
    echo "      <ComponentRef Id=\"GUI\" />" >> "$WIX_DIR/Product.wxs"
    echo "      <ComponentRef Id=\"ApplicationShortcut\" />" >> "$WIX_DIR/Product.wxs"
fi

cat >> "$WIX_DIR/Product.wxs" << EOF
      <ComponentRef Id="Dependencies" />
      <ComponentRef Id="GdalData" />
    </Feature>

    <!-- UI -->
    <UIRef Id="WixUI_InstallDir" />
    <Property Id="WIXUI_INSTALLDIR" Value="INSTALLFOLDER" />

    <!-- License -->
    <WixVariable Id="WixUILicenseRtf" Value="License.rtf" />

  </Product>
</Wix>
EOF

log_success "Generated Product.wxs"

# Generate License.rtf
cat > "$WIX_DIR/License.rtf" << 'EOF'
{\rtf1\ansi\deff0\nouicompat{\fonttbl{\f0\fnil\fcharset0 Calibri;}}
{\*\generator Riched20 10.0.19041}\viewkind4\uc1
\pard\sa200\sl276\slmult1\f0\fs22\lang9 MIT License\par
Copyright (c) 2025 Matthew Block\par
\par
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\par
\par
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\par
\par
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\par
}
EOF

log_success "Generated License.rtf"

# Copy executables and DLLs to WiX directory
log_info "Copying files to WiX directory..."

if [[ "$INCLUDE_CLI" == true ]]; then
    cp "$CLI_PATH" "$WIX_DIR/"
fi

if [[ "$INCLUDE_GUI" == true ]]; then
    cp "$GUI_PATH" "$WIX_DIR/"
fi

cp "$DEPS_DIR"/*.dll "$WIX_DIR/" 2>/dev/null || true

log_success "Files copied"

# Save WiX sources to output directory
WIX_SOURCE_DIR="$OUTPUT_DIR/wix-sources"
mkdir -p "$WIX_SOURCE_DIR"
cp -R "$WIX_DIR"/* "$WIX_SOURCE_DIR/"

log_success "WiX source files saved to: $WIX_SOURCE_DIR"

# Create build script for Windows
cat > "$WIX_SOURCE_DIR/build.bat" << 'EOF'
@echo off
REM Build script for Windows MSI installer
REM Requires WiX Toolset 3.11+ installed

echo Building MSI installer...

REM Check if WiX is installed
where candle.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: WiX Toolset not found in PATH
    echo Please install WiX Toolset from https://wixtoolset.org/
    exit /b 1
)

REM Compile WiX sources
echo Compiling Product.wxs...
candle.exe Product.wxs -ext WixUIExtension
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilation failed
    exit /b 1
)

REM Link to create MSI
echo Linking MSI...
light.exe Product.wixobj -ext WixUIExtension -out topo-gen.msi
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Linking failed
    exit /b 1
)

echo.
echo SUCCESS: MSI installer created: topo-gen.msi
echo.

REM Sign if certificate provided
if not "%1"=="" (
    echo Signing MSI with certificate: %1
    signtool.exe sign /sha1 %1 /fd SHA256 /t http://timestamp.digicert.com topo-gen.msi
    if %ERRORLEVEL% EQU 0 (
        echo MSI signed successfully
    ) else (
        echo WARNING: Code signing failed
    )
)

exit /b 0
EOF

chmod +x "$WIX_SOURCE_DIR/build.bat"

log_success "Created build.bat for Windows compilation"

# Create build script for cross-platform (if WiX available via Wine)
cat > "$WIX_SOURCE_DIR/build.sh" << 'EOF'
#!/bin/bash
# Build script for MSI installer (requires WiX Toolset on Windows or Wine)

set -e

echo "Building MSI installer..."

# Check if candle.exe is available
if ! command -v candle.exe &> /dev/null; then
    echo "ERROR: WiX Toolset not found"
    echo "On Windows: Install from https://wixtoolset.org/"
    echo "On Linux/macOS: Install WiX via Wine"
    exit 1
fi

# Compile
echo "Compiling Product.wxs..."
candle.exe Product.wxs -ext WixUIExtension

# Link
echo "Linking MSI..."
light.exe Product.wixobj -ext WixUIExtension -out topo-gen.msi

echo ""
echo "SUCCESS: MSI installer created: topo-gen.msi"
echo ""

# Sign if certificate provided
if [[ -n "$1" ]]; then
    echo "Signing MSI with certificate: $1"
    signtool.exe sign /sha1 "$1" /fd SHA256 /t http://timestamp.digicert.com topo-gen.msi
    echo "MSI signed successfully"
fi
EOF

chmod +x "$WIX_SOURCE_DIR/build.sh"

log_success "Created build.sh for cross-platform compilation"

# Create README
cat > "$WIX_SOURCE_DIR/README.md" << EOF
# WiX Source Files for MSI Installer

Generated MSI installer sources for $PRODUCT_NAME v$VERSION.

## Building the MSI

### On Windows (with WiX Toolset installed)

\`\`\`batch
build.bat
\`\`\`

### With Code Signing

\`\`\`batch
build.bat YOUR_CERT_THUMBPRINT
\`\`\`

### Requirements

- WiX Toolset 3.11+ from https://wixtoolset.org/
- (Optional) Code signing certificate for signtool.exe

### Output

- \`topo-gen.msi\` - Installable MSI package

## Files Included

- \`Product.wxs\` - Main WiX source file
- \`License.rtf\` - MIT License in RTF format
- \`*.exe\` - Executables
- \`*.dll\` - Dependency libraries
- \`build.bat\` - Windows build script
- \`build.sh\` - Cross-platform build script

## Installation Locations

- CLI: \`C:\\Program Files\\$PRODUCT_NAME\\topo-gen.exe\`
- GUI: \`C:\\Program Files\\$PRODUCT_NAME\\topo-gen-gui.exe\`
- DLLs: \`C:\\Program Files\\$PRODUCT_NAME\\*.dll\`
- Data: \`C:\\Program Files\\$PRODUCT_NAME\\data\\gdal\`

## Features

- Adds CLI to system PATH
- Sets GDAL_DATA environment variable
- Creates Start Menu shortcut (if GUI included)
- Proper Add/Remove Programs integration
- Major upgrade support (uninstalls old version first)

## Manual Compilation Steps

If you prefer to compile manually:

\`\`\`batch
REM 1. Compile WiX source
candle.exe Product.wxs -ext WixUIExtension

REM 2. Link to create MSI
light.exe Product.wixobj -ext WixUIExtension -out topo-gen.msi

REM 3. (Optional) Sign
signtool.exe sign /sha1 CERT_HASH /fd SHA256 /t http://timestamp.digicert.com topo-gen.msi
\`\`\`

## Customization

To customize the installer, edit \`Product.wxs\`:

- Change installation directory: Modify \`<Directory Id="INSTALLFOLDER">\`
- Add/remove components: Modify \`<Component>\` elements
- Change UI: Modify \`<UIRef>\` or add custom dialogs
- Update license: Replace \`License.rtf\`

## Testing

After building:

1. Install: Double-click \`topo-gen.msi\`
2. Verify CLI: Open cmd.exe and run \`topo-gen --version\`
3. Verify GUI: Check Start Menu for shortcut
4. Uninstall: Use Add/Remove Programs

## Troubleshooting

**Error: "candle.exe not found"**
- Install WiX Toolset and ensure it's in PATH

**Error: "light.exe failed with exit code"**
- Check Product.wxs for XML errors
- Ensure all file references exist

**MSI install fails**
- Check Windows Event Viewer for MSI logs
- Run with logging: \`msiexec /i topo-gen.msi /l*v install.log\`

## References

- WiX Tutorial: https://www.firegiant.com/wix/tutorial/
- WiX Documentation: https://wixtoolset.org/documentation/
EOF

log_success "Created README.md in WiX sources"

# Attempt compilation if requested and on Windows
if [[ "$COMPILE" == true ]]; then
    log_info "Attempting to compile MSI..."

    if command -v candle.exe &> /dev/null; then
        cd "$WIX_SOURCE_DIR"

        log_info "Running candle.exe..."
        if candle.exe Product.wxs -ext WixUIExtension; then
            log_success "Compilation successful"

            log_info "Running light.exe..."
            if light.exe Product.wixobj -ext WixUIExtension -out "topo-gen-${VERSION}.msi"; then
                log_success "MSI created successfully"

                # Sign if certificate provided
                if [[ -n "$SIGN_IDENTITY" ]] && command -v signtool.exe &> /dev/null; then
                    log_info "Signing MSI..."
                    if signtool.exe sign /sha1 "$SIGN_IDENTITY" /fd SHA256 /t http://timestamp.digicert.com "topo-gen-${VERSION}.msi"; then
                        log_success "MSI signed successfully"
                    else
                        log_warning "Code signing failed"
                    fi
                fi

                # Copy to output directory
                cp "topo-gen-${VERSION}.msi" "$OUTPUT_DIR/"
                log_success "MSI installer: $OUTPUT_DIR/topo-gen-${VERSION}.msi"
            else
                log_error "Linking failed"
            fi
        else
            log_error "Compilation failed"
        fi

        cd - > /dev/null
    else
        log_warning "WiX Toolset not found - cannot compile"
        log_info "Install WiX from https://wixtoolset.org/"
    fi
fi

# Clean up temporary directory
rm -rf "$TEMP_DIR"

echo ""
log_success "WiX source files created successfully!"
log_info "Location: $WIX_SOURCE_DIR"
echo ""

log_info "Next steps:"
echo ""
echo "  1. Transfer WiX sources to Windows machine:"
echo "     Copy $WIX_SOURCE_DIR to Windows"
echo ""
echo "  2. Build MSI on Windows:"
echo "     cd wix-sources"
echo "     build.bat"
echo ""
echo "  3. (Optional) Sign MSI:"
echo "     build.bat YOUR_CERT_THUMBPRINT"
echo ""
echo "  4. Test installation:"
echo "     Double-click topo-gen-${VERSION}.msi"
echo "     Verify with: topo-gen --version"
echo ""
echo "  5. Distribute:"
echo "     - Upload to website/GitHub releases"
echo "     - Users double-click to install"
echo ""

if [[ -z "$SIGN_IDENTITY" ]]; then
    log_warning "MSI is not code-signed"
    log_info "Users may see SmartScreen warnings"
    log_info "To sign: build.bat YOUR_CERT_THUMBPRINT"
fi
