"""Coverage Report.

Renders the HTML report of everything that is not covered and the summary table used in the documentation."""

####################################################################################################################################
import os

TITLE = "pgBackRest Coverage Report"

HTML_PRE = (
    '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">\n'
    '<html xmlns="http://www.w3.org/1999/xhtml">\n'
    "<head>\n"
    "  <title>" + TITLE + "</title>\n"
    '  <meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>\n'
    '  <style type="text/css">\n'
    "html\n"
    "{\n"
    "    background-color: #555555;\n"
    "    font-family: Avenir, Corbel, sans-serif;\n"
    "    color: white;\n"
    "    font-size: 12pt;\n"
    "    margin-top: 8px;\n"
    "    margin-left: 1%;\n"
    "    margin-right: 1%;\n"
    "    width: 98%;\n"
    "}\n"
    "\n"
    "body\n"
    "{\n"
    "    margin: 0px auto;\n"
    "    padding: 0px;\n"
    "    width: 100%;\n"
    "    text-align: justify;\n"
    "}\n"
    ".title\n"
    "{\n"
    "    width: 100%;\n"
    "    text-align: center;\n"
    "    font-size: 200%;\n"
    "}\n"
    "\n"
    ".list-table\n"
    "{\n"
    "    width: 100%;\n"
    "}\n"
    "\n"
    ".list-table-caption\n"
    "{\n"
    "    margin-top: 1em;\n"
    "    font-size: 130%;\n"
    "    margin-bottom: .25em;\n"
    "}\n"
    "\n"
    ".list-table-caption::after\n"
    "{\n"
    '    content: "Modules Tested for Coverage:";\n'
    "}\n"
    "\n"
    ".list-table-header-file\n"
    "{\n"
    "    padding-left: .5em;\n"
    "    padding-right: .5em;\n"
    "    background-color: #333333;\n"
    "    width: 100%;\n"
    "}\n"
    "\n"
    ".list-table-row-uncovered\n"
    "{\n"
    "    background-color: #580000;\n"
    "    color: white;\n"
    "    width: 100%;\n"
    "}\n"
    "\n"
    ".list-table-row-file\n"
    "{\n"
    "    padding-left: .5em;\n"
    "    padding-right: .5em;\n"
    "}\n"
    "\n"
    ".report-table\n"
    "{\n"
    "    width: 100%;\n"
    "}\n"
    "\n"
    ".report-table-caption\n"
    "{\n"
    "    margin-top: 1em;\n"
    "    font-size: 130%;\n"
    "    margin-bottom: .25em;\n"
    "}\n"
    "\n"
    ".report-table-caption::after\n"
    "{\n"
    '    content: " report:";\n'
    "}\n"
    "\n"
    ".report-table-header\n"
    "{\n"
    "}\n"
    "\n"
    ".report-table-header-line, .report-table-header-branch, .report-table-header-code\n"
    "{\n"
    "    padding-left: .5em;\n"
    "    padding-right: .5em;\n"
    "    background-color: #333333;\n"
    "}\n"
    "\n"
    ".report-table-header-code\n"
    "{\n"
    "    width: 100%;\n"
    "}\n"
    "\n"
    ".report-table-row-dot-tr, .report-table-row\n"
    "{\n"
    '    font-family: "Courier New", Courier, monospace;\n'
    "}\n"
    "\n"
    ".report-table-row-dot-skip\n"
    "{\n"
    "    height: 1em;\n"
    "    padding-top: .25em;\n"
    "    padding-bottom: .25em;\n"
    "    text-align: center;\n"
    "}\n"
    "\n"
    ".report-table-row-line, .report-table-row-branch, .report-table-row-branch-uncovered, .report-table-row-code"
    ", .report-table-row-code-uncovered\n"
    "{\n"
    "    padding-left: .5em;\n"
    "    padding-right: .5em;\n"
    "}\n"
    "\n"
    ".report-table-row-line\n"
    "{\n"
    "    text-align: right;\n"
    "}\n"
    "\n"
    ".report-table-row-branch, .report-table-row-branch-uncovered\n"
    "{\n"
    "    text-align: right;\n"
    "    white-space: nowrap;\n"
    "}\n"
    "\n"
    ".report-table-row-branch-uncovered\n"
    "{\n"
    "    background-color: #580000;\n"
    "    color: white;\n"
    "}\n"
    "\n"
    ".report-table-row-code, .report-table-row-code-uncovered\n"
    "{\n"
    "    white-space: pre;\n"
    "}\n"
    "\n"
    ".report-table-row-code-uncovered\n"
    "{\n"
    "    background-color: #580000;\n"
    "    color: white;\n"
    "}\n"
    "  </style>\n"
    "</head>\n"
    "<body>\n"
    '  <div class="title">' + TITLE + "</div>\n"
)

TOC_PRE = (
    '  <div class="list-table-caption"></div>\n'
    '  <table class="list-table">\n'
    '    <tr class="list-table-header">\n'
    '      <th class="list-table-header-file">FILE</th>\n'
    "    </tr>\n"
)

TOC_COVERED_PRE = '    <tr class="list-table-row-covered">\n      <td class="list-table-row-file">'
TOC_COVERED_POST = "</td>\n    </tr>\n"
TOC_UNCOVERED_PRE = '    <tr class="list-table-row-uncovered">\n      <td class="list-table-row-file">\n        <a href="#'
TOC_UNCOVERED_MID = '">'
TOC_UNCOVERED_POST = "</a>\n      </td>\n    </tr>\n"
TOC_POST = "  </table>\n"

RPT_PRE = '  <div class="report-table-caption"><a id="'
RPT_MID1 = '"></a>'
RPT_MID2 = (
    "</div>\n"
    '  <table class="report-table">\n'
    '    <tr class="report-table-header">\n'
    '      <th class="report-table-header-line">LINE</th>\n'
    '      <th class="report-table-header-branch">BRANCH</th>\n'
    '      <th class="report-table-header-code">CODE</th>\n'
    "    </tr>\n"
)

RPT_LINE_PRE = '    <tr class="report-table-row">\n      <td class="report-table-row-line">'
RPT_BRANCH_COVERED = '</td>\n      <td class="report-table-row-branch"></td>\n'
RPT_BRANCH_UNCOVERED_PRE = '</td>\n      <td class="report-table-row-branch-uncovered">'
RPT_BRANCH_UNCOVERED_POST = "</td>\n"
RPT_CODE = '      <td class="report-table-row-code">'
RPT_CODE_UNCOVERED = '      <td class="report-table-row-code-uncovered">'
RPT_LINE_POST = "</td>\n    </tr>\n"
RPT_SKIP = '    <tr class="report-table-row-dot">\n      <td class="report-table-row-dot-skip" colspan="3">...</td>\n    </tr>\n'
RPT_POST = TOC_POST

HTML_POST = "</body>\n</html>\n"


####################################################################################################################################
def _context_render(no, text):
    """Render a context line, i.e. a covered line shown around one that is missing coverage."""

    return RPT_LINE_PRE + str(no) + RPT_BRANCH_COVERED + RPT_CODE + text + RPT_LINE_POST


####################################################################################################################################
def _context_trim(text):
    """Is this line one that may be trimmed from the context, i.e. it carries no information on its own?"""

    text = text.strip()

    return text == "" or text == "{" or text == "}"


####################################################################################################################################
def _file_render(file, line_text_list):
    """Render the report for one file."""

    result = RPT_PRE + file.name + RPT_MID1 + file.name + RPT_MID2
    line_last = 0

    for line_idx, line in enumerate(file.line_list):
        # Check branch coverage and build report string
        branch_covered = True
        branch_str = "["

        if line.branch_list is not None:
            # A python branch is an arc to another line so name the line that was never jumped to, which says which path was
            # missed. A C branch has no such target and is shown as one mark per outcome.
            if line.target_list is not None:
                for branch, target in zip(line.branch_list, line.target_list):
                    if branch != 0:
                        continue

                    if len(branch_str) > 1:
                        branch_str += " "

                    # A negative target is the end of the function the line is in, i.e. the branch that never returned. The value
                    # is the line the function is defined on, which is of no use in the report.
                    branch_str += "-> return" if target < 0 else "-> %d" % target
                    branch_covered = False
            else:
                for branch in line.branch_list:
                    if len(branch_str) > 1:
                        branch_str += " "

                    if branch == 0:
                        branch_str += "-"
                        branch_covered = False
                    else:
                        branch_str += "+"

        branch_str += "]"

        # Add before context
        context_begin = line.no - 5 if line.no > 5 else 1
        context_end = line.no - 1 if line.no > 1 else 0

        if line_last != 0 and context_begin < line_last + 1:
            context_begin = line_last + 1

        if line_last != 0 and context_begin > line_last + 1:
            result += RPT_SKIP

            # Trim uninteresting lines from the top of the context
            for context_idx in range(context_begin, context_end + 1):
                if not _context_trim(line_text_list[context_idx - 1]):
                    break

                context_begin = context_idx + 1

        for context_idx in range(context_begin, context_end + 1):
            result += _context_render(context_idx, line_text_list[context_idx - 1])

        # Output coverage
        result += RPT_LINE_PRE + str(line.no)

        if not branch_covered:
            result += RPT_BRANCH_UNCOVERED_PRE + branch_str + RPT_BRANCH_UNCOVERED_POST
        else:
            result += RPT_BRANCH_COVERED

        result += RPT_CODE_UNCOVERED if line.hit == 0 else RPT_CODE
        result += line_text_list[line.no - 1] + RPT_LINE_POST

        line_last = line.no

        # Add after context
        context_begin = line.no + 1
        context_end = line.no + 5 if line.no <= len(line_text_list) - 5 else len(line_text_list)

        if line_idx < len(file.line_list) - 1:
            line_next = file.line_list[line_idx + 1]

            if context_end >= line_next.no:
                context_end = line_next.no - 1

        # Trim uninteresting lines from the bottom of the context
        for context_idx in range(context_end, context_begin - 1, -1):
            if not _context_trim(line_text_list[context_idx - 1]):
                break

            context_end = context_idx - 1

        for context_idx in range(context_begin, context_end + 1):
            result += _context_render(context_idx, line_text_list[context_idx - 1])

        line_last = context_end

    return result + RPT_POST


####################################################################################################################################
def report_render(coverage, source_read):
    """Render the coverage report.

    Files that are fully covered are listed in the table of contents but have no report section."""

    result = HTML_PRE + TOC_PRE

    # Build table of contents
    for file in coverage.file_list:
        if not file.line_list:
            result += TOC_COVERED_PRE + file.name + TOC_COVERED_POST
        else:
            result += TOC_UNCOVERED_PRE + file.name + TOC_UNCOVERED_MID + file.name + TOC_UNCOVERED_POST

    result += TOC_POST

    # Build files that are missing coverage
    for file in coverage.file_list:
        if file.line_list:
            result += _file_render(file, source_read(file.name).split("\n"))

    return result + HTML_POST


####################################################################################################################################
# Coverage summary for the documentation
SUMMARY_ROW = (
    "<table-row>\n"
    "    <table-cell>%s</table-cell>\n"
    "    <table-cell>%s</table-cell>\n"
    "    <table-cell>%s</table-cell>\n"
    "    <table-cell>%s</table-cell>\n"
    "</table-row>\n"
)


####################################################################################################################################
def summary_value(hit, total):
    """Render hit/total and the percentage, matching cvtPctToZ() in src/common/type/convert.c."""

    if total == 0:
        return "---"

    # A ratio of one is fixed at 100% so rounding cannot throw off the result
    if hit == total:
        percent = 10000
    else:
        percent = (int(hit / total * 100000) + 5) // 10

    percent = "%03u" % percent

    return "%u/%u (%s.%s%%)" % (hit, total, percent[:-2], percent[-2:])


####################################################################################################################################
def summary_render(coverage):
    """Render the summary table, aggregating the files of each code module into a single row."""

    result = ""
    module_list = {}
    total = [0, 0, 0, 0, 0, 0]

    for file in coverage.file_list:
        # Filter out anything that is not project source
        if file.name.startswith("test/") or file.name.startswith("doc/"):
            continue

        name = os.path.dirname(file.name)[len("src/") :]
        module = module_list.setdefault(name, [0, 0, 0, 0, 0, 0])
        value = (file.function_hit, file.function_total, file.branch_hit, file.branch_total, file.line_hit, file.line_total)

        for index, item in enumerate(value):
            module[index] += item
            total[index] += item

    for name in sorted(module_list):
        module = module_list[name]

        result += (
            SUMMARY_ROW
            % (
                name,
                summary_value(module[0], module[1]),
                summary_value(module[2], module[3]),
                summary_value(module[4], module[5]),
            )
            + "\n"
        )

    return result + SUMMARY_ROW % (
        "TOTAL",
        summary_value(total[0], total[1]),
        summary_value(total[2], total[3]),
        summary_value(total[4], total[5]),
    )
