## Switching language

The interface is available in **English** and **Italian**. Change it in **Settings → Language** (the row is labelled *Lingua · Language* so you can find it in either language). The player restarts and comes back where it was.

A new player starts in English (`UI_LANG_DEFAULT` in `src/config.h`). A player updated from a version without translations keeps Italian.

The web page is in Italian only for now.

## How translations work

In the source code interface strings are written in Italian and wrapped in `TR()`:

```cpp
ui_toast(TR("Aggiunto alla coda"));
snprintf(buf, sizeof buf, TR("%u brani \xE2\x80\xA2 %s"), n, dur);
```

`TR()` hashes the Italian text **at compile time** (FNV-1a) and, when the language is English, looks the hash up in a sorted table with a binary search. If a string has no translation, the Italian text is shown. There is no runtime cost in Italian and the lookup in English is a few comparisons.

The English table, `src/i18n_en.h`, is generated from a plain text file:

```
tools/i18n/en.tsv     one line per string: italian<TAB>english
tools/i18n/i18n.py    keeps en.tsv in sync with the sources and generates src/i18n_en.h
```

Lines in `en.tsv` use the same C escape sequences as the sources (`\n`, `\"`, `\xE2\x80\xA2` for •).

## Adding or changing a string

1. Write the Italian text in the code inside `TR("...")`.
2. Run `python3 tools/i18n/i18n.py`. New strings are added at the end of `en.tsv` with an empty translation, and strings no longer used are removed.
3. Fill in the English column in `en.tsv`.
4. Run `python3 tools/i18n/i18n.py` again to regenerate `src/i18n_en.h`, then build.

`python3 tools/i18n/i18n.py --check` lists missing translations and exits with an error if there are any, so it can run in CI.

Rules for translations:

- **Keep printf placeholders identical and in the same order** (`%u`, `%d`, `%s`, `%02d`…). The script refuses a translation whose placeholders differ.
- Numbers use the English decimal point (`%d.%02d V` instead of `%d,%02d V`).
- The screen is 240 px wide: keep labels about as long as the Italian ones. Check new text in the interface; long lines are cut with "…".
- Singular and plural are separate strings in the code (`1 brano` / `%u brani`), so translate each one.

`TR()` only works on string literals. For text that comes from a table at runtime, use `i18n_tr_dyn(text)`.

## Adding another language

The fonts cover Latin-1 and Latin Extended-A, which is enough for most European languages. Adding a third language needs a few small changes:

- a new value in `enum Lang` in `src/i18n.h`;
- a second table (for example `tools/i18n/de.tsv` → `src/i18n_de.h`): `i18n.py` currently handles `en.tsv` only;
- picking the table in `i18n_tr()` (`src/i18n.cpp`);
- the language row in `src/ui/page_settings.cpp`, which today toggles between two languages;
- the date format in `src/clock.cpp`.

Languages that need other scripts (Cyrillic, Greek, CJK…) also need new font ranges in `tools/gen_fonts.sh`.
