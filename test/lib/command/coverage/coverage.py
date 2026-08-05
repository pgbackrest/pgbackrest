"""Coverage Testing and Reporting.

Each unit test writes gcov JSON into test/result/coverage/raw as it completes; this merges them, applies the coverage exceptions,
warns about anything not fully covered, and writes the report."""

####################################################################################################################################
import json
import os

from command.test.define import (
    TEST_LANG_C,
    TEST_LANG_PYTHON,
    test_def_find,
    test_def_parse,
    test_lib_path,
    test_lib_split,
)
from common.error import check
from common.log import *
from common.storage import file_read, file_write, path_list


####################################################################################################################################
class CoverageLine:
    """A line of code, with a hit count for the line and for each branch on it.

    A python branch is an arc to another line, so target_list names where each branch goes and the report can say which path was
    never taken. C branches have no such target and the list is None."""

    def __init__(self, no, hit, branch_list, target_list=None):
        self.no = no
        self.hit = hit
        self.branch_list = branch_list  # Hit count per branch, or None when the line has no branches
        self.target_list = target_list  # Line each branch goes to, for python, or None for C


####################################################################################################################################
class CoverageFunction:
    """A function, with the range of lines it spans and how many times it was called."""

    def __init__(self, name, line_begin, line_end, hit):
        self.name = name
        self.line_begin = line_begin
        self.line_end = line_end
        self.hit = hit


####################################################################################################################################
class CoverageFile:
    """A code module, with the lines and functions coverage was collected for and the totals over them."""

    def __init__(self, name, lang=TEST_LANG_C):
        self.name = name
        self.lang = lang
        self.line_list = []
        self.function_list = []

        self.function_hit = 0
        self.function_total = 0
        self.branch_hit = 0
        self.branch_total = 0
        self.line_hit = 0
        self.line_total = 0

    ################################################################################################################################
    def covered(self):
        """Is every line and branch in this file covered?"""

        return self.line_hit == self.line_total and self.branch_hit == self.branch_total


####################################################################################################################################
def _file_merge_python(file, merge_file):
    """Merge python coverage for a file, keyed by line number.

    Which lines a run reports depends on how the module was loaded, e.g. a module that ran as the program reports fewer statements
    than the same module imported by a test, so a run may contribute a line the others never reported. Functions are not collected
    for python so there is nothing to merge for them."""

    line_map = {line.no: line for line in file.line_list}

    for merge_line in merge_file.line_list:
        line = line_map.get(merge_line.no)

        if line is None:
            file.line_list.append(merge_line)
            line_map[merge_line.no] = merge_line

            continue

        line.hit += merge_line.hit

        _branch_merge_python(line, merge_line)

    file.line_list.sort(key=lambda line: line.no)


####################################################################################################################################
def _branch_merge_python(line, merge_line):
    """Merge the python branches of a line, which are arcs to another line, keyed by where the arc goes.

    An arc that exists only when an exception is raised is unknown to a run that did not raise one, so an arc reported by one run
    and not another is added rather than being an error."""

    if merge_line.branch_list is None:
        return

    if line.branch_list is None:
        line.branch_list = list(merge_line.branch_list)
        line.target_list = list(merge_line.target_list)

        return

    branch = dict(zip(line.target_list, line.branch_list))

    for target, hit in zip(merge_line.target_list, merge_line.branch_list):
        branch[target] = branch.get(target, 0) + hit

    line.target_list = sorted(branch)
    line.branch_list = [branch[target] for target in line.target_list]


####################################################################################################################################
class Coverage:
    """Coverage for every code module reported, merged from the runs that produced it."""

    def __init__(self):
        self.file_list = []

    ################################################################################################################################
    def file_find(self, name):
        """Find a file by code module name, or None when it is not present."""

        for file in self.file_list:
            if file.name == name:
                return file

        return None

    ################################################################################################################################
    def merge(self, merge):
        """Merge coverage from another run.

        A file already present is summed line by line and branch by branch, which requires the two runs to have been built from the
        same source."""

        for merge_file in merge.file_list:
            file = self.file_find(merge_file.name)

            # If the file does not exist yet then take it as is
            if file is None:
                self.file_list.append(merge_file)

                continue

            # Python reports what it knows about rather than what the module contains, so two runs of the same file may not report
            # the same lines and are merged by line number
            if file.lang == TEST_LANG_PYTHON:
                _file_merge_python(file, merge_file)

                continue

            check(
                len(file.line_list) == len(merge_file.line_list) and len(file.function_list) == len(merge_file.function_list),
                "coverage for '%s' does not match the prior run" % file.name,
            )

            for line, merge_line in zip(file.line_list, merge_file.line_list):
                check(line.no == merge_line.no, "coverage line mismatch in '%s'" % file.name)

                line.hit += merge_line.hit

                if line.branch_list is not None:
                    check(
                        merge_line.branch_list is not None and len(line.branch_list) == len(merge_line.branch_list),
                        "coverage branches for '%s' do not match the prior run" % file.name,
                    )

                    for branch_idx, merge_branch in enumerate(merge_line.branch_list):
                        line.branch_list[branch_idx] += merge_branch

            merge_function_list = {function.name: function for function in merge_file.function_list}

            for function in file.function_list:
                merge_function = merge_function_list.get(function.name)

                check(merge_function is not None, "coverage for function '%s' is missing" % function.name)
                check(
                    function.line_begin == merge_function.line_begin and function.line_end == merge_function.line_end,
                    "coverage for function '%s' does not match the prior run" % function.name,
                )

                function.hit += merge_function.hit

        self.file_list.sort(key=lambda file: file.name)

    ################################################################################################################################
    def total_calculate(self):
        """Calculate the totals for every file."""

        for file in self.file_list:
            file.function_total = len(file.function_list)
            file.function_hit = sum(1 for function in file.function_list if function.hit > 0)
            file.line_total = len(file.line_list)
            file.line_hit = sum(1 for line in file.line_list if line.hit > 0)
            file.branch_total = 0
            file.branch_hit = 0

            for line in file.line_list:
                if line.branch_list is None:
                    continue

                file.branch_total += len(line.branch_list)
                file.branch_hit += sum(1 for branch in line.branch_list if branch > 0)

    ################################################################################################################################
    def covered_remove(self):
        """Remove lines that are fully covered so the report shows only what is missing."""

        for file in self.file_list:
            file.line_list = [
                line
                for line in file.line_list
                if line.hit == 0 or (line.branch_list is not None and any(branch == 0 for branch in line.branch_list))
            ]


####################################################################################################################################
def coverage_module_file(name, lang=TEST_LANG_C):
    """Map a code module name to the file name coverage reports it under.

    For example "common/error/error" becomes "src/common/error/error.c", and the python module "test/common/string_id" becomes
    "test/lib/common/string_id.py"."""

    # The python each tool is written in lives in its library rather than beside the tests
    if lang == TEST_LANG_PYTHON:
        lib, module = test_lib_split(name)

        return "%s/%s.py" % (test_lib_path(lib), module)

    if name.startswith("test/"):
        result = "test/src/" + name[len("test/") :] + ".c"
    else:
        result = "src/" + name + ".c"

    # A vendored module is included rather than compiled, as is a module named .inc
    if result.endswith(".vendor.c"):
        result += ".inc"
    elif result.endswith(".inc.c"):
        result = result[: -len(".inc.c")] + ".c.inc"

    return result


####################################################################################################################################
def _coverage_list_build(module_def_list, module_name_list):
    """Build the file names of the code modules that should be fully covered by the tests that ran.

    A module is mapped to a file name here rather than by the caller since the mapping depends on the language of the test that
    declared it. The file is also what identifies a code module, since the same module name in another language is another file."""

    result = []

    for module_name in module_name_list:
        module = test_def_find(module_def_list, module_name)

        for coverage in module.coverage_list:
            file = coverage_module_file(coverage.name, module.lang)

            if coverage.coverable and file not in result:
                result.append(file)

    # Remove code modules that require a test that was not run for full coverage
    for module in module_def_list:
        for coverage in module.coverage_list:
            file = coverage_module_file(coverage.name, module.lang)

            if file in result and module.name not in module_name_list:
                log(WARN, "module '%s' did not have all tests run required for coverage" % coverage.name)
                result.remove(file)

    return result


####################################################################################################################################
def cmd_coverage(config, module_list):
    """Merge the coverage the test modules given produced and write the report.

    Returns 1 when a module is missing coverage, else 0."""

    from command.coverage import filter_c, filter_py
    from command.coverage.report import report_render, summary_render

    module_def_list = test_def_parse(config.repo_path)
    coverage_module_list = _coverage_list_build(module_def_list, module_list)

    # Combine the coverage written by each test that ran. Both languages report into the same model so a run that covered C and
    # python produces a single report.
    path_raw = os.path.join(config.repo_path, "test/result/coverage/raw")
    coverage = Coverage()

    for name in path_list(path_raw, expression=r"\.json$", error_on_missing=True):
        raw = json.loads(file_read(os.path.join(path_raw, name)))

        # gcov reports files as an array while coverage.py reports them as an object keyed by path
        filter = filter_c if isinstance(raw["files"], list) else filter_py

        coverage.merge(filter.coverage_read(raw, coverage_module_list))

    # The source the tests were built from, which is the repository copy in the test path
    def source_read(name):
        return file_read(os.path.join(config.test_path, "repo", name))

    # Each filter applies the exceptions for its own language and leaves the other language alone
    filter_c.coverage_filter(coverage, source_read, config.coverage_summary, config.vm)
    filter_py.coverage_filter(coverage, source_read, config.coverage_summary, config.vm)

    coverage.total_calculate()

    # Write coverage summary
    if config.coverage_summary:
        check(config.vm != "none", "coverage summary must be run in vm")

        file_write(os.path.join(config.repo_path, "doc/xml/auto/metric-coverage-report.auto.xml"), summary_render(coverage))

    # Warn on missing coverage
    result = 0

    # A module that was expected to be covered but produced no coverage at all, e.g. because the test wrote no data, would
    # otherwise pass silently since there is nothing to report as uncovered
    for name in coverage_module_list:
        if coverage.file_find(name) is None:
            log(WARN, "module '%s' has no coverage data" % name)
            result += 1

    for file in coverage.file_list:
        if not file.covered():
            log(
                WARN,
                "module '%s' is not fully covered (%u/%u lines, %u/%u branches)"
                % (file.name, file.line_hit, file.line_total, file.branch_hit, file.branch_total),
            )
            result += 1

    # Write coverage report with the covered lines filtered out
    coverage.covered_remove()

    file_write(os.path.join(config.repo_path, "test/result/coverage/coverage.html"), report_render(coverage, source_read))

    return 1 if result > 0 else 0
