"""Test Code Linter.

The linter is made of the scan for content that could hide code, the line length check, the check that StringId macros encode what
they claim to, the check that a block macro is closed by the macro that matches it, and the check that every test module is
declared, so all of them are tested here along with the lexer the block macro check reads C source with and the command that walks
the repository and applies them."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stdout

from harness.test import *

from command.lint.ascii import *
from command.lint.lex import *
from command.lint.line import *
from command.lint.lint import *
from command.lint.macro import *
from command.lint.string_id import *
from common.error import *
from common.log import *
from common.render import LINE_LENGTH

# A test definition with no test modules at all, which is what a repository has to have before the linter can scan it
_DEFINE_NONE = b"unit: []\nintegration: []\nperformance: []\ntool: []\n"

# A line that runs past the line length, written out rather than given literally since this file is itself checked
_LINE_LONG = b"x" * (LINE_LENGTH + 1) + b"\n"

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
    """Lint a repository built from the files given, returning the number of errors found and what was reported."""

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

        return _capture(lambda: cmd_lint(path))


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
def test_lint_line():
    """A line that runs past the line length is reported with the line it is on."""

    # A line exactly at the line length is not over it, and neither is a file that does not end in a linefeed
    assert_equal(_capture(lambda: lint_line("x" * LINE_LENGTH + "\n" + "y" * LINE_LENGTH)), (0, ""))

    # Every line is reported rather than only the first, so the file says what it needs fixed
    result, output = _capture(lambda: lint_line("short\n" + "x" * (LINE_LENGTH + 1) + "\n" + "y" * (LINE_LENGTH + 8) + "\n"))

    assert_equal(result, 2)
    assert_in("line 2 is 133 characters (maximum is 132)", output)
    assert_in("line 3 is 140 characters (maximum is 132)", output)


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
def test_lex():
    """C source is lexed into tokens with the line each one is on and whether it is part of a directive."""

    # Every token kind, with the space dropped since a rule reads code rather than layout
    assert_equal(
        [(token.kind, token.text) for token in lex("/* c */ x = \"s\" + 'c' + 0x1p-3; // done\n")],
        [
            ("comment", "/* c */"),
            ("identifier", "x"),
            ("punct", "="),
            ("string", '"s"'),
            ("punct", "+"),
            ("char", "'c'"),
            ("punct", "+"),
            ("number", "0x1p-3"),
            ("punct", ";"),
            ("comment", "// done"),
        ],
    )

    # A comment that spans lines does not put what follows it on the line it began on
    assert_equal(
        [(token.text, token.line) for token in lex("a\n/* two\nthree */ b\nc\n")],
        [("a", 1), ("/* two\nthree */", 2), ("b", 3), ("c", 4)],
    )

    # A directive runs to the end of the line the splice has joined its continuations to, and every token is reported on the line it
    # is on in the source as written rather than the line it ended up on after splicing
    assert_equal(
        [(token.text, token.line, token.directive) for token in lex("#define A \\\n    B\nc;\n")],
        [("#", 1, True), ("define", 1, True), ("A", 1, True), ("B", 2, True), ("c", 3, False), (";", 3, False)],
    )

    # A "#" that is not the first thing on a line does not begin a directive
    assert_equal([(token.text, token.directive) for token in lex("a # b\n")], [("a", False), ("#", False), ("b", False)])

    # Source with nothing in it has no tokens
    assert_equal(list(lex("")), [])


####################################################################################################################################
def test_lex_error():
    """A character that is not part of any token is reported with the line it is on."""

    with assert_raises(ToolError) as error:
        list(lex("int a = 1;\n@\n"))

    assert_equal(str(error.exception), "line 2: cannot lex '@'")

    # The line is the line in the source as written, so a splice before the character does not shift it, and only what is on the
    # line is reported since the rest of the file says nothing about what could not be read
    with assert_raises(ToolError) as error:
        list(lex("#define A \\\n    1\n@ and more than twenty characters\n"))

    assert_equal(str(error.exception), "line 3: cannot lex '@ and more than twen'")


####################################################################################################################################
def test_lint_macro_pair():
    """A block macro is closed by the macro that matches it."""

    # Nesting that is correct, including the openers that share a closer with another opener
    assert_equal(
        _capture(
            lambda: lint_macro(
                "FUNCTION_TEST_BEGIN();\n"
                "FUNCTION_TEST_END();\n"
                "MEM_CONTEXT_TEMP_RESET_BEGIN(2)\n"
                "{\n"
                "    OBJ_NEW_BASE_EXTRA_BEGIN(Type, 1)\n"
                "    {\n"
                "    }\n"
                "    OBJ_NEW_END();\n"
                "}\n"
                "MEM_CONTEXT_TEMP_END();\n"
            )
        ),
        (0, ""),
    )

    # A closer that is an alias for the base closer, which is the case that expands identically and builds clean
    result, output = _capture(lambda: lint_macro("MEM_CONTEXT_OBJ_BEGIN(this)\n{\n}\nMEM_CONTEXT_END();\n"))

    assert_equal(result, 1)
    assert_in(
        "line 4: MEM_CONTEXT_OBJ_BEGIN() opened on line 1 is closed with MEM_CONTEXT_END() rather than MEM_CONTEXT_OBJ_END()",
        output,
    )

    # A closer with nothing open and blocks that are never closed, which is what the compiler does report but only after the macros
    # have been expanded, i.e. as braces that do not balance rather than as the macro that is missing
    result, output = _capture(lambda: lint_macro("TRY_END();\nMEM_CONTEXT_BEGIN(x)\nMEM_CONTEXT_TEMP_BEGIN()\n"))

    assert_equal(result, 3)
    assert_in("line 1: TRY_END() closes a block that was never opened", output)
    assert_in("line 3: MEM_CONTEXT_TEMP_BEGIN() is never closed with MEM_CONTEXT_TEMP_END()", output)
    assert_in("line 2: MEM_CONTEXT_BEGIN() is never closed with MEM_CONTEXT_END()", output)

    # What is left open is reported innermost first, which is the block that needs closing
    assert_true(output.index("line 3:") < output.index("line 2:"))

    # A macro named in a comment, in a string, or where it is defined is not a use of it
    assert_equal(
        _capture(
            lambda: lint_macro(
                "// Closed with FUNCTION_TEST_END()\n"
                'const char *const text = "TRY_BEGIN";\n'
                "#define OBJ_NEW_BEGIN(type) MEM_CONTEXT_NEW_BEGIN(type)\n"
            )
        ),
        (0, ""),
    )

    # Source the lexer cannot read is one error and nothing more, since the check reads tokens
    result, output = _capture(lambda: lint_macro("int a = @;\n"))

    assert_equal(result, 1)
    assert_in("line 1: cannot lex '@;'", output)


####################################################################################################################################
def test_lint_macro_define():
    """A macro named like a block macro is classified as opening a block or as not opening one."""

    # An opener, a closer, and a macro that is named like one but opens nothing, all classified
    assert_equal(
        _capture(lambda: lint_macro("#define TRY_BEGIN() ...\n#define TRY_END() ...\n#define BENCHMARK_BEGIN() ...\n")),
        (0, ""),
    )

    # A macro that is named like neither, which is most of them
    assert_equal(_capture(lambda: lint_macro("#define ANY 1\n")), (0, ""))

    # A block macro added with a closer of its own, which the pairing check has no way to report: neither name is known, so both
    # are skipped and every block they open passes unchecked
    result, output = _capture(lambda: lint_macro("#define LOCK_BEGIN(x) \\\n    do {\n#define LOCK_END() \\\n    } while (0)\n"))

    assert_equal(result, 2)
    assert_in("line 1: LOCK_BEGIN() is not classified in test/lib/command/lint/macro.py", output)
    assert_in("line 3: LOCK_END() is not classified in test/lib/command/lint/macro.py", output)

    # A directive that is not a define, and a define with nothing after it, i.e. the file ends before the name
    assert_equal(_capture(lambda: lint_macro('#include "x.h"\n#define\n')), (0, ""))


####################################################################################################################################
def test_lint_clean():
    """A repository with nothing to report passes silently."""

    result, output = _lint(
        {
            "README.md": b"clean text\twith a tab\n" + _LINE_LONG,
            "src/x.c": b'#define ANY STRID5("any", 0x65c10)\nTRY_BEGIN()\n{\n}\nTRY_END();\n',
            "src/x.h": b'#define LZ4 STRID6("lz4", 0x2068c1)\n',
            "src/x.c.inc": b'#define ASC STRID5S("asc", 1, 0xe614)\n',
            # Generated and vendored includes are not ours to fix, so the checks that read C source are not applied to them
            "src/x.auto.c.inc": b'#define ANY STRID5("any", 0x1)\nMEM_CONTEXT_OBJ_BEGIN(this)\nMEM_CONTEXT_END();\n',
            "src/x.vendor.c.inc": b'#define ANY STRID5("any", 0x1)\n#define LOCK_BEGIN(x) do {\n' + _LINE_LONG,
            # Documentation is exempt from the line length check by path and markdown by extension, as is vendored source above
            "doc/xml/user-guide.xml": _LINE_LONG,
            # A binary file that is on the skip list, which is a deliberate and reviewable decision
            "doc/resource/card.png": b"\x00binary\n",
        },
        symlink=True,
    )

    assert_equal(result, 0)
    assert_equal(output, "")


####################################################################################################################################
def test_lint_binary():
    """A binary file that is not on the skip list is an unscannable place to hide code."""

    result, output = _lint({"stray.bin": b"\x00\x01\x02"})

    assert_equal(result, 1)
    assert_in("unexpected binary file", output)
    assert_in("1 linter error(s) in 'stray.bin'", output)


####################################################################################################################################
def test_lint_error():
    """Every check is applied to C source and their errors are counted together."""

    result, output = _lint({"doc/x.md": b"line one\nit\xe2\x80\x99s\n"})

    assert_equal(result, 1)
    assert_in("line 2 contains disallowed character U+2019", output)

    # The warnings report the line they are on and nothing else, so the file they are for is named after them
    assert_in("1 linter error(s) in 'doc/x.md'", output)

    result, output = _lint({"src/x.c": b'#define ANY STRID5("any", 0x1)\n'})

    assert_equal(result, 1)
    assert_in("""should be 'STRID5("any", 0x65c10)'""", output)

    result, output = _lint({"src/x.c": b"MEM_CONTEXT_OBJ_BEGIN(this)\n{\n}\nMEM_CONTEXT_END();\n"})

    assert_equal(result, 1)
    assert_in("is closed with MEM_CONTEXT_END() rather than MEM_CONTEXT_OBJ_END()", output)

    # The block macro check reads every character in the file, so it waits for the file to be ASCII rather than reporting the
    # character the ASCII check has already reported as one the lexer cannot read
    result, output = _lint({"src/x.c": b"TRY_BEGIN()\n// it\xe2\x80\x99s open\n"})

    assert_equal(result, 1)
    assert_in("line 2 contains disallowed character U+2019", output)
    assert_not_in("TRY_BEGIN", output)

    # A file that is not exempt from the line length check, which is everything the exempt list above does not name
    result, output = _lint({"src/x.h": _LINE_LONG})

    assert_equal(result, 1)
    assert_in("line 1 is 133 characters (maximum is 132)", output)

    # A file with both kinds of error reports what it needs fixed rather than what stopped the scan
    result, output = _lint({"src/x.c": b'#define ANY STRID5("any", 0x1) // it\xe2\x80\x99s wrong\n'})

    assert_equal(result, 2)
    assert_in("U+2019", output)
    assert_in("should be", output)

    # A file with an error does not stop the scan, since the run continues either way and the rest of the repository has as much
    # right to be reported as the first file that failed
    result, output = _lint({"src/a.c": _LINE_LONG, "src/b.c": _LINE_LONG})

    assert_equal(result, 2)
    assert_in("1 linter error(s) in 'src/a.c'", output)
    assert_in("1 linter error(s) in 'src/b.c'", output)


####################################################################################################################################
def test_lint_lib_shadow():
    """A module may appear in only one library, since a duplicate would hide the shadowed one from every tool."""

    # The same module in two libraries, which python would resolve to whichever library came first on the path
    result, output = _lint({"build/lib/common/log.py": b"", "test/lib/common/log.py": b""})

    assert_equal(result, 1)
    assert_in("module 'common/log.py' is in the build and test libraries", output)

    # A library is the second component of the path, so a lib further down is not one
    result, output = _lint({"test/src/lib/common/log.py": b"", "test/lib/common/log.py": b""})

    assert_equal((result, output), (0, ""))

    # A module in one library and a different module in another
    result, output = _lint({"build/lib/common/log.py": b"", "test/lib/common/vm.py": b""})

    assert_equal((result, output), (0, ""))

    # Something under a library that is not a module at all
    result, output = _lint({"build/lib/common/log.py": b"", "test/lib/uncrustify.cfg": b""})

    assert_equal((result, output), (0, ""))


####################################################################################################################################
def test_lint_test_module():
    """A test module must be declared in define.yaml, since one that is not declared is never run."""

    # A C module is named in camel case and a python module exactly as it is declared, so both are found where they live
    file_map = {
        "test/define.yaml": _DEFINE_MODULE,
        "test/src/module/common/stackTraceTest.c": b"",
        "test/src/module/test/common/vm_test.py": b"",
    }

    result, output = _lint(file_map)

    assert_equal(result, 0)
    assert_equal(output, "")

    # A test module that was added but never declared, which is the case that has no other way to be reported
    result, output = _lint({**file_map, "test/src/module/common/type/cTest.c": b""})

    assert_equal(result, 1)
    assert_in("test module 'test/src/module/common/type/cTest.c' is not defined in test/define.yaml", output)

    # A file that is not where the test modules live is not a test module
    result, output = _lint({**file_map, "test/src/harness/config.c": b""})

    assert_equal((result, output), (0, ""))
