#!/bin/bash
# translate-all.sh - Use Claude Code to translate all languages
# This runs translate-language.sh for each supported language

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# First fill English (just copies source to translation)
echo "Filling English translations..."
python3 "$SCRIPT_DIR/fill-english.py"
echo ""

# Languages to translate with Claude (excluding English which is the source)
LANGUAGES="es fr de it pt ru zh_CN zh_TW ja ko ar nl pl tr"

echo "=========================================="
echo "Burner Translation - All Languages"
echo "=========================================="
echo ""
echo "This script will use Claude Code to translate the application"
echo "into all supported languages. This may take a while."
echo ""
echo "Languages: $LANGUAGES"
echo ""
read -p "Press Enter to continue or Ctrl+C to cancel..."
echo ""

for lang in $LANGUAGES; do
    echo ""
    echo "=========================================="
    echo "Translating: $lang"
    echo "=========================================="
    echo ""

    "$SCRIPT_DIR/translate-language.sh" "$lang"

    echo ""
    echo "Completed: $lang"
    echo ""

    # Small delay between languages
    sleep 2
done

echo ""
echo "=========================================="
echo "All translations complete!"
echo "=========================================="
echo ""
echo "Run scripts/compile-translations.sh to compile the .qm files."
