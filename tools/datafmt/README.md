# tools/datafmt: data-format survey scripts

Scratch-quality scripts from the 2026-11-04 survey behind docs/DATA_FORMATS.md (and docs/MODDING_PLAN.md). Not part
of the build. They read an extracted data.ssa (never committed: game data):

- `ssa.py` / `blast.py` / `extract.py`: read the .ssa archive and inflate its PK01 (PKWARE DCL) entries.
- `structs.py`: record layouts parsed from the sources -> `structs.json` (sizes match the static_asserts).
- `analyze.py <db dir>`: parse every db table with those layouts and print per-field statistics;
  `gen.py`/`assemble.py` build the DATA_FORMATS.md tables from the results.

The converter proposed in docs/DATA_FORMATS.md ("Recommendation") is tools/eedata (section 8 there); these
scripts are kept as the survey's record.
