"""Test Coverage Report.

The whole report is asserted rather than parts of it, since the report is the output of the coverage command and a change anywhere
in it should be deliberate. The constants the renderer builds from are used to write the expected report, which keeps the assertion
about the structure rather than about the styling."""

####################################################################################################################################
from harness.test import *

from command.coverage.coverage import Coverage, CoverageFile, CoverageLine
from command.coverage.report import *
from command.test.define import TEST_LANG_PYTHON

# C source to report against. The blank lines and braces are what the context trimming removes, and the gap between the functions is
# what puts a skip in the report.
SOURCE_C = (
    "/***/\n"  # 1
    "void\n"  # 2
    "funcOne(bool test)\n"  # 3
    "{\n"  # 4
    "    if (test)\n"  # 5
    "    {\n"  # 6
    "        doSomething();\n"  # 7
    "    }\n"  # 8
    "\n"  # 9
    "    return;\n"  # 10
    "}\n"  # 11
    "\n"  # 12
    "\n"  # 13
    "void\n"  # 14
    "funcTwo(void)\n"  # 15
    "{\n"  # 16
    "\n"  # 17
    "    doOther();\n"  # 18
    "}\n"  # 19
    "\n"  # 20
    "\n"  # 21
    "\n"  # 22
    "\n"  # 23
    "void\n"  # 24
    "funcThree(void)\n"  # 25
    "{\n"  # 26
    "\n"  # 27
    "\n"  # 28
    "\n"  # 29
    "\n"  # 30
    "    doThird();\n"  # 31
    "}\n"  # 32
    "\n"  # 33
)

# Python source to report against
SOURCE_PY = (
    "def func(a, b):\n"  # 1
    "    if a:\n"  # 2
    "        return 1\n"  # 3
    "\n"  # 4
    "    for item in b:\n"  # 5
    "        print(item)\n"  # 6
    "\n"  # 7
    "    return 0\n"  # 8
    "\n"  # 9
    "\n"  # 10
    "def other(a):\n"  # 11
    "    if a:\n"  # 12
    "        return 1\n"  # 13
)


####################################################################################################################################
def _source_read(name):
    """Read the source a file is reported against, which the report command reads from the repository copy."""

    return SOURCE_PY if name.endswith(".py") else SOURCE_C


####################################################################################################################################
def test_report_render():
    """A file with no lines left is listed but has no report, and one that is missing coverage is reported with its context."""

    coverage = Coverage()

    # A file that is fully covered has had every line removed by the time the report is rendered
    coverage.file_list.append(CoverageFile("src/common/covered.c"))

    file = CoverageFile("src/common/missing.c")
    coverage.file_list.append(file)

    # A line where one of the two branches was never taken, a line that never ran, and a line in the function below
    file.line_list.append(CoverageLine(5, 1, [1, 0]))
    file.line_list.append(CoverageLine(7, 0, None))
    file.line_list.append(CoverageLine(18, 0, None))
    file.line_list.append(CoverageLine(31, 0, None))

    expect = HTML_PRE + TOC_PRE

    # A covered file is named in the contents but is not a link, since there is nothing to link to
    expect += TOC_COVERED_PRE + "src/common/covered.c" + TOC_COVERED_POST
    expect += TOC_UNCOVERED_PRE + "src/common/missing.c" + TOC_UNCOVERED_MID + "src/common/missing.c" + TOC_UNCOVERED_POST
    expect += TOC_POST

    expect += RPT_PRE + "src/common/missing.c" + RPT_MID1 + "src/common/missing.c" + RPT_MID2

    # Context above the first line reported, which starts at the top of the file since the line is within five of it
    expect += RPT_LINE_PRE + "1" + RPT_BRANCH_COVERED + RPT_CODE + "/***/" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "2" + RPT_BRANCH_COVERED + RPT_CODE + "void" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "3" + RPT_BRANCH_COVERED + RPT_CODE + "funcOne(bool test)" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "4" + RPT_BRANCH_COVERED + RPT_CODE + "{" + RPT_LINE_POST

    # The line ran so only the branch is marked, with one mark per outcome
    expect += RPT_LINE_PRE + "5" + RPT_BRANCH_UNCOVERED_PRE + "[+ -]" + RPT_BRANCH_UNCOVERED_POST
    expect += RPT_CODE + "    if (test)" + RPT_LINE_POST

    # The context between the two lines is the brace, which is not trimmed here since it is above a line being reported
    expect += RPT_LINE_PRE + "6" + RPT_BRANCH_COVERED + RPT_CODE + "    {" + RPT_LINE_POST

    # The line never ran so the code is marked rather than the branch
    expect += RPT_LINE_PRE + "7" + RPT_BRANCH_COVERED + RPT_CODE_UNCOVERED + "        doSomething();" + RPT_LINE_POST

    # Context below, with the closing brace and the blank line at the end of the function trimmed off
    expect += RPT_LINE_PRE + "8" + RPT_BRANCH_COVERED + RPT_CODE + "    }" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "9" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "10" + RPT_BRANCH_COVERED + RPT_CODE + "    return;" + RPT_LINE_POST

    # The next line reported is far enough below that the report skips rather than running the context together, and the blank line
    # at the top of that context is trimmed
    expect += RPT_SKIP
    expect += RPT_LINE_PRE + "14" + RPT_BRANCH_COVERED + RPT_CODE + "void" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "15" + RPT_BRANCH_COVERED + RPT_CODE + "funcTwo(void)" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "16" + RPT_BRANCH_COVERED + RPT_CODE + "{" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "17" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "18" + RPT_BRANCH_COVERED + RPT_CODE_UNCOVERED + "    doOther();" + RPT_LINE_POST

    # Nothing but a brace and blank lines above the next line reported, so the whole context is trimmed and the skip stands alone
    expect += RPT_SKIP
    expect += RPT_LINE_PRE + "31" + RPT_BRANCH_COVERED + RPT_CODE_UNCOVERED + "    doThird();" + RPT_LINE_POST

    expect += RPT_POST + HTML_POST

    assert_equal(report_render(coverage, _source_read), expect)


####################################################################################################################################
def test_report_render_python():
    """A python branch is an arc to another line, so the report names the line that was never jumped to."""

    coverage = Coverage()

    file = CoverageFile("test/lib/common/py.py", TEST_LANG_PYTHON)
    coverage.file_list.append(file)

    # One of the two arcs was taken, then neither of them
    file.line_list.append(CoverageLine(2, 1, [1, 0], [3, 5]))
    file.line_list.append(CoverageLine(5, 1, [0, 0], [6, 8]))

    # An arc out of the function it is in, which python reports as the line the function is defined on, negated
    file.line_list.append(CoverageLine(12, 1, [0, 1], [-11, 13]))

    expect = HTML_PRE + TOC_PRE
    expect += TOC_UNCOVERED_PRE + "test/lib/common/py.py" + TOC_UNCOVERED_MID + "test/lib/common/py.py" + TOC_UNCOVERED_POST
    expect += TOC_POST

    expect += RPT_PRE + "test/lib/common/py.py" + RPT_MID1 + "test/lib/common/py.py" + RPT_MID2

    expect += RPT_LINE_PRE + "1" + RPT_BRANCH_COVERED + RPT_CODE + "def func(a, b):" + RPT_LINE_POST

    # Only the arc that was never taken is named, i.e. the branch that returns rather than the one that falls through
    expect += RPT_LINE_PRE + "2" + RPT_BRANCH_UNCOVERED_PRE + "[-> 5]" + RPT_BRANCH_UNCOVERED_POST
    expect += RPT_CODE + "    if a:" + RPT_LINE_POST

    expect += RPT_LINE_PRE + "3" + RPT_BRANCH_COVERED + RPT_CODE + "        return 1" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "4" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST

    # Both arcs are named when neither was taken, i.e. a loop that never ran and never finished
    expect += RPT_LINE_PRE + "5" + RPT_BRANCH_UNCOVERED_PRE + "[-> 6 -> 8]" + RPT_BRANCH_UNCOVERED_POST
    expect += RPT_CODE + "    for item in b:" + RPT_LINE_POST

    expect += RPT_LINE_PRE + "6" + RPT_BRANCH_COVERED + RPT_CODE + "        print(item)" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "7" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "8" + RPT_BRANCH_COVERED + RPT_CODE + "    return 0" + RPT_LINE_POST

    expect += RPT_LINE_PRE + "9" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "10" + RPT_BRANCH_COVERED + RPT_CODE + "" + RPT_LINE_POST
    expect += RPT_LINE_PRE + "11" + RPT_BRANCH_COVERED + RPT_CODE + "def other(a):" + RPT_LINE_POST

    # The arc out of the function is named as a return rather than by the line it goes to, which means nothing to a reader
    expect += RPT_LINE_PRE + "12" + RPT_BRANCH_UNCOVERED_PRE + "[-> return]" + RPT_BRANCH_UNCOVERED_POST
    expect += RPT_CODE + "    if a:" + RPT_LINE_POST

    expect += RPT_LINE_PRE + "13" + RPT_BRANCH_COVERED + RPT_CODE + "        return 1" + RPT_LINE_POST

    expect += RPT_POST + HTML_POST

    assert_equal(report_render(coverage, _source_read), expect)


####################################################################################################################################
def test_report_summary_value():
    """A value is rendered as hit over total with the percentage, the same way the C code renders it."""

    # Nothing to measure is not zero percent
    assert_equal(summary_value(0, 0), "---")

    # A ratio of one is fixed at 100% so rounding cannot make it anything else
    assert_equal(summary_value(5, 5), "5/5 (100.00%)")

    # Nothing hit is zero, padded out to the same shape
    assert_equal(summary_value(0, 5), "0/5 (0.00%)")

    # Rounding down and up
    assert_equal(summary_value(1, 3), "1/3 (33.33%)")
    assert_equal(summary_value(2, 3), "2/3 (66.67%)")
    assert_equal(summary_value(3, 4), "3/4 (75.00%)")


####################################################################################################################################
def test_report_summary_render():
    """The summary aggregates the files of a code module into one row and covers only project source."""

    coverage = Coverage()

    for name, value in (
        ("src/common/error.c", (2, 2, 3, 4, 10, 10)),
        ("src/common/log.c", (1, 2, 0, 0, 5, 6)),
        ("src/config/parse.c", (4, 4, 2, 2, 20, 20)),
        # The harness, the tool libraries, and the documentation are covered but are not what the summary reports on
        ("test/src/common/harness.c", (9, 9, 9, 9, 9, 9)),
        ("test/src/build.c", (9, 9, 9, 9, 9, 9)),
        ("build/lib/common/render.py", (9, 9, 9, 9, 9, 9)),
        ("doc/lib/command/doc.py", (9, 9, 9, 9, 9, 9)),
    ):
        file = CoverageFile(name)
        (
            file.function_hit,
            file.function_total,
            file.branch_hit,
            file.branch_total,
            file.line_hit,
            file.line_total,
        ) = value

        coverage.file_list.append(file)

    expect = SUMMARY_ROW % ("common", "3/4 (75.00%)", "3/4 (75.00%)", "15/16 (93.75%)") + "\n"
    expect += SUMMARY_ROW % ("config", "4/4 (100.00%)", "2/2 (100.00%)", "20/20 (100.00%)") + "\n"
    expect += SUMMARY_ROW % ("TOTAL", "7/8 (87.50%)", "5/6 (83.33%)", "35/36 (97.22%)")

    assert_equal(summary_render(coverage), expect)
