#!/usr/bin/env python3
"""Stops counting branches that the compiler added rather than the code.

    coverage_exclusions.py <gcovr --json output> <where to write the result>

gcovr can leave out the branches on lines matching a pattern, which takes
care of those that can be recognised from the line itself (see the Linux
workflow). Those found here need the lines around them to recognise, or
would make for a pattern nobody could read:

* A line of nothing but brackets and punctuation ("});"), where what's
  being closed is cleaned up: the end of a lambda, an initializer list or a
  call spread over several lines.
* "delete something;", which checks for null by itself.
* A line that carries on a statement begun above it (the arguments of a call
  spread over several lines) without a condition of its own. Its branches
  are for cleaning up the temporaries made so far if making the next fails.

No test can take the other side of any of these. A line with a condition in
it (if, a loop, &&, ||, ?:, a comparison) is always left alone.

Run from the folder that the paths in the file are relative to (the top of
the source tree).
"""
import json
import re
import sys

CONDITION = re.compile(r'\b(if|while|for|switch|case|else|default|catch)\b|&&|\|\||\?|<=|>=|==|!=')
PUNCTUATION_ONLY = re.compile(r'^[\]\)\}\{;,]+$')
PLAIN_DELETE = re.compile(r'^delete(\s*\[\])?\s+[\w\.\->\[\]:]+;$')
COMMENT = re.compile(r'//.*$')


def code(line):
    return COMMENT.sub('', line).strip()


def compiler_added(source, number):
    """Whether the branches of a line (numbered from 1) are the compiler's."""
    text = code(source[number - 1])
    if CONDITION.search(text):
        return False
    if PUNCTUATION_ONLY.match(text) or PLAIN_DELETE.match(text):
        return True

    # Carrying on from a line that ended part way through a call?
    previous = number - 2
    while previous >= 0 and not code(source[previous]):
        previous -= 1
    return previous >= 0 and code(source[previous])[-1] in ',('


def main():
    with open(sys.argv[1]) as fh:
        coverage = json.load(fh)

    excluded = 0
    for entry in coverage['files']:
        try:
            with open(entry['file'], errors='replace') as fh:
                source = fh.read().split('\n')
        except OSError:
            continue
        for line in entry['lines']:
            branches = [b for b in line.get('branches', []) if not b.get('gcovr/excluded')]
            if branches and line['line_number'] <= len(source) and compiler_added(source, line['line_number']):
                for branch in branches:
                    branch['gcovr/excluded'] = True
                excluded += len(branches)

    with open(sys.argv[2], 'w') as fh:
        json.dump(coverage, fh)
    print(f'{excluded} compiler-added branches no longer counted')


if __name__ == '__main__':
    main()
