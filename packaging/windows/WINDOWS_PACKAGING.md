# Windows Packaging Guide

Complete guide for creating Windows distribution packages (MSI, WinGet, Portable ZIP).

## Prerequisites

- Windows 10+ (x64)
- Built executables in `build/`
- Bundled dependencies (DLLs)
- (Optional) WiX Toolset 3.11+ for MSI compilation
- (Optional) Code signing certificate

## Quick Start

### Create All Windows Packages

```bash
# Build everything and create all Windows packages
./scripts/deploy.sh --all-windows --clean-build
```

This creates:
- Source tarball (`dist/source/`)
- Binary package (`dist/windows-x64/`)
- MSI installer WiX sources (`dist/windows-x64/wix-sources/`)
- WinGet package manifest (`dist/winget/`)
- Portable ZIP package (`dist/windows-x64/`)

### Individual Package Types

#### MSI Installer

```bash
# Generate WiX source files (cross-platform)
./scripts/deploy.sh --msi

# Or use script directly
./scripts/package/create-msi.sh \
    --cli build/topo-gen.exe \
    --gui build/topo-gen-gui.exe \
    --deps-dir dist/windows-deps
```

**Output**: `dist/windows-x64/wix-sources/` (requires Windows + WiX to compile)

**Use case**: Professional system-wide installation, enterprise deployment, Add/Remove Programs integration

#### WinGet Manifest

```bash
# Generate WinGet manifest
./scripts/deploy.sh --winget

# Or use script directly
./scripts/package/create-winget.sh \
    --installer-url https://github.com/user/topo-gen/releases/download/v0.22.001/topo-gen-0.22.001.msi \
    --version 0.22.001
```

**Output**: `dist/winget/manifests/`

**Use case**: Windows Package Manager distribution, modern Windows 10/11 users

#### Portable ZIP

```bash
# Create portable ZIP package
./scripts/deploy.sh --portable-zip

# Or use script directly
./scripts/package/create-portable-zip.sh \
    --cli build/topo-gen.exe \
    --gui build/topo-gen-gui.exe \
    --deps-dir dist/windows-deps
```

**Output**: `dist/windows-x64/topo-gen-{version}-windows-x64-portable.zip`

**Use case**: No-install deployment, USB drives, users without admin rights

## MSI Installer Details

### Overview

MSI (Microsoft Installer) provides professional Windows installation:
- Add/Remove Programs integration
- System-wide installation
- Start Menu shortcuts
- Automatic PATH configuration
- Proper uninstall support
- Group Policy deployment (enterprises)

### Creating MSI Packages

#### 1. Generate WiX Sources (Any Platform)

```bash
./scripts/package/create-msi.sh \
    --cli build/topo-gen.exe \
    --gui build/topo-gen-gui.exe \
    --deps-dir dist/windows-deps \
    --version 0.22.001
```

This creates `dist/windows-x64/wix-sources/` with:
- `Product.wxs` - WiX source file
- `License.rtf` - MIT License in RTF format
- `*.exe`, `*.dll` - Executables and dependencies
- `build.bat` - Windows build script
- `build.sh` - Cross-platform build script
- `README.md` - Compilation instructions

#### 2. Compile MSI (Requires Windows + WiX)

On Windows with WiX Toolset installed:

```batch
cd dist\windows-x64\wix-sources
build.bat
```

This produces: `topo-gen-{version}.msi`

#### 3. Sign MSI (Optional but Recommended)

```batch
build.bat YOUR_CERT_THUMBPRINT
```

Or manually:

```batch
signtool.exe sign /sha1 CERT_HASH /fd SHA256 /t http://timestamp.digicert.com topo-gen.msi
```

### MSI Installation Locations

- CLI: `C:\Program Files\Topographic Generator\topo-gen.exe`
- GUI: `C:\Program Files\Topographic Generator\topo-gen-gui.exe`
- DLLs: `C:\Program Files\Topographic Generator\*.dll`
- Data: `C:\Program Files\Topographic Generator\data\gdal\`
- Start Menu: `Start Menu\Programs\Topographic Generator\`

### MSI Features

- **Automatic PATH Setup**: Adds installation directory to system PATH
- **Environment Variables**: Sets GDAL_DATA automatically
- **Start Menu Shortcut**: Creates GUI shortcut (if GUI included)
- **Major Upgrades**: Automatically uninstalls old versions
- **Silent Installation**: Supports `/quiet` and `/passive` flags
- **Administrative Install**: Installs for all users

### WiX Toolset Installation

Download from: https://wixtoolset.org/

```batch
# Verify installation
candle.exe -?
light.exe -?
```

## WinGet Package Manager

### Overview

WinGet is Microsoft's official package manager for Windows:
- Built into Windows 11
- Available for Windows 10
- Community-driven package repository
- Automatic updates
- Simple installation: `winget install topo-gen`

### Creating WinGet Manifests

#### 1. Generate Manifest

```bash
./scripts/package/create-winget.sh \
    --installer-url https://github.com/matthewblock/topo-gen/releases/download/v0.22.001/topo-gen-0.22.001.msi \
    --version 0.22.001
```

This creates `dist/winget/manifests/` with:
- `{PackageId}.yaml` - Version manifest
- `{PackageId}.installer.yaml` - Installer manifest with URL and SHA256
- `{PackageId}.locale.en-US.yaml` - English locale manifest
- `README.md` - Submission guide
- `SUBMISSION_CHECKLIST.md` - Pre-flight checklist
- `validate.ps1` - PowerShell validation script

#### 2. Validate Manifest Locally

```powershell
# Windows PowerShell
winget validate dist\winget\manifests\MatthewBlock\TopographicGenerator\0.22.001\
```

#### 3. Test Installation Locally

```powershell
winget install --manifest dist\winget\manifests\MatthewBlock\TopographicGenerator\0.22.001\
```

#### 4. Update Product Code

Extract Product Code from MSI:

```powershell
# Right-click MSI > Properties > Details > Product Code
# Or check WiX Product.wxs for Product Id GUID
```

Edit `{PackageId}.installer.yaml`:

```yaml
ProductCode: '{YOUR-ACTUAL-GUID-HERE}'
```

### Submitting to winget-pkgs Repository

#### Prerequisites

1. MSI installer built and uploaded to public URL (GitHub releases)
2. SHA256 checksum calculated
3. Fork of [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)

#### Submission Steps

1. **Fork and Clone**:

```bash
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs
```

2. **Copy Manifests**:

```bash
# Copy generated manifests to winget-pkgs
cp -r dist/winget/manifests/MatthewBlock/TopographicGenerator/0.22.001 \
      manifests/m/MatthewBlock/TopographicGenerator/0.22.001/
```

3. **Create Branch and Commit**:

```bash
git checkout -b MatthewBlock.TopographicGenerator-0.22.001
git add manifests/m/MatthewBlock/TopographicGenerator/0.22.001/
git commit -m "Add MatthewBlock.TopographicGenerator version 0.22.001"
git push origin MatthewBlock.TopographicGenerator-0.22.001
```

4. **Create Pull Request** on GitHub

5. **Automated Validation** will run:
   - YAML syntax validation
   - Required fields check
   - URL accessibility test
   - SHA256 verification
   - Test installation on Windows VM

#### After Merge

Users can install with:

```powershell
winget search topo-gen
winget install MatthewBlock.TopographicGenerator
```

## Portable ZIP Package

### Overview

Portable ZIP provides:
- No installation required
- Runs from any directory or USB drive
- No admin privileges needed
- Self-contained with all dependencies
- Launcher scripts set environment variables

### Creating Portable ZIP

```bash
./scripts/package/create-portable-zip.sh \
    --cli build/topo-gen.exe \
    --gui build/topo-gen-gui.exe \
    --deps-dir dist/windows-deps \
    --data-dir /path/to/gdal/data
```

### Package Contents

```
topo-gen-0.22.001-portable/
├── topo-gen.exe           # CLI executable
├── topo-gen-gui.exe       # GUI application
├── *.dll                  # All dependency libraries
├── data/
│   └── gdal/              # GDAL data files
├── topo-gen.bat           # Batch launcher
├── topo-gen.ps1           # PowerShell launcher
├── topo-gen-gui.bat       # GUI batch launcher
├── README.txt             # Usage instructions
└── LICENSE.txt            # MIT License
```

### Launcher Scripts

#### Batch Launcher (`topo-gen.bat`)

Sets environment variables and launches CLI:

```batch
@echo off
set SCRIPT_DIR=%~dp0
set GDAL_DATA=%SCRIPT_DIR%data\gdal
set PATH=%SCRIPT_DIR%;%PATH%
"%SCRIPT_DIR%\topo-gen.exe" %*
```

#### PowerShell Launcher (`topo-gen.ps1`)

Modern PowerShell script:

```powershell
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$env:GDAL_DATA = Join-Path $ScriptDir "data\gdal"
$env:PATH = "$ScriptDir;$env:PATH"
& "$ScriptDir\topo-gen.exe" @Arguments
```

### Usage

Extract ZIP and:

**GUI**: Double-click `topo-gen-gui.exe` or `topo-gen-gui.bat`

**CLI**: Open Command Prompt and run:

```batch
cd path\to\extracted\folder
topo-gen.bat --help
topo-gen.bat --upper-left 47.6062,-122.3321 --lower-right 47.6020,-122.3280
```

**PowerShell**:

```powershell
cd path\to\extracted\folder
.\topo-gen.ps1 --help
```

## Code Signing

### Why Sign?

**Without signing**:
- Windows SmartScreen warnings
- "Windows protected your PC" messages
- Users must click "More info" → "Run anyway"
- Potential security warnings in enterprise environments

**With signing**:
- No warnings for users
- Professional appearance
- Required for some enterprise deployments
- Builds trust with users

### Requirements

- Code signing certificate from trusted CA (DigiCert, Sectigo, etc.)
- EV (Extended Validation) certificate recommended for instant trust
- Standard code signing certificate requires reputation building
- Cost: $100-$500/year

### Signing Executables

```batch
signtool.exe sign /sha1 CERT_THUMBPRINT /fd SHA256 /t http://timestamp.digicert.com topo-gen.exe
signtool.exe sign /sha1 CERT_THUMBPRINT /fd SHA256 /t http://timestamp.digicert.com topo-gen-gui.exe
```

### Signing MSI

```batch
signtool.exe sign /sha1 CERT_THUMBPRINT /fd SHA256 /t http://timestamp.digicert.com topo-gen.msi
```

### Signing in Build Scripts

WiX build script supports signing:

```batch
cd wix-sources
build.bat YOUR_CERT_THUMBPRINT
```

### Certificate Stores

#### View Certificates

```batch
certutil -store My
```

#### Export Certificate Thumbprint

```powershell
Get-ChildItem -Path Cert:\CurrentUser\My | Where-Object {$_.Subject -like "*Your Name*"}
```

## Dependency Bundling

### Prerequisites

All Windows packages require bundled DLLs. Use Phase 1 bundling script:

```bash
./packaging/windows/bundle_deps.sh \
    --executable build/topo-gen.exe \
    --output dist/windows-deps
```

This bundles:
- GDAL DLLs
- CGAL/GMP dependencies
- TBB libraries
- Qt6 libraries (for GUI)
- MSVC runtime

### Verifying Dependencies

Check executable dependencies:

```batch
# Using Dependency Walker or Dependencies.exe
Dependencies.exe topo-gen.exe

# Or via PowerShell
dumpbin /dependents topo-gen.exe
```

## Testing Packages

### MSI Testing

```batch
# Install
msiexec /i topo-gen-0.22.001.msi

# Silent install
msiexec /i topo-gen-0.22.001.msi /quiet /norestart

# With logging
msiexec /i topo-gen-0.22.001.msi /l*v install.log

# Verify installation
topo-gen --version
topo-gen --help

# Check Start Menu
# Look for: Start Menu > Topographic Generator

# Uninstall
msiexec /x topo-gen-0.22.001.msi
# Or use: Add/Remove Programs
```

### WinGet Testing

```powershell
# Validate manifest
winget validate dist\winget\manifests\...

# Test local installation
winget install --manifest dist\winget\manifests\...

# Verify
topo-gen --version

# Uninstall
winget uninstall MatthewBlock.TopographicGenerator
```

### Portable ZIP Testing

```batch
# Extract ZIP
Expand-Archive topo-gen-0.22.001-portable.zip -DestinationPath C:\Test

# Test CLI
cd C:\Test\topo-gen-0.22.001-portable
topo-gen.bat --version

# Test GUI
topo-gen-gui.bat

# Test PowerShell
powershell -File topo-gen.ps1 --help

# Verify no dependencies missing
# Should run without errors
```

## Distribution Workflow

### Complete Release Process

```bash
# 1. Update version in CMakeLists.txt
# project(... VERSION 0.23.0)

# 2. Build on Windows
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Bundle dependencies
./packaging/windows/bundle_deps.sh \
    --executable build/Release/topo-gen.exe \
    --output dist/windows-deps

# 4. Create all Windows packages
./scripts/deploy.sh --all-windows

# 5. Compile MSI on Windows
cd dist/windows-x64/wix-sources
build.bat

# 6. (Optional) Sign MSI
build.bat YOUR_CERT_THUMBPRINT

# 7. Upload MSI to GitHub releases
gh release create v0.23.0 \
    dist/windows-x64/wix-sources/topo-gen-0.23.0.msi \
    dist/windows-x64/topo-gen-0.23.0-portable.zip \
    --title "Topographic Generator v0.23.0" \
    --notes "Release notes here"

# 8. Update WinGet manifest with actual URL and SHA256
./scripts/package/create-winget.sh \
    --installer-url https://github.com/user/topo-gen/releases/download/v0.23.0/topo-gen-0.23.0.msi

# 9. Submit to winget-pkgs repository
# Follow winget submission guide in dist/winget/README.md
```

## Troubleshooting

### MSI Creation Fails

**Error**: "candle.exe not found"
- **Solution**: Install WiX Toolset from https://wixtoolset.org/
- Ensure WiX is in PATH

**Error**: "Product.wxs has errors"
- **Solution**: Check WiX XML syntax
- Validate with: `candle.exe Product.wxs -ext WixUIExtension`

### MSI Installation Fails

**Error**: "Installation failed with error 1603"
- **Solution**: Check install.log for details
- Run with logging: `msiexec /i installer.msi /l*v install.log`
- Common causes: insufficient permissions, conflicting software

**Error**: "This installation package is not supported"
- **Solution**: Ensure MSI is for correct architecture (x64)
- Check Windows version compatibility

### WinGet Submission Fails

**Error**: "InstallerSha256 mismatch"
- **Solution**: Recalculate SHA256: `certutil -hashfile installer.msi SHA256`
- Update manifest with correct hash (uppercase)

**Error**: "InstallerUrl not accessible"
- **Solution**: Ensure URL is public and returns HTTP 200
- GitHub releases must be published (not draft)

### Portable ZIP Issues

**Error**: "Missing DLL: xxx.dll"
- **Solution**: Bundle missing DLL from dependencies
- Use Dependencies.exe to identify all required DLLs

**Error**: "GDAL_DATA not found"
- **Solution**: Use launcher scripts (topo-gen.bat) which set GDAL_DATA
- Or manually: `set GDAL_DATA=%CD%\data\gdal`

**SmartScreen Warning**:
- **Solution**: Code sign executables
- Or users: Click "More info" → "Run anyway"

## Reference

- **WiX Toolset**: https://wixtoolset.org/
- **WiX Documentation**: https://wixtoolset.org/documentation/
- **WinGet CLI**: https://learn.microsoft.com/en-us/windows/package-manager/winget/
- **winget-pkgs Repository**: https://github.com/microsoft/winget-pkgs
- **Code Signing**: https://learn.microsoft.com/en-us/windows/win32/seccrypto/cryptography-tools

## Support

For issues with packaging scripts:
- `PACKAGING.md` - General packaging guide
- `packaging/common/dependencies.md` - Dependency details
- `packaging/windows/bundle_deps.sh` - Windows dependency bundling

For WiX or MSI issues:
- WiX documentation and community forums

For WinGet issues:
- microsoft/winget-pkgs contribution guidelines
