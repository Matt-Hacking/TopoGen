#!/bin/bash

# ============================================================================
# Automated Dependency Installer
# C++ Topographic Generator - Build Environment Setup
# ============================================================================
# Automatically installs all required and optional dependencies
# Supports macOS (Homebrew), Ubuntu/Debian (apt), and Conda

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
DRY_RUN=false
INSTALL_OPTIONAL=true
VERBOSE=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --required-only)
            INSTALL_OPTIONAL=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --dry-run          Show what would be installed without installing"
            echo "  --required-only    Only install required dependencies (skip optional)"
            echo "  --verbose, -v      Show detailed output"
            echo "  --help, -h         Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}C++ Topographic Generator - Dependency Installer${NC}            ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${YELLOW}🔍 DRY RUN MODE - No changes will be made${NC}"
    echo ""
fi

# Detect platform
PLATFORM="unknown"
PACKAGE_MANAGER="unknown"

if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    PACKAGE_MANAGER="brew"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
    if command -v apt-get &> /dev/null; then
        PACKAGE_MANAGER="apt"
    elif command -v dnf &> /dev/null; then
        PACKAGE_MANAGER="dnf"
    elif command -v yum &> /dev/null; then
        PACKAGE_MANAGER="yum"
    elif command -v pacman &> /dev/null; then
        PACKAGE_MANAGER="pacman"
    fi
fi

# Check for conda
if command -v conda &> /dev/null && [[ -n "$CONDA_PREFIX" ]]; then
    echo -e "${BLUE}ℹ  Conda environment detected: $CONDA_PREFIX${NC}"
    echo "   You can also use: conda install -c conda-forge cgal-cpp eigen boost-cpp gdal tbb"
    echo ""
fi

echo -e "Platform: ${BLUE}$PLATFORM${NC}"
echo -e "Package Manager: ${BLUE}$PACKAGE_MANAGER${NC}"
echo ""

if [[ "$PACKAGE_MANAGER" == "unknown" ]]; then
    echo -e "${RED}✗ Unsupported platform or package manager not found${NC}"
    echo ""
    echo "This script supports:"
    echo "  - macOS with Homebrew"
    echo "  - Ubuntu/Debian with apt"
    echo "  - Fedora/RHEL with dnf/yum"
    echo "  - Arch Linux with pacman"
    echo ""
    echo "Please install dependencies manually or use conda:"
    echo "  conda install -c conda-forge cgal-cpp eigen boost-cpp gdal tbb cmake"
    exit 1
fi

# ============================================================================
# Installation Functions
# ============================================================================

install_packages() {
    local packages=("$@")

    if [[ ${#packages[@]} -eq 0 ]]; then
        return 0
    fi

    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Installing packages: ${packages[*]}${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [[ "$DRY_RUN" == "true" ]]; then
        echo -e "${YELLOW}[DRY RUN]${NC} Would install: ${packages[*]}"
        return 0
    fi

    case $PACKAGE_MANAGER in
        brew)
            for pkg in "${packages[@]}"; do
                if brew list "$pkg" &>/dev/null; then
                    echo -e "${GREEN}✓ $pkg already installed${NC}"
                else
                    echo -e "${YELLOW}📦 Installing $pkg...${NC}"
                    if [[ "$VERBOSE" == "true" ]]; then
                        brew install "$pkg"
                    else
                        brew install "$pkg" &>/dev/null && echo -e "${GREEN}✓ $pkg installed${NC}" || echo -e "${RED}✗ Failed to install $pkg${NC}"
                    fi
                fi
            done
            ;;
        apt)
            echo "Updating package lists..."
            if [[ "$VERBOSE" == "true" ]]; then
                sudo apt-get update
            else
                sudo apt-get update &>/dev/null
            fi

            for pkg in "${packages[@]}"; do
                if dpkg -l | grep -q "^ii  $pkg "; then
                    echo -e "${GREEN}✓ $pkg already installed${NC}"
                else
                    echo -e "${YELLOW}📦 Installing $pkg...${NC}"
                    if [[ "$VERBOSE" == "true" ]]; then
                        sudo apt-get install -y "$pkg"
                    else
                        sudo apt-get install -y "$pkg" &>/dev/null && echo -e "${GREEN}✓ $pkg installed${NC}" || echo -e "${RED}✗ Failed to install $pkg${NC}"
                    fi
                fi
            done
            ;;
        dnf|yum)
            for pkg in "${packages[@]}"; do
                if rpm -q "$pkg" &>/dev/null; then
                    echo -e "${GREEN}✓ $pkg already installed${NC}"
                else
                    echo -e "${YELLOW}📦 Installing $pkg...${NC}"
                    if [[ "$VERBOSE" == "true" ]]; then
                        sudo $PACKAGE_MANAGER install -y "$pkg"
                    else
                        sudo $PACKAGE_MANAGER install -y "$pkg" &>/dev/null && echo -e "${GREEN}✓ $pkg installed${NC}" || echo -e "${RED}✗ Failed to install $pkg${NC}"
                    fi
                fi
            done
            ;;
        pacman)
            for pkg in "${packages[@]}"; do
                if pacman -Q "$pkg" &>/dev/null; then
                    echo -e "${GREEN}✓ $pkg already installed${NC}"
                else
                    echo -e "${YELLOW}📦 Installing $pkg...${NC}"
                    if [[ "$VERBOSE" == "true" ]]; then
                        sudo pacman -S --noconfirm "$pkg"
                    else
                        sudo pacman -S --noconfirm "$pkg" &>/dev/null && echo -e "${GREEN}✓ $pkg installed${NC}" || echo -e "${RED}✗ Failed to install $pkg${NC}"
                    fi
                fi
            done
            ;;
    esac

    echo ""
}

# ============================================================================
# Platform-Specific Package Lists
# ============================================================================

get_required_packages() {
    case $PACKAGE_MANAGER in
        brew)
            echo "cmake cgal eigen gdal curl openssl zlib"
            ;;
        apt)
            echo "build-essential cmake libcgal-dev libeigen3-dev libgdal-dev libcurl4-openssl-dev libssl-dev zlib1g-dev pkg-config"
            ;;
        dnf|yum)
            echo "cmake gcc-c++ CGAL-devel eigen3-devel gdal-devel libcurl-devel openssl-devel zlib-devel pkgconfig"
            ;;
        pacman)
            echo "cmake gcc cgal eigen gdal curl openssl zlib pkgconf"
            ;;
    esac
}

get_optional_packages() {
    case $PACKAGE_MANAGER in
        brew)
            echo "tbb libomp boost gmp mpfr ccache ninja nlohmann-json"
            ;;
        apt)
            echo "libtbb-dev libomp-dev libboost-all-dev libgmp-dev libmpfr-dev ccache ninja-build nlohmann-json3-dev"
            ;;
        dnf|yum)
            echo "tbb-devel libomp-devel boost-devel gmp-devel mpfr-devel ccache ninja-build json-devel"
            ;;
        pacman)
            echo "tbb openmp boost gmp mpfr ccache ninja nlohmann-json"
            ;;
    esac
}

# ============================================================================
# Main Installation
# ============================================================================

echo -e "${MAGENTA}Step 1: Checking package manager status${NC}"
echo ""

case $PACKAGE_MANAGER in
    brew)
        if ! command -v brew &> /dev/null; then
            echo -e "${RED}✗ Homebrew not found${NC}"
            echo ""
            echo "Please install Homebrew first:"
            echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
            exit 1
        fi
        echo -e "${GREEN}✓ Homebrew found${NC}: $(brew --version | head -1)"
        ;;
    apt)
        if ! command -v apt-get &> /dev/null; then
            echo -e "${RED}✗ apt-get not found${NC}"
            exit 1
        fi
        echo -e "${GREEN}✓ apt-get found${NC}"
        ;;
    dnf|yum)
        echo -e "${GREEN}✓ $PACKAGE_MANAGER found${NC}"
        ;;
    pacman)
        echo -e "${GREEN}✓ pacman found${NC}"
        ;;
esac

echo ""

# ============================================================================
# Install Required Dependencies
# ============================================================================

echo -e "${MAGENTA}Step 2: Installing required dependencies${NC}"
echo ""

required_packages=$(get_required_packages)
install_packages $required_packages

# ============================================================================
# Install Optional Dependencies
# ============================================================================

if [[ "$INSTALL_OPTIONAL" == "true" ]]; then
    echo -e "${MAGENTA}Step 3: Installing optional dependencies (performance & features)${NC}"
    echo ""

    optional_packages=$(get_optional_packages)
    install_packages $optional_packages
else
    echo -e "${YELLOW}Skipping optional dependencies (--required-only flag set)${NC}"
    echo ""
fi

# ============================================================================
# Platform-Specific Post-Installation
# ============================================================================

echo -e "${MAGENTA}Step 4: Platform-specific configuration${NC}"
echo ""

if [[ "$PLATFORM" == "macos" ]] && [[ "$DRY_RUN" == "false" ]]; then
    echo "Setting up macOS-specific environment..."

    # Set up Homebrew environment
    HOMEBREW_PREFIX=$(brew --prefix)
    export PATH="$HOMEBREW_PREFIX/bin:$PATH"
    export CMAKE_PREFIX_PATH="$HOMEBREW_PREFIX:$CMAKE_PREFIX_PATH"
    export PKG_CONFIG_PATH="$HOMEBREW_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"

    echo -e "${GREEN}✓ Homebrew environment configured${NC}"

    # Check for Xcode Command Line Tools
    if ! xcode-select -p &>/dev/null; then
        echo -e "${YELLOW}⚠  Xcode Command Line Tools not found${NC}"
        echo "Installing Xcode Command Line Tools..."
        if [[ "$DRY_RUN" == "false" ]]; then
            xcode-select --install
            echo "Please complete the Xcode installation and run this script again."
            exit 0
        fi
    else
        echo -e "${GREEN}✓ Xcode Command Line Tools found${NC}"
    fi

    # Verify OpenMP installation specifically
    if [[ -f "$HOMEBREW_PREFIX/lib/libomp.dylib" ]]; then
        echo -e "${GREEN}✓ OpenMP (libomp) is available${NC}"
    else
        echo -e "${YELLOW}⚠  OpenMP may not be properly installed${NC}"
        echo "   Try: brew reinstall libomp"
    fi

elif [[ "$PLATFORM" == "linux" ]] && [[ "$DRY_RUN" == "false" ]]; then
    echo "Setting up Linux-specific environment..."

    # Update library cache
    if command -v ldconfig &>/dev/null; then
        echo "Updating library cache..."
        if [[ "$VERBOSE" == "true" ]]; then
            sudo ldconfig
        else
            sudo ldconfig &>/dev/null
        fi
        echo -e "${GREEN}✓ Library cache updated${NC}"
    fi
fi

echo ""

# ============================================================================
# Create Environment Setup Script
# ============================================================================

if [[ "$DRY_RUN" == "false" ]]; then
    echo -e "${MAGENTA}Step 5: Creating environment setup script${NC}"
    echo ""

    ENV_SCRIPT="build_environment.sh"

    cat > "$ENV_SCRIPT" << 'EOF'
#!/bin/bash
# Auto-generated environment setup script
# Source this file before building: source build_environment.sh

EOF

    if [[ "$PLATFORM" == "macos" ]]; then
        cat >> "$ENV_SCRIPT" << EOF
# macOS Homebrew environment
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:\$PATH"
export CMAKE_PREFIX_PATH="/opt/homebrew:\$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:\$PKG_CONFIG_PATH"
export HOMEBREW_PREFIX="/opt/homebrew"

# LLVM/Clang (if using LLVM's clang)
if [ -d "/opt/homebrew/opt/llvm" ]; then
    export LLVM="\$(brew --prefix llvm)"
    export PATH="\$LLVM/bin:\$PATH"
    export LDFLAGS="-L\$LLVM/lib \$LDFLAGS"
    export CPPFLAGS="-I\$LLVM/include \$CPPFLAGS"
fi

# OpenMP
if [ -d "/opt/homebrew/opt/libomp" ]; then
    export OpenMP_ROOT="/opt/homebrew/opt/libomp"
fi
EOF
    elif [[ "$PLATFORM" == "linux" ]]; then
        cat >> "$ENV_SCRIPT" << 'EOF'
# Linux environment
export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:/usr/lib/pkgconfig:$PKG_CONFIG_PATH"

# Conda environment (if active)
if [ -n "$CONDA_PREFIX" ]; then
    export CMAKE_PREFIX_PATH="$CONDA_PREFIX:$CMAKE_PREFIX_PATH"
    export PKG_CONFIG_PATH="$CONDA_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
fi
EOF
    fi

    cat >> "$ENV_SCRIPT" << 'EOF'

echo "Build environment configured!"
echo "Ready to build with: ./scripts/quick-build.sh"
EOF

    chmod +x "$ENV_SCRIPT"
    echo -e "${GREEN}✓ Created $ENV_SCRIPT${NC}"
    echo "   Source this file before building: source $ENV_SCRIPT"
    echo ""
fi

# ============================================================================
# Verify Installation
# ============================================================================

echo -e "${MAGENTA}Step 6: Verifying installation${NC}"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${YELLOW}[DRY RUN]${NC} Skipping verification"
    echo ""
    echo "In real mode, this would run: ./scripts/check_dependencies.sh"
else
    echo "Running dependency check..."
    echo ""

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

    if [[ -f "$SCRIPT_DIR/check_dependencies.sh" ]]; then
        bash "$SCRIPT_DIR/check_dependencies.sh"
    else
        echo -e "${YELLOW}⚠  Dependency checker not found${NC}"
        echo "   Verify manually by running: cmake --version, gdal-config --version, etc."
        echo ""
    fi
fi

# ============================================================================
# Summary
# ============================================================================

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}Installation Complete${NC}                                            ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

if [[ "$DRY_RUN" == "true" ]]; then
    echo -e "${YELLOW}This was a dry run. Run without --dry-run to actually install packages.${NC}"
    echo ""
else
    echo -e "${GREEN}✓ Dependencies have been installed!${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Source the environment (if not already in your shell profile):"
    echo "     source build_environment.sh"
    echo ""
    echo "  2. Build the project:"
    echo "     cd cpp-version"
    echo "     ./scripts/quick-build.sh"
    echo ""
    echo "  3. Or use the full build script:"
    echo "     ./scripts/build_macos.sh    (macOS)"
    echo ""

    if [[ "$PLATFORM" == "macos" ]]; then
        echo "  macOS Note: If you encounter OpenMP issues, try:"
        echo "    brew reinstall libomp"
        echo "    brew reinstall llvm"
        echo ""
    fi
fi

exit 0
