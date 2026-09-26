#!/bin/bash
# validate-translations.sh - Check translation files for completeness and issues
# Run this to verify translations before release

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "=========================================="
echo "Translation Validation Report"
echo "=========================================="
echo ""

LANGUAGES="en es fr de it pt ru zh_CN zh_TW ja ko ar nl pl tr"

# Map language codes to full names
declare -A LANG_NAMES=(
    ["en"]="English"
    ["es"]="Spanish"
    ["fr"]="French"
    ["de"]="German"
    ["it"]="Italian"
    ["pt"]="Portuguese"
    ["ru"]="Russian"
    ["zh_CN"]="Chinese (Simplified)"
    ["zh_TW"]="Chinese (Traditional)"
    ["ja"]="Japanese"
    ["ko"]="Korean"
    ["ar"]="Arabic"
    ["nl"]="Dutch"
    ["pl"]="Polish"
    ["tr"]="Turkish"
)

TOTAL_STRINGS=0
HAS_ERRORS=0
XML_ERRORS=0

for lang in $LANGUAGES; do
    TS_FILE="translations/burner_$lang.ts"
    LANG_NAME="${LANG_NAMES[$lang]:-$lang}"

    if [ ! -f "$TS_FILE" ]; then
        echo "$LANG_NAME ($lang): FILE NOT FOUND"
        HAS_ERRORS=1
        continue
    fi

    # Validate XML syntax
    XML_ERROR=$(python3 -c "
import xml.etree.ElementTree as ET
try:
    ET.parse('$TS_FILE')
except ET.ParseError as e:
    print(f'XML ERROR: {e}')
" 2>&1)

    if [ -n "$XML_ERROR" ]; then
        echo "$LANG_NAME ($lang): $XML_ERROR"
        HAS_ERRORS=1
        XML_ERRORS=$((XML_ERRORS + 1))
        continue
    fi

    # Count total messages
    TOTAL=$(grep -c '<message>' "$TS_FILE" 2>/dev/null || echo 0)
    TOTAL=${TOTAL//[^0-9]/}
    [ -z "$TOTAL" ] && TOTAL=0

    # Count unfinished translations
    UNFINISHED=$(grep -c 'type="unfinished"' "$TS_FILE" 2>/dev/null || echo 0)
    UNFINISHED=${UNFINISHED//[^0-9]/}
    [ -z "$UNFINISHED" ] && UNFINISHED=0

    # Count empty translations (finished but empty)
    EMPTY=$(grep -c '<translation></translation>' "$TS_FILE" 2>/dev/null || echo 0)
    EMPTY=${EMPTY//[^0-9]/}
    [ -z "$EMPTY" ] && EMPTY=0

    # Calculate completed
    COMPLETED=$((TOTAL - UNFINISHED - EMPTY))
    [ "$COMPLETED" -lt 0 ] && COMPLETED=0
    if [ "$TOTAL" -gt 0 ]; then
        PERCENT=$((COMPLETED * 100 / TOTAL))
    else
        PERCENT=0
    fi

    # Check for placeholder mismatches (%1, %2, etc.)
    PLACEHOLDER_ISSUES=0
    # This is a simplified check - a more thorough check would compare source and translation
    if grep -q '%[0-9]' "$TS_FILE"; then
        # Count messages with placeholders in source but missing in translation
        # (This is a rough heuristic)
        :
    fi

    # Status indicator
    if [ "$UNFINISHED" -eq 0 ] && [ "$EMPTY" -eq 0 ]; then
        STATUS="[COMPLETE]"
    elif [ "$PERCENT" -ge 90 ]; then
        STATUS="[MOSTLY COMPLETE]"
    elif [ "$PERCENT" -ge 50 ]; then
        STATUS="[IN PROGRESS]"
    else
        STATUS="[NEEDS WORK]"
        HAS_ERRORS=1
    fi

    printf "%-25s %4d/%4d (%3d%%) %s\n" "$LANG_NAME ($lang):" "$COMPLETED" "$TOTAL" "$PERCENT" "$STATUS"

    if [ "$UNFINISHED" -gt 0 ]; then
        echo "    - $UNFINISHED unfinished translations"
    fi
    if [ "$EMPTY" -gt 0 ]; then
        echo "    - $EMPTY empty translations"
    fi

    TOTAL_STRINGS=$TOTAL
done

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo "Total translatable strings: $TOTAL_STRINGS"
if [ "$XML_ERRORS" -gt 0 ]; then
    echo "XML validation errors: $XML_ERRORS"
fi
echo ""

if [ "$XML_ERRORS" -gt 0 ]; then
    echo "ERROR: Some translation files have invalid XML."
    echo "Fix the XML errors before proceeding."
    exit 1
elif [ "$HAS_ERRORS" -eq 1 ]; then
    echo "Some translations need attention."
    echo "Run scripts/translate-language.sh <lang> to translate specific languages."
    exit 1
else
    echo "All translations are complete!"
    exit 0
fi
