#!/usr/bin/env python3
"""Turns gcovr's JSON summary of a coverage build (WITH_COVERAGE=1
./build_linux.sh, then the tests run) into a Markdown summary for the GitHub
Actions job summary page, laid out like freedv-backend's.

Usage:
  coverage_summary.py <gcovr JSON summary> [<name of the HTML report's artifact>]

The Markdown goes to $GITHUB_STEP_SUMMARY if set, otherwise stdout.

Code from elsewhere, the tests themselves and the GUI test agent are left out
of the headline figures; what all of src/ comes to with them is given as well.
"""
import json
import os
import platform
import re
import sys

EXCLUDE = r'^src/sox/|kiss_fftr?\.[ch]$|^src/audio/rtkit\.[ch]$|^src/test_agent\.(cpp|h)$|/test/'
KINDS = ('line', 'function', 'branch')


def totals(files):
    return {kind: {'count': sum(f[kind + '_total'] for f in files),
                   'covered': sum(f[kind + '_covered'] for f in files)} for kind in KINDS}


def cell(summary):
    if not summary['count']:
        return '–'
    return f"{100.0 * summary['covered'] / summary['count']:.1f}% ({summary['covered']}/{summary['count']})"


def file_cell(f, kind):
    return cell({'count': f[kind + '_total'], 'covered': f[kind + '_covered']})


def main():
    summary_file = sys.argv[1]
    artifact = sys.argv[2] if len(sys.argv) > 2 else 'coverage-html'
    everything = sorted(json.load(open(summary_file))['files'], key=lambda f: f['filename'])
    files = [f for f in everything if not re.search(EXCLUDE, f['filename'])]
    own = totals(files)
    whole = totals(everything)

    out = []
    out.append('## FreeDV GUI code coverage')
    out.append('')
    out.append(f'{platform.system()}, gcov, unit and GUI tests. Third-party code, the tests themselves and the '
               'GUI test agent are excluded, as are the branches that compilers add for exceptions, object '
               'construction, cleaning up and asserts. The '
               f'line-by-line report is in the `{artifact}` artifact of this run.')
    out.append('')
    out.append('| | Covered |')
    out.append('|---|---|')
    out.append(f"| Lines | {cell(own['line'])} |")
    out.append(f"| Functions | {cell(own['function'])} |")
    out.append(f"| Branches | {cell(own['branch'])} |")
    out.append(f'| Files | {len(files)} measured, {len(everything) - len(files)} excluded |')
    out.append('')
    out.append(f"**All of `src/`, including what's excluded above:** lines {cell(whole['line'])}, "
               f"functions {cell(whole['function'])}, branches {cell(whole['branch'])}.")
    out.append('')
    out.append('<details><summary>Coverage by file</summary>')
    out.append('')
    out.append('| File | Lines | Functions | Branches |')
    out.append('|---|---|---|---|')
    for f in files:
        name = f['filename']
        if name.startswith('src/'):
            name = name[len('src/'):]
        out.append(f"| `{name}` | {file_cell(f, 'line')} | {file_cell(f, 'function')} | {file_cell(f, 'branch')} |")
    out.append('')
    out.append('</details>')
    out.append('')

    markdown = '\n'.join(out)
    summary_path = os.environ.get('GITHUB_STEP_SUMMARY')
    if summary_path:
        with open(summary_path, 'a') as fh:
            fh.write(markdown)
    else:
        print(markdown)


if __name__ == '__main__':
    main()
