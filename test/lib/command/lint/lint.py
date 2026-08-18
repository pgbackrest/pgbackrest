"""Code Linter.

Scans every file in the repository for content that could hide code from review, checks that no line runs past the project line
length, checks that StringId macros encode what they claim to, checks that a block macro is closed by the macro that matches it, and
checks that every test module is declared in define.yaml.

Everything found is reported as a warning and the number of them is returned rather than raised, so the run continues. A linter
reads the source as it is written, which means a mistake in one place shows up wherever the source stopped making sense to it: a
misspelled block macro is skipped rather than opening a block, so the next closer is reported instead, hundreds of lines from the
line that needs fixing. The compiler places the same mistake exactly, so let the run reach it. Whatever runs the linter is
responsible for failing once it has."""

####################################################################################################################################
import os
import re

from command.lint.ascii import lint_ascii
from command.lint.line import lint_line
from command.lint.macro import lint_macro
from command.lint.string_id import lint_str_id
from command.test.define import TEST_MODULE_PATH, test_def_file, test_def_parse
from common.log import *
from common.storage import path_list_recurse

# Files that are exempt from the content checks. A binary file is an unscannable place to hide content, so each entry must be a
# deliberate, reviewable decision.
_FILE_SKIP_LIST = (
    "doc/resource/card.png",  # Card shown where a link to the documentation is posted
    "test/data/filecopy.table.bin",  # Binary test fixture
    "doc/resource/git-history.cache",  # Generated from git history, contains non-ASCII author names
)

# Paths that are exempt from the line length check, matched against the start of the name so an entry that ends in a slash exempts
# everything under it. The check is excludes rather than includes so a file that is added is checked without anyone remembering to
# add it, which is the way lines got long in the first place.
_LINE_SKIP_LIST = (
    "doc/manifest.xml",  # Documentation variables, which are a value per line
    "doc/resource/",  # Images and caches rather than source
    "doc/xml/",  # Documentation, which the coding standards exempt
    "test/certificate/",  # Generated certificates and keys
    "test/data/",  # Test fixtures
    "test/uncrustify.cfg",  # Written by uncrustify rather than by hand
)

# Extensions that are exempt from the line length check
_LINE_SKIP_EXT = (
    ".md",  # Markdown, written a paragraph per line so the editor wraps it and a diff is per paragraph
    ".vendor.h",  # Vendored source, which is kept as it came so it can be updated from upstream
    ".vendor.c.inc",
)

# A python module in a tool library, e.g. build/lib/common/render.py
_LIB_MODULE_EXP = re.compile(r"^([^/]+)/lib/(.+\.py)$")


####################################################################################################################################
def _lint_line_apply(name):
    """Should the line length check be applied to this file?"""

    return not name.startswith(_LINE_SKIP_LIST) and not name.endswith(_LINE_SKIP_EXT)


####################################################################################################################################
def _lint_c_apply(name):
    """Should the checks that read C source be applied to this file?

    They apply to C source, including hand-written .c.inc, but not to generated or vendored includes. Generated source cannot drift
    from what generated it and vendored source is kept as it came, so neither is ours to fix."""

    if name.endswith(".c") or name.endswith(".h"):
        return True

    return name.endswith(".c.inc") and not name.endswith(".auto.c.inc") and not name.endswith(".vendor.c.inc")


####################################################################################################################################
def cmd_lint(path_repo):
    """Lint every file in the repository and return the number of errors found."""

    result = 0
    lib_module = {}

    # File each test module declared in define.yaml lives in. A test module that is not declared is never built or run, and nothing
    # else reports it since the file is simply never read.
    test_module = {test_def_file(module) for module in test_def_parse(path_repo)}

    for name in path_list_recurse(path_repo):
        # Everything where the test modules live is a test module, so it must be declared in define.yaml
        if name.startswith(TEST_MODULE_PATH) and name not in test_module:
            log(WARN, "test module '%s' is not defined in test/define.yaml" % name)
            result += 1

        # A module may appear in only one library. Python resolves a module name to the first library on the path that has it and
        # ignores the rest, so a duplicate would hide the shadowed module from every tool with no indication that it had.
        match = _LIB_MODULE_EXP.match(name)

        if match is not None:
            lib, module = match.groups()

            if module in lib_module:
                log(WARN, "module '%s' is in the %s and %s libraries" % (module, lib_module[module], lib))
                result += 1

            lib_module[module] = lib

        # Skip files that are exempt from the content checks
        if name in _FILE_SKIP_LIST:
            continue

        # Read the file, skipping any that cannot be read. Committed files are always readable after checkout, so this only occurs
        # from local filesystem state, and readability is enforced by the build rather than the linter.
        try:
            with open(os.path.join(path_repo, name), "rb") as file:
                data = file.read()
        except OSError:
            continue

        # A binary file cannot be scanned for hidden content, so it is not allowed unless added to the skip list
        if b"\x00" in data:
            log(WARN, "unexpected binary file (add to the linter skip list if intentional)")
            error_total = 1
        # Otherwise the file must be 7-bit ASCII text, with the line length check applied to everything that is not exempt and the
        # StringId and block macro checks applied to C source
        else:
            error_ascii = lint_ascii(data)
            error_total = error_ascii
            source = data.decode("utf-8", errors="replace")

            if _lint_line_apply(name):
                error_total += lint_line(source)

            if _lint_c_apply(name):
                error_total += lint_str_id(source)

                # The block macro check reads every character in the file rather than scanning for a pattern, so it is applied only
                # to source that is ASCII. What it would report about a character it cannot read is what the ASCII check above has
                # already reported, in worse words.
                if error_ascii == 0:
                    error_total += lint_macro(source)

        # Name the file the warnings above are for, since they report the line they are on and nothing else
        if error_total > 0:
            log(WARN, "%u linter error(s) in '%s'" % (error_total, name))
            result += error_total

    return result
