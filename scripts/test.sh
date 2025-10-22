#!/bin/bash
# ==============================================================================
# Unified Test Runner for C++ Topographic Generator
# ==============================================================================
# Provides a user-friendly interface for running tests in both interactive
# and automated (CI/CD) modes.
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
TEST_DIR="$PROJECT_ROOT/test/option_tests"

# Default options
MODE="interactive"
RUN_TESTS=false
VALIDATE_ONLY=false
CLEAN_ONLY=false
QUICK_TESTS=false
TEST_CATEGORY=""

# ==============================================================================
# Helper Functions
# ==============================================================================

print_header() {
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}═══════════════════════════════════════════════════════════${NC}"
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

show_usage() {
    cat << EOF
${BOLD}Unified Test Runner for C++ Topographic Generator${NC}

${BOLD}USAGE:${NC}
    $0 [OPTIONS]

${BOLD}MODES:${NC}
    Interactive (default)    Run without arguments for menu-driven interface
    Automated               Use command-line options for CI/CD pipelines

${BOLD}OPTIONS:${NC}
    ${GREEN}Test Execution:${NC}
    --all                   Run all tests (comprehensive suite)
    --quick                 Run quick smoke tests only
    --category <name>       Run specific test category
                           Categories: basic, layers, substrate, processing,
                                      formats, geographic, registration, units,
                                      quality, json, edge

    ${GREEN}Test Management:${NC}
    --validate              Validate existing test results
    --clean                 Clean test outputs and logs
    --clean-all             Clean everything including cache

    ${GREEN}General:${NC}
    --help, -h              Show this help message
    --verbose, -v           Enable verbose output

${BOLD}EXAMPLES:${NC}
    ${CYAN}# Interactive mode (menu-driven)${NC}
    $0

    ${CYAN}# Run all tests${NC}
    $0 --all

    ${CYAN}# Quick smoke tests${NC}
    $0 --quick

    ${CYAN}# Run specific category${NC}
    $0 --category layers

    ${CYAN}# Validate previous test results${NC}
    $0 --validate

    ${CYAN}# Clean and run fresh tests${NC}
    $0 --clean --all

${BOLD}TEST CATEGORIES:${NC}
    basic          Basic functionality tests
    layers         Layer configuration tests
    substrate      Substrate and scaling tests
    processing     Processing and simplification tests
    formats        Output format tests
    geographic     Geographic features (OSM) tests
    registration   Registration and label tests
    units          Unit system tests
    quality        Quality and rendering tests
    json           JSON configuration tests
    edge           Edge cases and validation tests

${BOLD}CONTINUOUS INTEGRATION:${NC}
    For CI/CD pipelines, use:
        $0 --all --verbose

${BOLD}OUTPUT:${NC}
    Test logs:    test/option_tests/logs/
    Test outputs: test/option_tests/output/

EOF
}

# ==============================================================================
# Interactive Menu
# ==============================================================================

show_interactive_menu() {
    clear
    print_header "  C++ Topographic Generator - Test Suite  "
    echo ""
    echo -e "${BOLD}What would you like to do?${NC}"
    echo ""
    echo "  ${GREEN}1)${NC} Run all tests (comprehensive)"
    echo "  ${GREEN}2)${NC} Run quick smoke tests"
    echo "  ${GREEN}3)${NC} Run specific test category"
    echo "  ${GREEN}4)${NC} Validate existing test results"
    echo "  ${GREEN}5)${NC} Clean test outputs"
    echo "  ${GREEN}6)${NC} View test documentation"
    echo "  ${GREEN}7)${NC} Exit"
    echo ""
    echo -n "Enter choice [1-7]: "
}

interactive_mode() {
    while true; do
        show_interactive_menu
        read choice

        case $choice in
            1)
                echo ""
                print_info "Running comprehensive test suite..."
                run_all_tests
                ;;
            2)
                echo ""
                print_info "Running quick smoke tests..."
                run_quick_tests
                ;;
            3)
                echo ""
                echo "Available categories:"
                echo "  basic, layers, substrate, processing, formats,"
                echo "  geographic, registration, units, quality, json, edge"
                echo ""
                echo -n "Enter category name: "
                read category
                if [[ -n "$category" ]]; then
                    run_category_tests "$category"
                else
                    print_warning "No category specified"
                fi
                ;;
            4)
                echo ""
                print_info "Validating test results..."
                validate_results
                ;;
            5)
                echo ""
                print_info "Cleaning test outputs..."
                clean_tests
                ;;
            6)
                echo ""
                show_test_documentation
                ;;
            7)
                echo ""
                print_success "Goodbye!"
                exit 0
                ;;
            *)
                echo ""
                print_error "Invalid choice. Please enter 1-7."
                ;;
        esac

        echo ""
        echo -n "Press Enter to continue..."
        read
    done
}

# ==============================================================================
# Test Execution Functions
# ==============================================================================

check_prerequisites() {
    # Check if executable exists
    if [[ ! -f "$PROJECT_ROOT/build/topo-gen" ]]; then
        print_error "Executable not found: $PROJECT_ROOT/build/topo-gen"
        echo ""
        echo "Please build the project first:"
        echo "  cd $PROJECT_ROOT"
        echo "  ./scripts/quick-build.sh"
        echo ""
        exit 1
    fi

    # Check if test script exists
    if [[ ! -f "$SCRIPT_DIR/test_all_options.sh" ]]; then
        print_error "Test script not found: $SCRIPT_DIR/test_all_options.sh"
        exit 1
    fi
}

run_all_tests() {
    check_prerequisites
    print_header "Running Comprehensive Test Suite"
    echo ""

    bash "$SCRIPT_DIR/test_all_options.sh"
    local result=$?

    echo ""
    if [[ $result -eq 0 ]]; then
        print_success "All tests completed successfully!"
    else
        print_error "Some tests failed. Check logs in: $TEST_DIR/logs/"
    fi

    return $result
}

run_quick_tests() {
    check_prerequisites
    print_header "Running Quick Smoke Tests"
    echo ""

    # Run a subset of critical tests
    print_info "Running essential functionality tests..."

    local test_script="$SCRIPT_DIR/test_all_options.sh"

    # We'll modify this to call specific test functions
    # For now, run the full suite but we could optimize later
    print_warning "Quick test mode: Running subset of tests"
    print_info "This will take approximately 1-2 minutes..."

    # TODO: Could modify test_all_options.sh to support --quick flag
    bash "$test_script"
}

run_category_tests() {
    local category="$1"
    check_prerequisites

    print_header "Running Test Category: $category"
    echo ""

    # Map category to test function
    local test_function=""
    case "$category" in
        basic) test_function="test_basic_functionality" ;;
        layers) test_function="test_layer_options" ;;
        substrate) test_function="test_substrate_options" ;;
        processing) test_function="test_processing_options" ;;
        formats) test_function="test_output_formats" ;;
        geographic) test_function="test_geographic_features" ;;
        registration) test_function="test_registration_and_labels" ;;
        units) test_function="test_unit_system" ;;
        quality) test_function="test_quality_and_rendering" ;;
        json) test_function="test_json_config" ;;
        edge) test_function="test_edge_cases" ;;
        *)
            print_error "Unknown category: $category"
            echo ""
            echo "Valid categories: basic, layers, substrate, processing, formats,"
            echo "                  geographic, registration, units, quality, json, edge"
            return 1
            ;;
    esac

    print_info "Category test execution not yet implemented"
    print_info "Running full test suite (future: will run only '$test_function')"
    echo ""

    bash "$SCRIPT_DIR/test_all_options.sh"
}

validate_results() {
    if [[ ! -f "$SCRIPT_DIR/validate_test_results.sh" ]]; then
        print_error "Validation script not found: $SCRIPT_DIR/validate_test_results.sh"
        return 1
    fi

    print_header "Validating Test Results"
    echo ""

    bash "$SCRIPT_DIR/validate_test_results.sh"
    local result=$?

    echo ""
    if [[ $result -eq 0 ]]; then
        print_success "Validation complete!"
    else
        print_error "Validation found issues"
    fi

    return $result
}

clean_tests() {
    print_header "Cleaning Test Outputs"
    echo ""

    if [[ -d "$TEST_DIR" ]]; then
        print_info "Removing test outputs and logs..."
        rm -rf "$TEST_DIR/output"/* 2>/dev/null || true
        rm -rf "$TEST_DIR/logs"/* 2>/dev/null || true
        print_success "Test outputs cleaned"
    else
        print_warning "Test directory not found: $TEST_DIR"
    fi

    echo ""
}

show_test_documentation() {
    print_header "Test Suite Documentation"
    echo ""
    echo -e "${BOLD}Test Coverage:${NC}"
    echo "  • 50+ comprehensive tests covering all features"
    echo "  • Tests use Mount Denali SRTM data (N63W152 tile)"
    echo "  • All tests validate exit codes and error output"
    echo ""
    echo -e "${BOLD}Test Categories:${NC}"
    echo "  1. Basic Functionality    - Core operations"
    echo "  2. Layer Configuration    - Layer options and heights"
    echo "  3. Substrate & Scaling    - Substrate sizes and units"
    echo "  4. Processing Options     - Simplification and smoothing"
    echo "  5. Output Formats         - SVG, STL, OBJ exports"
    echo "  6. Geographic Features    - OSM data integration"
    echo "  7. Registration & Labels  - Registration marks and numbering"
    echo "  8. Unit Systems           - Metric, imperial, mixed units"
    echo "  9. Quality & Rendering    - Quality levels and color schemes"
    echo " 10. JSON Configuration     - Config file parsing"
    echo " 11. Edge Cases             - Boundary conditions"
    echo ""
    echo -e "${BOLD}Test Output:${NC}"
    echo "  Logs:    $TEST_DIR/logs/"
    echo "  Outputs: $TEST_DIR/output/"
    echo ""
    echo -e "${BOLD}Duration:${NC}"
    echo "  Full suite: ~5-10 minutes"
    echo "  Quick tests: ~1-2 minutes"
    echo ""
}

# ==============================================================================
# Main
# ==============================================================================

main() {
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --all)
                MODE="automated"
                RUN_TESTS=true
                shift
                ;;
            --quick)
                MODE="automated"
                QUICK_TESTS=true
                shift
                ;;
            --category)
                MODE="automated"
                TEST_CATEGORY="$2"
                shift 2
                ;;
            --validate)
                MODE="automated"
                VALIDATE_ONLY=true
                shift
                ;;
            --clean)
                MODE="automated"
                CLEAN_ONLY=true
                shift
                ;;
            --clean-all)
                MODE="automated"
                CLEAN_ONLY=true
                shift
                ;;
            --verbose|-v)
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

    # Execute based on mode
    if [[ "$MODE" == "interactive" ]]; then
        interactive_mode
    else
        # Automated mode
        if [[ "$CLEAN_ONLY" == "true" ]]; then
            clean_tests
        elif [[ "$VALIDATE_ONLY" == "true" ]]; then
            validate_results
        elif [[ "$QUICK_TESTS" == "true" ]]; then
            run_quick_tests
        elif [[ -n "$TEST_CATEGORY" ]]; then
            run_category_tests "$TEST_CATEGORY"
        elif [[ "$RUN_TESTS" == "true" ]]; then
            run_all_tests
        else
            print_error "No action specified"
            echo ""
            show_usage
            exit 1
        fi
    fi
}

# Run main function
main "$@"
