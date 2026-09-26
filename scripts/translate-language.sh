#!/bin/bash
# translate-language.sh - Use Claude Code to translate a specific language
# Usage: ./translate-language.sh <language_code>
# Example: ./translate-language.sh es

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

if [ -z "$1" ]; then
    echo "Usage: $0 <language_code>"
    echo ""
    echo "Available languages:"
    echo "  es    - Spanish (Español)"
    echo "  fr    - French (Français)"
    echo "  de    - German (Deutsch)"
    echo "  it    - Italian (Italiano)"
    echo "  pt    - Portuguese (Português)"
    echo "  ru    - Russian (Русский)"
    echo "  zh_CN - Chinese Simplified (简体中文)"
    echo "  zh_TW - Chinese Traditional (繁體中文)"
    echo "  ja    - Japanese (日本語)"
    echo "  ko    - Korean (한국어)"
    echo "  ar    - Arabic (العربية)"
    echo "  nl    - Dutch (Nederlands)"
    echo "  pl    - Polish (Polski)"
    echo "  tr    - Turkish (Türkçe)"
    exit 1
fi

LANG_CODE="$1"
TS_FILE="$PROJECT_DIR/translations/burner_$LANG_CODE.ts"

if [ ! -f "$TS_FILE" ]; then
    echo "Error: Translation file not found: $TS_FILE"
    echo "Run scripts/update-sources.sh first to generate translation files."
    exit 1
fi

# Special case: English just copies source to translation
if [ "$LANG_CODE" = "en" ]; then
    echo "English translations: copying source strings..."
    python3 "$SCRIPT_DIR/fill-english.py"
    echo "Done."
    exit 0
fi

# Map language codes to full names
declare -A LANG_NAMES=(
    ["es"]="Spanish"
    ["fr"]="French"
    ["de"]="German"
    ["it"]="Italian"
    ["pt"]="Portuguese"
    ["ru"]="Russian"
    ["zh_CN"]="Simplified Chinese"
    ["zh_TW"]="Traditional Chinese"
    ["ja"]="Japanese"
    ["ko"]="Korean"
    ["ar"]="Arabic"
    ["nl"]="Dutch"
    ["pl"]="Polish"
    ["tr"]="Turkish"
)

LANG_NAME="${LANG_NAMES[$LANG_CODE]:-$LANG_CODE}"

echo "Translating to $LANG_NAME ($LANG_CODE)..."
echo "Translation file: $TS_FILE"
echo ""

# Create the prompt for Claude Code
PROMPT="Translate the Qt translation file: translations/burner_$LANG_CODE.ts

This is a Qt Linguist .ts file for a CD/DVD/Blu-ray burning application. For each <message> with:
  <translation type=\"unfinished\"></translation>

Do the following:
1. Read the <source> text (English)
2. Translate to $LANG_NAME
3. Replace the empty <translation> with translated text
4. Remove type=\"unfinished\" attribute

Guidelines:
- Keep translations concise for UI labels
- Preserve format specifiers: %1, %2, %n (placeholders)
- Preserve HTML tags: <b>, <br/>, etc.
- Preserve keyboard shortcuts with & (e.g., &File -> &Archivo)
- Keep technical terms (CD, DVD, ISO, VCD, SVCD) as-is or use common localizations
- Maintain consistent terminology
- Only translate content inside <translation> tags

Read the file and update all unfinished translations to $LANG_NAME."

# Run Claude Code with the translation prompt
cd "$PROJECT_DIR"
echo "Running Claude Code to translate..."
echo ""

# Use --permission-mode to control tool access
# bypassPermissions: skip all permission prompts (for automation)
# Alternatively use: --allowedTools 'Edit(translations/*),Read(translations/*),Glob,Grep'
claude -p "$PROMPT" \
       --model sonnet \
       --permission-mode bypassPermissions \
       --max-turns 50

echo ""

# Validate XML after translation
echo "Validating XML..."
XML_ERROR=$(python3 -c "
import xml.etree.ElementTree as ET
try:
    ET.parse('$TS_FILE')
    print('')
except ET.ParseError as e:
    print(f'ERROR: {e}')
" 2>&1)

if [ -n "$XML_ERROR" ]; then
    echo "WARNING: Translation file has invalid XML!"
    echo "$XML_ERROR"
    echo ""
    echo "The translation file may need manual repair."
    echo "Check for mismatched <source>/<translation> tags."
    exit 1
fi

echo "Translation complete for $LANG_NAME."
echo "Run scripts/compile-translations.sh to compile the .qm files."
