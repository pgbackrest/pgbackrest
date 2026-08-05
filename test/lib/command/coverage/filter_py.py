"""Python Coverage Filter.

Converts coverage.py JSON into the coverage model and applies the same uncovered/uncoverable annotations the C filter does, so both
languages are reported the same way and can be combined in one report.

Two things differ from gcov and are worth knowing when reading a combined report:

- Python records whether a line ran, not how many times, so a hit count here is only ever zero or one. The report never shows the
  count so nothing is lost, but a merge cannot sum runs the way it does for C.
- Function coverage is not collected, since it is only reported in the documentation summary and that covers the project source
  rather than the harness.
- A branch is an arc out of a line rather than a condition within an expression, so "if a and b" is one decision to python and two
  branches to gcc. Full branch coverage therefore means less in python than it does in C.
"""

####################################################################################################################################
import re

from command.test.define import TEST_LANG_PYTHON


####################################################################################################################################
def coverage_read(raw, module_list):
    """Read coverage.py JSON into a coverage object, keeping only the files that match a covered code module."""

    from command.coverage.coverage import Coverage, CoverageFile, CoverageLine

    result = Coverage()

    for path, file_raw in raw["files"].items():
        # coverage.py reports the path it measured, which is longer than the module name the report uses
        name = next((module for module in module_list if path.endswith(module)), None)

        if name is None:
            continue

        file = CoverageFile(name, TEST_LANG_PYTHON)

        # Function coverage is not collected. It is only ever reported in the documentation summary, which covers the project
        # source rather than the harness, so there is nothing to report it against.

        # Collect the branches on each line, which coverage.py reports as arcs from one line to another
        branch = {}

        for source, target in file_raw["executed_branches"]:
            branch.setdefault(source, []).append((target, 1))

        for source, target in file_raw["missing_branches"]:
            branch.setdefault(source, []).append((target, 0))

        for no in sorted(set(file_raw["executed_lines"]) | set(file_raw["missing_lines"])):
            hit = 1 if no in file_raw["executed_lines"] else 0
            branch_list = None
            target_list = None

            if no in branch:
                arc_list = sorted(branch[no])
                target_list = [target for target, _ in arc_list]
                branch_list = [hit for _, hit in arc_list]

            file.line_list.append(CoverageLine(no, hit, branch_list, target_list))

        result.file_list.append(file)

    result.file_list.sort(key=lambda file: file.name)

    return result


####################################################################################################################################
def _exception_exp(coverage_summary):
    """Build the expressions that match a coverage exception.

    Only uncoverable counts for the summary, so the summary reports what genuinely cannot be covered. There is no vm_covered here
    since no python code is reachable only inside a container."""

    uncover_branch = "uncoverable_branch" if coverage_summary else "uncover(ed|able)_branch"
    uncover = "uncoverable" if coverage_summary else "uncover(ed|able)"

    return re.compile(r"\{\+{0,1}(%s)" % uncover_branch), re.compile(r"\{\+{0,1}(%s)[^_]" % uncover)


####################################################################################################################################
def coverage_filter(coverage, source_read, coverage_summary, vm):
    """Apply the coverage exceptions to every file, using the source the test was run from."""

    branch_exp, line_exp = _exception_exp(coverage_summary)

    for file in coverage.file_list:
        if file.lang != TEST_LANG_PYTHON:
            continue

        line_text_list = source_read(file.name).split("\n")

        for line in file.line_list:
            text = line_text_list[line.no - 1]

            # If not covered then check for line coverage exceptions
            if line.hit == 0 and line_exp.search(text):
                line.hit = 1

            # If not covered then check for branch coverage exceptions
            if line.branch_list is not None and branch_exp.search(text):
                # Remove branch coverage so it is not reported
                line.branch_list = None
                line.target_list = None
