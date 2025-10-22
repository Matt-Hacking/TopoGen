#!/bin/bash

# C++ Topographic Generator Version Control Script
# Creates versioned archives with proper indexing and file filtering
#
# Usage:
#   ./create_version.sh [description]              # Auto-increment revision
#   ./create_version.sh [description] --version MM.mm.rrr  # Specify version
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

# Configuration
PROJECT_NAME="cpp-topographic-generator"
VERSION_DIR="version_control"
PARENT_VERSION_DIR="../version_control"
DATE_FORMAT="%Y%m%d_%H%M%S"
CMAKE_FILE="CMakeLists.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to extract current version from CMakeLists.txt
get_cmake_version() {
    if [[ ! -f "$CMAKE_FILE" ]]; then
        log_error "CMakeLists.txt not found"
        exit 1
    fi

    # Extract version from project(TopographicGenerator VERSION X.Y.Z ...)
    local version=$(grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$CMAKE_FILE" | \
                    sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')

    if [[ -z "$version" ]]; then
        log_error "Could not extract version from CMakeLists.txt"
        exit 1
    fi

    echo "$version"
}

# Function to parse version into components
parse_version() {
    local version="$1"

    # Split version into major.minor.revision
    if [[ ! "$version" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
        log_error "Invalid version format: $version (expected MM.mm.rrr)"
        exit 1
    fi

    local major="${BASH_REMATCH[1]}"
    local minor="${BASH_REMATCH[2]}"
    local revision="${BASH_REMATCH[3]}"

    echo "$major $minor $revision"
}

# Function to increment revision number
increment_revision() {
    local version="$1"
    local components=($(parse_version "$version"))
    local major="${components[0]}"
    local minor="${components[1]}"
    local revision="${components[2]}"

    # Increment revision (remove leading zeros, add 1, re-pad)
    revision=$((10#$revision + 1))
    revision=$(printf "%03d" $revision)

    echo "${major}.${minor}.${revision}"
}

# Function to update CMakeLists.txt with new version
update_cmake_version() {
    local new_version="$1"

    log_info "Updating CMakeLists.txt version to $new_version"

    # Backup original
    cp "$CMAKE_FILE" "${CMAKE_FILE}.bak"

    # Update version in place
    sed -i.tmp -E "s/(project\\(.*VERSION )[0-9]+\\.[0-9]+\\.[0-9]+(.*)/\\1${new_version}\\2/" "$CMAKE_FILE"
    rm -f "${CMAKE_FILE}.tmp"

    # Verify the update
    local updated_version=$(get_cmake_version)
    if [[ "$updated_version" != "$new_version" ]]; then
        log_error "Failed to update version in CMakeLists.txt"
        mv "${CMAKE_FILE}.bak" "$CMAKE_FILE"
        exit 1
    fi

    rm -f "${CMAKE_FILE}.bak"
    log_success "Updated CMakeLists.txt to version $new_version"
}

# Function to validate description
validate_description() {
    local desc="$1"
    if [[ -z "$desc" ]]; then
        log_error "Description is required"
        echo "Usage: $0 <description> [--version MM.mm.rrr]"
        echo "Example: $0 \"performance_optimizations\""
        echo "Example: $0 \"bug_fixes\" --version 0.22.005"
        exit 1
    fi

    # Sanitize description for filename
    desc=$(echo "$desc" | tr ' ' '_' | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9_-]//g')
    if [[ ${#desc} -gt 50 ]]; then
        desc="${desc:0:50}"
        log_warning "Description truncated to 50 characters: $desc"
    fi

    echo "$desc"
}

# Function to get project statistics
get_project_stats() {
    local src_files=$(find src/ -name "*.cpp" -o -name "*.hpp" 2>/dev/null | wc -l | tr -d ' ')
    local include_files=$(find include/ -name "*.h" -o -name "*.hpp" 2>/dev/null | wc -l | tr -d ' ')
    local total_lines=$(find src/ include/ -name "*.cpp" -o -name "*.hpp" -o -name "*.h" 2>/dev/null | xargs wc -l 2>/dev/null | tail -1 | awk '{print $1}' || echo "0")
    local cmake_files=$(find . -name "CMakeLists.txt" 2>/dev/null | wc -l | tr -d ' ')

    echo "Source files: $src_files, Include files: $include_files, Total lines: $total_lines, CMake files: $cmake_files"
}

# Function to create version notes
create_version_notes() {
    local version="$1"
    local description="$2"
    local timestamp="$3"
    local archive_name="$4"
    local stats="$5"

    local notes_file="${VERSION_DIR}/${PROJECT_NAME}_v${version}_${description}_notes.md"

    cat > "$notes_file" << EOF
# C++ Topographic Generator - Version $version

**Archive**: \`$archive_name\`
**Date**: $(date "+%B %d, %Y")
**Timestamp**: $timestamp
**Description**: $description
**Version**: $version

## Version Summary

This version contains the C++ implementation of the high-performance topographic generator.

### Project Statistics
$stats

### Archive Contents

#### Included Files
- **Source code** (\`src/\`) - Core engine, export modules, CLI
- **Headers** (\`include/\`) - Public API headers
- **Vendored dependencies** (\`external/\`) - libigl, nlohmann_json
- **Build scripts** (\`scripts/\`) - Build, test, deployment automation
- **Development tools** (\`development_tools/\`) - Profiling and debugging scripts
- **Build system** (\`CMakeLists.txt\`) - CMake configuration
- **Documentation** (\`doc/\`, \`README.md\`) - User and developer documentation
- **License files** (\`LICENSE\`, \`COPYRIGHT\`) - MIT License and attribution

#### Excluded Files
- Build artifacts (\`build/\`)
- Test outputs (\`test/\`, \`output/\`)
- Cache data (\`cache/\`)
- Version archives (\`version_control/*.zip\`)
- Private development files (\`CLAUDE.md\`, \`.claude/\`)
- IDE files (\`.vscode/\`, \`.idea/\`)

### Build Instructions

1. **Extract Archive**:
   \`\`\`bash
   unzip $archive_name
   cd ${PROJECT_NAME}_v${version}
   \`\`\`

2. **Install Dependencies**:
   \`\`\`bash
   # macOS
   brew install cmake cgal eigen gdal tbb libomp

   # Ubuntu
   sudo apt install cmake libcgal-dev libeigen3-dev libgdal-dev libtbb-dev
   \`\`\`

3. **Build**:
   \`\`\`bash
   ./scripts/quick-build.sh --release
   # Or use setup wizard
   ./scripts/setup_environment.sh --auto-install --auto-build
   \`\`\`

4. **Test**:
   \`\`\`bash
   ./scripts/test.sh --all
   \`\`\`

### Version $version Changes

$(if [[ "$description" == *"initial"* ]]; then
    echo "- Initial release"
    echo "- Complete feature parity with Python version"
    echo "- High-performance C++ implementation"
elif [[ "$description" == *"beta"* ]]; then
    echo "- Public BETA release"
    echo "- Production-ready feature set"
    echo "- Comprehensive test coverage"
elif [[ "$description" == *"performance"* ]]; then
    echo "- Performance optimizations"
    echo "- Enhanced parallel processing"
    echo "- Memory usage improvements"
elif [[ "$description" == *"bug"* || "$description" == *"fix"* ]]; then
    echo "- Bug fixes and stability improvements"
    echo "- Enhanced error handling"
    echo "- Build system improvements"
else
    echo "- $description"
    echo "- Continued development and refinement"
fi)

### Technical Information

**Build System**: CMake 3.20+
**C++ Standard**: C++20
**Dependencies**: CGAL, Eigen3, GDAL, TBB, OpenMP (optional), libigl (vendored)
**Platform Support**: macOS (Apple Silicon/Intel), Linux, Windows (WSL)

### License

MIT License - See LICENSE file for full text
Copyright (c) 2025 Matthew Block

Core algorithms adapted from Bambu Slicer (libslic3r) geometry processing.

---

**Archive Size**: $(if [[ -f "${VERSION_DIR}/${archive_name}" ]]; then du -h "${VERSION_DIR}/${archive_name}" | cut -f1; else echo "TBD"; fi)
**Extraction**: Self-contained, no external data dependencies for building
**Documentation**: See README.md and doc/ directory

Generated by C++ Topographic Generator Version Control System
EOF

    echo "$notes_file"
}

# Function to create the version archive
create_archive() {
    local version="$1"
    local description="$2"
    local timestamp="$3"
    local notes_file="$4"

    # Check if Git is available and initialized
    local use_git=false
    if command -v git &> /dev/null && [[ -d ".git" ]]; then
        use_git=true
        log_info "Git repository detected - using git archive"
    else
        log_info "No Git repository - using manual zip"
    fi

    # Create a copy of the notes file at the project root for inclusion in archive
    local temp_notes="VERSION_NOTES.md"
    cp "$notes_file" "$temp_notes"

    local archive_name result size

    if [[ "$use_git" == true ]]; then
        # Use git archive (creates tar.gz)
        archive_name="${PROJECT_NAME}_v${version}_${description}_${timestamp}.tar.gz"
        local archive_path="${VERSION_DIR}/${archive_name}"

        log_info "Creating archive with git: $archive_name"

        # Add VERSION_NOTES.md to git index temporarily (won't be committed)
        git add -f "$temp_notes" 2>/dev/null || true

        # Create archive from current HEAD including VERSION_NOTES.md
        git archive --format=tar.gz \
            --prefix="${PROJECT_NAME}_v${version}/" \
            -o "$archive_path" \
            HEAD

        result=$?

        # Remove VERSION_NOTES.md from index
        git reset HEAD "$temp_notes" 2>/dev/null || true

    else
        # Fallback to manual zip creation
        archive_name="${PROJECT_NAME}_v${version}_${description}_${timestamp}.zip"
        local archive_path="${VERSION_DIR}/${archive_name}"

        log_info "Creating archive manually: $archive_name"

        # A complete archive will include the following (and only the following):
        #     doc/                  - Documentation files
        #     include/              - Public header files
        #     scripts/              - Build, test, and deployment scripts
        #     src/                  - Source code (core, export, cli)
        #     external/             - Vendored dependencies (libigl, nlohmann_json)
        #     development_docs/     - Development documentation
        #     development_tools/    - Development profiling and debugging scripts
        #     CMakeLists.txt        - Main build configuration
        #     COPYRIGHT             - Copyright notice
        #     LICENSE               - MIT License
        #     README.md             - Project README
        #     VERSION_NOTES.md      - Version-specific release notes
        #
        # Explicitly excluded:
        #     CLAUDE.md             - Private development process (not for distribution)
        #     .claude/              - AI assistant state (not for distribution)

        # Create zip with essential files only
        zip -r "$archive_path" \
            doc \
            include \
            scripts \
            src \
            external \
            development_docs \
            development_tools \
            packaging \
            CMakeLists.txt \
            COPYRIGHT \
            LICENSE \
            README.md \
            VERSION_NOTES.md \
            -x "CLAUDE.md" \
            -x ".claude/*" \
            -x "CLAUDE_NOTES/*" \
            -x "dist/*" \
            -x "docs/*" \
            -x "build/*" \
            -x "output/*" \
            -x "cache/*" \
            -x "test/*" \
            -x "tests/*" \
            -x "*/__pycache__/*" \
            -x "*.pyc" \
            -x "*.pyo" \
            -x "*/.DS_Store" \
            -x "*/._*" \
            -x "*.log" \
            -x "*.tmp" \
            -x "*.temp" \
            -x "*~" \
            -x "*.swp" \
            -x "*.swo" \
            -x ".vscode/*" \
            -x ".idea/*" \
            -x "*.user" \
            -x "CMakeCache.txt" \
            -x "CMakeFiles/*" \
            -x "Makefile" \
            -x "cmake_install.cmake" \
            -x "*/.git/*" \
            -x ".gitignore" \
            -x "*.stl" \
            -x "*.svg" \
            -x "*.obj" \
            -x "*.ply" \
            -x "*.iges" \
            -x "*.step" \
            -x "*.nurbs" \
            -x "version_control/*.zip" \
            -x "version_control/*.tar.gz" \
            -x "dist/*" \
            -x "*.tar.gz" \
            -x "*.tar.bz2" \
            -x "*.zip" \
            >/dev/null

        result=$?
    fi

    # Clean up temporary notes file
    rm -f "$temp_notes"

    if [[ $result -eq 0 ]]; then
        size=$(du -h "$archive_path" | cut -f1)
        log_success "Archive created: $archive_path ($size)"
        return 0
    else
        log_error "Failed to create archive"
        return 1
    fi
}

# Function to display version summary
display_summary() {
    local version="$1"
    local description="$2"
    local archive_path="$3"
    local notes_file="$4"

    echo
    echo "=================================="
    echo "Version $version Created Successfully"
    echo "=================================="
    echo "Description: $description"
    echo "Archive: $(basename "$archive_path")"
    echo "Notes: $(basename "$notes_file")"
    echo "Size: $(du -h "$archive_path" | cut -f1)"
    echo
    echo "Files included:"
    echo "  ✓ Source code (src/)"
    echo "  ✓ Headers (include/)"
    echo "  ✓ Vendored dependencies (external/)"
    echo "  ✓ Build scripts (scripts/)"
    echo "  ✓ Development tools (development_tools/)"
    echo "  ✓ Build system (CMakeLists.txt)"
    echo "  ✓ Documentation (doc/, README.md, VERSION_NOTES.md)"
    echo "  ✓ License files (LICENSE, COPYRIGHT)"
    echo
    echo "Files excluded:"
    echo "  ✗ Build artifacts (build/)"
    echo "  ✗ Output files (output/, *.stl, *.svg)"
    echo "  ✗ Test files and logs (test/)"
    echo "  ✗ Cache and temporary files (cache/)"
    echo "  ✗ IDE and editor files (.vscode/, .idea/)"
    echo "  ✗ Version archives (version_control/*.zip)"
    echo "  ✗ Private development files (CLAUDE.md, .claude/)"
    echo
    echo "To extract and build:"
    echo "  unzip $(basename "$archive_path")"
    echo "  cd ${PROJECT_NAME}_v${version}"
    echo "  ./scripts/setup_environment.sh --auto-install --auto-build"
    echo
    echo "Or manual build:"
    echo "  ./scripts/quick-build.sh --release"
    echo
}

# Function to ensure version control directory exists
ensure_version_dir() {
    if [[ ! -d "$VERSION_DIR" ]]; then
        log_info "Creating version control directory: $VERSION_DIR"
        mkdir -p "$VERSION_DIR"
    fi
}

# Main script execution
main() {
    local description_input="$1"
    local specified_version=""

    # Parse arguments
    shift
    while [[ $# -gt 0 ]]; do
        case $1 in
            --version)
                specified_version="$2"
                shift 2
                ;;
            -h|--help)
                cat << EOF
C++ Topographic Generator Version Control Script

Usage:
  $0 <description>                    # Auto-increment revision
  $0 <description> --version MM.mm.rrr   # Specify version

Arguments:
  description    Short description of changes (e.g., 'performance_optimizations')

Options:
  --version MM.mm.rrr    Specify exact version (format: major.minor.revision)
  --help, -h             Show this help message

Examples:
  $0 "public_beta_release"
  $0 "bug_fixes" --version 0.22.005
  $0 "performance_improvements"

Version Format:
  MM.mm.rrr where:
    MM  = Major version (e.g., 0, 1, 2)
    mm  = Minor version (e.g., 22, 23)
    rrr = Revision (3-digit, zero-padded, e.g., 001, 012, 123)

Behavior:
  - If --version specified: Updates CMakeLists.txt and uses that version
  - If no --version: Auto-increments revision from CMakeLists.txt version
  - Archives always include updated CMakeLists.txt with correct version

EOF
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    log_info "C++ Topographic Generator Version Control Script"
    echo

    # Validate we're in the right directory
    if [[ ! -f "$CMAKE_FILE" ]] || [[ ! -d "src" ]]; then
        log_error "This script must be run from the C++ topographic generator root directory"
        log_error "Expected to find CMakeLists.txt and src/ directory"
        exit 1
    fi

    # Validate and sanitize description
    local description=$(validate_description "$description_input")

    # Ensure version control directory exists
    ensure_version_dir

    # Determine version
    local version
    if [[ -n "$specified_version" ]]; then
        # User specified version - validate and use it
        log_info "Using specified version: $specified_version"
        parse_version "$specified_version" > /dev/null  # Validate format
        version="$specified_version"
        update_cmake_version "$version"
    else
        # Auto-increment from current CMakeLists.txt version
        local current_version=$(get_cmake_version)
        log_info "Current CMakeLists.txt version: $current_version"
        version=$(increment_revision "$current_version")
        log_info "Auto-incrementing to version: $version"
        update_cmake_version "$version"
    fi

    # Generate timestamp
    local timestamp=$(date "+$DATE_FORMAT")

    # Get project statistics
    local stats=$(get_project_stats)
    log_info "Project stats: $stats"

    # Create version notes
    log_info "Creating version notes..."
    local archive_name="${PROJECT_NAME}_v${version}_${description}_${timestamp}.zip"
    local notes_file=$(create_version_notes "$version" "$description" "$timestamp" "$archive_name" "$stats")

    # Create archive
    log_info "Creating version archive..."
    if create_archive "$version" "$description" "$timestamp" "$notes_file"; then
        local archive_path="${VERSION_DIR}/${archive_name}"

        # Create Git tag if Git is available
        if command -v git &> /dev/null && [[ -d ".git" ]]; then
            log_info "Creating Git tag: v${version}"
            if git tag -a "v${version}" -m "Version ${version}: ${description}" 2>/dev/null; then
                log_success "Git tag v${version} created"
                log_info "To push tag to remote: git push origin v${version}"
            else
                log_warning "Git tag v${version} already exists or could not be created"
                log_info "To overwrite existing tag: git tag -f -a v${version} -m \"Version ${version}: ${description}\""
            fi
        else
            log_info "Git not available - skipping tag creation"
        fi

        # Display summary
        display_summary "$version" "$description" "$archive_path" "$notes_file"

        log_success "Version $version created successfully!"
        log_info "CMakeLists.txt updated to version $version"
        log_info "VERSION_NOTES.md included at root of archive"

        # Remind about Git workflow if Git is available
        if command -v git &> /dev/null && [[ -d ".git" ]]; then
            echo ""
            log_info "Git Workflow Reminder:"
            echo "  1. Review changes: git status"
            echo "  2. Commit if needed: git add -A && git commit -m \"Version $version release\""
            echo "  3. Push tag to remote: git push origin v${version}"
            echo "  4. Create GitHub release from tag: gh release create v${version}"
        fi

        exit 0
    else
        log_error "Failed to create version $version"
        exit 1
    fi
}

# Run main function
main "$@"
