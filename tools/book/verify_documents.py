"""Verify only managed documentation, routes, snippets and the generated book."""
from pathlib import Path
from urllib.parse import unquote, parse_qs
import importlib.util
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
BOOK = ROOT / 'docs/interactive_book'
SPEC = importlib.util.spec_from_file_location('book_builder', BOOK / 'build_book.py')
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)
ROOT_DOCS = ['README.md', 'README_kr.md', 'CHANGELOG.md', 'CONTRIBUTING.md',
             'SECURITY.md', 'THIRD_PARTY_NOTICES.md', 'AGENTS.md']


def anchors(text):
    seen, result, fenced = {}, set(), False
    for line in text.splitlines():
        if re.match(r'^\s*(```|~~~)', line):
            fenced = not fenced
        if fenced or not re.match(r'^#{1,6}\s', line):
            continue
        slug = re.sub(r'[^\w\- ]', '', re.sub(r'^#+\s+', '', line).strip().lower()).replace(' ', '-')
        ordinal = seen.get(slug, 0)
        result.add(slug if not ordinal else f'{slug}-{ordinal}')
        seen[slug] = ordinal + 1
    return result


def main():
    expected = builder.build(complete=True)[0].encode('utf-8')
    if (BOOK / 'index.html').read_bytes() != expected:
        raise ValueError('Generated HTML is stale; run python docs/interactive_book/build_book.py')
    manifest = builder.read_json(BOOK / 'manifest.json')
    if any(c['status'] != 'complete' or not c['technicalReviewed'] or not c['editorialReviewed'] for c in manifest['chapters']):
        raise ValueError(f'All {builder.CHAPTER_COUNT} manuscripts must be reviewed')
    ids = {c['slug']: builder.Fragment((BOOK/c['file']).read_text(encoding='utf-8')).ids for c in manifest['chapters']}
    migration = builder.read_json(BOOK / 'migration.json')
    if any(not i['reviewed'] or not i['destination'] for d in migration['documents'] for i in d['items']):
        raise ValueError('Unreviewed migration item')
    files = [ROOT/p for p in ROOT_DOCS]
    for file in files:
        text = file.read_text(encoding='utf-8-sig')
        for match in re.finditer(r'\[[^\]]+\]\(([^)]+)\)', text):
            href = match[1]
            if href.startswith(('https://', 'http://', 'mailto:')):
                continue
            parts = unquote(href).split('#', 1)
            target = (file.parent/parts[0]).resolve() if parts[0] else file
            if not target.is_relative_to(ROOT) or not target.exists():
                raise ValueError(f'Broken local link in {file.name}: {href}')
            if len(parts) == 2 and parts[1]:
                if target == BOOK/'index.html':
                    builder.validate_links(['#'+parts[1]], manifest['chapters'], ids)
                elif target.suffix == '.md' and parts[1] not in anchors(target.read_text(encoding='utf-8-sig')):
                    raise ValueError(f'Broken Markdown anchor: {file.name}: {href}')
                elif target.suffix == '.html' and target != BOOK/'index.html':
                    raise ValueError(f'Use the book entry point: {href}')
    english = (ROOT/'README.md').read_text(encoding='utf-8')
    korean = (ROOT/'README_kr.md').read_text(encoding='utf-8')
    for text in (english, korean):
        if 'docs/interactive_book/index.html' not in text:
            raise ValueError('Both READMEs must link the offline book')
    quick = re.search(r'```cpp\r?\n(.*?)\r?\n```', english, re.S)
    if not quick or quick[1].replace('\r\n','\n').strip() != (ROOT/'samples/package_consumer/main.cpp').read_text(encoding='utf-8').strip():
        raise ValueError('README quick start differs from package consumer')
    for path in (ROOT/'tools').glob('*.ps1'):
        if path.name == 'check-docs.ps1':
            continue
        text = path.read_text(encoding='utf-8-sig').replace('\\','/')
        for record in migration['documents']:
            if record['status'] == 'removed' and record['source'] in text:
                raise ValueError(f'Stale tool dependency: {path.name}: {record["source"]}')
    print(f'Managed documentation verified: {len(files)} root files, {builder.CHAPTER_COUNT} chapters, 20 labs, '
          f'{sum(len(d["items"]) for d in migration["documents"])} reviewed migration items; links and consumer snippet match')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        print(f'Documentation verification failed: {error}', file=sys.stderr)
        sys.exit(1)
