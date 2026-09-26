#!/usr/bin/env python3
"""Fill English translations with source strings (since they're the same)."""

import sys
import re
import xml.etree.ElementTree as ET
from pathlib import Path


def fix_common_xml_errors(ts_path: Path) -> bool:
    """Attempt to fix common XML errors in .ts files.

    Returns True if fixes were applied.
    """
    content = ts_path.read_text()
    original = content

    # Fix <source>...</translation> -> <source>...</source>\n        <translation>...</translation>
    def fix_source_translation(match):
        text = match.group(1)
        return f'<source>{text}</source>\n        <translation>{text}</translation>'

    # Fix <translation>...</source> -> <translation>...</translation>
    def fix_translation_source(match):
        text = match.group(1)
        return f'<translation>{text}</translation>'

    content = re.sub(r'<source>([^<]*)</translation>', fix_source_translation, content)
    content = re.sub(r'<translation>([^<]*)</source>', fix_translation_source, content)

    if content != original:
        ts_path.write_text(content)
        return True
    return False


def fill_english_translations(ts_path: Path) -> int:
    """Fill empty translations with source strings for English."""
    # Try to fix common XML errors first
    try:
        tree = ET.parse(ts_path)
    except ET.ParseError as e:
        print(f"XML parse error: {e}")
        print("Attempting to fix common XML errors...")
        if fix_common_xml_errors(ts_path):
            print("Applied fixes, retrying parse...")
            try:
                tree = ET.parse(ts_path)
            except ET.ParseError as e2:
                print(f"Still invalid XML after fixes: {e2}")
                raise
        else:
            raise
    root = tree.getroot()

    count = 0
    for message in root.iter('message'):
        source = message.find('source')
        translation = message.find('translation')

        if source is not None and translation is not None:
            # Check if unfinished (has type="unfinished" and empty text)
            if translation.get('type') == 'unfinished' and not translation.text:
                translation.text = source.text
                del translation.attrib['type']
                count += 1

    # Write with XML declaration and proper encoding
    tree.write(ts_path, encoding='utf-8', xml_declaration=True)

    # ElementTree doesn't write DOCTYPE, so we need to add it back
    content = ts_path.read_text()
    if '<!DOCTYPE TS>' not in content:
        content = content.replace(
            "<?xml version='1.0' encoding='utf-8'?>",
            '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>'
        )
        ts_path.write_text(content)

    return count

if __name__ == '__main__':
    project_dir = Path(__file__).parent.parent
    en_ts = project_dir / 'translations' / 'burner_en.ts'

    if not en_ts.exists():
        print(f"Error: {en_ts} not found")
        sys.exit(1)

    count = fill_english_translations(en_ts)
    print(f"Filled {count} English translations")
