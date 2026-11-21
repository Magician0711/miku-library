# Miku Library - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only
"""Interface translations: i18n/<language>.json maps each Simplified Chinese source text
to its translation.

  python tools/i18n.py check     every text translated, placeholders intact (CI runs this)
  python tools/i18n.py update    add new texts with an empty translation, drop unused ones

Source texts are the literals in qsTr("...") in qml/ and in
QCoreApplication::translate("Miku", "...") in src/.
"""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LANGUAGES = ["zh-TW", "en", "ja"]
STRING = r'"(?:[^"\\\n]|\\.)*"'
QML_CALL = re.compile(r'qsTr\(\s*(' + STRING + r')\s*[,)]')
CPP_CALL = re.compile(r'QCoreApplication::translate\(\s*"Miku"\s*,\s*((?:' + STRING + r'\s*)+)[,)]')
PLACEHOLDER = re.compile(r'%\d')
ESCAPES = {'n': '\n', 't': '\t', '"': '"', '\\': '\\', "'": "'"}


def unescape(literal):
    return re.sub(r'\\(.)', lambda m: ESCAPES.get(m.group(1), m.group(1)), literal[1:-1])


def sources():
    """Every source text, in the order it first appears."""
    found = {}
    for path in sorted((ROOT / 'qml').glob('*.qml')):
        for m in QML_CALL.finditer(path.read_text(encoding='utf-8')):
            found.setdefault(unescape(m.group(1)), path.name)
    for path in sorted((ROOT / 'src').rglob('*.[ch]*')):
        for m in CPP_CALL.finditer(path.read_text(encoding='utf-8')):
            text = ''.join(unescape(part) for part in re.findall(STRING, m.group(1)))
            found.setdefault(text, path.name)
    return found


def load(language):
    path = ROOT / 'i18n' / f'{language}.json'
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}


def save(language, table):
    path = ROOT / 'i18n' / f'{language}.json'
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps(table, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')


def check():
    texts = sources()
    problems = 0
    for language in LANGUAGES:
        table = load(language)
        missing = [t for t in texts if not table.get(t)]
        unused = [t for t in table if t not in texts]
        broken = [t for t in texts if table.get(t)
                  and sorted(PLACEHOLDER.findall(t)) != sorted(PLACEHOLDER.findall(table[t]))]
        for kind, items in (('missing', missing), ('unused', unused), ('placeholders differ', broken)):
            for text in items:
                print(f'{language}: {kind}: {text!r}')
        problems += len(missing) + len(unused) + len(broken)
        print(f'{language}: {len(texts) - len(missing)}/{len(texts)} translated')
    return problems == 0


def update():
    texts = sources()
    for language in LANGUAGES:
        table = load(language)
        save(language, {t: table.get(t, '') for t in texts})
    print(f'{len(texts)} texts')


if __name__ == '__main__':
    command = sys.argv[1] if len(sys.argv) > 1 else 'check'
    if command == 'update':
        update()
    elif command == 'check':
        sys.exit(0 if check() else 1)
    else:
        sys.exit(__doc__)
