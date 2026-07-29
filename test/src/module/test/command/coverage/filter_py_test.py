"""Test Python Coverage Filter."""

####################################################################################################################################
from harness.test import *

from command.coverage.coverage import Coverage, CoverageFile, CoverageLine
from command.coverage.filter_py import *
from command.test.define import TEST_LANG_C, TEST_LANG_PYTHON

# Coverage as coverage.py reports it, i.e. the lines that ran, the lines that did not, and the arcs between them
RAW = {
    "files": {
        "/test/repo/test/lib/common/covered.py": {
            "executed_lines": [1, 2, 3, 5],
            "missing_lines": [],
            "executed_branches": [[2, 3], [2, 5]],
            "missing_branches": [],
        },
        # A file that is measured but is not one of the modules being reported on, e.g. a module a test only depends on
        "/test/repo/test/lib/common/other.py": {
            "executed_lines": [1],
            "missing_lines": [2],
            "executed_branches": [],
            "missing_branches": [],
        },
    }
}

# Source the annotations are read from
SOURCE = (
    "def func(a):\n"  # 1
    "    if a:  # {uncovered_branch}\n"  # 2
    "        return 1\n"  # 3
    "\n"  # 4
    "    return 0  # {uncovered}\n"  # 5
)

# The same source annotated as unable to be covered rather than as not covered yet
SOURCE_UNCOVERABLE = (
    "def func(a):\n"  # 1
    "    if a:  # {uncoverable_branch}\n"  # 2
    "        return 1\n"  # 3
    "\n"  # 4
    "    return 0  # {uncoverable}\n"  # 5
)


####################################################################################################################################
def test_filter_py_read():
    """Only the files being reported on are read, with the arcs out of a line collected onto it."""

    coverage = coverage_read(RAW, ["test/lib/common/covered.py"])

    assert_equal([file.name for file in coverage.file_list], ["test/lib/common/covered.py"])

    file = coverage.file_list[0]

    assert_equal(file.lang, TEST_LANG_PYTHON)

    # Python records whether a line ran rather than how many times, so a hit is only ever zero or one
    assert_equal([(line.no, line.hit) for line in file.line_list], [(1, 1), (2, 1), (3, 1), (5, 1)])

    # Arcs are collected onto the line they leave, in order of where they go, and a line with no arcs has none
    assert_equal(file.line_list[1].branch_list, [1, 1])
    assert_equal(file.line_list[1].target_list, [3, 5])
    assert_is_none(file.line_list[0].branch_list)
    assert_is_none(file.line_list[0].target_list)

    # Function coverage is not collected since the summary it feeds covers the project source rather than the harness
    assert_equal(file.function_list, [])


####################################################################################################################################
def test_filter_py_read_missing():
    """A line or arc that was never reached is what the report is for."""

    coverage = coverage_read(
        {
            "files": {
                "/test/repo/test/lib/common/covered.py": {
                    "executed_lines": [1, 2, 3],
                    "missing_lines": [5],
                    "executed_branches": [[2, 3]],
                    "missing_branches": [[2, 5]],
                }
            }
        },
        ["test/lib/common/covered.py"],
    )

    file = coverage.file_list[0]

    assert_equal([(line.no, line.hit) for line in file.line_list], [(1, 1), (2, 1), (3, 1), (5, 0)])

    # The arc that was taken and the one that was not are both on the line they leave
    assert_equal(file.line_list[1].branch_list, [1, 0])
    assert_equal(file.line_list[1].target_list, [3, 5])


####################################################################################################################################
def test_filter_py_exception():
    """A line or branch annotated in the source is treated as covered."""

    coverage = Coverage()

    file = CoverageFile("test/lib/common/covered.py", TEST_LANG_PYTHON)
    coverage.file_list.append(file)

    file.line_list.append(CoverageLine(2, 1, [1, 0], [3, 5]))
    file.line_list.append(CoverageLine(5, 0, None))

    # A C file in the same report is left to the C filter
    file_c = CoverageFile("src/common/error.c")
    file_c.line_list.append(CoverageLine(5, 0, [0, 0]))
    coverage.file_list.append(file_c)

    coverage_filter(coverage, lambda name: SOURCE, False, "none")

    # The branch is removed rather than marked, so the report has nothing to show for it
    assert_is_none(file.line_list[0].branch_list)
    assert_is_none(file.line_list[0].target_list)

    # The line is marked as having run
    assert_equal(file.line_list[1].hit, 1)

    # The C file is untouched
    assert_equal(file_c.line_list[0].hit, 0)
    assert_equal(file_c.line_list[0].branch_list, [0, 0])


####################################################################################################################################
def test_filter_py_exception_summary():
    """Only uncoverable counts for the documentation summary, so the summary reports what genuinely cannot be covered."""

    coverage = Coverage()

    file = CoverageFile("test/lib/common/covered.py", TEST_LANG_PYTHON)
    coverage.file_list.append(file)

    file.line_list.append(CoverageLine(2, 1, [1, 0], [3, 5]))
    file.line_list.append(CoverageLine(5, 0, None))

    coverage_filter(coverage, lambda name: SOURCE, True, "u22")

    # Uncovered says the code is not covered yet rather than that it cannot be, so the summary still counts it
    assert_equal(file.line_list[0].branch_list, [1, 0])
    assert_equal(file.line_list[0].target_list, [3, 5])
    assert_equal(file.line_list[1].hit, 0)

    # Uncoverable is what the summary honors
    coverage_filter(coverage, lambda name: SOURCE_UNCOVERABLE, True, "u22")

    assert_is_none(file.line_list[0].branch_list)
    assert_equal(file.line_list[1].hit, 1)
