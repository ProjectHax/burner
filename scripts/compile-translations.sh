#!/bin/bash
# compile-translations.sh - Compile .ts files to .qm binary files
# Run this after translations are complete

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Find lrelease
LRELEASE="${LRELEASE:-/opt/Qt6/bin/lrelease}"
if [ ! -x "$LRELEASE" ]; then
    LRELEASE="$(which lrelease 2>/dev/null || which lrelease6 2>/dev/null || echo "")"
fi

if [ -z "$LRELEASE" ] || [ ! -x "$LRELEASE" ]; then
    echo "Error: lrelease not found. Please install Qt6 linguist tools."
    echo "Set LRELEASE environment variable to specify the path."
    exit 1
fi

echo "Using lrelease: $LRELEASE"
echo "Compiling translation files..."
echo ""

LANGUAGES="en es fr de it pt ru zh_CN zh_TW ja ko ar nl pl tr"

for lang in $LANGUAGES; do
    TS_FILE="translations/burner_$lang.ts"
    QM_FILE="translations/burner_$lang.qm"

    if [ -f "$TS_FILE" ]; then
        echo "  Compiling $TS_FILE -> $QM_FILE"
        "$LRELEASE" "$TS_FILE" -qm "$QM_FILE" 2>/dev/null
    else
        echo "  Warning: $TS_FILE not found, skipping"
    fi
done

echo ""
echo "Done! Compiled translation files are in translations/*.qm"
echo ""
echo "Translation statistics:"
for lang in $LANGUAGES; do
    QM_FILE="translations/burner_$lang.qm"
    if [ -f "$QM_FILE" ]; then
        SIZE=$(stat -c%s "$QM_FILE" 2>/dev/null || stat -f%z "$QM_FILE" 2>/dev/null)
        echo "  $lang: $SIZE bytes"
    fi
done
