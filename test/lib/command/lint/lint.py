"""Code Linter.

Scans every file in the repository for content that could hide code from review and checks that StringId macros encode what they
claim to."""

####################################################################################################################################
import os

from command.lint.ascii import lint_ascii
from command.lint.string_id import lint_str_id
from common.error import TestError
from common.log import *
from common.storage import path_list_recurse

# Files that are exempt from the content checks. A binary file is an unscannable place to hide content, so each entry must be a
# deliberate, reviewable decision.
_FILE_SKIP_LIST = (
    "doc/resource/logo.png",  # Project logo
    "test/data/filecopy.table.bin",  # Binary test fixture
    "doc/resource/git-history.cache",  # Generated from git history, contains non-ASCII author names
)


####################################################################################################################################
def _lint_str_id_apply(name):
    """Should the StringId check be applied to this file?

    It applies to C source, including hand-written .c.inc, but not to generated or vendored includes."""

    if name.endswith(".c") or name.endswith(".h"):
        return True

    return name.endswith(".c.inc") and not name.endswith(".auto.c.inc") and not name.endswith(".vendor.c.inc")


####################################################################################################################################
def cmd_lint(path_repo):
    """Lint every file in the repository."""

    for name in path_list_recurse(path_repo):
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
        # Otherwise the file must be 7-bit ASCII text, with the StringId check applied to C source
        else:
            error_total = lint_ascii(data)

            if _lint_str_id_apply(name):
                error_total += lint_str_id(data.decode("utf-8", errors="replace"))

        if error_total > 0:
            raise TestError("%u linter error(s) in '%s' (see warnings above)" % (error_total, name))
