"""Assemble the offline book. --check is read-only; --require-complete gates release."""
from argparse import ArgumentParser
from hashlib import sha256
from html.parser import HTMLParser
from html import escape
from pathlib import Path
import json
import re
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
# C01 opens part 0 with the P chapters; existing C numbers stay stable for links.
EXPECTED_CHAPTERS = (["C01"] + [f"P{i:02}" for i in range(1, 4)] + [f"C{i:02}" for i in range(2, 29)] +
                     [f"A{i:02}" for i in range(1, 6)])
CHAPTER_COUNT = len(EXPECTED_CHAPTERS)
FIGURE_KINDS = ('sequence', 'state', 'ring', 'slots', 'queue', 'bytes', 'graph', 'layers')


def read_json(path):
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or value.get("schemaVersion") != 1:
        raise ValueError(f"{path.name}: unsupported schemaVersion")
    return value


def contained(base, relative):
    path = (base / relative).resolve()
    if Path(relative).is_absolute() or not path.is_relative_to(base.resolve()):
        raise ValueError(f"Path outside allowlist root: {relative}")
    return path


def unique(items, label):
    if len(set(items)) != len(items):
        raise ValueError(f"Duplicate {label}")


class Fragment(HTMLParser):
    def __init__(self, html):
        super().__init__()
        self.ids, self.text, self.sources, self.links, self.evidence, self.labs = [], [], [], [], [], []
        self.prose = []
        self.components = {kind: [] for kind in ('figure', 'walkthrough', 'compare', 'quiz', 'term', 'map')}
        self.map_focus = []
        self.in_prose = 0
        self.feed(html)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'data-map' in attrs:
            self.map_focus.extend(filter(None, attrs.get('data-focus', '').split(',')))
        for kind in self.components:
            if 'data-' + kind in attrs:
                self.components[kind].append(attrs['data-' + kind])
        if tag in ("p", "li", "td", "pre"):
            self.in_prose += 1
        if "id" in attrs:
            self.ids.append(attrs["id"])
        if "data-file" in attrs:
            self.sources.append((attrs["data-file"], attrs.get("data-find", "")))
        if tag == "a" and "href" in attrs:
            self.links.append(attrs["href"])
        if "data-evidence" in attrs:
            self.evidence.append(attrs["data-evidence"])
        if "data-lab" in attrs:
            self.labs.append(attrs["data-lab"])
        if tag in ("script", "iframe", "object") or any(k.startswith("on") for k in attrs):
            raise ValueError("Executable content in manuscript")

    def handle_data(self, data):
        self.text.append(data)
        if self.in_prose:
            self.prose.append(data)

    def handle_endtag(self, tag):
        if tag in ("p", "li", "td", "pre"):
            self.in_prose = max(0, self.in_prose - 1)


class ArchiveAudit(HTMLParser):
    def __init__(self, text, sections):
        super().__init__()
        self.stack = []
        self.archive_depth = None
        self.marked = set()
        self.count = 0
        self.feed(text)
        if self.marked != {s['id'] for s in sections if s.get('archive')}:
            raise ValueError('Archive manifest and data-archive sections differ')

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if self.archive_depth is not None and len(self.stack) == self.archive_depth + 1:
            if tag not in ('h2', 'p', 'details') or (tag == 'details' and attrs.get('class') != 'archive'):
                raise ValueError('Archive section only permits h2, p and details.archive children')
        if 'data-archive' in attrs:
            if tag != 'section' or self.archive_depth is not None:
                raise ValueError('Invalid archive section')
            self.archive_depth = len(self.stack)
            self.marked.add(attrs.get('id'))
        if self.archive_depth is not None and tag == 'details' and attrs.get('class') == 'archive':
            self.count += 1
        if tag not in ('area', 'base', 'br', 'col', 'embed', 'hr', 'img', 'input', 'link', 'meta', 'param', 'source', 'track', 'wbr'):
            self.stack.append(tag)

    def handle_endtag(self, tag):
        if self.stack and self.stack[-1] == tag:
            self.stack.pop()
            if self.archive_depth == len(self.stack):
                self.archive_depth = None


def check_test(item):
    test_raw = contained(ROOT, item["testSource"]).read_bytes()
    if sha256(test_raw).hexdigest() != item["reviewedTestSha256"]:
        raise ValueError(f"{item['id']}: test source changed; review required")
    suite, name = item["testName"].split(".", 1)
    pattern = rf"\bTEST(?:_F|_P)?\s*\(\s*{re.escape(suite)}\s*,\s*{re.escape(name)}\s*\)"
    if len(re.findall(pattern, test_raw.decode("utf-8-sig"))) != 1:
        raise ValueError(f"{item['id']}: test declaration missing or ambiguous")


def collect_evidence(evidence, sources, chapters):
    unique([e["id"] for e in evidence["items"]], "evidence id")
    result = []
    for item in evidence["items"]:
        source = sources.get(item["source"])
        if source is None:
            raise ValueError(f"{item['id']}: source not allowlisted")
        if source["sha256"] != item["reviewedSourceSha256"]:
            raise ValueError(f"{item['id']}: source changed; review required (hash is never refreshed automatically)")
        text = source["text"]
        start, end = item["selector"]["start"], item["selector"]["end"]
        if not start or not end or text.count(start) != 1 or text.count(end) != 1:
            raise ValueError(f"{item['id']}: selector missing or ambiguous")
        offset, limit = text.index(start), text.index(end)
        if limit <= offset:
            raise ValueError(f"{item['id']}: inverted selector")
        if item["selector"].get("includeEnd", False):
            limit += len(end)
        chapter = chapters.get(item["chapter"])
        if chapter is None or item["section"] not in [s["id"] for s in chapter["sections"]]:
            raise ValueError(f"{item['id']}: missing chapter/section")
        if item.get("kind") not in ("implementation", "contract", "historical"):
            raise ValueError(f"{item['id']}: invalid evidence kind")
        check_test(item)
        result.append({**item, "startLine": text.count("\n", 0, offset) + 1,
                       "endLine": text.count("\n", 0, limit), "commit": source["commit"]})
    return result


def check_migration(migration, complete, fragments=None):
    records = migration["documents"]
    originals = [r for r in records if r["revision"] == migration["originalCommit"]]
    if len(originals) != 24:
        raise ValueError("Migration must retain all 24 original documents")
    unique([(r["source"], r["revision"]) for r in records], "migration revision")
    for record in records:
        if not record["items"]:
            raise ValueError(f"Empty migration inventory: {record['source']}")
        if record["revision"] == "worktree" and record["status"] != "removed":
            path = contained(ROOT, record["source"])
            if sha256(path.read_bytes()).hexdigest() != record["sourceSha256"]:
                raise ValueError(f"Migration original changed: {record['source']}; reconcile content before continuing")
        if record["revision"] == "worktree" and record["status"] == "removed" and contained(ROOT, record["source"]).exists():
            raise ValueError(f"Removed documentation reappeared: {record['source']}")
        if complete and (record["status"] not in ("removed", "retained_fixture") or
                         any(not i["reviewed"] or not i["destination"] for i in record["items"])):
            raise ValueError(f"Migration not complete: {record['source']}")
        for item in record["items"]:
            if not item.get("destination"):
                continue
            slug, anchor = item["destination"].split("#", 1)
            if fragments is not None:
                text = fragments.get(slug, "")
                match = re.search(r'<details class="archive" id="' + re.escape(anchor) + r'">.*?</div></details>', text, re.S)
                if match is None or sha256(match[0].encode()).hexdigest() != item["preservation"]["fragmentSha256"]:
                    raise ValueError(f"Migration preserved fragment changed: {record['source']}:{item['id']}")
                proof = item["preservation"]
                if sha256(item["content"].encode()).hexdigest() != proof["originalContentSha256"]:
                    raise ValueError("Migration inventory text changed")
                if proof["type"] == "source-and-notes":
                    raw = contained(ROOT, proof["source"]).read_bytes()
                    if proof.get("comparison") == "lf-normalized":
                        matches = sha256(raw.replace(b"\r\n", b"\n")).hexdigest() == proof["originalNormalizedSourceSha256"]
                    else:
                        matches = sha256(raw).hexdigest() == proof["originalSourceSha256"]
                    if not matches:
                        raise ValueError(f"Historical source equivalence changed: {proof['source']}")


def table(headers, rows):
    return '<div class="table-wrap"><table><thead><tr>' + ''.join('<th>' + escape(h) + '</th>' for h in headers) + '</tr></thead><tbody>' + ''.join('<tr>' + ''.join('<td>' + c + '</td>' for c in row) + '</tr>' for row in rows) + '</tbody></table></div>'


def collect_components(definitions, fragments, evidence, glossary, chapters, sources):
    """Resolve declarative references before any browser executes the book."""
    figures, walks, quizzes, system = definitions
    groups = {'figure': figures['figures'], 'walkthrough': walks['walkthroughs'],
              'compare': walks['compares'], 'quiz': quizzes['quizzes'],
              'term': glossary['items'], 'map': [system] if system.get('layers') else []}
    refs = {kind: [] for kind in groups}
    uses = {}
    for index, text in fragments:
        parsed = Fragment(text)
        for kind, ids in parsed.components.items():
            refs[kind].extend(ids)
            for ident in ids:
                uses.setdefault((kind, ident), []).append(list(chapters)[index])
    lookup = {}
    for kind, items in groups.items():
        unique([item['id'] for item in items], kind + ' id')
        lookup[kind] = {item['id']: item for item in items}
        for ident in refs[kind]:
            if ident not in lookup[kind]:
                raise ValueError(f'{ident}: unknown {kind}')
        if kind not in ('term', 'map'):
            for ident in lookup[kind]:
                if ident not in refs[kind]:
                    raise ValueError(f'{ident}: unused {kind}')
            if kind in ('walkthrough', 'compare', 'quiz'):
                unique(refs[kind], kind + ' use')
    ev = {item['id']: item for item in evidence}

    def reference(ident, value, kind):
        if value not in lookup[kind]:
            raise ValueError(f'{ident}: unknown {kind} {value}')

    def source_ref(ident, value):
        if value not in ev:
            raise ValueError(f'{ident}: unknown evidence {value}')
        return ev[value]

    def focus(ident, item, evidence_id):
        e = source_ref(ident, evidence_id)
        lines = sources[e['source']]['text'].splitlines()
        selected = '\n'.join(lines[e['startLine'] - 1:e['endLine']])
        boundary = item.get('focus')
        if boundary:
            start, end = boundary['start'], boundary['end']
            if not start or not end or selected.count(start) != 1 or selected.count(end) != 1:
                raise ValueError(f'{ident}: focus missing or ambiguous')
            a, b = selected.index(start), selected.index(end) + len(end)
            if b < a:
                raise ValueError(f'{ident}: inverted focus')
            item['startLine'] = e['startLine'] + selected.count('\n', 0, a)
            item['endLine'] = e['startLine'] + selected.count('\n', 0, b)
        else:
            item['startLine'], item['endLine'] = e['startLine'], e['endLine']

    for item in groups['figure']:
        ident = item['id']
        if item['kind'] not in FIGURE_KINDS:
            raise ValueError(f'{ident}: unknown figure kind')
        for step in item.get('steps', []):
            if step.get('evidence'):
                source_ref(ident, step['evidence'])
            if step.get('walkthrough'):
                target = step['walkthrough']
                reference(ident, target['id'], 'walkthrough')
                if not 0 <= target['step'] < len(lookup['walkthrough'][target['id']]['steps']):
                    raise ValueError(f'{ident}: invalid walkthrough step')
    for item in groups['walkthrough']:
        ident = item['id']
        source_ref(ident, item['evidence'])
        if not item.get('steps'):
            raise ValueError(f'{ident}: no walkthrough steps')
        for step in item['steps']:
            focus(ident, step, step.get('evidence', item['evidence']))
            for key, kind in (('calls', 'walkthrough'), ('mistake', 'compare'), ('term', 'term')):
                if step.get(key):
                    reference(ident, step[key], kind)
            if step.get('figure'):
                target = step['figure']
                reference(ident, target['id'], 'figure')
                if not 0 <= target['step'] < max(1, len(lookup['figure'][target['id']].get('steps', []))):
                    raise ValueError(f'{ident}: invalid figure step')
    for item in groups['compare']:
        ident = item['id']
        source_ref(ident, item['evidence'])
        for side in ('left', 'right') if item.get('mode') == 'evolution' else ('wrong', 'right'):
            part = item[side]
            if part.get('evidence'):
                focus(ident, part, part['evidence'])
            elif not part.get('label') or not part.get('code'):
                raise ValueError(f'{ident}: comparison needs labelled code')
    lab_ids = {lab['id'] for lab in read_json(HERE / 'manifest.json')['labs']}
    for item in groups['quiz']:
        ident = item['id']
        if uses.get(('quiz', ident)) != [item['chapter']]:
            raise ValueError(f'{ident}: quiz chapter mismatch')
        if sum(option.get('correct') is True for option in item['options']) != 1:
            raise ValueError(f'{ident}: quiz needs exactly one answer')
        if any(not option.get('text') or not option.get('explain') for option in item['options']):
            raise ValueError(f'{ident}: quiz explanation missing')
        source_ref(ident, item['evidence'])
        if item.get('lab') and item['lab'] not in lab_ids:
            raise ValueError(f'{ident}: unknown lab')
    nodes = [node for layer in system.get('layers', []) for node in layer['nodes']]
    unique([node['id'] for node in nodes], 'map node')
    node_ids = {node['id'] for node in nodes}
    for _, text in fragments:
        if any(value not in node_ids for value in Fragment(text).map_focus):
            raise ValueError('system: unknown data-focus node')
    for node in nodes:
        for chapter in node['chapters']:
            if chapter not in chapters:
                raise ValueError(f"{node['id']}: unknown chapter")
        for value in node.get('evidence', []):
            source_ref(node['id'], value)
        if any(lab not in lab_ids for lab in node.get('labs', [])):
            raise ValueError(f"{node['id']}: unknown lab")
    for link in system.get('links', []):
        if link['from'] not in node_ids or link['to'] not in node_ids:
            raise ValueError('system: unknown map link endpoint')
    for chapter in chapters.values():
        if any(value not in node_ids for value in chapter.get('mapFocus', [])):
            raise ValueError(f"{chapter['id']}: unknown mapFocus")
    for scene in system.get('scenes', []):
        if any(value not in node_ids for value in scene['nodes']) or any(c not in chapters for c in scene['chapters']):
            raise ValueError(f"{scene['id']}: invalid map scene")
    return {key: groups[kind] for key, kind in (('figures', 'figure'), ('walkthroughs', 'walkthrough'),
            ('compares', 'compare'), ('quizzes', 'quiz'))} | {'map': system}


def validate_links(links, chapters, ids):
    from urllib.parse import parse_qs, unquote
    by_slug = {c["slug"]: c for c in chapters}
    for link in links:
        if link.startswith(("https://", "http://", "mailto:")):
            continue
        if not link.startswith("#"):
            raise ValueError(f"Manuscript must use canonical local routes: {link}")
        fragment = unquote(link[1:])
        if fragment.startswith("chapter="):
            values = parse_qs(fragment)
            slug = values.get("chapter", [""])[0]
            if slug not in by_slug:
                raise ValueError(f"Unknown chapter link: {link}")
            section = values.get("section", [""])[0]
            if section and section not in ids.get(slug, []):
                raise ValueError(f"Unknown section link: {link}")
        elif fragment not in {f"p{i}" for i in range(1, 11)} and not any(fragment in group for group in ids.values()) and fragment != "content":
            raise ValueError(f"Unknown anchor: {link}")


def build(complete=False):
    manifest = read_json(HERE / "manifest.json")
    chapters = manifest["chapters"]
    if [c["id"] for c in chapters] != EXPECTED_CHAPTERS:
        raise ValueError("Manifest must contain C01, P01-P03, C02-C28 and A01-A05 in order")
    if any(c.get("level") not in ("입문", "중급", "심화", "참고") for c in chapters):
        raise ValueError("Every chapter needs a reader level")
    unique([c["slug"] for c in chapters], "chapter slug")
    if [lab["id"] for lab in manifest["labs"]] != [f"L{i:02}" for i in range(1, 21)]:
        raise ValueError("Manifest must contain L01-L20")
    by_id = {c["id"]: c for c in chapters}
    for route in manifest["readingRoutes"]:
        if any(c not in by_id for c in route["chapters"]):
            raise ValueError("Unknown chapter in reading route")
    fragments = []
    chapter_ids = {}
    chapter_links = []
    for chapter in chapters:
        if chapter["status"] not in ("planned", "excerpt", "draft", "complete"):
            raise ValueError(f"{chapter['id']}: invalid status")
        if chapter["file"] is None:
            if chapter["status"] != "planned" or chapter["sections"]:
                raise ValueError(f"{chapter['id']}: missing manuscript")
            if complete:
                raise ValueError(f"{chapter['id']}: manuscript not complete")
            continue
        text = contained(HERE, chapter["file"]).read_text(encoding="utf-8")
        parsed = Fragment(text)
        chapter_ids[chapter["slug"]] = parsed.ids
        chapter_links.extend(parsed.links)
        if not "".join(parsed.prose).strip():
            raise ValueError(f"{chapter['id']}: empty manuscript")
        chapter['archiveCount'] = ArchiveAudit(text, chapter['sections']).count
        if re.search(r"\bTODO\b|\bTBD\b|자리표시자", "".join(parsed.text), re.I):
            raise ValueError(f"{chapter['id']}: unfinished marker in manuscript")
        for section in chapter["sections"]:
            if section["id"] not in parsed.ids:
                raise ValueError(f"{chapter['id']}: missing section {section['id']}")
        if complete and (chapter["status"] != "complete" or not chapter["technicalReviewed"] or
                         not chapter["editorialReviewed"]):
            raise ValueError(f"{chapter['id']}: technical/editorial review not complete")
        fragments.append((chapters.index(chapter), text))
    for lab in manifest["labs"]:
        if lab["chapter"] not in by_id:
            raise ValueError(f"{lab['id']}: unknown chapter")
        if lab["model"] is not None:
            contained(HERE, lab["model"]).read_text(encoding="utf-8")
        if complete and lab["status"] != "complete":
            raise ValueError(f"{lab['id']}: lab not complete")
    unique(manifest["sourceFiles"], "source path")
    sources = {}
    for name in manifest["sourceFiles"]:
        raw = contained(ROOT, name).read_bytes()
        sources[name] = {"text": raw.decode("utf-8-sig"), "sha256": sha256(raw).hexdigest(),
                         "commit": manifest["sourceCommit"]}
    evidence = collect_evidence(read_json(HERE / "evidence.json"), sources, by_id)
    glossary = read_json(HERE / "glossary.json")
    unique([g["id"] for g in glossary["items"]], "glossary id")
    for term in glossary["items"]:
        if not term["definition"] or not term["aliases"] or any(c not in by_id for c in term["chapters"]):
            raise ValueError(f"{term['id']}: invalid glossary entry")
    migration = read_json(HERE / "migration.json")
    check_migration(migration, complete, {chapters[n]["slug"]:text for n,text in fragments})
    html = (HERE / "book_template.html").read_text(encoding="utf-8")
    html = html.replace("<!-- CHAPTERS -->", "\n".join(t for _, t in sorted(fragments)))
    parsed = Fragment(html)
    unique(parsed.ids, "HTML id")
    for file, find in parsed.sources:
        if file not in sources or not find or find not in sources[file]["text"]:
            raise ValueError(f"Missing source reference: {file}: {find}")
    progress = sum(c["status"] == "complete" for c in chapters)
    api = read_json(HERE / "generated/api.json")
    actual_headers = sorted(p.relative_to(ROOT).as_posix() for p in (ROOT / "include/rionetlite").glob("*.h"))
    if [h["path"] for h in api["headers"]] != actual_headers:
        raise ValueError("API JSON header inventory is stale")
    for header in api["headers"]:
        if sha256(contained(ROOT, header["path"]).read_bytes()).hexdigest() != header["sha256"]:
            raise ValueError(f"API JSON header changed: {header['path']}")
    html = html.replace("<!-- API_REFERENCE -->", table(["헤더", "심볼", "설명"], [
        [escape(s["header"]), '<code>'+escape(s["symbol"])+ '</code>', escape(s["description"])] for s in api["symbols"]]))
    html = html.replace("<!-- GLOSSARY -->", table(["용어", "의미", "검색 별칭", "관련 장"], [
        [escape(g["term"]),escape(g["definition"]),escape(', '.join(g["aliases"])),', '.join('<a href="#chapter='+by_id[c]["slug"]+'">'+c+'</a>' for c in g["chapters"])] for g in glossary["items"]]))
    html = html.replace("<!-- MIGRATION_SUMMARY -->", table(["원본", "개정본", "항목", "처리 상태"], [
        [escape(d["source"]),escape(d["revision"][:12]),str(len(d["items"])),escape(d["status"])] for d in migration["documents"]]))
    verification = read_json(HERE / "verification.json")
    if complete and any(r["status"] == "pending" for r in verification["checks"]):
        raise ValueError("Final verification still pending")
    html = html.replace("<!-- VERIFICATION_SUMMARY -->", table(["검사", "결과", "범위·근거"], [
        [escape(r["name"]),escape(r["status"]),escape(r["result"])] for r in verification["checks"]]))
    validate_links(chapter_links, chapters, chapter_ids)
    evidence_ids = {e["id"] for e in evidence}
    if any(e not in evidence_ids for e in parsed.evidence):
        raise ValueError("Unknown data-evidence reference: " + str(set(parsed.evidence)-evidence_ids))
    lab_ids = {l["id"] for l in manifest["labs"]}
    if set(parsed.labs) != lab_ids or len(parsed.labs) != len(lab_ids):
        raise ValueError("Each lab must occur exactly once in the manuscripts")
    data = {"manifest": manifest, "evidence": evidence, "glossary": glossary["items"], "api":api}
    definitions = [read_json(HERE / name) for name in ('figures.json', 'walkthroughs.json', 'quizzes.json', 'map.json')]
    data.update(collect_components(definitions, fragments, evidence, glossary, by_id, sources))
    script = "const BOOK = " + json.dumps(data, ensure_ascii=False).replace("<", "\\u003c") + ";\n"
    script += "const SOURCES = " + json.dumps(sources, ensure_ascii=False).replace("<", "\\u003c") + ";\n"
    models = manifest.get("modelFiles", [])
    if any(lab["model"] not in models for lab in manifest["labs"] if lab["model"]):
        raise ValueError("Lab model missing from modelFiles")
    unique(models, "model path")
    for model in models:
        script += contained(HERE, model).read_text(encoding="utf-8") + "\n"
    script += (HERE / "book.js").read_text(encoding="utf-8")
    if "</script" in script.lower():
        raise ValueError("Unsafe inline script terminator")
    html = html.replace("<!-- STYLES -->", "<style>" + (HERE / "book.css").read_text(encoding="utf-8") + "</style>")
    html = html.replace("<!-- SCRIPTS -->", "<script>" + script + "</script>")
    html = html.replace("{{COMMIT}}", manifest["sourceCommit"][:12])
    html = html.replace("{{PROGRESS}}", f"집필 완료 {progress}/{len(chapters)}")
    html = html.replace("{{EXCERPTS}}", str(len(fragments)))
    html = html.replace("{{SOURCE_COUNT}}", str(len(sources)))
    if re.search(r"\{\{[A-Z_]+\}\}|<!-- (STYLES|SCRIPTS|CHAPTERS) -->", html):
        raise ValueError("Unresolved template slot")
    if len(html.encode('utf-8')) > 4 * 1024 * 1024:
        raise ValueError('Book exceeds 4 MiB budget')
    return html, progress, len(evidence)


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--require-complete", action="store_true")
    parser.add_argument("--output", type=Path, default=HERE / "index.html")
    args = parser.parse_args()
    try:
        html, progress, evidence_count = build(args.require_complete)
        if args.check:
            if not args.output.is_file() or args.output.read_bytes() != html.encode("utf-8"):
                raise ValueError("Generated HTML is stale; run python docs/interactive_book/build_book.py")
            mode = "manuscript/lab/migration completion enforced" if args.require_complete else "consistency check"
            print(f"Book verified (read-only): {progress}/{CHAPTER_COUNT} complete chapters, {evidence_count} reviewed source selections; {mode}. Native and browser execution are separate checks.")
        else:
            output = args.output.resolve()
            if output.suffix != ".html" or (output.is_relative_to(ROOT) and output != HERE / "index.html" and
                    not output.is_relative_to(ROOT / ".artifacts") and not output.is_relative_to(ROOT / "artifacts")):
                raise ValueError("Use index.html, an artifacts HTML path, or an external validation path for --output")
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_bytes(html.encode("utf-8"))
            print(f"Built {output}: {progress}/{CHAPTER_COUNT} complete chapters, {evidence_count} reviewed source selections.")
    except (ValueError, KeyError, OSError, TypeError) as error:
        print(f"Book validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
