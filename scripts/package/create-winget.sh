#!/bin/bash
#
# create-winget.sh - Generate WinGet package manifest
#
# Creates WinGet (Windows Package Manager) manifest files for Topographic Generator.
# Can be submitted to the winget-pkgs repository for official distribution.
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

Generate WinGet package manifest for Topographic Generator.

Options:
    --installer-url URL     MSI installer URL (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --sha256 HASH           SHA256 checksum (auto-calculated if URL is local file)
    --output-dir DIR        Output directory (default: dist/winget/)
    --publisher NAME        Publisher name (default: Matthew Block)
    --package-id ID         Package identifier (default: MatthewBlock.TopographicGenerator)
    --homepage URL          Homepage URL
    --license LICENSE       License (default: MIT)
    --help                  Show this help message

Examples:
    # From GitHub release
    $0 --installer-url https://github.com/user/topo-gen/releases/download/v0.22.001/topo-gen-0.22.001.msi

    # From local file (calculates SHA256)
    $0 --installer-url dist/windows-x64/topo-gen-0.22.001.msi

    # With explicit SHA256
    $0 --installer-url URL --sha256 abc123...

    # Custom output location
    $0 --installer-url URL --output-dir ~/winget-manifests

Notes:
    - If --sha256 is not provided and URL is a local file, SHA256 is calculated
    - Package ID format: Publisher.PackageName (no spaces)
    - For submission to winget-pkgs, follow naming conventions
    - WinGet manifest spec: https://github.com/microsoft/winget-cli

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
INSTALLER_URL=""
VERSION=$(get_version)
SHA256=""
OUTPUT_DIR="$PROJECT_ROOT/dist/winget"
PUBLISHER="Matthew Block"
PACKAGE_ID="MatthewBlock.TopographicGenerator"
HOMEPAGE="https://github.com/matthewblock/topo-gen"
LICENSE="MIT"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --installer-url)
            INSTALLER_URL="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --sha256)
            SHA256="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --publisher)
            PUBLISHER="$2"
            shift 2
            ;;
        --package-id)
            PACKAGE_ID="$2"
            shift 2
            ;;
        --homepage)
            HOMEPAGE="$2"
            shift 2
            ;;
        --license)
            LICENSE="$2"
            shift 2
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
if [[ -z "$INSTALLER_URL" ]]; then
    log_error "Installer URL is required"
    usage
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "WinGet Manifest Generation"
log_info "  Package ID: $PACKAGE_ID"
log_info "  Version: $VERSION"
log_info "  Installer: $INSTALLER_URL"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Calculate SHA256 if not provided and URL is local file
if [[ -z "$SHA256" ]]; then
    if [[ -f "$INSTALLER_URL" ]]; then
        log_info "Calculating SHA256 checksum..."
        if command -v shasum &> /dev/null; then
            SHA256=$(shasum -a 256 "$INSTALLER_URL" | awk '{print $1}')
        elif command -v sha256sum &> /dev/null; then
            SHA256=$(sha256sum "$INSTALLER_URL" | awk '{print $1}')
        else
            log_error "Neither shasum nor sha256sum found"
            exit 1
        fi
        log_success "SHA256: $SHA256"
    else
        log_warning "SHA256 not provided and URL is not a local file"
        log_info "You will need to add SHA256 manually to the installer manifest"
        SHA256="<INSERT_SHA256_HERE>"
    fi
fi

# Convert to uppercase for WinGet convention
SHA256=$(echo "$SHA256" | tr '[:lower:]' '[:upper:]')

# Create manifest directory structure
MANIFEST_DIR="$OUTPUT_DIR/manifests/${PACKAGE_ID//.//}/$VERSION"
mkdir -p "$MANIFEST_DIR"

log_info "Generating WinGet manifest files..."

# Generate version manifest (main manifest)
cat > "$MANIFEST_DIR/${PACKAGE_ID}.yaml" << EOF
# Created using create-winget.sh
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.version.1.6.0.schema.json

PackageIdentifier: $PACKAGE_ID
PackageVersion: $VERSION
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
EOF

log_success "Generated version manifest"

# Generate installer manifest
cat > "$MANIFEST_DIR/${PACKAGE_ID}.installer.yaml" << EOF
# Created using create-winget.sh
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.installer.1.6.0.schema.json

PackageIdentifier: $PACKAGE_ID
PackageVersion: $VERSION
Platform:
- Windows.Desktop
MinimumOSVersion: 10.0.0.0
InstallerType: wix
Scope: machine
InstallModes:
- interactive
- silent
- silentWithProgress
InstallerSwitches:
  Silent: /quiet /norestart
  SilentWithProgress: /passive /norestart
  Custom: /norestart
UpgradeBehavior: install
ProductCode: '{PRODUCT_CODE_PLACEHOLDER}'
Installers:
- Architecture: x64
  InstallerUrl: $INSTALLER_URL
  InstallerSha256: $SHA256
ManifestType: installer
ManifestVersion: 1.6.0
EOF

log_success "Generated installer manifest"

# Generate locale manifest (default: en-US)
cat > "$MANIFEST_DIR/${PACKAGE_ID}.locale.en-US.yaml" << EOF
# Created using create-winget.sh
# yaml-language-server: \$schema=https://aka.ms/winget-manifest.defaultLocale.1.6.0.schema.json

PackageIdentifier: $PACKAGE_ID
PackageVersion: $VERSION
PackageLocale: en-US
Publisher: $PUBLISHER
PublisherUrl: $HOMEPAGE
PublisherSupportUrl: $HOMEPAGE/issues
PackageName: Topographic Generator
PackageUrl: $HOMEPAGE
License: $LICENSE
LicenseUrl: $HOMEPAGE/blob/main/LICENSE
ShortDescription: High-performance topographic model generator for laser cutting and 3D printing
Description: |-
  Topographic Generator is a professional-grade tool for creating laser-cuttable
  topographic models from elevation data. It supports SRTM elevation tiles,
  contour generation, and exports to SVG (laser cutting) and STL (3D printing).

  Features:
  - High-performance C++ implementation with CGAL geometry processing
  - Automatic SRTM elevation data downloading
  - Contour polygon generation and simplification
  - OpenStreetMap feature integration (roads, buildings, waterways)
  - Multi-format export (SVG, STL, GeoJSON, Shapefile)
  - Command-line interface and GUI application

  Use cases:
  - Laser cutting layered topographic models
  - 3D printing elevation maps
  - Educational geography projects
  - Custom topographic artwork
Moniker: topo-gen
Tags:
- topography
- laser-cutting
- 3d-printing
- elevation
- contours
- maps
- geography
- srtm
- dem
- gis
ManifestType: defaultLocale
ManifestVersion: 1.6.0
EOF

log_success "Generated locale manifest (en-US)"

# Create README for submission
cat > "$OUTPUT_DIR/README.md" << EOF
# WinGet Package Manifest for Topographic Generator

Generated WinGet package manifest for Topographic Generator v$VERSION.

## Files Generated

\`\`\`
manifests/
└── ${PACKAGE_ID//.//}/
    └── $VERSION/
        ├── $PACKAGE_ID.yaml                    # Version manifest
        ├── $PACKAGE_ID.installer.yaml          # Installer manifest
        └── $PACKAGE_ID.locale.en-US.yaml       # Locale manifest (en-US)
\`\`\`

## Local Testing

### 1. Install winget CLI
WinGet is included in Windows 11 and recent Windows 10 builds.

### 2. Test manifest locally

\`\`\`powershell
# Validate manifest
winget validate $MANIFEST_DIR

# Test installation from local manifest
winget install --manifest $MANIFEST_DIR
\`\`\`

### 3. Verify installation

\`\`\`cmd
topo-gen --version
\`\`\`

### 4. Uninstall

\`\`\`powershell
winget uninstall $PACKAGE_ID
\`\`\`

## Submitting to winget-pkgs Repository

### Prerequisites

1. GitHub account
2. Forked copy of [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs)
3. MSI installer hosted on public URL (GitHub releases recommended)

### Submission Steps

#### 1. Fork and Clone

\`\`\`bash
# Fork microsoft/winget-pkgs on GitHub
git clone https://github.com/YOUR_USERNAME/winget-pkgs.git
cd winget-pkgs
\`\`\`

#### 2. Copy Manifests

\`\`\`bash
# Copy manifest directory to winget-pkgs
cp -r $MANIFEST_DIR manifests/${PACKAGE_ID:0:1}/${PACKAGE_ID//.//}/$VERSION/
\`\`\`

#### 3. Update Product Code

Edit the installer manifest to add the actual MSI Product Code:

\`\`\`bash
# Get Product Code from MSI
# On Windows:
# - Right-click MSI > Properties > Details
# - Or use: msiexec /i topo-gen.msi /qn /l*v install.log
#   then search install.log for "ProductCode"

# Update ProductCode in installer manifest
ProductCode: '{YOUR-ACTUAL-GUID-HERE}'
\`\`\`

#### 4. Validate

\`\`\`powershell
# Validate manifest locally
winget validate manifests/${PACKAGE_ID:0:1}/${PACKAGE_ID//.//}/$VERSION/
\`\`\`

#### 5. Create Pull Request

\`\`\`bash
git checkout -b $PACKAGE_ID-$VERSION
git add manifests/${PACKAGE_ID:0:1}/${PACKAGE_ID//.//}/$VERSION/
git commit -m "Add $PACKAGE_ID version $VERSION"
git push origin $PACKAGE_ID-$VERSION
\`\`\`

Then create a pull request on GitHub.

#### 6. Automated Validation

The winget-pkgs repository has automated checks that will:
- Validate YAML syntax
- Check required fields
- Verify installer URL is accessible
- Validate SHA256 checksum
- Test installation on Windows VM

### Pull Request Guidelines

- **Title**: \`Add $PACKAGE_ID version $VERSION\`
- **Description**: Brief description of package
- Follow the PR template
- Respond to reviewer feedback
- Be patient - review can take several days

## Updating to New Versions

For subsequent releases:

1. Generate new manifest with updated version and installer URL
2. Copy to winget-pkgs repository
3. Create pull request with both old and new versions

WinGet supports multiple versions in the repository.

## Package Naming Conventions

- **Package ID**: \`Publisher.PackageName\` (no spaces)
- **Publisher**: Your name or organization
- **Package Name**: Product name without spaces
- **Moniker**: Short command-line friendly name (lowercase)

## Required Fields

### Version Manifest
- PackageIdentifier
- PackageVersion
- DefaultLocale
- ManifestType
- ManifestVersion

### Installer Manifest
- PackageIdentifier
- PackageVersion
- Installers (with Architecture, InstallerUrl, InstallerSha256)
- ManifestType
- ManifestVersion

### Locale Manifest
- PackageIdentifier
- PackageVersion
- PackageLocale
- Publisher
- PackageName
- License
- ShortDescription
- ManifestType
- ManifestVersion

## Optional but Recommended Fields

- PublisherUrl
- PublisherSupportUrl
- PackageUrl
- LicenseUrl
- Description
- Moniker
- Tags
- ProductCode (for MSI installers)

## Troubleshooting

### Validation Fails

**Error**: "Invalid YAML syntax"
- Check for proper indentation (2 spaces)
- Ensure no tabs
- Validate YAML at https://www.yamllint.com/

**Error**: "InstallerSha256 mismatch"
- Recalculate SHA256: \`certutil -hashfile installer.msi SHA256\`
- Ensure uppercase in manifest

**Error**: "InstallerUrl not accessible"
- Verify URL is public and returns 200 OK
- Check URL doesn't redirect to login page
- GitHub releases must be published (not draft)

### Installation Fails

**Error**: "Package not found"
- Ensure package is in winget-pkgs main branch
- Update winget: \`winget upgrade winget\`
- Refresh source: \`winget source reset\`

**Error**: "Installer failed to install"
- Test MSI manually
- Check MSI logs: \`msiexec /i installer.msi /l*v log.txt\`
- Verify system requirements

## References

- **WinGet Documentation**: https://learn.microsoft.com/en-us/windows/package-manager/
- **winget-pkgs Repository**: https://github.com/microsoft/winget-pkgs
- **Manifest Schema**: https://github.com/microsoft/winget-cli/tree/master/schemas
- **Contribution Guide**: https://github.com/microsoft/winget-pkgs/blob/master/CONTRIBUTING.md

## Support

For issues with this manifest generator, create an issue on GitHub.
For issues with WinGet or winget-pkgs submission, consult the official documentation.
EOF

log_success "Generated README.md"

# Create validation script
cat > "$OUTPUT_DIR/validate.ps1" << 'EOF'
# PowerShell script to validate WinGet manifest
# Run with: powershell -ExecutionPolicy Bypass -File validate.ps1

param(
    [string]$ManifestPath = "manifests"
)

Write-Host "Validating WinGet manifest..." -ForegroundColor Cyan

# Check if winget is available
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: winget not found" -ForegroundColor Red
    Write-Host "Install from: https://aka.ms/getwinget" -ForegroundColor Yellow
    exit 1
}

# Find manifest directory (looks for *.yaml files)
$manifestFiles = Get-ChildItem -Path $ManifestPath -Recurse -Filter "*.yaml"

if ($manifestFiles.Count -eq 0) {
    Write-Host "ERROR: No manifest files found in $ManifestPath" -ForegroundColor Red
    exit 1
}

$manifestDir = $manifestFiles[0].Directory.FullName

Write-Host "Found manifest directory: $manifestDir" -ForegroundColor Green
Write-Host ""

# Validate manifest
Write-Host "Running winget validate..." -ForegroundColor Cyan
winget validate $manifestDir

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "SUCCESS: Manifest is valid!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To test installation:" -ForegroundColor Cyan
    Write-Host "  winget install --manifest $manifestDir" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "ERROR: Validation failed" -ForegroundColor Red
    Write-Host "Review errors above and fix manifest files" -ForegroundColor Yellow
    exit 1
}
EOF

log_success "Generated validate.ps1"

# Create submission checklist
cat > "$OUTPUT_DIR/SUBMISSION_CHECKLIST.md" << EOF
# WinGet Submission Checklist

Use this checklist before submitting to microsoft/winget-pkgs.

## Pre-Submission

- [ ] MSI installer is built and tested
- [ ] MSI is uploaded to public URL (GitHub releases recommended)
- [ ] Installer URL is accessible without authentication
- [ ] SHA256 checksum is calculated and verified
- [ ] Product Code is extracted from MSI and added to manifest

## Manifest Validation

- [ ] All YAML files have valid syntax (no tabs, proper indentation)
- [ ] PackageIdentifier follows convention: Publisher.PackageName
- [ ] PackageVersion matches installer version
- [ ] License field is populated
- [ ] ShortDescription is under 256 characters
- [ ] Tags are relevant and lowercase
- [ ] All URLs are valid and accessible

## Local Testing

- [ ] Run: \`winget validate manifests/...\`
- [ ] Run: \`winget install --manifest manifests/...\`
- [ ] Verify CLI works: \`topo-gen --version\`
- [ ] Verify GUI launches (if included)
- [ ] Test uninstall: \`winget uninstall $PACKAGE_ID\`
- [ ] Verify clean uninstall (no leftover files)

## Repository Preparation

- [ ] Fork microsoft/winget-pkgs
- [ ] Create feature branch
- [ ] Copy manifest to correct directory structure
- [ ] Commit with message: "Add $PACKAGE_ID version $VERSION"
- [ ] Push branch to your fork

## Pull Request

- [ ] Create PR from your fork to microsoft/winget-pkgs
- [ ] Title: "Add $PACKAGE_ID version $VERSION"
- [ ] Fill out PR template
- [ ] Wait for automated checks to pass
- [ ] Respond to reviewer feedback
- [ ] Be patient (review can take days)

## Post-Merge

- [ ] Verify package appears in WinGet: \`winget search topo-gen\`
- [ ] Test installation from repository: \`winget install $PACKAGE_ID\`
- [ ] Update project documentation with WinGet installation instructions
- [ ] Announce availability to users

## Notes

- First-time submissions may take longer to review
- Maintainers may request changes to manifest
- Keep installer URL stable (don't delete releases)
- For updates, submit new version as separate PR
EOF

log_success "Generated SUBMISSION_CHECKLIST.md"

echo ""
log_success "WinGet manifest created successfully!"
log_info "Location: $MANIFEST_DIR"
echo ""

log_info "Manifest files:"
ls -1 "$MANIFEST_DIR"
echo ""

log_info "Next steps:"
echo ""
echo "  1. Validate manifest:"
echo "     winget validate $MANIFEST_DIR"
echo ""
echo "  2. Test locally (Windows):"
echo "     winget install --manifest $MANIFEST_DIR"
echo ""
echo "  3. Update Product Code in installer manifest:"
echo "     Edit: $MANIFEST_DIR/${PACKAGE_ID}.installer.yaml"
echo "     Replace: {PRODUCT_CODE_PLACEHOLDER}"
echo "     With MSI Product Code GUID"
echo ""
echo "  4. Submit to winget-pkgs:"
echo "     Follow instructions in $OUTPUT_DIR/README.md"
echo ""

log_info "See also:"
echo "  - $OUTPUT_DIR/README.md - Detailed submission guide"
echo "  - $OUTPUT_DIR/SUBMISSION_CHECKLIST.md - Pre-flight checklist"
echo "  - $OUTPUT_DIR/validate.ps1 - PowerShell validation script"
echo ""
