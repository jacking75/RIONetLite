"""Regression checks for the authoring contract, using isolated temporary copies."""
from contextlib import redirect_stderr, redirect_stdout
from importlib.util import module_from_spec, spec_from_file_location
from io import StringIO
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import json
import hashlib
import shutil
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
BOOK = ROOT / "docs/interactive_book"
SPEC = spec_from_file_location("book_builder", BOOK / "build_book.py")
builder = module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class BookContentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.book = self.root / "docs/interactive_book"
        shutil.copytree(BOOK, self.book, ignore=shutil.ignore_patterns("__pycache__"))
        manifest = builder.read_json(BOOK / "manifest.json")
        migration = builder.read_json(BOOK / "migration.json")
        files = set(manifest["sourceFiles"])
        files.update(e["testSource"] for e in builder.read_json(BOOK / "evidence.json")["items"])
        files.update(r["source"] for r in migration["documents"] if r["revision"] == "worktree")
        for name in files:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            if (ROOT / name).is_file():
                shutil.copyfile(ROOT / name, target)
        for name, value in (("ROOT", self.root), ("HERE", self.book)):
            patcher = patch.object(builder, name, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def change(self, name, transform):
        path = self.book / name
        value = json.loads(path.read_text(encoding="utf-8"))
        transform(value)
        path.write_text(json.dumps(value, ensure_ascii=False), encoding="utf-8")

    def test_deterministic_generation_and_explicit_incompleteness(self):
        first = builder.build()
        self.assertEqual(first, builder.build())
        self.assertTrue(f"집필 완료 {first[1]}/{builder.CHAPTER_COUNT}" in first[0])
        self.change("manifest.json", lambda m: m["chapters"][0].update(status="excerpt", technicalReviewed=False))
        with self.assertRaisesRegex(ValueError, "not complete"):
            builder.build(complete=True)

    def test_component_references_are_validated(self):
        path = self.book / 'chapters/p03_echo_overview.html'
        original = path.read_text(encoding='utf-8')
        path.write_text(original + '<figure data-figure="F-missing"></figure>', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'F-missing: unknown figure'):
            builder.build()
        path.write_text(original, encoding='utf-8')
        self.change('figures.json', lambda d: d['figures'].append({'id': 'F-unused', 'kind': 'state'}))
        with self.assertRaisesRegex(ValueError, 'F-unused: unused figure'):
            builder.build()
        self.change('figures.json', lambda d: d['figures'].pop())
        path.write_text(original + '<div data-walkthrough="W99"></div>', encoding='utf-8')
        self.change('walkthroughs.json', lambda d: d['walkthroughs'].append({
            'id': 'W99', 'evidence': 'echo.main', 'steps': [
                {'focus': {'start': ' ', 'end': ' '}}]}))
        with self.assertRaisesRegex(ValueError, 'W99: focus missing or ambiguous'):
            builder.build()

    def test_check_is_read_only_and_reports_stale_output(self):
        output = self.book / "index.html"
        output.write_bytes(builder.build()[0].encode("utf-8"))
        before = output.read_bytes(), output.stat().st_mtime_ns
        with patch.object(sys, "argv", ["build_book.py", "--check", "--output", str(output)]), redirect_stdout(StringIO()):
            self.assertEqual(builder.main(), 0)
        self.assertEqual((output.read_bytes(), output.stat().st_mtime_ns), before)
        output.write_text("stale", encoding="utf-8")
        with patch.object(sys, "argv", ["build_book.py", "--check", "--output", str(output)]), redirect_stderr(StringIO()):
            self.assertEqual(builder.main(), 1)
        self.assertEqual(output.read_text(encoding="utf-8"), "stale")

    def test_missing_chapter_is_rejected(self):
        self.change("manifest.json", lambda m: m["chapters"].pop())
        with self.assertRaisesRegex(ValueError, "C01, P01-P03, C02-C28"):
            builder.build()

    def test_duplicate_slug_is_rejected(self):
        self.change("manifest.json", lambda m: m["chapters"][1].update(slug=m["chapters"][0]["slug"]))
        with self.assertRaisesRegex(ValueError, "Duplicate chapter slug"):
            builder.build()

    def test_duplicate_html_id_is_rejected(self):
        path = self.book / "chapters/c04_echo_walkthrough.html"
        path.write_text(path.read_text(encoding="utf-8") + '<p id="framing-stream">duplicate</p>', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Duplicate HTML id"):
            builder.build()

    def test_source_change_requires_review_and_does_not_update_hash(self):
        evidence = (self.book / "evidence.json").read_bytes()
        source = self.root / "samples/echo_server/main.cpp"
        source.write_bytes(source.read_bytes() + b"\n// changed\n")
        with self.assertRaisesRegex(ValueError, "source changed; review required"):
            builder.build()
        self.assertEqual((self.book / "evidence.json").read_bytes(), evidence)

    def test_missing_and_ambiguous_selectors_are_rejected(self):
        self.change("evidence.json", lambda e: e["items"][0]["selector"].update(start="not a function"))
        with self.assertRaisesRegex(ValueError, "selector missing or ambiguous"):
            builder.build()
        self.change("evidence.json", lambda e: e["items"][0]["selector"].update(start="return"))
        with self.assertRaisesRegex(ValueError, "selector missing or ambiguous"):
            builder.build()

    def test_test_declaration_must_exist(self):
        self.change("evidence.json", lambda e: e["items"][0].update(testName="Missing.Test"))
        with self.assertRaisesRegex(ValueError, "test declaration missing"):
            builder.build()

    def test_test_source_change_requires_review(self):
        source = self.root / "tests/unit/rio_request_test.cpp"
        source.write_bytes(source.read_bytes() + b"\n// changed\n")
        with self.assertRaisesRegex(ValueError, "test source changed"):
            builder.build()

    def test_migration_original_change_blocks_deletion_assumptions(self):
        source = self.root / "docs/operations.md"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes(b"new operational requirement\n")
        self.change("migration.json", lambda m: [r.update(status="inventoried") for r in m["documents"]
                    if r["source"] == "docs/operations.md" and r["revision"] == "worktree"])
        with self.assertRaisesRegex(ValueError, "Migration original changed"):
            builder.build()

    def test_empty_manuscript_is_not_accepted_as_complete(self):
        path = self.book / "chapters/c04_echo_walkthrough.html"
        path.write_text('<section id="p3"></section>', encoding="utf-8")
        self.change("manifest.json", lambda m: m["chapters"][3].update(status="complete"))
        with self.assertRaisesRegex(ValueError, "empty manuscript"):
            builder.build()

    def test_heading_only_manuscript_is_not_accepted(self):
        path = self.book / "chapters/c04_echo_walkthrough.html"
        path.write_text('<section id="p3"><h2>Only a heading</h2></section>', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "empty manuscript"):
            builder.build()

    def test_embedded_source_cannot_terminate_the_data_script(self):
        path = self.root / "injected.cpp"
        path.write_bytes(b'// </script><script>unsafe()</script>\n')
        self.change("manifest.json", lambda m: m["sourceFiles"].append("injected.cpp"))
        html = builder.build()[0]
        self.assertFalse("<script>unsafe()" in html)
        self.assertTrue("\\u003c/script>" in html)

    def test_api_header_hash_is_checked(self):
        self.change("generated/api.json", lambda a: a["headers"][0].update(sha256="0"*64))
        with self.assertRaisesRegex(ValueError, "API JSON header changed"):
            builder.build()

    def test_historical_eol_comparison_allows_only_line_ending_changes(self):
        source = self.root / "tools/build.ps1"
        source.write_bytes(source.read_bytes().replace(b"\r\n", b"\n"))
        def reviewed_hash():
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            self.change("evidence.json", lambda e: [row.update(reviewedSourceSha256=digest)
                        for row in e["items"] if row["source"] == "tools/build.ps1"])
        reviewed_hash()
        builder.build()
        source.write_bytes(source.read_bytes() + b"\n# new behavior requires a new content review\n")
        reviewed_hash()
        with self.assertRaisesRegex(ValueError, "Historical source equivalence changed"):
            builder.build()

    def test_preserved_original_cannot_silently_disappear(self):
        migration = builder.read_json(self.book / "migration.json")
        item = migration["documents"][0]["items"][0]
        anchor = item["destination"].split("#")[1]
        path = self.book / "chapters/a04_historical_records.html"
        path.write_text(path.read_text(encoding="utf-8").replace('id="'+anchor+'"', 'id="removed-history"'), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Migration preserved fragment changed"):
            builder.build()

    def test_unknown_evidence_and_broken_chapter_links_are_rejected(self):
        path = self.book / "chapters/c04_echo_walkthrough.html"
        original = path.read_text(encoding="utf-8")
        path.write_text(original + '<button data-evidence="missing.evidence">source</button>', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Unknown data-evidence"):
            builder.build()
        path.write_text(original + '<a href="#chapter=packet-framing&section=not-a-section">link</a>', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Unknown section link"):
            builder.build()

    def test_manuscript_cannot_embed_executable_content(self):
        path = self.book / "chapters/c04_echo_walkthrough.html"
        path.write_text(path.read_text(encoding="utf-8") + '<img onerror="unsafe()">', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Executable content"):
            builder.build()

    def test_unknown_schema_and_path_traversal_are_rejected(self):
        self.change("glossary.json", lambda g: g.update(schemaVersion=999))
        with self.assertRaisesRegex(ValueError, "unsupported schemaVersion"):
            builder.build()
        with self.assertRaisesRegex(ValueError, "outside allowlist"):
            builder.contained(self.book, "../../../outside.txt")

    def test_original_24_documents_and_existing_technical_book_are_inventoried(self):
        migration = builder.read_json(self.book / "migration.json")
        originals = [r for r in migration["documents"] if r["revision"] == migration["originalCommit"]]
        self.assertEqual(len(originals), 24)
        technical = next(r for r in migration["documents"] if r["source"] == "docs/rionetlite_book.html")
        self.assertTrue(any(i["id"] == "atlas-49" for i in technical["items"]))
        self.assertTrue(all(i["content"] for r in migration["documents"] for i in r["items"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
