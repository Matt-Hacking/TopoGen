#!/bin/bash
# ==============================================================================
# Continuous Integration Script for C++ Topographic Generator
# ==============================================================================
# Automated build, test, and validation pipeline for CI/CD systems
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
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
CI_LOG_DIR="$PROJECT_ROOT/ci_logs"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# CI Options
SKIP_DEPS_CHECK=false
SKIP_BUILD=false
SKIP_TESTS=false
CLEAN_BUILD=false
BUILD_TYPE="Release"
VERBOSE=false

# Exit codes
EXIT_SUCCESS=0
EXIT_DEPS_FAILED=1
EXIT_BUILD_FAILED=2
EXIT_TEST_FAILED=3
EXIT_VALIDATE_FAILED=4

# ==============================================================================
# Helper Functions
# ==============================================================================

print_header() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC} $(printf '%-64s' "$1") ${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_step() {
    echo ""
    echo -e "${BOLD}${BLUE}▶ $1${NC}"
    echo ""
}

print_info() {
    echo -e "${BLUE}ℹ${NC}  $1"
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC}  $1"
}

log_section() {
    echo "" | tee -a "$CI_LOG"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$CI_LOG"
    echo "$1" | tee -a "$CI_LOG"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" | tee -a "$CI_LOG"
    echo "" | tee -a "$CI_LOG"
}

show_usage() {
    cat << EOF
${BOLD}Continuous Integration Script${NC}

${BOLD}USAGE:${NC}
    $0 [OPTIONS]

${BOLD}OPTIONS:${NC}
    ${GREEN}Pipeline Control:${NC}
    --skip-deps             Skip dependency check
    --skip-build            Skip build step
    --skip-tests            Skip test execution
    --clean                 Clean build before starting

    ${GREEN}Build Configuration:${NC}
    --debug                 Build in Debug mode (default: Release)
    --release               Build in Release mode

    ${GREEN}General:${NC}
    --verbose, -v           Enable verbose output
    --help, -h              Show this help message

${BOLD}EXAMPLES:${NC}
    ${CYAN}# Full CI pipeline${NC}
    $0

    ${CYAN}# Clean build and test${NC}
    $0 --clean

    ${CYAN}# Build only (skip tests)${NC}
    $0 --skip-tests

    ${CYAN}# Debug build with verbose output${NC}
    $0 --debug --verbose

${BOLD}CI PIPELINE STAGES:${NC}
    1. Environment Check    - Verify dependencies and tools
    2. Clean (optional)     - Remove previous build artifacts
    3. Build                - Compile project
    4. Test                 - Run comprehensive test suite
    5. Validate             - Validate test outputs
    6. Report               - Generate CI report

${BOLD}OUTPUT:${NC}
    CI logs:  ci_logs/ci_<timestamp>.log
    Reports:  ci_logs/ci_report_<timestamp>.txt

${BOLD}EXIT CODES:${NC}
    0   Success
    1   Dependency check failed
    2   Build failed
    3   Tests failed
    4   Validation failed

EOF
}

# ==============================================================================
# CI Pipeline Functions
# ==============================================================================

initialize_ci() {
    print_header "Continuous Integration Pipeline"

    # Create CI log directory
    mkdir -p "$CI_LOG_DIR"

    # Set up logging
    CI_LOG="$CI_LOG_DIR/ci_${TIMESTAMP}.log"
    CI_REPORT="$CI_LOG_DIR/ci_report_${TIMESTAMP}.txt"

    # Log CI run info
    {
        echo "CI Pipeline Run"
        echo "Start Time: $(date)"
        echo "Build Type: $BUILD_TYPE"
        echo "Project Root: $PROJECT_ROOT"
        echo ""
    } > "$CI_LOG"

    print_info "CI log: $CI_LOG"
    print_info "Build type: $BUILD_TYPE"
    echo ""
}

check_dependencies() {
    if [[ "$SKIP_DEPS_CHECK" == "true" ]]; then
        print_step "Skipping dependency check (--skip-deps)"
        return 0
    fi

    print_step "STAGE 1: Checking Dependencies"

    log_section "STAGE 1: DEPENDENCY CHECK" >> "$CI_LOG"

    if [[ ! -f "$SCRIPT_DIR/check_dependencies.sh" ]]; then
        print_error "Dependency checker not found"
        return $EXIT_DEPS_FAILED
    fi

    if bash "$SCRIPT_DIR/check_dependencies.sh" >> "$CI_LOG" 2>&1; then
        print_success "All dependencies satisfied"
        return 0
    else
        print_error "Dependency check failed"
        echo ""
        print_info "Check the log for details: $CI_LOG"
        return $EXIT_DEPS_FAILED
    fi
}

clean_build_artifacts() {
    if [[ "$CLEAN_BUILD" == "false" ]]; then
        return 0
    fi

    print_step "STAGE 2: Cleaning Build Artifacts"

    log_section "STAGE 2: CLEAN BUILD" >> "$CI_LOG"

    print_info "Removing build directory..."
    rm -rf "$BUILD_DIR"
    print_success "Build artifacts cleaned"
}

build_project() {
    if [[ "$SKIP_BUILD" == "true" ]]; then
        print_step "Skipping build (--skip-build)"
        return 0
    fi

    print_step "STAGE 3: Building Project"

    log_section "STAGE 3: BUILD" >> "$CI_LOG"

    print_info "Build type: $BUILD_TYPE"
    print_info "Build directory: $BUILD_DIR"
    echo ""

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure with CMake
    print_info "Running CMake configuration..."
    if cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        >> "$CI_LOG" 2>&1; then
        print_success "CMake configuration successful"
    else
        print_error "CMake configuration failed"
        echo ""
        print_info "Check the log for details: $CI_LOG"
        return $EXIT_BUILD_FAILED
    fi

    echo ""

    # Build
    print_info "Compiling..."
    local nproc=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

    if make -j"$nproc" >> "$CI_LOG" 2>&1; then
        print_success "Build successful"
    else
        print_error "Build failed"
        echo ""
        print_info "Check the log for details: $CI_LOG"
        return $EXIT_BUILD_FAILED
    fi

    # Verify executable
    if [[ -f "$BUILD_DIR/topo-gen" ]]; then
        print_success "Executable created: topo-gen"
        local size=$(du -h "$BUILD_DIR/topo-gen" | cut -f1)
        print_info "Binary size: $size"
    else
        print_error "Executable not found"
        return $EXIT_BUILD_FAILED
    fi

    cd "$PROJECT_ROOT"
    return 0
}

run_tests() {
    if [[ "$SKIP_TESTS" == "true" ]]; then
        print_step "Skipping tests (--skip-tests)"
        return 0
    fi

    print_step "STAGE 4: Running Tests"

    log_section "STAGE 4: TEST EXECUTION" >> "$CI_LOG"

    if [[ ! -f "$SCRIPT_DIR/test.sh" ]]; then
        print_error "Test runner not found"
        return $EXIT_TEST_FAILED
    fi

    print_info "Running comprehensive test suite..."
    echo ""

    if bash "$SCRIPT_DIR/test.sh" --all --verbose >> "$CI_LOG" 2>&1; then
        print_success "All tests passed"
        return 0
    else
        print_error "Tests failed"
        echo ""
        print_info "Check the log for details: $CI_LOG"
        return $EXIT_TEST_FAILED
    fi
}

validate_outputs() {
    if [[ "$SKIP_TESTS" == "true" ]]; then
        print_step "Skipping validation (tests skipped)"
        return 0
    fi

    print_step "STAGE 5: Validating Test Outputs"

    log_section "STAGE 5: OUTPUT VALIDATION" >> "$CI_LOG"

    if [[ ! -f "$SCRIPT_DIR/test.sh" ]]; then
        print_error "Test script not found"
        return $EXIT_VALIDATE_FAILED
    fi

    print_info "Validating test results..."
    echo ""

    if bash "$SCRIPT_DIR/test.sh" --validate >> "$CI_LOG" 2>&1; then
        print_success "Validation passed"
        return 0
    else
        print_error "Validation failed"
        echo ""
        print_info "Check the log for details: $CI_LOG"
        return $EXIT_VALIDATE_FAILED
    fi
}

generate_report() {
    print_step "STAGE 6: Generating CI Report"

    local end_time=$(date)
    local duration=$SECONDS

    cat > "$CI_REPORT" << EOF
╔══════════════════════════════════════════════════════════════════╗
║  CI Pipeline Report - C++ Topographic Generator                 ║
╚══════════════════════════════════════════════════════════════════╝

RUN INFORMATION
───────────────────────────────────────────────────────────────────
Start Time:      $(head -2 "$CI_LOG" | tail -1 | cut -d: -f2-)
End Time:        $end_time
Duration:        ${duration}s
Build Type:      $BUILD_TYPE
Project Root:    $PROJECT_ROOT

PIPELINE STAGES
───────────────────────────────────────────────────────────────────
✓ Stage 1: Dependencies  $(grep -q "DEPENDENCY CHECK" "$CI_LOG" && echo "[PASS]" || echo "[SKIP]")
✓ Stage 2: Clean         $([ "$CLEAN_BUILD" == "true" ] && echo "[DONE]" || echo "[SKIP]")
✓ Stage 3: Build         $(grep -q "BUILD" "$CI_LOG" && echo "[PASS]" || echo "[SKIP]")
✓ Stage 4: Test          $(grep -q "TEST EXECUTION" "$CI_LOG" && echo "[PASS]" || echo "[SKIP]")
✓ Stage 5: Validate      $(grep -q "OUTPUT VALIDATION" "$CI_LOG" && echo "[PASS]" || echo "[SKIP]")

BUILD ARTIFACTS
───────────────────────────────────────────────────────────────────
EOF

    if [[ -f "$BUILD_DIR/topo-gen" ]]; then
        {
            echo "Executable:      $BUILD_DIR/topo-gen"
            echo "Binary Size:     $(du -h "$BUILD_DIR/topo-gen" | cut -f1)"
            echo "Architecture:    $(file "$BUILD_DIR/topo-gen" | cut -d: -f2)"
        } >> "$CI_REPORT"
    else
        echo "Executable:      [NOT FOUND]" >> "$CI_REPORT"
    fi

    cat >> "$CI_REPORT" << EOF

TEST RESULTS
───────────────────────────────────────────────────────────────────
EOF

    if [[ -d "$PROJECT_ROOT/test/option_tests/logs" ]]; then
        local test_count=$(find "$PROJECT_ROOT/test/option_tests/logs" -name "*.log" 2>/dev/null | wc -l | tr -d ' ')
        echo "Test Runs:       $test_count" >> "$CI_REPORT"
    else
        echo "Test Runs:       [NO TESTS]" >> "$CI_REPORT"
    fi

    cat >> "$CI_REPORT" << EOF

LOG FILES
───────────────────────────────────────────────────────────────────
CI Log:          $CI_LOG
Report:          $CI_REPORT

═══════════════════════════════════════════════════════════════════

EOF

    print_success "CI report generated: $CI_REPORT"
    echo ""

    # Display report
    cat "$CI_REPORT"
}

# ==============================================================================
# Main CI Pipeline
# ==============================================================================

main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-deps)
                SKIP_DEPS_CHECK=true
                shift
                ;;
            --skip-build)
                SKIP_BUILD=true
                shift
                ;;
            --skip-tests)
                SKIP_TESTS=true
                shift
                ;;
            --clean)
                CLEAN_BUILD=true
                shift
                ;;
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --release)
                BUILD_TYPE="Release"
                shift
                ;;
            --verbose|-v)
                VERBOSE=true
                set -x
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

    # Initialize
    initialize_ci

    # Track overall result
    PIPELINE_FAILED=false

    # Run pipeline stages
    if ! check_dependencies; then
        PIPELINE_FAILED=true
        exit $EXIT_DEPS_FAILED
    fi

    clean_build_artifacts

    if ! build_project; then
        PIPELINE_FAILED=true
        generate_report
        exit $EXIT_BUILD_FAILED
    fi

    if ! run_tests; then
        PIPELINE_FAILED=true
        generate_report
        exit $EXIT_TEST_FAILED
    fi

    if ! validate_outputs; then
        PIPELINE_FAILED=true
        generate_report
        exit $EXIT_VALIDATE_FAILED
    fi

    # Generate final report
    generate_report

    # Final status
    if [[ "$PIPELINE_FAILED" == "false" ]]; then
        print_header "CI Pipeline: SUCCESS"
        print_success "All stages completed successfully!"
        echo ""
        exit $EXIT_SUCCESS
    else
        print_header "CI Pipeline: FAILED"
        print_error "Pipeline failed. Check logs for details."
        echo ""
        exit 1
    fi
}

# Run main function
main "$@"
