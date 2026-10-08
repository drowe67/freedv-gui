# Translating FreeDV

FreeDV uses [GNU gettext](https://www.gnu.org/software/gettext/) via wxWidgets for translations.
`freedv.pot` is the template containing every translatable string in the application.

## Starting a new translation

1. Download all the source code to the src/ folder and execute the following command.

   ```
   msginit --locale=pt_BR --input=freedv.pot --output-file=pt_BR.po
   ```

   Where `pt_BR.po` is the name of the language you're translating to (typically a [ISO 639 language code](https://en.wikipedia.org/wiki/List_of_ISO_639_language_codes) and optionally a [ISO 3166 country code](https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes)).
   Remember that this command works for all languages you want to translate to; English will currently always be the base language.

2. A .po file will be created, then you can follow the rest of the guide normally.
3. Translate the strings in `pt_BR.po` using a text editor or a tool such as [Poedit](https://poedit.net/).
4. Add the language code (e.g. `pt_BR`) on its own line in `LINGUAS`.
5. Rebuild FreeDV. The translation will be used automatically when your operating system's
   language is set to that language.
6. To force FreeDV to execute in a given language for testing (replace `pt_BR` as appropriate):
   a. macOS: `./src/FreeDV.app/Contents/MacOS/FreeDV -AppleLanguages '(pt_BR)'`
   b. Linux: `LANG=pt_BR src/freedv`
   c. Windows: [Locale Emulator](https://github.com/xupefei/Locale-Emulator) may be of help (note: project is no longer being developed)

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

# The below may also be handy for catching strings improperly marked as "do not translate"
xgettext --from-code=UTF-8 --keyword=_ --keyword=wxTRANSLATE --keyword=wxT -o freedv.po $(find src -name "*.cpp" -o -name "*.h")
```

Mark user-visible strings in the source with `_("...")`. Use `wxPLURAL()` for strings that
depend on a number, and `wxTRANSLATE()` + `wxGetTranslation()` for strings in static arrays.
Don't mark config keys, protocol values, mode names (RADEV1, 700D, etc.) or log messages.
