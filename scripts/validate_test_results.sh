#!/bin/bash
# ==============================================================================
# Test Results Validation Script
# ==============================================================================
# Validates output files from test_all_options.sh
# Checks file existence, sizes, formats, and content validity
# ==============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_DIR="$PROJECT_ROOT/test/option_tests"
LOG_DIR="$TEST_DIR/logs"
OUTPUT_DIR="$TEST_DIR/output"

# Validation counters
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
WARNINGS=0

# ==============================================================================
# UTILITY FUNCTIONS
# ==============================================================================

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

check_pass() {
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    echo -e "${GREEN}✓${NC} $1"
}

check_fail() {
    FAILED_CHECKS=$((FAILED_CHECKS + 1))
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    echo -e "${RED}✗${NC} $1"
}

check_warn() {
    WARNINGS=$((WARNINGS + 1))
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
    echo -e "${YELLOW}⚠${NC} $1"
}

# ==============================================================================
# VALIDATION FUNCTIONS
# ==============================================================================

validate_log_file() {
    local log_file="$1"
    local test_name=$(basename "$log_file" .log)

    if [[ ! -f "$log_file" ]]; then
        check_fail "Log file missing: $test_name"
        return 1
    fi

    # Check for errors in log
    if grep -q "Error:" "$log_file" 2>/dev/null; then
        check_fail "$test_name: Errors found in log"
        return 1
    fi

    # Check for fatal errors
    if grep -q "Fatal:" "$log_file" 2>/dev/null || grep -q "Segmentation fault" "$log_file" 2>/dev/null; then
        check_fail "$test_name: Fatal errors in log"
        return 1
    fi

    # Check if log has reasonable size (not empty, not too large)
    local size=$(stat -f%z "$log_file" 2>/dev/null || stat -c%s "$log_file" 2>/dev/null || echo 0)
    if [[ $size -lt 10 ]]; then
        check_warn "$test_name: Log file suspiciously small ($size bytes)"
    elif [[ $size -gt 10000000 ]]; then
        check_warn "$test_name: Log file very large ($size bytes)"
    else
        check_pass "$test_name: Log valid"
    fi

    return 0
}

validate_svg_file() {
    local svg_file="$1"
    local test_name=$(basename "$svg_file" .svg)

    if [[ ! -f "$svg_file" ]]; then
        check_fail "SVG file missing: $test_name"
        return 1
    fi

    # Check file is not empty
    local size=$(stat -f%z "$svg_file" 2>/dev/null || stat -c%s "$svg_file" 2>/dev/null || echo 0)
    if [[ $size -lt 100 ]]; then
        check_fail "$test_name: SVG file too small ($size bytes)"
        return 1
    fi

    # Check for SVG header
    if ! grep -q "<svg" "$svg_file"; then
        check_fail "$test_name: Invalid SVG (no svg tag)"
        return 1
    fi

    # Check for valid XML closing
    if ! grep -q "</svg>" "$svg_file"; then
        check_warn "$test_name: SVG may be incomplete (no closing tag)"
    fi

    check_pass "$test_name: SVG valid ($size bytes)"
    return 0
}

validate_stl_file() {
    local stl_file="$1"
    local test_name=$(basename "$stl_file" .stl)

    if [[ ! -f "$stl_file" ]]; then
        check_fail "STL file missing: $test_name"
        return 1
    fi

    # Check file is not empty
    local size=$(stat -f%z "$stl_file" 2>/dev/null || stat -c%s "$stl_file" 2>/dev/null || echo 0)
    if [[ $size -lt 84 ]]; then  # Minimum binary STL size
        check_fail "$test_name: STL file too small ($size bytes)"
        return 1
    fi

    # Check if binary STL (starts with 80-byte header)
    local header=$(head -c 5 "$stl_file" 2>/dev/null)
    if [[ "$header" == "solid" ]]; then
        # ASCII STL
        if ! grep -q "endsolid" "$stl_file"; then
            check_warn "$test_name: ASCII STL may be incomplete"
        fi
        check_pass "$test_name: ASCII STL valid ($size bytes)"
    else
        # Binary STL - check 80-byte header + triangle count
        if [[ $size -lt 84 ]]; then
            check_fail "$test_name: Binary STL header incomplete"
            return 1
        fi
        check_pass "$test_name: Binary STL valid ($size bytes)"
    fi

    return 0
}

validate_obj_file() {
    local obj_file="$1"
    local test_name=$(basename "$obj_file" .obj)

    if [[ ! -f "$obj_file" ]]; then
        check_fail "OBJ file missing: $test_name"
        return 1
    fi

    # Check file is not empty
    local size=$(stat -f%z "$obj_file" 2>/dev/null || stat -c%s "$obj_file" 2>/dev/null || echo 0)
    if [[ $size -lt 10 ]]; then
        check_fail "$test_name: OBJ file too small ($size bytes)"
        return 1
    fi

    # Check for vertices
    if ! grep -q "^v " "$obj_file"; then
        check_fail "$test_name: OBJ has no vertices"
        return 1
    fi

    # Check for faces
    if ! grep -q "^f " "$obj_file"; then
        check_warn "$test_name: OBJ has no faces"
    fi

    check_pass "$test_name: OBJ valid ($size bytes)"
    return 0
}

# ==============================================================================
# MAIN VALIDATION
# ==============================================================================

validate_all_logs() {
    print_header "Validating Log Files"

    local log_count=0
    for log_file in "$LOG_DIR"/*.log; do
        if [[ -f "$log_file" ]]; then
            validate_log_file "$log_file"
            log_count=$((log_count + 1))
        fi
    done

    echo "Validated $log_count log files"
    echo ""
}

validate_all_outputs() {
    print_header "Validating Output Files"

    # SVG files
    local svg_count=0
    for svg_file in "$OUTPUT_DIR"/*.svg; do
        if [[ -f "$svg_file" ]]; then
            validate_svg_file "$svg_file"
            svg_count=$((svg_count + 1))
        fi
    done
    if [[ $svg_count -gt 0 ]]; then
        echo "Validated $svg_count SVG files"
    fi

    # STL files
    local stl_count=0
    for stl_file in "$OUTPUT_DIR"/*.stl; do
        if [[ -f "$stl_file" ]]; then
            validate_stl_file "$stl_file"
            stl_count=$((stl_count + 1))
        fi
    done
    if [[ $stl_count -gt 0 ]]; then
        echo "Validated $stl_count STL files"
    fi

    # OBJ files
    local obj_count=0
    for obj_file in "$OUTPUT_DIR"/*.obj; do
        if [[ -f "$obj_file" ]]; then
            validate_obj_file "$obj_file"
            obj_count=$((obj_count + 1))
        fi
    done
    if [[ $obj_count -gt 0 ]]; then
        echo "Validated $obj_count OBJ files"
    fi

    echo ""
}

check_expected_outputs() {
    print_header "Checking Expected Outputs"

    # Test cases that should produce specific outputs
    local expected_tests=(
        "test_01_default"
        "test_22_format_svg"
        "test_23_format_stl"
        "test_36_land_feet"
    )

    for test in "${expected_tests[@]}"; do
        local has_output=false

        # Check for any output file with this base name
        if ls "$OUTPUT_DIR"/${test}* 1> /dev/null 2>&1; then
            has_output=true
        fi

        if $has_output; then
            check_pass "Expected output found: $test"
        else
            check_fail "Missing expected output: $test"
        fi
    done

    echo ""
}

generate_summary_report() {
    print_header "Validation Summary"

    # Count files
    local total_logs=$(find "$LOG_DIR" -name "*.log" 2>/dev/null | wc -l | tr -d ' ')
    local total_svgs=$(find "$OUTPUT_DIR" -name "*.svg" 2>/dev/null | wc -l | tr -d ' ')
    local total_stls=$(find "$OUTPUT_DIR" -name "*.stl" 2>/dev/null | wc -l | tr -d ' ')
    local total_objs=$(find "$OUTPUT_DIR" -name "*.obj" 2>/dev/null | wc -l | tr -d ' ')

    echo "Files Generated:"
    echo "  Log files: $total_logs"
    echo "  SVG files: $total_svgs"
    echo "  STL files: $total_stls"
    echo "  OBJ files: $total_objs"
    echo ""

    echo "Validation Results:"
    echo -e "  Total checks: $TOTAL_CHECKS"
    echo -e "  ${GREEN}Passed: $PASSED_CHECKS${NC}"
    echo -e "  ${RED}Failed: $FAILED_CHECKS${NC}"
    echo -e "  ${YELLOW}Warnings: $WARNINGS${NC}"
    echo ""

    local pass_rate=0
    if [[ $TOTAL_CHECKS -gt 0 ]]; then
        pass_rate=$((PASSED_CHECKS * 100 / TOTAL_CHECKS))
    fi

    echo "Pass rate: ${pass_rate}%"
    echo ""

    # Disk usage
    local log_size=$(du -sh "$LOG_DIR" 2>/dev/null | cut -f1)
    local output_size=$(du -sh "$OUTPUT_DIR" 2>/dev/null | cut -f1)
    echo "Disk Usage:"
    echo "  Logs: $log_size"
    echo "  Outputs: $output_size"
    echo ""
}

main() {
    print_header "Test Results Validation"
    echo "Test directory: $TEST_DIR"
    echo "Validation time: $(date)"
    echo ""

    # Check if test directory exists
    if [[ ! -d "$TEST_DIR" ]]; then
        echo -e "${RED}Error: Test directory not found: $TEST_DIR${NC}"
        echo "Run test_all_options.sh first"
        exit 1
    fi

    # Run validations
    validate_all_logs
    validate_all_outputs
    check_expected_outputs
    generate_summary_report

    # Exit code
    if [[ $FAILED_CHECKS -eq 0 ]]; then
        echo -e "${GREEN}✓ ALL VALIDATIONS PASSED!${NC}"
        exit 0
    else
        echo -e "${RED}✗ SOME VALIDATIONS FAILED${NC}"
        exit 1
    fi
}

main "$@"
