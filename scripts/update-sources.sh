#!/bin/bash
# update-sources.sh - Extract translatable strings from source code
# Run this after adding/modifying translatable strings

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Find lupdate
LUPDATE="${LUPDATE:-/opt/Qt6/bin/lupdate}"
if [ ! -x "$LUPDATE" ]; then
    LUPDATE="$(which lupdate 2>/dev/null || which lupdate6 2>/dev/null || echo "")"
fi

if [ -z "$LUPDATE" ] || [ ! -x "$LUPDATE" ]; then
    echo "Error: lupdate not found. Please install Qt6 linguist tools."
    echo "Set LUPDATE environment variable to specify the path."
    exit 1
fi

echo "Using lupdate: $LUPDATE"
echo "Extracting translatable strings from source code..."

LANGUAGES="en es fr de it pt ru zh_CN zh_TW ja ko ar nl pl tr"

for lang in $LANGUAGES; do
    echo "  Updating burner_$lang.ts..."
    "$LUPDATE" -no-obsolete -source-language en -target-language "$lang" src -ts "translations/burner_$lang.ts" 2>/dev/null
done

echo ""
echo "Done! Translation files updated."
echo "Run scripts/translate-language.sh <lang> to translate a specific language."
echo "Run scripts/translate-all.sh to translate all languages."
