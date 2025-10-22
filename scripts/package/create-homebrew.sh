#!/bin/bash
#
# create-homebrew.sh - Generate Homebrew formula
#
# Creates a Homebrew formula for installing Topographic Generator via brew.
# Can be used for submission to Homebrew core or for a custom tap.
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

Generate a Homebrew formula for Topographic Generator.

Options:
    --tarball URL           Source tarball URL (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output FILE           Output formula file (default: topo-gen.rb)
    --output-dir DIR        Output directory (default: dist/homebrew/)
    --sha256 HASH           SHA256 checksum (auto-calculated if not provided)
    --homepage URL          Homepage URL
    --description TEXT      Short description
    --license LICENSE       License (default: MIT)
    --help                  Show this help message

Examples:
    # Generate formula from GitHub release
    $0 --tarball https://github.com/user/topo-gen/archive/v0.22.001.tar.gz

    # Generate with explicit SHA256
    $0 --tarball https://example.com/topo-gen-0.22.001.tar.gz \\
       --sha256 abc123...

    # Custom output location
    $0 --tarball URL --output-dir ~/homebrew-tap/Formula

Notes:
    - If --sha256 is not provided, the script will download and calculate it
    - For GitHub releases, use the archive URL, not the release page
    - The formula will include all build dependencies

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
TARBALL_URL=""
VERSION=$(get_version)
OUTPUT_FILE="topo-gen.rb"
OUTPUT_DIR="$PROJECT_ROOT/dist/homebrew"
SHA256=""
HOMEPAGE="https://github.com/matthewblock/topo-gen"
DESCRIPTION="High-performance topographic model generator"
LICENSE="MIT"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --tarball)
            TARBALL_URL="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --sha256)
            SHA256="$2"
            shift 2
            ;;
        --homepage)
            HOMEPAGE="$2"
            shift 2
            ;;
        --description)
            DESCRIPTION="$2"
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
if [[ -z "$TARBALL_URL" ]]; then
    log_error "Tarball URL is required"
    usage
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "Homebrew Formula Generation"
log_info "  Version: $VERSION"
log_info "  Tarball: $TARBALL_URL"
log_info "  Output: $OUTPUT_DIR/$OUTPUT_FILE"
echo ""

# Calculate SHA256 if not provided
if [[ -z "$SHA256" ]]; then
    log_info "Calculating SHA256 checksum..."
    log_info "Downloading tarball..."

    TEMP_TARBALL=$(mktemp)

    if curl -sL "$TARBALL_URL" -o "$TEMP_TARBALL"; then
        if command -v shasum &> /dev/null; then
            SHA256=$(shasum -a 256 "$TEMP_TARBALL" | awk '{print $1}')
        elif command -v sha256sum &> /dev/null; then
            SHA256=$(sha256sum "$TEMP_TARBALL" | awk '{print $1}')
        else
            log_error "Neither shasum nor sha256sum found"
            rm -f "$TEMP_TARBALL"
            exit 1
        fi
        rm -f "$TEMP_TARBALL"
        log_success "SHA256: $SHA256"
    else
        log_error "Failed to download tarball"
        rm -f "$TEMP_TARBALL"
        exit 1
    fi
fi

# Generate formula
log_info "Generating Homebrew formula..."

FORMULA_PATH="$OUTPUT_DIR/$OUTPUT_FILE"

cat > "$FORMULA_PATH" << EOF
class TopoGen < Formula
  desc "$DESCRIPTION"
  homepage "$HOMEPAGE"
  url "$TARBALL_URL"
  sha256 "$SHA256"
  license "$LICENSE"
  version "$VERSION"

  # Build dependencies
  depends_on "cmake" => :build

  # Runtime dependencies
  depends_on "cgal"
  depends_on "eigen"
  depends_on "gdal"
  depends_on "tbb"
  depends_on "libomp"

  # Optional dependencies
  depends_on "qt@6" => :optional

  def install
    # Set up build environment
    ENV.append "LDFLAGS", "-L#{Formula["libomp"].opt_lib}"
    ENV.append "CPPFLAGS", "-I#{Formula["libomp"].opt_include}"

    # Configure with CMake
    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DCMAKE_INSTALL_PREFIX=#{prefix}",
                    *std_cmake_args

    # Build
    system "cmake", "--build", "build", "--parallel", ENV.make_jobs

    # Install
    system "cmake", "--install", "build"

    # Install GDAL data files
    (share/"topo-gen/gdal").install Dir["#{Formula["gdal"].opt_share}/gdal/*"]
  end

  def caveats
    <<~EOS
      GDAL data files have been installed to:
        #{share}/topo-gen/gdal

      To use them, set the GDAL_DATA environment variable:
        export GDAL_DATA=#{share}/topo-gen/gdal

      You may want to add this to your shell profile (~/.bash_profile or ~/.zshrc).

      For GUI support, install with Qt:
        brew install topo-gen --with-qt@6
    EOS
  end

  test do
    # Test CLI executable
    assert_match version.to_s, shell_output("#{bin}/topo-gen --version")

    # Test basic functionality
    system bin/"topo-gen", "--help"
  end
end
EOF

log_success "Formula generated: $FORMULA_PATH"

# Verify formula syntax
log_info "Verifying formula syntax..."

if command -v brew &> /dev/null; then
    if brew formula "$FORMULA_PATH" &> /dev/null; then
        log_success "Formula syntax is valid"
    else
        log_warning "Formula syntax check failed (brew formula command failed)"
        log_info "This may be normal if the formula isn't installed yet"
    fi
else
    log_warning "Homebrew not found - skipping syntax verification"
fi

echo ""
log_success "Homebrew formula created successfully!"
log_info "Location: $FORMULA_PATH"
echo ""

log_info "Next steps:"
echo ""
echo "  1. Test the formula locally:"
echo "     brew install --build-from-source $FORMULA_PATH"
echo ""
echo "  2. Create a tap (custom repository):"
echo "     mkdir -p ~/homebrew-topo-gen/Formula"
echo "     cp $FORMULA_PATH ~/homebrew-topo-gen/Formula/"
echo "     cd ~/homebrew-topo-gen"
echo "     git init && git add . && git commit -m \"Add topo-gen formula\""
echo "     git remote add origin https://github.com/yourusername/homebrew-topo-gen"
echo "     git push -u origin main"
echo ""
echo "  3. Users can then install with:"
echo "     brew tap yourusername/topo-gen"
echo "     brew install topo-gen"
echo ""
echo "  4. Or submit to Homebrew core:"
echo "     - Fork homebrew-core on GitHub"
echo "     - Add formula to Formula/ directory"
echo "     - Submit pull request"
echo "     - Follow Homebrew contribution guidelines"
echo ""

# Create bottle (binary package) instructions
cat > "$OUTPUT_DIR/BOTTLING.md" << 'EOF'
# Creating Homebrew Bottles

Bottles are pre-built binary packages that speed up installation.

## Steps to Create Bottles

### 1. Build the formula
```bash
brew install --build-bottle topo-gen.rb
```

### 2. Create the bottle
```bash
brew bottle topo-gen
```

This creates a `.tar.gz` file like:
`topo-gen--0.22.001.arm64_monterey.bottle.tar.gz`

### 3. Upload bottles
Upload bottles to a public URL (GitHub releases, S3, etc.)

### 4. Update formula with bottle information
```ruby
bottle do
  root_url "https://github.com/yourusername/topo-gen/releases/download/v0.22.001"
  sha256 cellar: :any, arm64_monterey: "abc123..."
  sha256 cellar: :any, arm64_ventura:  "def456..."
  sha256 cellar: :any, arm64_sonoma:   "ghi789..."
end
```

### 5. Test installation from bottle
```bash
brew uninstall topo-gen
brew install topo-gen
```

## Multi-Platform Bottles

To support both Apple Silicon and Intel:

1. Build on arm64 macOS: `brew bottle topo-gen`
2. Build on x86_64 macOS: `brew bottle topo-gen`
3. Update formula with both SHA256s

## Continuous Integration

Use GitHub Actions to automatically build bottles:

```yaml
name: Build Homebrew Bottles

on:
  release:
    types: [published]

jobs:
  bottle:
    strategy:
      matrix:
        os: [macos-12, macos-13, macos-14]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v3
      - name: Build bottle
        run: |
          brew install --build-bottle ./topo-gen.rb
          brew bottle topo-gen
      - name: Upload bottle
        uses: actions/upload-artifact@v3
        with:
          name: bottles
          path: "*.bottle.tar.gz"
```

See: https://docs.brew.sh/Bottles
EOF

log_info "Created bottling guide: $OUTPUT_DIR/BOTTLING.md"

echo ""
log_info "Formula ready for testing and distribution!"
