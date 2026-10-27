"""Build a standalone offline book: python docs/interactive_book/build_book.py"""
from pathlib import Path
import hashlib
import json
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
FILES = ['samples/echo_server/main.cpp', 'include/rionetlite/packet_decoder.h',
         'include/rionetlite/event_handler.h', 'include/rionetlite/server.h',
         'src/detail/session_lifecycle.h', 'RIONetLite/impl/server_net_io_worker.h',
         'RIONetLite/impl/send_immediately.h', 'RIONetLite/impl/server_net.h',
         'tests/unit/rio_request_test.cpp', 'tests/unit/correctness_primitives_test.cpp',
         'tests/integration/smoke_test.cpp', 'include/rionetlite/queued_event_adapter.h']
sources = {}
for name in FILES:
    raw = (ROOT / name).read_bytes()
    sources[name] = {'text': raw.decode('utf-8-sig'), 'sha256': hashlib.sha256(raw).hexdigest()}
commit = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=ROOT, text=True).strip()
data = json.dumps(sources, ensure_ascii=False).replace('<', '\\u003c')
html = (HERE / 'book_template.html').read_text(encoding='utf-8')
html = html.replace('<!-- STYLES -->', '<style>' + (HERE / 'book.css').read_text(encoding='utf-8') + '</style>')
html = html.replace('<!-- SCRIPTS -->', '<script>const SOURCES = ' + data + ';\n' + (HERE / 'book.js').read_text(encoding='utf-8') + '</script>')
html = html.replace('{{COMMIT}}', commit)
(HERE / 'index.html').write_text(html, encoding='utf-8')
print(f'Built {HERE / "index.html"}: {len(sources)} source snapshots, commit {commit}')
