"""Test Code Linter.

The linter is made of the scan for content that could hide code, the check that StringId macros encode what they claim to, and the
check that every test module is declared, so all of them are tested here along with the command that walks the repository and
applies them."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stdout

from harness.test import *

from command.lint.ascii import *
from command.lint.lint import *
from command.lint.string_id import *
from common.error import *
from common.log import *

# A test definition with no test modules at all, which is what a repository has to have before the linter can scan it
_DEFINE_NONE = b"unit: []\nintegration: []\nperformance: []\ntool: []\n"

# A test definition with a module in each language, since the file a module lives in is named differently for each
_DEFINE_MODULE = b"""
unit:
  - name: common/stack-trace
    total: 1

integration: []
performance: []

tool:
  - name: test/common/vm

    coverage:
      - test/common/vm
"""


####################################################################################################################################
def _capture(function):
    """Run a function and return what it returned along with what it reported.

    Timestamps are suppressed so the report is exact and the log settings are put back afterwards."""

    output = io.StringIO()

    log_init(WARN, False)

    try:
        with redirect_stdout(output):
            result = function()
    finally:
        log_init(INFO, True)

    return result, output.getvalue()


####################################################################################################################################
def _lint(file_map, symlink=False):
    """Lint a repository built from the files given, returning what was reported and the error raised, if any."""

    # Every repository has a test definition, so supply one with no modules unless the test is providing its own
    file_map = {"test/define.yaml": _DEFINE_NONE, **file_map}

    with tempfile.TemporaryDirectory() as path:
        for name, content in file_map.items():
            os.makedirs(os.path.dirname(os.path.join(path, name)), exist_ok=True)

            with open(os.path.join(path, name), "wb") as file:
                file.write(content)

        # A link to a file that is not there, which is the readable case that can be provoked without changing permissions
        if symlink:
            os.symlink("missing.txt", os.path.join(path, "dangling.txt"))

        def run():
            try:
                cmd_lint(path)
            except ToolError as error:
                return str(error)

            return None

        return _capture(run)


####################################################################################################################################
def test_lint_ascii():
    """A character outside 7-bit ASCII is reported with the line it is on."""

    # Nothing to report for source that is already 7-bit ASCII
    assert_equal(_capture(lambda: lint_ascii(b"clean text\twith a tab\n")), (0, ""))

    # A right single quote, which is what a word processor turns an apostrophe into. It is written as bytes since the source this
    # test lives in must itself be 7-bit ASCII.
    result, output = _capture(lambda: lint_ascii(b"line one\nit\xe2\x80\x99s\n"))

    assert_equal(result, 1)
    assert_in("line 2 contains disallowed character U+2019", output)

    # Every shape the decoder reports on, i.e. four byte, two byte, a byte that starts no sequence at all, and a sequence cut short
    # by the end of the file
    result, output = _capture(lambda: lint_ascii(b"\xf0\x9f\x98\x80\n\xc3\xa9\n\x80\n\xe2\x80"))

    assert_equal(result, 4)
    assert_in("line 1 contains disallowed character U+1F600", output)
    assert_in("line 2 contains disallowed character U+00E9", output)
    assert_in("line 3 contains disallowed character U+0080", output)
    assert_in("line 4 contains disallowed character U+0080", output)


####################################################################################################################################
def test_lint_str_id_valid():
    """A macro that encodes what it claims to is not reported."""

    result, output = _capture(
        lambda: lint_str_id(
            '#define ANY STRID5("any", 0x65c10)\n'
            '#define LZ4 STRID6("lz4", 0x2068c1)\n'
            '#define ASC STRID5S("asc", 1, 0xe614)\n'
            '#define TIME STRID5S("time", 6, 0x56a680e)\n'
        )
    )

    assert_equal(result, 0)
    assert_equal(output, "")

    # Source with no macros at all has nothing to check
    assert_equal(_capture(lambda: lint_str_id("int main(void) { return 0; }\n")), (0, ""))


####################################################################################################################################
def test_lint_str_id_skip():
    """What is not an encoded string is skipped."""

    # The macro definitions themselves, which name their parameters
    result, output = _capture(
        lambda: lint_str_id(
            "#define STRID5(str, strId)                                          strId\n"
            "#define STRID5S(str, seq, strId)                                    strId\n"
            "#define STRID6(str, strId)                                          strId\n"
            "#define STRID6S(str, seq, strId)                                    strId\n"
        )
    )

    assert_equal(result, 0)
    assert_equal(output, "")

    # A quoted string inside a string, i.e. a macro in a test that is checking the macro
    assert_equal(_capture(lambda: lint_str_id('TEST_RESULT_Z(text, "STRID5(\\"any\\", 0x1)", "render");\n')), (0, ""))

    # A value that is itself defined by the test
    assert_equal(_capture(lambda: lint_str_id('TEST_RESULT_UINT(value, STRID5("any", TEST_VALUE), "encode");\n')), (0, ""))


####################################################################################################################################
def test_lint_str_id_error():
    """A macro that does not encode what it claims to is reported, and every macro is checked rather than only the first."""

    # The value does not match the string
    result, output = _capture(lambda: lint_str_id('#define ANY STRID5("any", 0x1)\n'))

    assert_equal(result, 1)
    assert_in("""'STRID5("any", 0x1)' should be 'STRID5("any", 0x65c10)'""", output)

    # The sequence is part of what is encoded, so a value from another sequence does not match
    result, output = _capture(lambda: lint_str_id('#define ASC STRID5S("asc", 2, 0xe614)\n'))

    assert_equal(result, 1)
    assert_in("""'STRID5S("asc", 2, 0xe614)' should be 'STRID5S("asc", 2, 0xe616)'""", output)

    # The string parameter must be quoted, which catches a value used where a string belongs
    result, output = _capture(lambda: lint_str_id("#define ANY STRID5(any, 0x65c10)\n"))

    assert_equal(result, 1)
    assert_in("""'STRID5(any, 0x65c10)' must have quotes around string parameter 'any'""", output)

    # A string the encoding cannot hold, reported with what was wrong with it
    result, output = _capture(lambda: lint_str_id('#define BAD STRID5("under_score", 0x1)\n'))

    assert_equal(result, 1)
    assert_in("""'STRID5("under_score", 0x1)' is not valid: 'under_score' contains invalid characters""", output)

    # A macro with no value at all, which is a single parameter rather than a missing one
    result, output = _capture(lambda: lint_str_id('#define ANY STRID5("any")\n'))

    assert_equal(result, 1)
    assert_in("""'STRID5("any")' should be 'STRID5("any", 0x65c10)'""", output)

    # Every macro is checked, so the count is what the file needs fixed rather than what stopped the scan
    result, output = _capture(lambda: lint_str_id('#define ANY STRID5("any", 0x1)\n#define LZ4 STRID6("lz4", 0x2)\n'))

    assert_equal(result, 2)
    assert_in("""'STRID5("any", 0x1)' should be""", output)
    assert_in("""'STRID6("lz4", 0x2)' should be""", output)


####################################################################################################################################
def test_lint_clean():
    """A repository with nothing to report passes silently."""

    error, output = _lint(
        {
            "README.md": b"clean text\twith a tab\n",
            "src/x.c": b'#define ANY STRID5("any", 0x65c10)\n',
            "src/x.h": b'#define LZ4 STRID6("lz4", 0x2068c1)\n',
            "src/x.c.inc": b'#define ASC STRID5S("asc", 1, 0xe614)\n',
            # Generated and vendored includes are not ours to fix, so the StringId check is not applied to them
            "src/x.auto.c.inc": b'#define ANY STRID5("any", 0x1)\n',
            "src/x.vendor.c.inc": b'#define ANY STRID5("any", 0x1)\n',
            # A binary file that is on the skip list, which is a deliberate and reviewable decision
            "doc/resource/card.png": b"\x00binary\n",
        },
        symlink=True,
    )

    assert_is_none(error)
    assert_equal(output, "")


####################################################################################################################################
def test_lint_binary():
    """A binary file that is not on the skip list is an unscannable place to hide code."""

    error, output = _lint({"stray.bin": b"\x00\x01\x02"})

    assert_equal(error, "1 linter error(s) in 'stray.bin' (see warnings above)")
    assert_in("unexpected binary file", output)


####################################################################################################################################
def test_lint_error():
    """Both checks are applied to C source and their errors are counted together."""

    error, output = _lint({"doc/x.md": b"line one\nit\xe2\x80\x99s\n"})

    assert_equal(error, "1 linter error(s) in 'doc/x.md' (see warnings above)")
    assert_in("line 2 contains disallowed character U+2019", output)

    error, output = _lint({"src/x.c": b'#define ANY STRID5("any", 0x1)\n'})

    assert_equal(error, "1 linter error(s) in 'src/x.c' (see warnings above)")
    assert_in("""should be 'STRID5("any", 0x65c10)'""", output)

    # A file with both kinds of error reports what it needs fixed rather than what stopped the scan
    error, output = _lint({"src/x.c": b'#define ANY STRID5("any", 0x1) // it\xe2\x80\x99s wrong\n'})

    assert_equal(error, "2 linter error(s) in 'src/x.c' (see warnings above)")
    assert_in("U+2019", output)
    assert_in("should be", output)


####################################################################################################################################
def test_lint_lib_shadow():
    """A module may appear in only one library, since a duplicate would hide the shadowed one from every tool."""

    # The same module in two libraries, which python would resolve to whichever library came first on the path
    error, output = _lint({"build/lib/common/log.py": b"", "test/lib/common/log.py": b""})

    assert_equal(error, "module 'common/log.py' is in the build and test libraries")

    # A library is the second component of the path, so a lib further down is not one
    error, output = _lint({"test/src/lib/common/log.py": b"", "test/lib/common/log.py": b""})

    assert_is_none(error)

    # A module in one library and a different module in another
    error, output = _lint({"build/lib/common/log.py": b"", "test/lib/common/vm.py": b""})

    assert_is_none(error)

    # Something under a library that is not a module at all
    error, output = _lint({"build/lib/common/log.py": b"", "test/lib/uncrustify.cfg": b""})

    assert_is_none(error)


####################################################################################################################################
def test_lint_test_module():
    """A test module must be declared in define.yaml, since one that is not declared is never run."""

    # A C module is named in camel case and a python module exactly as it is declared, so both are found where they live
    file_map = {
        "test/define.yaml": _DEFINE_MODULE,
        "test/src/module/common/stackTraceTest.c": b"",
        "test/src/module/test/common/vm_test.py": b"",
    }

    error, output = _lint(file_map)

    assert_is_none(error)
    assert_equal(output, "")

    # A test module that was added but never declared, which is the case that has no other way to be reported
    error, output = _lint({**file_map, "test/src/module/common/type/cTest.c": b""})

    assert_equal(error, "test module 'test/src/module/common/type/cTest.c' is not defined in test/define.yaml")

    # A file that is not where the test modules live is not a test module
    error, output = _lint({**file_map, "test/src/harness/config.c": b""})

    assert_is_none(error)
