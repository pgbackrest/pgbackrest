"""Test C Coverage Filter."""

####################################################################################################################################
from harness.test import *

from command.coverage.coverage import Coverage, CoverageFile, CoverageLine
from command.coverage.filter_c import *
from command.test.define import TEST_LANG_PYTHON

# Source the annotations are read from. The debug logging and the assertion are what the filter drops so the report is about code
# that matters rather than about macros that can never go both ways.
SOURCE = (
    "/***/\n"  # 1
    "void\n"  # 2
    "funcOne(bool test)\n"  # 3
    "{\n"  # 4
    "    FUNCTION_LOG_BEGIN(logLevelDebug);\n"  # 5
    "        FUNCTION_LOG_PARAM(BOOL, test);\n"  # 6
    "    FUNCTION_LOG_END();\n"  # 7
    "\n"  # 8
    "    ASSERT(test != NULL);\n"  # 9
    "\n"  # 10
    "    if (test)                                                       // {uncovered_branch}\n"  # 11
    "        doSomething();                                              // {uncovered}\n"  # 12
    "\n"  # 13
    "    doOther();                                                      // {vm_covered}\n"  # 14
    "\n"  # 15
    "    FUNCTION_LOG_RETURN_VOID();\n"  # 16
    "}\n"  # 17
    "\n"  # 18
)


####################################################################################################################################
def _coverage_build():
    """Build the coverage a run of the source above would produce."""

    result = Coverage()

    file = CoverageFile("src/common/error.c")
    result.file_list.append(file)

    for no, hit, branch_list in (
        (5, 1, None),
        (6, 1, None),
        (7, 1, None),
        (9, 1, [1, 0]),
        (11, 1, [1, 0]),
        (12, 0, None),
        (14, 0, None),
        (16, 1, None),
    ):
        file.line_list.append(CoverageLine(no, hit, branch_list))

    return result, file


####################################################################################################################################
def test_filter_c_read():
    """Only the files being reported on are read, with the functions and the branches on each line."""

    coverage = coverage_read(
        {
            "files": [
                {
                    "file": "/test/repo/src/common/error.c",
                    "functions": [{"name": "errorNew", "start_line": 10, "end_line": 20, "execution_count": 3}],
                    "lines": [
                        {"line_number": 10, "count": 3, "branches": []},
                        {"line_number": 12, "count": 3, "branches": [{"count": 3}, {"count": 0}]},
                        {"line_number": 14, "count": 0},
                    ],
                },
                # A shimmed .c.inc is compiled from the copy in the unit path, which is not under src
                {
                    "file": "/test/unit-0/none/command/backup/process.c.inc",
                    "functions": [],
                    "lines": [{"line_number": 5, "count": 1, "branches": []}],
                },
                # A file that is compiled but is not one of the modules being reported on
                {"file": "/test/repo/src/common/other.c", "functions": [], "lines": [{"line_number": 1, "count": 1}]},
            ]
        },
        ["src/common/error.c", "src/command/backup/process.c.inc"],
    )

    assert_equal([file.name for file in coverage.file_list], ["src/command/backup/process.c.inc", "src/common/error.c"])

    file = coverage.file_list[1]

    # A function is reported with the lines it spans and how many times it was called
    assert_equal([(f.name, f.line_begin, f.line_end, f.hit) for f in file.function_list], [("errorNew", 10, 20, 3)])

    # C records how many times a line ran, and a line with no branches has none
    assert_equal([(line.no, line.hit) for line in file.line_list], [(10, 3), (12, 3), (14, 0)])
    assert_is_none(file.line_list[0].branch_list)
    assert_equal(file.line_list[1].branch_list, [3, 0])
    assert_is_none(file.line_list[2].branch_list)

    # A C branch has no target, which is what tells the report to mark each outcome rather than name a line
    assert_is_none(file.line_list[1].target_list)


####################################################################################################################################
def test_filter_c_log():
    """Debug logging that ran is dropped, since it would otherwise be most of the report."""

    coverage, file = _coverage_build()

    coverage_filter(coverage, lambda name: SOURCE, False, "none")

    # The log begin, param, end, and the return that is the last thing in the function are all gone
    assert_equal([line.no for line in file.line_list], [9, 11, 12, 14])


####################################################################################################################################
def test_filter_c_exception():
    """A branch that can never go both ways is dropped and an annotated line is treated as covered."""

    coverage, file = _coverage_build()

    coverage_filter(coverage, lambda name: SOURCE, False, "none")

    # An assertion only fails when the code is wrong, so its branch is not something a test can cover
    assert_equal(file.line_list[0].no, 9)
    assert_is_none(file.line_list[0].branch_list)

    # A branch annotated in the source is dropped the same way
    assert_equal(file.line_list[1].no, 11)
    assert_is_none(file.line_list[1].branch_list)

    # An annotated line is treated as having run
    assert_equal(file.line_list[2].no, 12)
    assert_equal(file.line_list[2].hit, 1)

    # Code that only a container can reach is covered when the tests are not running in one
    assert_equal(file.line_list[3].no, 14)
    assert_equal(file.line_list[3].hit, 1)


####################################################################################################################################
def test_filter_c_exception_vm():
    """In a vm the code a container reaches really is covered, so the annotation for it does not apply."""

    coverage, file = _coverage_build()

    coverage_filter(coverage, lambda name: SOURCE, False, "u22")

    assert_equal(file.line_list[3].no, 14)
    assert_equal(file.line_list[3].hit, 0)


####################################################################################################################################
def test_filter_c_exception_summary():
    """Only uncoverable counts for the documentation summary, so the summary reports what genuinely cannot be covered."""

    coverage, file = _coverage_build()

    # A python file in the same report is left to the python filter
    file_py = CoverageFile("test/lib/common/log.py", TEST_LANG_PYTHON)
    file_py.line_list.append(CoverageLine(5, 0, [0], [7]))
    coverage.file_list.append(file_py)

    coverage_filter(coverage, lambda name: SOURCE, True, "u22")

    # Uncovered says the code is not covered yet rather than that it cannot be, so the summary still counts it
    assert_equal(file.line_list[1].no, 11)
    assert_equal(file.line_list[1].branch_list, [1, 0])
    assert_equal(file.line_list[2].hit, 0)

    # An assertion is still dropped, since that is about what a branch can do rather than about what is covered
    assert_is_none(file.line_list[0].branch_list)

    # The python file is untouched
    assert_equal(file_py.line_list[0].hit, 0)
    assert_equal(file_py.line_list[0].branch_list, [0])
