#!/bin/bash
#
# delete-vcpkg-packages.sh - Delete all vcpkg NuGet packages from GitHub Packages
#
# This script removes all NuGet packages created by vcpkg binary caching.
# After deletion, the next CI build will recreate the binary cache from scratch.
#
# Usage: ./scripts/delete-vcpkg-packages.sh [--dry-run]
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

# Parse arguments
DRY_RUN=false
if [[ "$1" == "--dry-run" ]]; then
    DRY_RUN=true
    log_warning "DRY RUN MODE - No packages will be deleted"
fi

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    log_error "GitHub CLI (gh) is not installed"
    log_info "Install from: https://cli.github.com/"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    log_error "Not authenticated with GitHub CLI"
    log_info "Run: gh auth login"
    exit 1
fi

# Get current user
OWNER=$(gh api user --jq '.login')
log_info "GitHub user: $OWNER"

# Package type (vcpkg uses NuGet)
PACKAGE_TYPE="nuget"

log_info "Fetching list of ${PACKAGE_TYPE} packages..."
echo ""

# Create temporary file for package list
TEMP_FILE=$(mktemp)
trap "rm -f $TEMP_FILE" EXIT

# Fetch all packages (handle pagination)
PAGE=1
TOTAL_COUNT=0

while true; do
    RESPONSE=$(gh api "users/${OWNER}/packages?package_type=${PACKAGE_TYPE}&per_page=100&page=${PAGE}" 2>&1)

    # Check if response is empty array (no more pages)
    if [[ "$RESPONSE" == "[]" ]]; then
        break
    fi

    # Extract package names and append to file
    echo "$RESPONSE" | jq -r '.[].name' >> "$TEMP_FILE" 2>/dev/null || {
        log_error "Failed to parse API response"
        log_info "Response: $RESPONSE"
        exit 1
    }

    COUNT=$(echo "$RESPONSE" | jq '. | length' 2>/dev/null || echo 0)
    if [[ $COUNT -eq 0 ]]; then
        break
    fi

    TOTAL_COUNT=$((TOTAL_COUNT + COUNT))
    log_info "  Page $PAGE: Found $COUNT packages (total: $TOTAL_COUNT)"

    PAGE=$((PAGE + 1))
done

if [[ ! -s "$TEMP_FILE" ]]; then
    log_warning "No ${PACKAGE_TYPE} packages found"
    exit 0
fi

PACKAGE_COUNT=$(wc -l < "$TEMP_FILE" | tr -d ' ')

echo ""
log_success "Found $PACKAGE_COUNT packages to delete"
echo ""

# Show first 10 packages as preview
log_info "Preview of packages (first 10):"
head -n 10 "$TEMP_FILE" | while read -r pkg; do
    echo "  - $pkg"
done

if [[ $PACKAGE_COUNT -gt 10 ]]; then
    echo "  ... and $((PACKAGE_COUNT - 10)) more"
fi

echo ""

# Confirm deletion
if [[ "$DRY_RUN" == false ]]; then
    log_warning "⚠️  WARNING: This will PERMANENTLY DELETE all $PACKAGE_COUNT packages!"
    log_warning "⚠️  vcpkg binary cache will be rebuilt on next CI run (~10-15 min extra)"
    echo ""
    echo "Type 'DELETE' to confirm, or Ctrl+C to cancel: "
    read -r CONFIRM

    if [[ "$CONFIRM" != "DELETE" ]]; then
        log_info "Deletion cancelled"
        exit 0
    fi
    echo ""
fi

# Delete each package
DELETED=0
FAILED=0

log_info "Deleting packages..."
echo ""

while IFS= read -r package; do
    # URL-encode package name (replace special characters)
    ENCODED_PKG=$(printf %s "$package" | jq -sRr @uri)

    if [[ "$DRY_RUN" == true ]]; then
        log_info "[DRY RUN] Would delete: $package"
        DELETED=$((DELETED + 1))
    else
        echo -n "  Deleting: $package ... "

        if gh api \
            --method DELETE \
            -H "Accept: application/vnd.github+json" \
            "users/${OWNER}/packages/${PACKAGE_TYPE}/${ENCODED_PKG}" \
            > /dev/null 2>&1; then
            echo -e "${GREEN}✓${NC}"
            DELETED=$((DELETED + 1))
        else
            echo -e "${RED}✗${NC}"
            FAILED=$((FAILED + 1))
        fi
    fi
done < "$TEMP_FILE"

echo ""

if [[ "$DRY_RUN" == true ]]; then
    log_success "DRY RUN complete: Would delete $DELETED packages"
else
    log_success "Deleted $DELETED packages successfully"

    if [[ $FAILED -gt 0 ]]; then
        log_warning "Failed to delete $FAILED packages (may already be deleted)"
    fi

    echo ""
    log_info "Next steps:"
    echo "  1. Next CI build will be slower (~10-15 min extra) as vcpkg rebuilds cache"
    echo "  2. Subsequent builds will be fast again with new binary cache"
    echo "  3. Consider switching to GitHub Actions cache to avoid future package buildup"
fi
