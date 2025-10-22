#!/bin/bash
# ==============================================================================
# Clean Script for C++ Topographic Generator
# ==============================================================================
# Removes build artifacts, test outputs, and cached data
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License
# ==============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default options
CLEAN_BUILD=false
CLEAN_TEST=false
CLEAN_CACHE=false
CLEAN_OUTPUT=false
CLEAN_ALL=false
DRY_RUN=false
VERBOSE=false

# ==============================================================================
# Helper Functions
# ==============================================================================

print_header() {
    echo -e "${BLUE}══════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}══════════════════════════════════════════════════════${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC}  $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

show_usage() {
    cat << EOF
${BOLD}Clean Script for C++ Topographic Generator${NC}

${BOLD}USAGE:${NC}
    $0 [OPTIONS]

${BOLD}OPTIONS:${NC}
    ${GREEN}Cleaning Targets:${NC}
    --build                 Clean build artifacts (build/ directory)
    --test                  Clean test outputs and logs
    --cache                 Clean downloaded SRTM tiles and caches
    --output                Clean generated output files
    --all                   Clean everything (build + test + cache + output)

    ${GREEN}Behavior:${NC}
    --dry-run, -n           Show what would be deleted without deleting
    --verbose, -v           Show detailed output
    --help, -h              Show this help message

${BOLD}EXAMPLES:${NC}
    ${CYAN}# Clean build artifacts only${NC}
    $0 --build

    ${CYAN}# Clean test outputs${NC}
    $0 --test

    ${CYAN}# Clean everything${NC}
    $0 --all

    ${CYAN}# Dry run to see what would be deleted${NC}
    $0 --all --dry-run

    ${CYAN}# Clean build and test (but keep cache)${NC}
    $0 --build --test

${BOLD}WHAT GETS CLEANED:${NC}
    ${GREEN}--build:${NC}
        • build/ directory (all CMake artifacts and executables)
        • *.o object files
        • *.a static libraries

    ${GREEN}--test:${NC}
        • test/option_tests/output/ (test output files)
        • test/option_tests/logs/ (test log files)
        • test/output/ (general test outputs)

    ${GREEN}--cache:${NC}
        • cache/tiles/ (downloaded SRTM elevation tiles)
        • *.hgt cached elevation files

    ${GREEN}--output:${NC}
        • output/ directory (generated models)
        • *.stl, *.svg, *.obj files in project root

${BOLD}NOTES:${NC}
    • Use --dry-run first to preview what will be deleted
    • Cache cleaning will require re-downloading SRTM tiles
    • Build cleaning will require a full rebuild

EOF
}

clean_directory() {
    local dir="$1"
    local description="$2"

    if [[ ! -d "$dir" ]]; then
        if [[ "$VERBOSE" == "true" ]]; then
            print_info "Directory does not exist: $dir"
        fi
        return 0
    fi

    if [[ "$DRY_RUN" == "true" ]]; then
        local size=$(du -sh "$dir" 2>/dev/null | cut -f1 || echo "unknown")
        print_info "[DRY RUN] Would delete: $description ($size)"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "    Path: $dir"
        fi
    else
        local size=$(du -sh "$dir" 2>/dev/null | cut -f1 || echo "unknown")
        print_info "Deleting: $description ($size)"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "    Path: $dir"
        fi
        rm -rf "$dir"
        print_success "Deleted: $description"
    fi
}

clean_pattern() {
    local pattern="$1"
    local description="$2"
    local base_dir="${3:-.}"

    cd "$PROJECT_ROOT/$base_dir"

    local files=$(find . -name "$pattern" -type f 2>/dev/null)
    local count=$(echo "$files" | grep -c . || echo 0)

    if [[ $count -eq 0 ]]; then
        if [[ "$VERBOSE" == "true" ]]; then
            print_info "No files matching: $pattern"
        fi
        return 0
    fi

    if [[ "$DRY_RUN" == "true" ]]; then
        print_info "[DRY RUN] Would delete $count file(s): $description"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "$files" | sed 's/^/    /'
        fi
    else
        print_info "Deleting $count file(s): $description"
        if [[ "$VERBOSE" == "true" ]]; then
            echo "$files" | sed 's/^/    /'
        fi
        find . -name "$pattern" -type f -delete
        print_success "Deleted $count file(s): $description"
    fi
}

# ==============================================================================
# Cleaning Functions
# ==============================================================================

clean_build_artifacts() {
    print_header "Cleaning Build Artifacts"
    echo ""

    cd "$PROJECT_ROOT"

    # Clean build directory
    clean_directory "$PROJECT_ROOT/build" "Build directory"

    # Clean object files
    clean_pattern "*.o" "Object files"

    # Clean static libraries
    clean_pattern "*.a" "Static libraries"

    # Clean CMake cache files in project root
    if [[ "$DRY_RUN" == "true" ]]; then
        if [[ -f "$PROJECT_ROOT/CMakeCache.txt" ]]; then
            print_info "[DRY RUN] Would delete: CMakeCache.txt"
        fi
    else
        if [[ -f "$PROJECT_ROOT/CMakeCache.txt" ]]; then
            rm -f "$PROJECT_ROOT/CMakeCache.txt"
            print_success "Deleted: CMakeCache.txt"
        fi
    fi

    echo ""
}

clean_test_outputs() {
    print_header "Cleaning Test Outputs"
    echo ""

    cd "$PROJECT_ROOT"

    # Clean test option outputs
    clean_directory "$PROJECT_ROOT/test/option_tests/output" "Test option outputs"
    clean_directory "$PROJECT_ROOT/test/option_tests/logs" "Test logs"

    # Clean general test outputs
    clean_directory "$PROJECT_ROOT/test/output" "General test outputs"

    # Clean test temporary files
    clean_pattern "*.log" "Test log files" "test"

    echo ""
}

clean_cache_data() {
    print_header "Cleaning Cache Data"
    echo ""

    cd "$PROJECT_ROOT"

    # Clean SRTM tile cache
    clean_directory "$PROJECT_ROOT/cache/tiles" "SRTM tile cache"

    # Clean any .hgt files
    clean_pattern "*.hgt" "Cached elevation files"

    print_warning "Note: SRTM tiles will be re-downloaded on next use"

    echo ""
}

clean_output_files() {
    print_header "Cleaning Output Files"
    echo ""

    cd "$PROJECT_ROOT"

    # Clean output directory
    clean_directory "$PROJECT_ROOT/output" "Output directory"

    # Clean output files in project root
    clean_pattern "*.stl" "STL files"
    clean_pattern "*.svg" "SVG files"
    clean_pattern "*.obj" "OBJ files"
    clean_pattern "*.ply" "PLY files"
    clean_pattern "*.png" "PNG files"

    echo ""
}

# ==============================================================================
# Main
# ==============================================================================

main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --build)
                CLEAN_BUILD=true
                shift
                ;;
            --test)
                CLEAN_TEST=true
                shift
                ;;
            --cache)
                CLEAN_CACHE=true
                shift
                ;;
            --output)
                CLEAN_OUTPUT=true
                shift
                ;;
            --all)
                CLEAN_ALL=true
                shift
                ;;
            --dry-run|-n)
                DRY_RUN=true
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                shift
                ;;
            --help|-h)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                echo ""
                show_usage
                exit 1
                ;;
        esac
    done

    # If --all is specified, set all flags
    if [[ "$CLEAN_ALL" == "true" ]]; then
        CLEAN_BUILD=true
        CLEAN_TEST=true
        CLEAN_CACHE=true
        CLEAN_OUTPUT=true
    fi

    # Check if any cleaning option was specified
    if [[ "$CLEAN_BUILD" == "false" && "$CLEAN_TEST" == "false" && \
          "$CLEAN_CACHE" == "false" && "$CLEAN_OUTPUT" == "false" ]]; then
        print_error "No cleaning target specified"
        echo ""
        show_usage
        exit 1
    fi

    # Print dry run notice
    if [[ "$DRY_RUN" == "true" ]]; then
        echo ""
        print_warning "DRY RUN MODE - No files will be deleted"
        echo ""
    fi

    # Execute cleaning operations
    if [[ "$CLEAN_BUILD" == "true" ]]; then
        clean_build_artifacts
    fi

    if [[ "$CLEAN_TEST" == "true" ]]; then
        clean_test_outputs
    fi

    if [[ "$CLEAN_CACHE" == "true" ]]; then
        clean_cache_data
    fi

    if [[ "$CLEAN_OUTPUT" == "true" ]]; then
        clean_output_files
    fi

    # Final summary
    if [[ "$DRY_RUN" == "true" ]]; then
        echo ""
        print_info "Dry run complete. No files were deleted."
        print_info "Run without --dry-run to actually delete files."
    else
        echo ""
        print_success "Cleaning complete!"

        if [[ "$CLEAN_BUILD" == "true" ]]; then
            print_info "Rebuild required: ./scripts/quick-build.sh"
        fi
    fi

    echo ""
}

# Run main function
main "$@"
