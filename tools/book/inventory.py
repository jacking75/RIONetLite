"""Read-only migration inventory audit; originals are retained in migration.json."""
from collections import Counter
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]
ledger = json.loads((ROOT/'docs/interactive_book/migration.json').read_text(encoding='utf-8'))
documents = ledger['documents']
items = [item for document in documents for item in document['items']]
assert sum(d['revision'] == ledger['originalCommit'] for d in documents) == 24
assert len(documents) == 34 and len(items) == 493
assert all(i['reviewed'] and i['destination'] and i['preservation'] for i in items)
print(json.dumps({'originalDocuments':24, 'revisions':len(documents), 'contentItems':len(items),
                  'statuses':dict(Counter(d['status'] for d in documents)),
                  'dispositions':dict(Counter(i['disposition'] for i in items))}, indent=2))
