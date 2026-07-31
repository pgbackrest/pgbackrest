"""Test Coverage Testing and Reporting."""

####################################################################################################################################
import io
import json
import os
import tempfile
from contextlib import redirect_stdout

from harness.test import *

from command.coverage.coverage import *
from command.coverage.coverage import Coverage, CoverageFile, CoverageFunction, CoverageLine
from command.test.define import TEST_LANG_C, TEST_LANG_PYTHON
from common.error import *
from common.log import *

# A define file with one C test and one python test, which is what the coverage command reads to know what should be covered
DEFINE = """
unit:
  - name: common/error

    coverage:
      - common/error
      - common/errorInternal: noCode

  - name: common/stack-trace

    coverage:
      - common/stackTrace

integration: []
performance: []

tool:
  - name: test/common/log

    coverage:
      - test/common/log
"""

# Source of the C module, which the report is rendered against
SOURCE_C = "/***/\nvoid\nfuncOne(void)\n{\n    doOne();\n}\n"

# Source of the python module
SOURCE_PY = "def func(a):\n    if a:\n        return 1\n\n    return 0\n"


####################################################################################################################################
class Config:
    """What the coverage command reads from the command line."""

    def __init__(self, repo_path, test_path, module, vm="none", coverage_summary=False):
        self.repo_path = repo_path
        self.test_path = test_path
        self.module = module
        self.vm = vm
        self.coverage_summary = coverage_summary


####################################################################################################################################
def _line(no, hit, branch_list=None, target_list=None):
    """Build a coverage line."""

    return CoverageLine(no, hit, branch_list, target_list)


####################################################################################################################################
def _file(name, line_list, lang=TEST_LANG_C, function_list=()):
    """Build a coverage file from the lines and functions given."""

    result = CoverageFile(name, lang)
    result.line_list = list(line_list)
    result.function_list = list(function_list)

    return result


####################################################################################################################################
def _coverage(file_list):
    """Build a coverage object from the files given."""

    result = Coverage()
    result.file_list = list(file_list)

    return result


####################################################################################################################################
def test_coverage_file_find():
    """A file is found by code module name and missing is reported by returning nothing."""

    coverage = _coverage([_file("src/common/error.c", []), _file("src/common/log.c", [])])

    assert_equal(coverage.file_find("src/common/log.c").name, "src/common/log.c")
    assert_is_none(coverage.file_find("src/common/bogus.c"))


####################################################################################################################################
def test_coverage_total():
    """Totals are counted over the lines and functions, and a file is covered only when every line and branch was."""

    file = _file(
        "src/common/error.c",
        [_line(5, 3), _line(7, 0), _line(9, 1, [3, 0])],
        function_list=[CoverageFunction("errorNew", 1, 10, 3), CoverageFunction("errorFree", 12, 20, 0)],
    )
    coverage = _coverage([file])

    coverage.total_calculate()

    assert_equal((file.line_hit, file.line_total), (2, 3))
    assert_equal((file.branch_hit, file.branch_total), (1, 2))
    assert_equal((file.function_hit, file.function_total), (1, 2))
    assert_false(file.covered())

    # A file with everything hit is covered, and so is one with nothing in it at all
    file = _file("src/common/log.c", [_line(5, 1), _line(7, 2, [1, 1])])
    coverage = _coverage([file, _file("src/common/empty.c", [])])

    coverage.total_calculate()

    assert_true(file.covered())
    assert_true(coverage.file_list[1].covered())


####################################################################################################################################
def test_coverage_covered_remove():
    """Only what is missing is left, since the report is about what still needs a test."""

    file = _file(
        "src/common/error.c",
        [_line(5, 3), _line(7, 0), _line(9, 1, [3, 0]), _line(11, 1, [1, 1]), _line(13, 0, [0, 0])],
    )
    coverage = _coverage([file])

    coverage.covered_remove()

    # A line that ran with every branch taken is gone; a line that never ran or has a branch that was never taken stays
    assert_equal([line.no for line in file.line_list], [7, 9, 13])


####################################################################################################################################
def test_coverage_merge_c():
    """C coverage is summed line by line and branch by branch, since both runs compiled the same source."""

    coverage = _coverage(
        [
            _file(
                "src/common/error.c",
                [_line(5, 1), _line(7, 0, [1, 0])],
                function_list=[CoverageFunction("errorNew", 1, 10, 1)],
            )
        ]
    )

    coverage.merge(
        _coverage(
            [
                _file(
                    "src/common/error.c",
                    [_line(5, 2), _line(7, 3, [0, 4])],
                    function_list=[CoverageFunction("errorNew", 1, 10, 2)],
                ),
                # A file the first run did not report is taken as it is
                _file("src/common/log.c", [_line(3, 1)]),
            ]
        )
    )

    file = coverage.file_find("src/common/error.c")

    assert_equal([(line.no, line.hit) for line in file.line_list], [(5, 3), (7, 3)])
    assert_equal(file.line_list[1].branch_list, [1, 4])
    assert_equal(file.function_list[0].hit, 3)

    # Files are sorted so the report is in a fixed order however the runs came back
    assert_equal([file.name for file in coverage.file_list], ["src/common/error.c", "src/common/log.c"])


####################################################################################################################################
def test_coverage_merge_c_error():
    """C coverage that does not line up means the runs were built from different source, which cannot be merged."""

    def merge(line_list, function_list=(), merge_line_list=None, merge_function_list=()):
        coverage = _coverage([_file("src/common/error.c", line_list, function_list=function_list)])

        coverage.merge(
            _coverage([_file("src/common/error.c", merge_line_list, function_list=merge_function_list)]),
        )

    # A different number of lines
    with assert_raises(ToolError) as error:
        merge([_line(5, 1)], merge_line_list=[_line(5, 1), _line(7, 1)])

    assert_equal(str(error.exception), "coverage for 'src/common/error.c' does not match the prior run")

    # The same number of lines but not the same lines
    with assert_raises(ToolError) as error:
        merge([_line(5, 1)], merge_line_list=[_line(7, 1)])

    assert_equal(str(error.exception), "coverage line mismatch in 'src/common/error.c'")

    # A line that has branches in one run and not the other
    with assert_raises(ToolError) as error:
        merge([_line(5, 1, [1, 0])], merge_line_list=[_line(5, 1)])

    assert_equal(str(error.exception), "coverage branches for 'src/common/error.c' do not match the prior run")

    # A function that is not in the other run
    with assert_raises(ToolError) as error:
        merge(
            [_line(5, 1)],
            function_list=[CoverageFunction("errorNew", 1, 10, 1)],
            merge_line_list=[_line(5, 1)],
            merge_function_list=[CoverageFunction("errorFree", 1, 10, 1)],
        )

    assert_equal(str(error.exception), "coverage for function 'errorNew' is missing")

    # A function that does not span the same lines
    with assert_raises(ToolError) as error:
        merge(
            [_line(5, 1)],
            function_list=[CoverageFunction("errorNew", 1, 10, 1)],
            merge_line_list=[_line(5, 1)],
            merge_function_list=[CoverageFunction("errorNew", 1, 12, 1)],
        )

    assert_equal(str(error.exception), "coverage for function 'errorNew' does not match the prior run")


####################################################################################################################################
def test_coverage_merge_python():
    """Python coverage is merged by line and by where an arc goes, since a run reports only what it knows about."""

    coverage = _coverage(
        [
            _file(
                "test/lib/common/log.py",
                [_line(2, 1, [1, 0], [3, 5]), _line(5, 0)],
                TEST_LANG_PYTHON,
            )
        ]
    )

    coverage.merge(
        _coverage(
            [
                _file(
                    "test/lib/common/log.py",
                    [
                        # An arc the first run reported, one it did not, and a line it did not report at all
                        _line(2, 1, [0, 1, 1], [3, 5, 9]),
                        _line(5, 1),
                        _line(7, 1),
                    ],
                    TEST_LANG_PYTHON,
                )
            ]
        )
    )

    file = coverage.file_find("test/lib/common/log.py")

    # The line the second run added is in place, in line order
    assert_equal([(line.no, line.hit) for line in file.line_list], [(2, 2), (5, 1), (7, 1)])

    # The arc neither run took is still missing and the one only the second run knew about is there
    assert_equal(file.line_list[0].target_list, [3, 5, 9])
    assert_equal(file.line_list[0].branch_list, [1, 1, 1])


####################################################################################################################################
def test_coverage_merge_python_branch():
    """Branches arrive whichever way round the runs come back."""

    # A run with no branches on a line takes the branches the other run reported
    coverage = _coverage([_file("test/lib/common/log.py", [_line(2, 1)], TEST_LANG_PYTHON)])
    coverage.merge(_coverage([_file("test/lib/common/log.py", [_line(2, 1, [1, 0], [3, 5])], TEST_LANG_PYTHON)]))

    file = coverage.file_find("test/lib/common/log.py")

    assert_equal(file.line_list[0].branch_list, [1, 0])
    assert_equal(file.line_list[0].target_list, [3, 5])

    # A run with branches is not given up when the other run reported none
    coverage = _coverage([_file("test/lib/common/log.py", [_line(2, 1, [1, 0], [3, 5])], TEST_LANG_PYTHON)])
    coverage.merge(_coverage([_file("test/lib/common/log.py", [_line(2, 1)], TEST_LANG_PYTHON)]))

    file = coverage.file_find("test/lib/common/log.py")

    assert_equal(file.line_list[0].branch_list, [1, 0])
    assert_equal(file.line_list[0].target_list, [3, 5])


####################################################################################################################################
def test_coverage_module_file():
    """A code module is mapped to the file coverage reports it under, which depends on the language of the test."""

    # Project source
    assert_equal(coverage_module_file("common/error"), "src/common/error.c")

    # The harness and the documentation tool live beside their own source
    assert_equal(coverage_module_file("test/common/harnessLog"), "test/src/common/harnessLog.c")

    # A module that is included rather than compiled
    assert_equal(coverage_module_file("command/backup/process.inc"), "src/command/backup/process.c.inc")
    assert_equal(coverage_module_file("common/regExp.vendor"), "src/common/regExp.vendor.c.inc")

    # The python each tool is written in lives in its library rather than beside the tests, and the same module name in another
    # language is another file
    assert_equal(coverage_module_file("test/common/string_id", TEST_LANG_PYTHON), "test/lib/common/string_id.py")
    assert_equal(coverage_module_file("build/common/render", TEST_LANG_PYTHON), "build/lib/common/render.py")

    with assert_raises(ToolError) as error:
        coverage_module_file("common/string_id", TEST_LANG_PYTHON)

    assert_equal(str(error.exception), "python module 'common/string_id' must be in one of these libraries: build, doc, test")


####################################################################################################################################
def _cmd_coverage(path, module_list, raw_map, vm="none", coverage_summary=False, define=DEFINE):
    """Run the coverage command over a repository built from the raw coverage given.

    The command reads the raw coverage from the repository and the source from the copy the tests ran against, so both are written
    here the way a run leaves them."""

    path_repo = os.path.join(path, "repo")
    path_raw = os.path.join(path_repo, "test/result/coverage/raw")

    os.makedirs(path_raw, exist_ok=True)
    os.makedirs(os.path.join(path_repo, "src/common"), exist_ok=True)
    os.makedirs(os.path.join(path_repo, "test/lib/common"), exist_ok=True)

    for name, content in (
        ("test/define.yaml", define),
        ("src/common/error.c", SOURCE_C),
        ("src/common/stackTrace.c", SOURCE_C),
        ("test/lib/common/log.py", SOURCE_PY),
    ):
        with open(os.path.join(path_repo, name), "w") as file:
            file.write(content)

    for name, raw in raw_map.items():
        with open(os.path.join(path_raw, name), "w") as file:
            file.write(json.dumps(raw))

    output = io.StringIO()

    log_init(WARN, False)

    try:
        with redirect_stdout(output):
            result = cmd_coverage(Config(path_repo, path, module_list, vm, coverage_summary))
    finally:
        log_init(INFO, True)

    return result, output.getvalue(), path_repo


####################################################################################################################################
def _raw_c(name, line_list):
    """Build gcov json, which reports files as an array."""

    return {"files": [{"file": "/repo/" + name, "functions": [], "lines": line_list}]}


####################################################################################################################################
def _raw_py(name, executed, missing, executed_branch=(), missing_branch=()):
    """Build coverage.py json, which reports files as an object keyed by path."""

    return {
        "files": {
            "/repo/"
            + name: {
                "executed_lines": list(executed),
                "missing_lines": list(missing),
                "executed_branches": [list(arc) for arc in executed_branch],
                "missing_branches": [list(arc) for arc in missing_branch],
            }
        }
    }


####################################################################################################################################
def test_coverage_command():
    """Both languages are read into one report, which is written where the documentation build looks for it."""

    with tempfile.TemporaryDirectory() as path:
        result, output, path_repo = _cmd_coverage(
            path,
            ["common/error", "test/common/log"],
            {
                "test-common-error.json": _raw_c(
                    "src/common/error.c", [{"line_number": 5, "count": 1}, {"line_number": 6, "count": 1}]
                ),
                "test-test-common-log.json": _raw_py("test/lib/common/log.py", [1, 2, 3, 5], [], [[2, 3], [2, 5]]),
            },
        )

        assert_equal(result, 0)
        assert_equal(output, "")

        # The report is written for every module, whether or not it had anything missing
        report = open(os.path.join(path_repo, "test/result/coverage/coverage.html")).read()

        assert_in("src/common/error.c", report)
        assert_in("test/lib/common/log.py", report)


####################################################################################################################################
def test_coverage_command_missing():
    """A module that is not fully covered, or that produced nothing at all, is reported and fails the run."""

    with tempfile.TemporaryDirectory() as path:
        result, output, path_repo = _cmd_coverage(
            path,
            ["common/error", "test/common/log"],
            {
                "test-common-error.json": _raw_c(
                    "src/common/error.c", [{"line_number": 5, "count": 0}, {"line_number": 6, "count": 1}]
                ),
            },
        )

        # A module with a line that never ran
        assert_equal(result, 1)
        assert_in("module 'src/common/error.c' is not fully covered (1/2 lines, 0/0 branches)", output)

        # A module that produced no coverage at all, which would otherwise pass silently since there is nothing to report
        assert_in("module 'test/lib/common/log.py' has no coverage data", output)

        # What is missing is in the report, and the line that ran is context rather than a row of its own
        report = open(os.path.join(path_repo, "test/result/coverage/coverage.html")).read()

        assert_in("report-table-row-code-uncovered", report)


####################################################################################################################################
def test_coverage_command_partial():
    """A module covered by a test that was not run cannot be reported on, since it would look uncovered."""

    with tempfile.TemporaryDirectory() as path:
        result, output, path_repo = _cmd_coverage(
            path,
            ["common/error"],
            {
                "test-common-error.json": _raw_c("src/common/error.c", [{"line_number": 5, "count": 1}]),
            },
            define=DEFINE.replace("      - common/stackTrace", "      - common/error"),
        )

        assert_equal(result, 0)
        assert_in("module 'common/error' did not have all tests run required for coverage", output)


####################################################################################################################################
def test_coverage_command_summary():
    """The summary the documentation builds from is written only when it is asked for, and only from a real run."""

    with tempfile.TemporaryDirectory() as path:
        result, output, path_repo = _cmd_coverage(
            path,
            ["common/error"],
            {"test-common-error.json": _raw_c("src/common/error.c", [{"line_number": 5, "count": 1}])},
            vm="u22",
            coverage_summary=True,
        )

        assert_equal(result, 0)

        summary = open(os.path.join(path_repo, "doc/xml/auto/metric-coverage-report.auto.xml")).read()

        assert_in("<table-cell>common</table-cell>", summary)
        assert_in("<table-cell>TOTAL</table-cell>", summary)

        # A summary from a run outside a vm would report code as uncovered that a vm covers
        with assert_raises(ToolError) as error:
            _cmd_coverage(
                path,
                ["common/error"],
                {"test-common-error.json": _raw_c("src/common/error.c", [{"line_number": 5, "count": 1}])},
                coverage_summary=True,
            )

        assert_equal(str(error.exception), "coverage summary must be run in vm")
