# SQLite Dictionary Schema

This is the schema for the `dictionary_type=sqlite` candidate source.

## Goals

- Keep lookup simple: query by exact code and return ordered candidates.
- Keep metadata extensible without changing the core lookup table.
- Preserve enough ranking fields for later frequency learning.

## Tables

```sql
CREATE TABLE IF NOT EXISTS entries (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    weight INTEGER NOT NULL DEFAULT 0,
    user_weight INTEGER NOT NULL DEFAULT 0,
    flags INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_entries_code_rank
ON entries(code, weight DESC, user_weight DESC, id ASC);

CREATE TABLE IF NOT EXISTS metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS user_entries (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    weight INTEGER NOT NULL DEFAULT 0,
    user_weight INTEGER NOT NULL DEFAULT 0,
    deleted INTEGER NOT NULL DEFAULT 0,
    created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    UNIQUE(code, text)
);

CREATE INDEX IF NOT EXISTS idx_user_entries_code_rank
ON user_entries(code, deleted, weight DESC, user_weight DESC, id ASC);

CREATE TABLE IF NOT EXISTS user_learning_events (
    id INTEGER PRIMARY KEY,
    code TEXT NOT NULL,
    text TEXT NOT NULL,
    comment TEXT NOT NULL DEFAULT '',
    delta_user_weight INTEGER NOT NULL,
    created_at_utc TEXT NOT NULL DEFAULT (datetime('now')),
    reverted_at_utc TEXT
);

CREATE INDEX IF NOT EXISTS idx_user_learning_events_active
ON user_learning_events(reverted_at_utc, id DESC);
```

## Query Contract

Exact lookup returns base dictionary rows joined with user weights:

```sql
SELECT e.code, e.text, e.comment, e.weight,
       e.user_weight + COALESCE(u.user_weight, 0) AS effective_user_weight
FROM entries e
LEFT JOIN user_entries u
  ON u.code = e.code AND u.text = e.text AND u.deleted = 0
WHERE e.code = ?
ORDER BY effective_user_weight DESC, e.weight DESC, e.id ASC;
```

When `prefix_candidates=1`, lookup runs in two stages. It first runs the exact
lookup above. If that fills the candidate limit, prefix lookup stops there.
Otherwise it fills the remaining slots with a code range so the `code` index
can be used:

```sql
SELECT text, comment
FROM entries
WHERE code >= ? AND code < ? AND code <> ?
ORDER BY weight DESC, user_weight DESC, code ASC, id ASC;
```

The upper bound is the next lexicographic prefix. For example, `nih` scans
`code >= 'nih' AND code < 'nii'`.

The source should map each row to:

```cpp
Candidate{text, comment, weight, user_weight}
```

When a candidate is selected, the SQLite source upserts a `user_entries` row
with a positive `user_weight` delta and appends a `user_learning_events` row.
Management tools can mark the most recent unreverted event as reverted and
subtract its delta from `user_entries.user_weight`, which lets mistaken learning
be undone without deleting the word itself.

When a candidate is deleted, the SQLite source upserts a `user_entries` row with
`deleted = 1`. Tombstones hide both standalone user words and base dictionary
rows with the same `code+text` pair. Restoring a user entry sets `deleted = 0`
without changing its stored weights.

## Stats Contract

`SqliteCandidateSource::GetStats()` should report:

- `source_type = "sqlite"`
- `status = "loaded"` when the database opens and the schema is usable
- `configured = true` when `dictionary_path` is not empty
- `loaded = true` only after successful open/schema validation
- `code_count = COUNT(DISTINCT code)`
- `candidate_count = COUNT(*)`

## Migration Notes

The TSV source is a prototype loader. A later migration tool can import:

```text
code<Tab>text<Tab>comment<Tab>weight<Tab>user_weight
```

as:

```sql
INSERT INTO entries(code, text, comment, weight, user_weight)
VALUES (?, ?, ?, ?, ?);
```

`comment`, `weight`, and `user_weight` are optional in TSV. Missing numeric
fields import as `0`.
