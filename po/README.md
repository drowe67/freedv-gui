# Translating FreeDV

FreeDV uses [GNU gettext](https://www.gnu.org/software/gettext/) via wxWidgets for translations.
`freedv.pot` is the template containing every translatable string in the application.

## Starting a new translation

1. Create a `.po` file for your language from the template, e.g. for Brazilian Portuguese:

   ```
   msginit --locale=pt_BR --input=freedv.pot --output-file=pt_BR.po
   ```

2. Translate the strings in `pt_BR.po` using a text editor or a tool such as [Poedit](https://poedit.net/).
3. Add the language code (e.g. `pt_BR`) on its own line in `LINGUAS`.
4. Rebuild FreeDV. The translation will be used automatically when your operating system's
   language is set to that language.

Notes for translators:

* Keep format specifiers such as `%s`, `%d` and `%3.1f` in your translation, in the same order.
  The build will fail if they don't match.
* `&` marks the keyboard shortcut for a menu item or button (e.g. `&File` = Alt+F). Keep one `&`
  in the translation, before a letter that isn't already used in the same menu or window.
* Keep any leading or trailing spaces and `\n` line breaks.
* Some labels are abbreviated to fit a small space (e.g. `Errs:`, `ClkOff:`). Please keep
  translations of these about the same length.
* Comments beginning with `TRANSLATORS:` give extra context for some strings.

## Updating after source changes (developers)

```
make update-pot   # regenerate freedv.pot from the source code
make update-po    # merge new/changed strings into each <lang>.po
```

Mark user-visible strings in the source with `_("...")`. Use `wxPLURAL()` for strings that
depend on a number, and `wxTRANSLATE()` + `wxGetTranslation()` for strings in static arrays.
Don't mark config keys, protocol values, mode names (RADEV1, 700D, etc.) or log messages.
