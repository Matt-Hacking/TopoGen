#!/bin/bash

# ============================================================================
# Complete Environment Setup - One Command Solution
# C++ Topographic Generator - Interactive Setup Wizard
# ============================================================================
# Checks dependencies, offers to install missing ones, and optionally builds
# This is the recommended way to set up a new development environment

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Configuration
AUTO_INSTALL=false
AUTO_BUILD=false
SKIP_OPTIONAL=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --auto-install|-y)
            AUTO_INSTALL=true
            shift
            ;;
        --auto-build|-b)
            AUTO_BUILD=true
            shift
            ;;
        --required-only)
            SKIP_OPTIONAL=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Complete environment setup for C++ Topographic Generator"
            echo ""
            echo "Options:"
            echo "  --auto-install, -y    Automatically install missing dependencies"
            echo "  --auto-build, -b      Automatically build after setup"
            echo "  --required-only       Only install required dependencies"
            echo "  --help, -h            Show this help message"
            echo ""
            echo "Interactive mode (default):"
            echo "  ./scripts/setup_environment.sh"
            echo ""
            echo "Fully automated mode:"
            echo "  ./scripts/setup_environment.sh -y -b"
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

# ============================================================================
# Welcome Banner
# ============================================================================

clear

echo -e "${CYAN}"
cat << "EOF"
╔══════════════════════════════════════════════════════════════════╗
║                                                                  ║
║    _____ ____  ____   ____    ____                              ║
║   |_   _/ __ \|  _ \ / __ \  / __ \  ___ _ __                   ║
║     | || |  | | |_) | |  | || |  __| / _ \ '_ \                 ║
║     | || |  | |  __/| |  | || | |_ ||  __/ | | |                ║
║     |_||_|  |_|_|    \____/  \____| \___|_| |_|                 ║
║                                                                  ║
║    C++ Topographic Generator - Environment Setup                ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo ""
echo -e "${BOLD}Welcome to the interactive setup wizard!${NC}"
echo ""
echo "This script will:"
echo "  1. Check your system for required dependencies"
echo "  2. Install any missing dependencies (with your permission)"
echo "  3. Configure your build environment"
echo "  4. Optionally build the project"
echo ""

if [[ "$AUTO_INSTALL" == "true" ]]; then
    echo -e "${YELLOW}⚡ Auto-install mode enabled${NC}"
fi

if [[ "$AUTO_BUILD" == "true" ]]; then
    echo -e "${YELLOW}⚡ Auto-build mode enabled${NC}"
fi

echo ""
echo "Press Enter to continue or Ctrl+C to cancel..."

if [[ "$AUTO_INSTALL" == "false" ]] && [[ "$AUTO_BUILD" == "false" ]]; then
    read
fi

echo ""

# ============================================================================
# Step 1: Check Dependencies
# ============================================================================

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}Step 1: Checking Dependencies${NC}                                    ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ ! -f "$SCRIPT_DIR/check_dependencies.sh" ]]; then
    echo -e "${RED}✗ Dependency checker script not found!${NC}"
    echo "Expected at: $SCRIPT_DIR/check_dependencies.sh"
    exit 1
fi

# Run dependency check and capture exit code
set +e
bash "$SCRIPT_DIR/check_dependencies.sh"
CHECK_RESULT=$?
set -e

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# ============================================================================
# Step 2: Install Missing Dependencies (if needed)
# ============================================================================

if [[ $CHECK_RESULT -ne 0 ]]; then
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}  ${MAGENTA}Step 2: Install Missing Dependencies${NC}                            ${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
    echo ""

    echo -e "${YELLOW}Some dependencies are missing or outdated.${NC}"
    echo ""

    SHOULD_INSTALL=false

    if [[ "$AUTO_INSTALL" == "true" ]]; then
        SHOULD_INSTALL=true
        echo -e "${GREEN}Auto-install enabled - proceeding with installation...${NC}"
    else
        echo -e "${BOLD}Would you like to install missing dependencies now?${NC}"
        echo -n "(yes/no) [yes]: "
        read answer
        answer=${answer:-yes}

        if [[ "$answer" =~ ^[Yy] ]]; then
            SHOULD_INSTALL=true
        fi
    fi

    if [[ "$SHOULD_INSTALL" == "true" ]]; then
        echo ""

        if [[ ! -f "$SCRIPT_DIR/install_dependencies.sh" ]]; then
            echo -e "${RED}✗ Installer script not found!${NC}"
            echo "Expected at: $SCRIPT_DIR/install_dependencies.sh"
            exit 1
        fi

        INSTALL_ARGS=""
        if [[ "$SKIP_OPTIONAL" == "true" ]]; then
            INSTALL_ARGS="--required-only"
        fi

        bash "$SCRIPT_DIR/install_dependencies.sh" $INSTALL_ARGS

        echo ""
        echo -e "${GREEN}✓ Installation complete!${NC}"
        echo ""

        # Re-run dependency check to verify
        echo "Verifying installation..."
        echo ""
        bash "$SCRIPT_DIR/check_dependencies.sh"
        echo ""

    else
        echo ""
        echo -e "${YELLOW}⚠  Skipping installation.${NC}"
        echo ""
        echo "You can install dependencies later by running:"
        echo "  ./scripts/install_dependencies.sh"
        echo ""
        echo "Or install manually:"
        echo "  macOS:       brew install cmake cgal eigen gdal tbb libomp"
        echo "  Ubuntu:      sudo apt install cmake libcgal-dev libeigen3-dev libgdal-dev"
        echo ""
        exit 0
    fi

else
    echo -e "${GREEN}✓ All dependencies are satisfied!${NC}"
    echo ""
fi

# ============================================================================
# Step 3: Configure Build Environment
# ============================================================================

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}Step 3: Configure Build Environment${NC}                              ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if environment script exists
ENV_SCRIPT="$SCRIPT_DIR/../build_environment.sh"

if [[ -f "$ENV_SCRIPT" ]]; then
    echo -e "${GREEN}✓ Environment configuration found${NC}"
    echo ""
    echo "To use in future sessions, add this to your shell profile:"
    echo ""
    echo -e "${CYAN}    source $(realpath "$ENV_SCRIPT")${NC}"
    echo ""

    # Source the environment for this session
    source "$ENV_SCRIPT"
else
    echo -e "${YELLOW}⚠  No environment configuration found${NC}"
    echo "   The installer should have created build_environment.sh"
    echo ""
fi

# ============================================================================
# Step 4: Test Build (Optional)
# ============================================================================

echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║${NC}  ${MAGENTA}Step 4: Test Build${NC}                                               ${CYAN}║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
echo ""

SHOULD_BUILD=false

if [[ "$AUTO_BUILD" == "true" ]]; then
    SHOULD_BUILD=true
    echo -e "${GREEN}Auto-build enabled - starting build...${NC}"
elif [[ "$AUTO_INSTALL" == "false" ]]; then
    echo -e "${BOLD}Would you like to build the project now?${NC}"
    echo -n "(yes/no) [yes]: "
    read answer
    answer=${answer:-yes}

    if [[ "$answer" =~ ^[Yy] ]]; then
        SHOULD_BUILD=true
    fi
fi

if [[ "$SHOULD_BUILD" == "true" ]]; then
    echo ""

    # Navigate to project root (one level up from scripts/)
    PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
    cd "$PROJECT_ROOT"

    if [[ -f "$SCRIPT_DIR/quick-build.sh" ]]; then
        echo "Running quick build script..."
        echo ""
        bash "$SCRIPT_DIR/quick-build.sh"

        BUILD_RESULT=$?

        if [[ $BUILD_RESULT -eq 0 ]]; then
            echo ""
            echo -e "${GREEN}✓ Build successful!${NC}"
            echo ""

            # Test the executable
            if [[ -f "build/topo-gen" ]]; then
                echo "Testing executable..."
                if ./build/topo-gen --help &>/dev/null; then
                    echo -e "${GREEN}✓ Executable works correctly!${NC}"
                    echo ""

                    echo "Binary information:"
                    file build/topo-gen
                    echo "Size: $(du -h build/topo-gen | cut -f1)"
                    echo ""
                fi
            fi
        else
            echo ""
            echo -e "${RED}✗ Build failed${NC}"
            echo ""
            echo "Please check the error messages above."
            echo "You can try building manually with:"
            echo "  cd cpp-version"
            echo "  ./scripts/quick-build.sh"
            echo ""
            exit 1
        fi

    else
        echo -e "${YELLOW}⚠  Build script not found at: $SCRIPT_DIR/quick-build.sh${NC}"
        echo ""
        echo "You can build manually with:"
        echo "  mkdir -p build && cd build"
        echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
        echo "  make -j\$(nproc)"
        echo ""
    fi

else
    echo -e "${YELLOW}Skipping build.${NC}"
    echo ""
    echo "You can build later with:"
    echo "  ./scripts/quick-build.sh"
    echo "  or"
    echo "  ./scripts/build_macos.sh"
    echo ""
fi

# ============================================================================
# Final Summary
# ============================================================================

echo -e "${CYAN}"
cat << "EOF"
╔══════════════════════════════════════════════════════════════════╗
║                                                                  ║
║    ✓ Setup Complete!                                            ║
║                                                                  ║
╚══════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo ""
echo -e "${GREEN}Your environment is ready for development!${NC}"
echo ""
echo -e "${BOLD}Quick Reference:${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Build the project:"
echo -e "  ${CYAN}./scripts/quick-build.sh${NC}              # Fast incremental build"
echo -e "  ${CYAN}./scripts/build_macos.sh${NC}             # Full macOS build"
echo ""
echo "Run the application:"
echo -e "  ${CYAN}./build/topo-gen --help${NC}              # Show usage"
echo -e "  ${CYAN}./build/topo-gen --bounds \"47.6,-122.3,47.7,-122.2\" --layers 5${NC}"
echo ""
echo "Development tools:"
echo -e "  ${CYAN}./scripts/check_dependencies.sh${NC}      # Verify dependencies"
echo -e "  ${CYAN}./scripts/validate_build_environment.sh${NC}  # Validate setup"
echo ""
echo "For testing (outputs to test/output/):"
echo -e "  ${CYAN}cd test${NC}"
echo -e "  ${CYAN}../build/topo-gen [args] --output test/output/${NC}"
echo ""

if [[ -f "$ENV_SCRIPT" ]]; then
    echo "Environment configuration:"
    echo -e "  ${CYAN}source $(realpath "$ENV_SCRIPT")${NC}"
    echo "  (Add to ~/.bashrc or ~/.zshrc for persistence)"
    echo ""
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo -e "${BOLD}Documentation:${NC}"
echo "  Project docs:     ./docs/"
echo "  Build guide:      ./CLAUDE.md"
echo "  License:          ./LICENSE"
echo ""
echo -e "${GREEN}Happy coding! 🚀${NC}"
echo ""

exit 0
