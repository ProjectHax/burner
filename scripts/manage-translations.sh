#!/bin/bash
# manage-translations.sh - Main translation management menu
# Interactive script for managing Burner translations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

show_menu() {
    clear
    echo "=========================================="
    echo "  Burner Translation Management"
    echo "=========================================="
    echo ""
    echo "  1) Update source strings (lupdate)"
    echo "  2) Translate specific language"
    echo "  3) Translate all languages"
    echo "  4) Compile translations (lrelease)"
    echo "  5) Validate translations"
    echo "  6) Full workflow (update, translate all, compile)"
    echo ""
    echo "  q) Quit"
    echo ""
    echo -n "Select an option: "
}

translate_menu() {
    echo ""
    echo "Available languages:"
    echo "  1) es    - Spanish"
    echo "  2) fr    - French"
    echo "  3) de    - German"
    echo "  4) it    - Italian"
    echo "  5) pt    - Portuguese"
    echo "  6) ru    - Russian"
    echo "  7) zh_CN - Chinese (Simplified)"
    echo "  8) zh_TW - Chinese (Traditional)"
    echo "  9) ja    - Japanese"
    echo " 10) ko    - Korean"
    echo " 11) ar    - Arabic"
    echo " 12) nl    - Dutch"
    echo " 13) pl    - Polish"
    echo " 14) tr    - Turkish"
    echo ""
    echo -n "Enter language number or code: "
    read lang_input

    case "$lang_input" in
        1|es) LANG="es" ;;
        2|fr) LANG="fr" ;;
        3|de) LANG="de" ;;
        4|it) LANG="it" ;;
        5|pt) LANG="pt" ;;
        6|ru) LANG="ru" ;;
        7|zh_CN) LANG="zh_CN" ;;
        8|zh_TW) LANG="zh_TW" ;;
        9|ja) LANG="ja" ;;
        10|ko) LANG="ko" ;;
        11|ar) LANG="ar" ;;
        12|nl) LANG="nl" ;;
        13|pl) LANG="pl" ;;
        14|tr) LANG="tr" ;;
        *) echo "Invalid selection"; return ;;
    esac

    "$SCRIPT_DIR/translate-language.sh" "$LANG"
}

while true; do
    show_menu
    read choice

    case "$choice" in
        1)
            echo ""
            "$SCRIPT_DIR/update-sources.sh"
            echo ""
            read -p "Press Enter to continue..."
            ;;
        2)
            translate_menu
            echo ""
            read -p "Press Enter to continue..."
            ;;
        3)
            echo ""
            "$SCRIPT_DIR/translate-all.sh"
            echo ""
            read -p "Press Enter to continue..."
            ;;
        4)
            echo ""
            "$SCRIPT_DIR/compile-translations.sh"
            echo ""
            read -p "Press Enter to continue..."
            ;;
        5)
            echo ""
            "$SCRIPT_DIR/validate-translations.sh" || true
            echo ""
            read -p "Press Enter to continue..."
            ;;
        6)
            echo ""
            echo "Running full translation workflow..."
            echo ""
            "$SCRIPT_DIR/update-sources.sh"
            echo ""
            "$SCRIPT_DIR/translate-all.sh"
            echo ""
            "$SCRIPT_DIR/compile-translations.sh"
            echo ""
            "$SCRIPT_DIR/validate-translations.sh" || true
            echo ""
            read -p "Press Enter to continue..."
            ;;
        q|Q)
            echo "Goodbye!"
            exit 0
            ;;
        *)
            echo "Invalid option"
            sleep 1
            ;;
    esac
done
