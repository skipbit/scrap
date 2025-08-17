#!/bin/bash

# The scrap Header Declaration Checker
# Ensures headers contain declarations only (no implementations)

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_ROOT/src"

# Patterns that indicate implementations in headers (violations)
VIOLATION_PATTERNS=(
    '= default;'
    '= delete;'
    '{\s*return\s'           # inline getters
    '{\s*\w+.*=.*;\s*}'      # simple assignments
    '{\s*if\s*('             # conditional logic
    '{\s*for\s*('            # loops
    '{\s*while\s*('          # while loops
    '{\s*switch\s*('         # switch statements
)

# Exceptions (these are allowed in headers)
EXCEPTION_PATTERNS=(
    'constexpr.*{'           # constexpr functions
    'template.*{'            # template functions
    'virtual.*= 0;'          # pure virtual functions
    'inline.*{'              # explicitly marked inline functions (legacy)
)

violations_found=0
total_files=0

echo -e "${GREEN}🔍 Checking headers for declaration-only compliance...${NC}"
echo "Scanning: $SRC_DIR"
echo

# Find all header files
while IFS= read -r -d '' header_file; do
    ((total_files++))
    relative_path="${header_file#$PROJECT_ROOT/}"

    # Check for violations
    file_violations=()

    # Read file line by line
    line_number=0
    while IFS= read -r line; do
        ((line_number++))

        # Skip empty lines and comments
        [[ "$line" =~ ^[[:space:]]*$ ]] && continue
        [[ "$line" =~ ^[[:space:]]*// ]] && continue
        [[ "$line" =~ ^[[:space:]]*\* ]] && continue

        # Check for exception patterns first
        is_exception=false
        for exception_pattern in "${EXCEPTION_PATTERNS[@]}"; do
            if [[ "$line" =~ $exception_pattern ]]; then
                is_exception=true
                break
            fi
        done

        # If it's an exception, skip violation checking
        [[ "$is_exception" == true ]] && continue

        # Check for violation patterns
        for violation_pattern in "${VIOLATION_PATTERNS[@]}"; do
            if [[ "$line" =~ $violation_pattern ]]; then
                file_violations+=("$line_number: $line")
                break
            fi
        done

    done < "$header_file"

    # Report violations for this file
    if [[ ${#file_violations[@]} -gt 0 ]]; then
        ((violations_found++))
        echo -e "${RED}❌ $relative_path${NC}"
        for violation in "${file_violations[@]}"; do
            echo -e "   ${YELLOW}Line $violation${NC}"
        done
        echo
    fi

done < <(find "$SRC_DIR" -name "*.h" -type f -print0)

# Summary
echo -e "${GREEN}📊 Summary:${NC}"
echo "Files checked: $total_files"
echo "Files with violations: $violations_found"

if [[ $violations_found -eq 0 ]]; then
    echo -e "${GREEN}✅ All headers follow declaration-only style!${NC}"
    exit 0
else
    echo -e "${RED}❌ Found violations in $violations_found file(s)${NC}"
    echo
    echo -e "${YELLOW}💡 Fix suggestions:${NC}"
    echo "1. Move implementations to corresponding .cpp files"
    echo "2. Use '= default' and '= delete' in .cpp files only"
    echo "3. Keep only declarations in .h files"
    echo "4. Exceptions: constexpr, templates, pure virtuals are allowed"
    echo
    echo "See docs/CODINGSTYLE.md for detailed guidelines."
    exit 1
fi
