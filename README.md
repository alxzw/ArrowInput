# ArrowInput

ArrowInput is an x64-only Windows TSF input method project.

The current goal is to keep the TSF/UI shell usable while the input algorithm is
developed and tested independently.

## Current Status

- Builds a x64 COM DLL: `build\x64\Debug\ArrowInputTextService.dll`
- Deploys test artifacts to `D:\MyApp\ArrowInput`
- Reads runtime config from `%APPDATA%\ArrowInput\config.ini`
- Uses TSF key sinks and focus sinks
- Supports preedit display, candidate window, paging, mouse selection, and
  focus-loss cleanup
- Supports Chinese/English mode, full/half shape mode, Chinese punctuation, and
  shortcut pass-through for Ctrl/Alt combinations
- Has an algorithm boundary:
  - `IInputAlgorithm::QueryCandidates(code)`
  - candidate source selection through `dictionary_type`
  - TSV dictionary loading through `dictionary_path`
  - standalone `AlgorithmProbe.exe` for algorithm testing

## Build

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
```

## Deploy

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Debug
```

The default test install directory is:

```text
D:\MyApp\ArrowInput
```

If the DLL is loaded, deployment writes:

```text
D:\MyApp\ArrowInput\ArrowInputTextService.dll.pending
```

Sign out or otherwise unload the DLL, then replace the live DLL with the pending
one.

## Install

TSF profile registration requires elevation. Run PowerShell as Administrator:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1 -Configuration Debug
```

## Uninstall

Run PowerShell as Administrator:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\uninstall.ps1 -Configuration Debug
```

Unregistering a TSF profile does not force processes that already loaded the
DLL to unload it. To find or stop processes that still lock the deployed DLL:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\release_loaded_binaries.ps1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\release_loaded_binaries.ps1 -StopLockingProcesses
```

You can also ask uninstall to do that after unregistering:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\uninstall.ps1 -StopLockingProcesses
```

## Algorithm Probe

Build the standalone algorithm probe:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_algorithm_probe.ps1 -Configuration Debug
```

Run one-shot queries:

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type tsv --dict tools\AlgorithmProbe\sample_dictionary.tsv ni hao
```

Limit returned candidates for lookup experiments:

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type sqlite --dict D:\MyApp\ArrowInput\experiments\rime_luna.db --limit 20 yi nih
```

Query the dictionary currently selected by `%APPDATA%\ArrowInput\config.ini`:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\probe_current_dictionary.ps1 -Limit 10 yi nih
```

Run a fuller health check for the currently selected dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\check_current_dictionary.ps1 -Limit 5 ni nih yi zhongguo
```

Manage user words in a SQLite dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db add --code ceshi --text 测试 --comment "ce shi" --user-weight 5000
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db list
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db user-stats
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db export --output D:\MyApp\ArrowInput\experiments\user_entries.tsv
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db import --input D:\MyApp\ArrowInput\experiments\user_entries.tsv
```

For the currently configured SQLite dictionary, the shorter wrapper can be used:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 user-stats
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 undo-delete
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 undo-learning
```

Run interactive queries:

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --dict tools\AlgorithmProbe\sample_dictionary.tsv --interactive
```

Print source statistics:

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type tsv --dict tools\AlgorithmProbe\sample_dictionary.tsv --stats
```

`dictionary_type=tsv` and `dictionary_type=sqlite` are implemented. Other
values report `unsupported source type`.

The SQLite schema is documented in
`docs\sqlite_dictionary_schema.md`.

TSV dictionary format:

```text
code<Tab>text<Tab>comment<Tab>weight<Tab>user_weight
```

The dictionary is UTF-8. Lines starting with `#` are ignored. `comment`,
`weight`, and `user_weight` are optional. Candidates are ordered by
`weight DESC`, then `user_weight DESC`, then file order.

Runtime candidate lookup is capped by `max_candidates_per_query` in
`config.ini` to keep large dictionaries responsive. The default is 30.
When `prefix_candidates=1`, ArrowInput also shows candidates whose full code
starts with the current composition, such as `nihao` while the composition is
`nih`. Runtime lookup first returns exact-code candidates, then fills any
remaining slots with longer prefix matches.

Continuous pinyin is parsed before lookup. For example, `shurufa` can also
query dictionary rows stored as `shu ru fa`.

Short pinyin acronyms are also queried as fallback candidates. For example,
`zg` can match `zhong guo`, and `srf` can match `shu ru fa`.
Mixed prefix input is supported too, such as `zguo` or `zhg` for `zhong guo`
and `srfa` or `shrf` for `shu ru fa`.
Optional fuzzy pinyin can be enabled with `fuzzy_pinyin=1`; it expands common
pairs such as `z/zh`, `c/ch`, `s/sh`, `an/ang`, `en/eng`, and `in/ing`.

SQLite dictionaries include a `user_entries` table for learning and manual user
words. Selecting a candidate records a user weight boost, so repeated choices
rise ahead of unselected candidates with the same code. Learning events are
also recorded, which makes the latest automatic boost reversible:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db learning-history
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db undo-learning
```

Wrong or unwanted words can be hidden with a user tombstone:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db delete --code ni --text 你
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db deleted
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db undo-delete
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db restore --code ni --text 你
D:\MyApp\ArrowInput\AlgorithmProbe.exe --type sqlite --dict D:\MyApp\ArrowInput\experiments\rime_luna.db --limit 5 --delete ni 1
```

During composition, `Shift+Delete` hides the currently highlighted candidate
and refreshes the candidate window.

Validate a TSV dictionary before importing or deploying it:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\validate_tsv_dictionary.ps1 -InputPath data\seed_pinyin.tsv
```

Validation rejects duplicate `code+text` rows, invalid integer weights, extra
fields, leading/trailing whitespace, malformed codes, overlong fields, and
weights outside the configured range. Codes currently allow lowercase ASCII
letters and apostrophes. The default limits are intentionally broad for
experiments and can be adjusted with `-MaxCodeLength`, `-MaxTextLength`,
`-MaxCommentLength`, `-MinWeight`, `-MaxWeight`, `-MinUserWeight`, and
`-MaxUserWeight`.

Convert a simple external text dictionary to ArrowInput TSV:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\convert_text_dictionary_to_tsv.ps1 -InputPath D:\path\to\source.txt -OutputPath D:\path\to\converted.tsv
```

By default the converter reads `code text comment weight user_weight` columns,
split by tabs when present and otherwise by whitespace. Use `-Columns` when the
external order differs, for example `-Columns text,code,weight`.

Rime `.dict.yaml` files can also be converted. Entries after the `...` marker
are read as `text<Tab>code<Tab>weight`, and code spaces are removed for
ArrowInput lookup while the original code is kept as the comment:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\convert_text_dictionary_to_tsv.ps1 -InputPath D:\path\to\luna_pinyin.dict.yaml -OutputPath D:\path\to\rime_converted.tsv -SourceFormat rime
```

Append one experimental TSV entry, validate the file, rebuild SQLite, and query
the new code:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\add_tsv_entry.ps1 -DictionaryPath D:\MyApp\ArrowInput\seed_pinyin.tsv -Code ceshi -Text 测试 -Comment "ce shi" -Weight 1000 -DatabasePath D:\MyApp\ArrowInput\seed_pinyin.db
```

Merge a batch TSV into the seed dictionary, skip duplicate `code+text` rows,
validate, and rebuild SQLite:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\merge_tsv_dictionary.ps1 -TargetPath D:\MyApp\ArrowInput\seed_pinyin.tsv -SourcePath D:\path\to\new_words.tsv -DatabasePath D:\MyApp\ArrowInput\seed_pinyin.db
```

Preview a merge without modifying the target dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\preview_tsv_merge.ps1 -TargetPath D:\MyApp\ArrowInput\experiments\trial1.tsv -SourcePath D:\path\to\new_words.tsv
```

Promote preferred candidates from one TSV, such as `seed_pinyin.tsv`, above
other candidates with the same code:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\promote_tsv_candidates.ps1 -TargetPath D:\MyApp\ArrowInput\experiments\trial1.tsv -SourcePath D:\MyApp\ArrowInput\seed_pinyin.tsv
```

Create an isolated experiment dictionary from the seed dictionary and switch to
it immediately:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\new_experiment_dictionary.ps1 -Name trial1 -UseNow
```

Switch to an existing experiment by name:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\use_experiment_dictionary.ps1 -Name trial1
```

After editing an experiment TSV, validate and rebuild its SQLite DB:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\rebuild_experiment_dictionary.ps1 -Name trial1 -UseNow
```

Backup and restore an experiment dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\backup_experiment_dictionary.ps1 -Name trial1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\list_experiment_backups.ps1 -Name trial1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\restore_experiment_dictionary.ps1 -Name trial1 -UseNow
```

Import an external text dictionary into an experiment. This converts the source
when needed, validates it, previews the merge, backs up the experiment, merges,
rebuilds SQLite, and optionally switches to the rebuilt DB:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\import_external_dictionary_to_experiment.ps1 -Name trial1 -SourcePath D:\path\to\source.txt -Delimiter space -UseNow
```

Add `-PreviewOnly` to stop after validation and merge preview. Use
`-SourceFormat rime` to import a Rime dictionary directly. Add
`-CreateIfMissing` to create the experiment from `seed_pinyin.tsv` before
importing when it does not exist yet. Add `-PromoteSourceTsv` to lift preferred
entries, such as seed candidates, above imported candidates with the same code:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\import_external_dictionary_to_experiment.ps1 -Name trial1 -SourcePath D:\path\to\luna_pinyin.dict.yaml -SourceFormat rime -CreateIfMissing -PromoteSourceTsv D:\MyApp\ArrowInput\seed_pinyin.tsv -UseNow
```

## Tests

Run all current checks:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_all.ps1 -Configuration Debug
```

Run only the algorithm probe regression test:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_algorithm_probe.ps1 -Configuration Debug
```

Build a draft SQLite dictionary from TSV:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\import_tsv_to_sqlite.ps1 -InputPath tools\AlgorithmProbe\sample_dictionary.tsv -OutputPath build\tools\AlgorithmProbe\sample_dictionary.db
```

`deploy.ps1` also regenerates the deployed sample SQLite dictionary from the
sample TSV before copying it to `D:\MyApp\ArrowInput`. If `AlgorithmProbe.exe`
has been built, it is also copied to the deploy root and can use the deployed
`sqlite3.dll` directly:

```powershell
D:\MyApp\ArrowInput\AlgorithmProbe.exe --type sqlite --dict D:\MyApp\ArrowInput\experiments\rime_luna.db --limit 20 yi nih
```

Run the SQLite import smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_sqlite_import.ps1
```

Query a generated SQLite dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\query_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db ni hao
```

Inspect SQLite dictionary statistics and the top ranked candidates for a code:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Limit 5 -Metadata ni hao
```

Inspect prefix lookup as the running input method sees it:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Limit 5 nih
```

Benchmark exact or prefix lookup latency:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Limit 30 -Iterations 100 ni shi yi
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Limit 30 -Iterations 100 nih niha
```

Add `-Explain` to include SQLite query plans and confirm index usage:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Explain nih
```

Find high-conflict codes with many candidates:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -TopCodes 20
```

Query the deployed sample SQLite dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\query_sqlite.ps1 ni hao
```

Inspect the current Rime experiment dictionary:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\inspect_sqlite_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db -Limit 10 ni nihao zhongguo
```

An experimental seed dictionary is also deployed as `seed_pinyin.tsv` and
`seed_pinyin.db`. Query it with:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\query_sqlite.ps1 nihao shurufa -DatabasePath D:\MyApp\ArrowInput\seed_pinyin.db
```

Use the generated database from ArrowInput by setting:

```ini
dictionary_type=sqlite
dictionary_path=build\tools\AlgorithmProbe\sample_dictionary.db
```

For the deployed test copy, switch the runtime user config with:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type sqlite
```

To test the experimental seed database instead of the tiny sample database:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type sqlite -Path D:\MyApp\ArrowInput\seed_pinyin.db
```

Switch back to TSV with:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type tsv
```

`set_dictionary.ps1` verifies the target dictionary path by default. For
intentional broken-path testing, pass `-AllowMissing`.

Check the deployed files, pending binaries, current user config, and recent
runtime log with:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\status.ps1
```

After signing out or otherwise unloading ArrowInput, apply pending DLL updates
with:

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\apply_pending.ps1
```
