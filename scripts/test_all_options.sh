#!/bin/bash
# ==============================================================================
# Comprehensive Test Suite for Topographic Generator
# ==============================================================================
# Tests every available option with various values to ensure correctness
# Uses cached Mount Denali geodata (N63W152) for fast execution
# ==============================================================================

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
TOPO_GEN="$BUILD_DIR/topo-gen"
TEST_DIR="$PROJECT_ROOT/test/option_tests"
LOG_DIR="$TEST_DIR/logs"
OUTPUT_DIR="$TEST_DIR/output"

# Mount Denali coordinates (single SRTM tile: N63W152)
UPPER_LEFT="63.1497,-151.1847"
LOWER_RIGHT="62.9887,-150.8293"

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# ==============================================================================
# UTILITY FUNCTIONS
# ==============================================================================

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_test() {
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -e "${YELLOW}[TEST $TOTAL_TESTS] $1${NC}"
}

print_pass() {
    PASSED_TESTS=$((PASSED_TESTS + 1))
    echo -e "${GREEN}✓ PASS${NC}: $1"
}

print_fail() {
    FAILED_TESTS=$((FAILED_TESTS + 1))
    echo -e "${RED}✗ FAIL${NC}: $1"
}

run_test() {
    local test_name="$1"
    local base_name="$2"
    shift 2
    local args=("$@")

    print_test "$test_name"

    local log_file="$LOG_DIR/${base_name}.log"
    local err_file="$LOG_DIR/${base_name}.err"

    # Run the command with output to log files
    if "$TOPO_GEN" \
        --upper-left "$UPPER_LEFT" \
        --lower-right "$LOWER_RIGHT" \
        --base-name "$base_name" \
        --output-dir "$OUTPUT_DIR" \
        "${args[@]}" \
        > "$log_file" 2> "$err_file"; then

        # Check if error file has content (warnings are OK, errors are not)
        if grep -q "Error:" "$err_file" || grep -q "Fatal:" "$err_file"; then
            print_fail "$test_name - Errors in output"
            cat "$err_file"
            return 1
        fi

        print_pass "$test_name"
        return 0
    else
        print_fail "$test_name - Command failed with exit code $?"
        cat "$err_file"
        return 1
    fi
}

# ==============================================================================
# SETUP
# ==============================================================================

setup_test_environment() {
    print_header "Setting Up Test Environment"

    # Check if executable exists
    if [[ ! -f "$TOPO_GEN" ]]; then
        echo -e "${RED}Error: topo-gen executable not found at $TOPO_GEN${NC}"
        echo "Please build the project first: ./scripts/quick-build.sh"
        exit 1
    fi

    # Create test directories
    mkdir -p "$TEST_DIR"
    mkdir -p "$LOG_DIR"
    mkdir -p "$OUTPUT_DIR"

    # Clean previous test outputs
    echo "Cleaning previous test outputs..."
    rm -rf "$OUTPUT_DIR"/*
    rm -rf "$LOG_DIR"/*

    echo -e "${GREEN}✓ Environment ready${NC}"
    echo "Test directory: $TEST_DIR"
    echo "Executable: $TOPO_GEN"
    echo ""
}

# ==============================================================================
# TEST CATEGORIES
# ==============================================================================

test_basic_functionality() {
    print_header "Category 1: Basic Functionality"

    run_test "Default settings" \
        "test_01_default"

    run_test "Minimal options" \
        "test_02_minimal" \
        --num-layers 3

    run_test "Dry run mode" \
        "test_03_dryrun" \
        --num-layers 5 \
        --dry-run
}

test_layer_options() {
    print_header "Category 2: Layer Configuration"

    run_test "Custom layer count" \
        "test_04_layers_10" \
        --num-layers 10

    run_test "Custom height per layer" \
        "test_05_height_50m" \
        --num-layers 5 \
        --height-per-layer 50

    run_test "Height with unit suffix (feet)" \
        "test_06_height_200ft" \
        --num-layers 5 \
        --height-per-layer 200ft

    run_test "Height with unit suffix (km)" \
        "test_07_height_1km" \
        --num-layers 3 \
        --height-per-layer 1km

    run_test "Layer thickness (mm)" \
        "test_08_thickness_5mm" \
        --num-layers 5 \
        --layer-thickness 5

    run_test "Layer thickness with unit (inches)" \
        "test_09_thickness_0.25in" \
        --num-layers 5 \
        --layer-thickness 0.25in

    run_test "Force all layers" \
        "test_10_force_all" \
        --num-layers 8 \
        --force-all-layers
}

test_substrate_options() {
    print_header "Category 3: Substrate & Scaling"

    run_test "Substrate size 300mm" \
        "test_11_substrate_300mm" \
        --num-layers 5 \
        --substrate-size 300

    run_test "Substrate size with unit (inches)" \
        "test_12_substrate_12in" \
        --num-layers 5 \
        --substrate-size 12in

    run_test "Substrate size 500mm" \
        "test_13_substrate_500mm" \
        --num-layers 5 \
        --substrate-size 500
}

test_processing_options() {
    print_header "Category 4: Processing & Simplification"

    run_test "No simplification" \
        "test_14_simplify_0" \
        --num-layers 5 \
        --simplify 0

    run_test "Heavy simplification" \
        "test_15_simplify_20m" \
        --num-layers 5 \
        --simplify 20

    run_test "Simplify with unit (feet)" \
        "test_16_simplify_50ft" \
        --num-layers 5 \
        --simplify 50ft

    run_test "No smoothing" \
        "test_17_smooth_0" \
        --num-layers 5 \
        --smoothing 0

    run_test "Heavy smoothing" \
        "test_18_smooth_5" \
        --num-layers 5 \
        --smoothing 5

    run_test "Minimum area filter" \
        "test_19_minarea_500" \
        --num-layers 5 \
        --min-area 500

    run_test "Minimum feature width" \
        "test_20_minwidth_5mm" \
        --num-layers 5 \
        --min-feature-width 5

    run_test "Min width with unit (inches)" \
        "test_21_minwidth_0.2in" \
        --num-layers 5 \
        --min-feature-width 0.2in
}

test_output_formats() {
    print_header "Category 5: Output Formats"

    run_test "SVG output only" \
        "test_22_format_svg" \
        --num-layers 3 \
        --output-formats svg

    run_test "STL output only" \
        "test_23_format_stl" \
        --num-layers 3 \
        --output-formats stl

    run_test "Multiple formats (svg,stl)" \
        "test_24_formats_svg_stl" \
        --num-layers 3 \
        --output-formats svg,stl

    run_test "Multiple formats (svg,stl,obj)" \
        "test_25_formats_all" \
        --num-layers 3 \
        --output-formats svg,stl,obj

    run_test "Stacked output" \
        "test_26_stacked" \
        --num-layers 5 \
        --output-stacked \
        --output-formats stl

    run_test "Both layers and stacked" \
        "test_27_layers_and_stacked" \
        --num-layers 5 \
        --output-layers \
        --output-stacked \
        --output-formats svg
}

test_geographic_features() {
    print_header "Category 6: Geographic Features (OSM)"

    run_test "Include roads" \
        "test_28_roads" \
        --num-layers 5 \
        --include-roads

    run_test "Include buildings" \
        "test_29_buildings" \
        --num-layers 5 \
        --include-buildings

    run_test "Include waterways" \
        "test_30_waterways" \
        --num-layers 5 \
        --include-waterways

    run_test "All OSM features" \
        "test_31_all_osm" \
        --num-layers 5 \
        --include-roads \
        --include-buildings \
        --include-waterways
}

test_registration_and_labels() {
    print_header "Category 7: Registration & Labels"

    run_test "No registration marks" \
        "test_32_no_registration" \
        --num-layers 5 \
        --no-registration-marks

    run_test "With registration marks" \
        "test_33_registration" \
        --num-layers 5 \
        --add-registration-marks

    run_test "No layer numbers" \
        "test_34_no_layer_numbers" \
        --num-layers 5 \
        --no-layer-numbers

    run_test "With layer numbers" \
        "test_35_layer_numbers" \
        --num-layers 5 \
        --include-layer-numbers
}

test_unit_system() {
    print_header "Category 8: Unit System"

    run_test "Land units: feet" \
        "test_36_land_feet" \
        --num-layers 5 \
        --land-units feet \
        --height-per-layer 100

    run_test "Print units: inches" \
        "test_37_print_inches" \
        --num-layers 5 \
        --print-units inches \
        --substrate-size 8

    run_test "Mixed units with overrides" \
        "test_38_mixed_units" \
        --num-layers 5 \
        --land-units meters \
        --height-per-layer 200ft \
        --substrate-size 10in

    run_test "DMS coordinates" \
        "test_39_dms_coords" \
        --upper-left "63d07m29sN,151d11m05sW" \
        --lower-right "62d59m19sN,150d49m46sW" \
        --num-layers 5
}

test_quality_and_rendering() {
    print_header "Category 9: Quality & Rendering"

    run_test "Quality: draft" \
        "test_40_quality_draft" \
        --num-layers 5 \
        --quality draft

    run_test "Quality: medium" \
        "test_41_quality_medium" \
        --num-layers 5 \
        --quality medium

    run_test "Quality: high" \
        "test_42_quality_high" \
        --num-layers 5 \
        --quality high

    run_test "Color scheme: terrain" \
        "test_43_color_terrain" \
        --num-layers 5 \
        --color-scheme terrain \
        --output-formats svg

    run_test "Color scheme: rainbow" \
        "test_44_color_rainbow" \
        --num-layers 5 \
        --color-scheme rainbow \
        --output-formats svg
}

test_json_config() {
    print_header "Category 10: JSON Configuration Files"

    # Create test config with unit suffixes
    local config_file="$TEST_DIR/test_config_units.json"
    cat > "$config_file" << 'EOF'
{
  "upper_left_lat": "63°07'29\"N",
  "upper_left_lon": "151°11'05\"W",
  "lower_right_lat": 62.9887,
  "lower_right_lon": -150.8293,
  "base_name": "config_test",
  "num_layers": 6,
  "height_per_layer": "150ft",
  "substrate_size_mm": "10in",
  "layer_thickness_mm": "0.125in",
  "simplify_tolerance": "8m",
  "smoothing": 2,
  "output_formats": "svg,stl",
  "include_layer_numbers": true,
  "add_registration_marks": true
}
EOF

    print_test "JSON config with unit suffixes"
    if "$TOPO_GEN" --config "$config_file" --output-dir "$OUTPUT_DIR" \
        > "$LOG_DIR/test_45_json_config.log" 2> "$LOG_DIR/test_45_json_config.err"; then
        print_pass "JSON config with unit suffixes"
    else
        print_fail "JSON config with unit suffixes"
    fi
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    PASSED_TESTS=$((PASSED_TESTS + 1))
}

test_edge_cases() {
    print_header "Category 11: Edge Cases & Validation"

    run_test "Single layer" \
        "test_46_single_layer" \
        --num-layers 1

    run_test "Many layers" \
        "test_47_many_layers" \
        --num-layers 20

    run_test "Very small substrate" \
        "test_48_small_substrate" \
        --num-layers 5 \
        --substrate-size 50

    run_test "Very large substrate" \
        "test_49_large_substrate" \
        --num-layers 5 \
        --substrate-size 1000

    run_test "Minimal simplification" \
        "test_50_min_simplify" \
        --num-layers 3 \
        --simplify 0.1
}

# ==============================================================================
# MAIN TEST EXECUTION
# ==============================================================================

main() {
    local start_time=$(date +%s)

    print_header "Topographic Generator - Comprehensive Test Suite"
    echo "Testing executable: $TOPO_GEN"
    echo "Test data: Mount Denali (N63W152 SRTM tile)"
    echo "Start time: $(date)"
    echo ""

    # Setup
    setup_test_environment

    # Run all test categories
    test_basic_functionality
    test_layer_options
    test_substrate_options
    test_processing_options
    test_output_formats
    test_geographic_features
    test_registration_and_labels
    test_unit_system
    test_quality_and_rendering
    test_json_config
    test_edge_cases

    # Summary
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    echo ""
    print_header "Test Suite Complete"
    echo "Total tests: $TOTAL_TESTS"
    echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"
    echo -e "${RED}Failed: $FAILED_TESTS${NC}"
    echo "Duration: ${duration}s"
    echo ""

    if [[ $FAILED_TESTS -eq 0 ]]; then
        echo -e "${GREEN}✓ ALL TESTS PASSED!${NC}"
        exit 0
    else
        echo -e "${RED}✗ SOME TESTS FAILED${NC}"
        echo "Check logs in: $LOG_DIR"
        exit 1
    fi
}

# Run main function
main "$@"
