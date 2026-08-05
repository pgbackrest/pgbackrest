"""C Coverage Filter.

Converts gcov JSON into the coverage model and applies the exceptions that make the report meaningful for C source:

- Debug logging that was covered is dropped, since it is not interesting and would otherwise dominate the report.
- A line or branch marked uncovered/uncoverable in a comment is treated as covered. Only uncoverable is honored when building the
  documentation summary, so the summary reports what genuinely cannot be covered.
- Branches in assertions and in macros that expand to more than they appear to are dropped, since they can never be taken both ways
  in a test."""

####################################################################################################################################
import re

from command.coverage.coverage import Coverage, CoverageFile, CoverageFunction, CoverageLine
from command.test.define import TEST_LANG_C

# Debug logging that is not interesting for coverage reporting
_LOG_EXP = re.compile(r"\s+FUNCTION_(LOG|TEST)_(VOID|BEGIN|END|PARAM(|_P|_PP))\(")
_LOG_RETURN_EXP = re.compile(r"\s+FUNCTION_(LOG|TEST)_RETURN_VOID\(")


####################################################################################################################################
def _module_match(path, module):
    """Does a path gcov reported come from a code module?

    A module is normally compiled from the repository so the path ends with the module file name. A shimmed .c.inc is compiled from
    the copy the build writes to the unit include path, which has no src prefix so that it resolves ahead of the repository, and is
    matched on the rest of the name."""

    if path.endswith(module):
        return True

    return module.startswith("src/") and module.endswith(".c.inc") and path.endswith(module[len("src/") :])


####################################################################################################################################
def coverage_read(raw, module_list):
    """Read gcov JSON into a coverage object, keeping only the files that match a covered code module."""

    result = Coverage()

    for file_raw in raw["files"]:
        # gcov reports the path it compiled, which is longer than the module name the report uses
        name = next((module for module in module_list if _module_match(file_raw["file"], module)), None)

        if name is None:
            continue

        file = CoverageFile(name)

        for function_raw in file_raw["functions"]:
            file.function_list.append(
                CoverageFunction(
                    function_raw["name"], function_raw["start_line"], function_raw["end_line"], function_raw["execution_count"]
                )
            )

        for line_raw in file_raw["lines"]:
            branch_list = None

            if line_raw.get("branches"):
                branch_list = [branch["count"] for branch in line_raw["branches"]]

            file.line_list.append(CoverageLine(line_raw["line_number"], line_raw["count"], branch_list))

        result.file_list.append(file)

    result.file_list.sort(key=lambda file: file.name)

    return result


####################################################################################################################################
def _exception_exp(coverage_summary, vm):
    """Build the expressions that match a coverage exception.

    Only uncoverable counts for the summary, and vm_covered only applies when not running in a vm, since there the code really is
    covered."""

    uncover_branch = "uncoverable_branch" if coverage_summary else "uncover(ed|able)_branch"
    uncover = "uncoverable" if coverage_summary else "uncover(ed|able)"
    vm_covered = "|vm_covered" if vm == "none" else ""

    branch = re.compile(
        r"\s{4}[A-Z][A-Z0-9_]+\([^\?]*\)|\s{4}(ASSERT|CHECK|CHECK_FMT|assert|switch\s)\(|\{\+{0,1}(%s%s)"
        % (uncover_branch, vm_covered)
    )
    line = re.compile(r"\{\+{0,1}(%s%s)[^_]" % (uncover, vm_covered))

    return branch, line


####################################################################################################################################
def coverage_filter(coverage, source_read, coverage_summary, vm):
    """Apply the coverage exceptions to every file, using the source the test was built from."""

    branch_exp, line_exp = _exception_exp(coverage_summary, vm)

    for file in coverage.file_list:
        if file.lang != TEST_LANG_C:
            continue

        line_text_list = source_read(file.name).split("\n")
        line_list = []

        for line in file.line_list:
            text = line_text_list[line.no - 1]

            # Remove covered lines for debug logging. These are not very interesting for coverage reporting.
            if line.hit != 0:
                if _LOG_EXP.search(text) or (_LOG_RETURN_EXP.search(text) and line_text_list[line.no] == "}"):
                    continue

            # If not covered then check for line coverage exceptions
            if line.hit == 0 and line_exp.search(text):
                line.hit = 1

            # If not covered then check for branch coverage exceptions
            if line.branch_list is not None and branch_exp.search(text):
                # Remove branch coverage so it is not reported
                line.branch_list = None

            line_list.append(line)

        file.line_list = line_list
