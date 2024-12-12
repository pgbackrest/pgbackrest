/***********************************************************************************************************************************
Coverage Testing and Reporting
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/json.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/list.h"

#include "command/test/coverage.h"
#include "command/test/define.h"
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Object types
***********************************************************************************************************************************/
typedef struct TestCoverageLine
{
    unsigned int no;                                                // Line number
    unsigned int hit;                                               // Total times line was hit
    List *branchList;                                               // List of branches
} TestCoverageLine;

typedef struct TestCoverageFunction
{
    String *name;                                                   // Function name
    unsigned int lineBegin;                                         // Beginning line of function
    unsigned int lineEnd;                                           // Beginning line of function
    unsigned int hit;                                               // Total times function was hit
} TestCoverageFunction;

typedef struct TestCoverageFile
{
    String *name;                                                   // File name
    List *lineList;                                                 // List of lines
    List *functionList;                                             // List of functions

    unsigned int functionHit;                                       // Functions hit
    unsigned int functionTotal;                                     // Total functions
    unsigned int branchHit;                                         // Branches hit
    unsigned int branchTotal;                                       // Total branches
    unsigned int lineHit;                                           // Lines hit
    unsigned int lineTotal;                                         // Total lines
} TestCoverageFile;

typedef struct TestCoverage
{
    List *fileList;                                                 // List of files
} TestCoverage;

/***********************************************************************************************************************************
New coverage
***********************************************************************************************************************************/
static TestCoverage *
testCvgNew(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    OBJ_NEW_BEGIN(TestCoverage, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (TestCoverage)
        {
            .fileList = lstNewP(sizeof(TestCoverageFile), .comparator = lstComparatorStr),
        };
    }
    OBJ_NEW_END();

    FUNCTION_LOG_RETURN(TEST_COVERAGE, this);
}

/***********************************************************************************************************************************
Merge coverage from gcov json
***********************************************************************************************************************************/
static void
testCvgMerge(const TestCoverage *const this, const String *const json, const StringList *const moduleList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TEST_COVERAGE, this);
        FUNCTION_LOG_PARAM(STRING, json);
        FUNCTION_LOG_PARAM(STRING_LIST, moduleList);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(json != NULL);
    ASSERT(moduleList != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Read gcov json
        const TestCoverage *const merge = testCvgNew();
        JsonRead *const read = jsonReadNew(json);
        jsonReadObjectBegin(read);

        while (jsonReadUntil(read, jsonTypeObjectEnd))
        {
            const String *const key = jsonReadKey(read);

            if (strEqZ(key, "files"))
            {
                jsonReadArrayBegin(read);

                while (jsonReadUntil(read, jsonTypeArrayEnd))
                {
                    TestCoverageFile file = {0};
                    bool fileAdd = false;

                    MEM_CONTEXT_OBJ_BEGIN(merge->fileList)
                    {
                        file.lineList = lstNewP(sizeof(TestCoverageLine));
                        file.functionList = lstNewP(sizeof(TestCoverageFunction), .comparator = lstComparatorStr);
                    }
                    MEM_CONTEXT_OBJ_END();

                    jsonReadObjectBegin(read);

                    while (jsonReadUntil(read, jsonTypeObjectEnd))
                    {
                        const String *const key = jsonReadKey(read);

                        if (strEqZ(key, "file"))
                        {
                            const String *const fileName = jsonReadStr(read);

                            for (unsigned int moduleIdx = 0; moduleIdx < strLstSize(moduleList); moduleIdx++)
                            {
                                const String *const module = strLstGet(moduleList, moduleIdx);

                                if (strEndsWith(fileName, module))
                                {
                                    MEM_CONTEXT_OBJ_BEGIN(merge->fileList)
                                    {
                                        file.name = strDup(module);
                                    }
                                    MEM_CONTEXT_OBJ_END();

                                    fileAdd = true;
                                    break;
                                }
                            }
                        }
                        else if (strEqZ(key, "functions"))
                        {
                            jsonReadArrayBegin(read);

                            while (jsonReadUntil(read, jsonTypeArrayEnd))
                            {
                                TestCoverageFunction function = {0};

                                jsonReadObjectBegin(read);

                                while (jsonReadUntil(read, jsonTypeObjectEnd))
                                {
                                    const String *const key = jsonReadKey(read);

                                    if (strEqZ(key, "name"))
                                    {
                                        MEM_CONTEXT_OBJ_BEGIN(file.functionList)
                                        {
                                            function.name = jsonReadStr(read);
                                        }
                                        MEM_CONTEXT_OBJ_END();
                                    }
                                    else if (strEqZ(key, "start_line"))
                                    {
                                        function.lineBegin = jsonReadUInt(read);
                                    }
                                    else if (strEqZ(key, "end_line"))
                                    {
                                        function.lineEnd = jsonReadUInt(read);
                                    }
                                    else if (strEqZ(key, "execution_count"))
                                    {
                                        function.hit = jsonReadUInt(read);
                                    }
                                    else
                                        jsonReadSkip(read);
                                }

                                jsonReadObjectEnd(read);

                                ASSERT(function.name != NULL);
                                ASSERT(function.lineBegin != 0);
                                ASSERT(function.lineEnd != 0);

                                lstAdd(file.functionList, &function);
                            }

                            jsonReadArrayEnd(read);
                        }
                        else if (strEqZ(key, "lines"))
                        {
                            jsonReadArrayBegin(read);

                            while (jsonReadUntil(read, jsonTypeArrayEnd))
                            {
                                TestCoverageLine line = {0};

                                jsonReadObjectBegin(read);

                                while (jsonReadUntil(read, jsonTypeObjectEnd))
                                {
                                    const String *const key = jsonReadKey(read);

                                    if (strEqZ(key, "branches"))
                                    {
                                        jsonReadArrayBegin(read);

                                        while (jsonReadUntil(read, jsonTypeArrayEnd))
                                        {
                                            unsigned int branchHit = 0;

                                            jsonReadObjectBegin(read);

                                            while (jsonReadUntil(read, jsonTypeObjectEnd))
                                            {
                                                if (line.branchList == NULL)
                                                {
                                                    MEM_CONTEXT_OBJ_BEGIN(file.lineList)
                                                    {
                                                        line.branchList = lstNewP(sizeof(unsigned int));
                                                    }
                                                    MEM_CONTEXT_OBJ_END();
                                                }

                                                const String *const key = jsonReadKey(read);

                                                if (strEqZ(key, "count"))
                                                    branchHit = jsonReadUInt(read);
                                                else
                                                    jsonReadSkip(read);
                                            }

                                            jsonReadObjectEnd(read);
                                            lstAdd(line.branchList, &branchHit);
                                        }

                                        jsonReadArrayEnd(read);
                                    }
                                    else if (strEqZ(key, "count"))
                                        line.hit = jsonReadUInt(read);
                                    else if (strEqZ(key, "line_number"))
                                        line.no = jsonReadUInt(read);
                                    else
                                        jsonReadSkip(read);
                                }

                                jsonReadObjectEnd(read);

                                ASSERT(line.no != 0);

                                lstAdd(file.lineList, &line);
                            }

                            jsonReadArrayEnd(read);
                        }
                        else
                            jsonReadSkip(read);
                    }

                    jsonReadObjectEnd(read);

                    if (fileAdd)
                    {
                        ASSERT(file.name != NULL);

                        lstAdd(merge->fileList, &file);
                    }
                }

                jsonReadArrayEnd(read);
            }
            else
                jsonReadSkip(read);
        }

        jsonReadObjectEnd(read);

        lstSort(merge->fileList, sortOrderAsc);

        // Merge into coverage
        for (unsigned int fileIdx = 0; fileIdx < lstSize(merge->fileList); fileIdx++)
        {
            const TestCoverageFile *const mergeFile = lstGet(merge->fileList, fileIdx);
            const TestCoverageFile *const thisFile = lstFind(this->fileList, &mergeFile->name);
            ASSERT(mergeFile != NULL);

            // If the file exists then merge
            if (thisFile != NULL)
            {
                ASSERT(lstSize(thisFile->lineList) == lstSize(mergeFile->lineList));
                ASSERT(lstSize(thisFile->functionList) == lstSize(mergeFile->functionList));

                for (unsigned int lineIdx = 0; lineIdx < lstSize(thisFile->lineList); lineIdx++)
                {
                    TestCoverageLine *const thisLine = lstGet(thisFile->lineList, lineIdx);
                    const TestCoverageLine *const mergeLine = lstGet(mergeFile->lineList, lineIdx);
                    ASSERT(thisLine->no == mergeLine->no);

                    thisLine->hit += mergeLine->hit;

                    if (thisLine->branchList != NULL)
                    {
                        ASSERT(mergeLine->branchList != NULL);
                        ASSERT(lstSize(thisLine->branchList) == lstSize(mergeLine->branchList));

                        for (unsigned int branchIdx = 0; branchIdx < lstSize(thisLine->branchList); branchIdx++)
                        {
                            *(unsigned int *)lstGet(thisLine->branchList, branchIdx) += *(unsigned int *)lstGet(
                                mergeLine->branchList, branchIdx);
                        }
                    }
                }

                for (unsigned int functionIdx = 0; functionIdx < lstSize(thisFile->functionList); functionIdx++)
                {
                    TestCoverageFunction *const thisFunction = lstGet(thisFile->functionList, functionIdx);
                    const TestCoverageFunction *const mergeFunction = lstFind(mergeFile->functionList, &thisFunction->name);
                    ASSERT(mergeFunction != NULL);
                    ASSERT(thisFunction->lineBegin == mergeFunction->lineBegin);
                    ASSERT(thisFunction->lineEnd == mergeFunction->lineEnd);

                    thisFunction->hit += mergeFunction->hit;
                }
            }
            // Else add the file
            else
            {
                MEM_CONTEXT_OBJ_BEGIN(this->fileList)
                {
                    const TestCoverageFile file =
                    {
                        .name = strDup(mergeFile->name),
                        .lineList = lstMove(mergeFile->lineList, lstMemContext(this->fileList)),
                        .functionList = lstMove(mergeFile->functionList, lstMemContext(this->fileList)),
                    };

                    lstAdd(this->fileList, &file);
                }
                MEM_CONTEXT_OBJ_END();
            }
        }

        lstSort(this->fileList, sortOrderAsc);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/***********************************************************************************************************************************
Coverage report
***********************************************************************************************************************************/
#define TEST_CVG_HTML_TITLE                                         "pgBackRest Coverage Report"

// {uncrustify_off -  - comment inside string}
#define TEST_CVG_HTML_PRE                                                                                                          \
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"          \
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"                                                                              \
    "<head>\n"                                                                                                                     \
    "  <title>" TEST_CVG_HTML_TITLE "</title>\n"                                                                                   \
    "  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\"></meta>\n"                                            \
    "  <style type=\"text/css\">\n"                                                                                                \
    "html\n"                                                                                                                       \
    "{\n"                                                                                                                          \
    "    background-color: #555555;\n"                                                                                             \
    "    font-family: Avenir, Corbel, sans-serif;\n"                                                                               \
    "    color: white;\n"                                                                                                          \
    "    font-size: 12pt;\n"                                                                                                       \
    "    margin-top: 8px;\n"                                                                                                       \
    "    margin-left: 1%;\n"                                                                                                       \
    "    margin-right: 1%;\n"                                                                                                      \
    "    width: 98%;\n"                                                                                                            \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    "body\n"                                                                                                                       \
    "{\n"                                                                                                                          \
    "    margin: 0px auto;\n"                                                                                                      \
    "    padding: 0px;\n"                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "    text-align: justify;\n"                                                                                                   \
    "}\n"                                                                                                                          \
    ".title\n"                                                                                                                     \
    "{\n"                                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "    text-align: center;\n"                                                                                                    \
    "    font-size: 200%;\n"                                                                                                       \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table\n"                                                                                                                \
    "{\n"                                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table-caption\n"                                                                                                        \
    "{\n"                                                                                                                          \
    "    margin-top: 1em;\n"                                                                                                       \
    "    font-size: 130%;\n"                                                                                                       \
    "    margin-bottom: .25em;\n"                                                                                                  \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table-caption::after\n"                                                                                                 \
    "{\n"                                                                                                                          \
    "    content: \"Modules Tested for Coverage:\";\n"                                                                             \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table-header-file\n"                                                                                                    \
    "{\n"                                                                                                                          \
    "    padding-left: .5em;\n"                                                                                                    \
    "    padding-right: .5em;\n"                                                                                                   \
    "    background-color: #333333;\n"                                                                                             \
    "    width: 100%;\n"                                                                                                           \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table-row-uncovered\n"                                                                                                  \
    "{\n"                                                                                                                          \
    "    background-color: #580000;\n"                                                                                             \
    "    color: white;\n"                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".list-table-row-file\n"                                                                                                       \
    "{\n"                                                                                                                          \
    "    padding-left: .5em;\n"                                                                                                    \
    "    padding-right: .5em;\n"                                                                                                   \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table\n"                                                                                                              \
    "{\n"                                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-caption\n"                                                                                                      \
    "{\n"                                                                                                                          \
    "    margin-top: 1em;\n"                                                                                                       \
    "    font-size: 130%;\n"                                                                                                       \
    "    margin-bottom: .25em;\n"                                                                                                  \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-caption::after\n"                                                                                               \
    "{\n"                                                                                                                          \
    "    content: \" report:\";\n"                                                                                                 \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-header\n"                                                                                                       \
    "{\n"                                                                                                                          \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-header-line, .report-table-header-branch, .report-table-header-code\n"                                          \
    "{\n"                                                                                                                          \
    "    padding-left: .5em;\n"                                                                                                    \
    "    padding-right: .5em;\n"                                                                                                   \
    "    background-color: #333333;\n"                                                                                             \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-header-code\n"                                                                                                  \
    "{\n"                                                                                                                          \
    "    width: 100%;\n"                                                                                                           \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-dot-tr, .report-table-row\n"                                                                                \
    "{\n"                                                                                                                          \
    "    font-family: \"Courier New\", Courier, monospace;\n"                                                                      \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-dot-skip\n"                                                                                                 \
    "{\n"                                                                                                                          \
    "    height: 1em;\n"                                                                                                           \
    "    padding-top: .25em;\n"                                                                                                    \
    "    padding-bottom: .25em;\n"                                                                                                 \
    "    text-align: center;\n"                                                                                                    \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-line, .report-table-row-branch, .report-table-row-branch-uncovered, .report-table-row-code"                 \
    ", .report-table-row-code-uncovered\n"                                                                                         \
    "{\n"                                                                                                                          \
    "    padding-left: .5em;\n"                                                                                                    \
    "    padding-right: .5em;\n"                                                                                                   \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-line\n"                                                                                                     \
    "{\n"                                                                                                                          \
    "    text-align: right;\n"                                                                                                     \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-branch, .report-table-row-branch-uncovered\n"                                                               \
    "{\n"                                                                                                                          \
    "    text-align: right;\n"                                                                                                     \
    "    white-space: nowrap;\n"                                                                                                   \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-branch-uncovered\n"                                                                                         \
    "{\n"                                                                                                                          \
    "    background-color: #580000;\n"                                                                                             \
    "    color: white;\n"                                                                                                          \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-code, .report-table-row-code-uncovered\n"                                                                   \
    "{\n"                                                                                                                          \
    "    white-space: pre;\n"                                                                                                      \
    "}\n"                                                                                                                          \
    "\n"                                                                                                                           \
    ".report-table-row-code-uncovered\n"                                                                                           \
    "{\n"                                                                                                                          \
    "    background-color: #580000;\n"                                                                                             \
    "    color: white;\n"                                                                                                          \
    "}\n"                                                                                                                          \
    "  </style>\n"                                                                                                                 \
    "</head>\n"                                                                                                                    \
    "<body>\n"                                                                                                                     \
    "  <div class=\"title\">" TEST_CVG_HTML_TITLE "</div>\n"
// {uncrustify_on}

#define TEST_CVG_HTML_TOC_PRE                                                                                                      \
    "  <div class=\"list-table-caption\"></div>\n"                                                                                 \
    "  <table class=\"list-table\">\n"                                                                                             \
    "    <tr class=\"list-table-header\">\n"                                                                                       \
    "      <th class=\"list-table-header-file\">FILE</th>\n"                                                                       \
    "    </tr>\n"

#define TEST_CVG_HTML_TOC_COVERED_PRE                                                                                              \
    "    <tr class=\"list-table-row-covered\">\n"                                                                                  \
    "      <td class=\"list-table-row-file\">"

#define TEST_CVG_HTML_TOC_COVERED_POST                                                                                             \
    "</td>\n"                                                                                                                      \
    "    </tr>\n"

#define TEST_CVG_HTML_TOC_UNCOVERED_PRE                                                                                            \
    "    <tr class=\"list-table-row-uncovered\">\n"                                                                                \
    "      <td class=\"list-table-row-file\">\n"                                                                                   \
    "        <a href=\"#"

#define TEST_CVG_HTML_TOC_UNCOVERED_MID                                                                                            \
    "\">"

#define TEST_CVG_HTML_TOC_UNCOVERED_POST                                                                                           \
    "</a>\n"                                                                                                                       \
    "      </td>\n"                                                                                                                \
    "    </tr>\n"

#define TEST_CVG_HTML_TOC_POST                                                                                                     \
    "  </table>\n"

#define TEST_CVG_HTML_RPT_PRE                                                                                                      \
    "  <div class=\"report-table-caption\"><a id=\""

#define TEST_CVG_HTML_RPT_MID1                                                                                                     \
    "\"></a>"

#define TEST_CVG_HTML_RPT_MID2                                                                                                     \
    "</div>\n"                                                                                                                     \
    "  <table class=\"report-table\">\n"                                                                                           \
    "    <tr class=\"report-table-header\">\n"                                                                                     \
    "      <th class=\"report-table-header-line\">LINE</th>\n"                                                                     \
    "      <th class=\"report-table-header-branch\">BRANCH</th>\n"                                                                 \
    "      <th class=\"report-table-header-code\">CODE</th>\n"                                                                     \
    "    </tr>\n"

#define TEST_CVG_HTML_RPT_LINE_PRE                                                                                                 \
    "    <tr class=\"report-table-row\">\n"                                                                                        \
    "      <td class=\"report-table-row-line\">"

#define TEST_CVG_HTML_RPT_BRANCH_COVERED                                                                                           \
    "</td>\n"                                                                                                                      \
    "      <td class=\"report-table-row-branch\"></td>\n"

#define TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_PRE                                                                                     \
    "</td>\n"                                                                                                                      \
    "      <td class=\"report-table-row-branch-uncovered\">"

#define TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_POST                                                                                    \
    "</td>\n"

#define TEST_CVG_HTML_RPT_CODE                                                                                                     \
    "      <td class=\"report-table-row-code\">"

#define TEST_CVG_HTML_RPT_CODE_UNCOVERED                                                                                           \
    "      <td class=\"report-table-row-code-uncovered\">"

#define TEST_CVG_HTML_RPT_LINE_POST                                                                                                \
    "</td>\n"                                                                                                                      \
    "    </tr>\n"

#define TEST_CVG_HTML_RPT_SKIP                                                                                                     \
    "    <tr class=\"report-table-row-dot\">\n"                                                                                    \
    "      <td class=\"report-table-row-dot-skip\" colspan=\"3\">...</td>\n"                                                       \
    "    </tr>\n"

#define TEST_CVG_HTML_RPT_POST                                      TEST_CVG_HTML_TOC_POST

#define TEST_CVG_HTML_POST                                                                                                         \
    "</body>\n"                                                                                                                    \
    "</html>\n"

static String *
testCvgReport(const TestCoverage *const coverage, const String *const pathTest)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TEST_COVERAGE, coverage);
        FUNCTION_LOG_PARAM(STRING, pathTest);
    FUNCTION_LOG_END();

    ASSERT(coverage != NULL);
    ASSERT(pathTest != NULL);

    String *const result = strCatZ(strNew(), TEST_CVG_HTML_PRE TEST_CVG_HTML_TOC_PRE);

    // Build table of contents
    MEM_CONTEXT_TEMP_BEGIN()
    {
        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            const TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);
            const String *const fileName = file->name;

            if (lstEmpty(file->lineList))
                strCatFmt(result, TEST_CVG_HTML_TOC_COVERED_PRE "%s" TEST_CVG_HTML_TOC_COVERED_POST, strZ(fileName));
            else
            {
                strCatFmt(
                    result, TEST_CVG_HTML_TOC_UNCOVERED_PRE "%s" TEST_CVG_HTML_TOC_UNCOVERED_MID "%s"
                    TEST_CVG_HTML_TOC_UNCOVERED_POST, strZ(fileName), strZ(fileName));
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    strCatZ(result, TEST_CVG_HTML_TOC_POST);

    // Build files that are missing coverage
    for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);

            if (!lstEmpty(file->lineList))
            {
                const StringList *const lineTextList = strLstNewSplitZ(
                    strNewBuf(
                        storageGetP(storageNewReadP(storagePosixNewP(pathTest), strNewFmt("repo/%s", strZ(file->name))))), "\n");
                unsigned int lineLast = 0;
                const String *const fileName = file->name;

                strCatFmt(
                    result, TEST_CVG_HTML_RPT_PRE "%s" TEST_CVG_HTML_RPT_MID1 "%s" TEST_CVG_HTML_RPT_MID2,
                    strZ(fileName), strZ(fileName));

                for (unsigned int lineIdx = 0; lineIdx < lstSize(file->lineList); lineIdx++)
                {
                    const TestCoverageLine *const line = lstGet(file->lineList, lineIdx);

                    // Check branch coverage and build report string
                    bool branchCovered = true;
                    String *const branchStr = strCatZ(strNew(), "[");

                    if (line->branchList != NULL)
                    {
                        for (unsigned int branchIdx = 0; branchIdx < lstSize(line->branchList); branchIdx++)
                        {
                            if (strSize(branchStr) > 1)
                                strCatChr(branchStr, ' ');

                            if ((*(unsigned int *)lstGet(line->branchList, branchIdx)) == 0)
                            {
                                strCatChr(branchStr, '-');
                                branchCovered = false;
                            }
                            else
                                strCatChr(branchStr, '+');
                        }
                    }

                    strCatChr(branchStr, ']');

                    // Add before context
                    unsigned int contextBegin = line->no > 5 ? line->no - 5 : 1;
                    unsigned int contextEnd = line->no > 1 ? line->no - 1 : 0;

                    if (lineLast != 0 && contextBegin < lineLast + 1)
                        contextBegin = lineLast + 1;

                    if (lineLast != 0 && contextBegin > lineLast + 1)
                    {
                        strCatZ(result, TEST_CVG_HTML_RPT_SKIP);

                        for (unsigned int contextIdx = contextBegin; contextIdx <= contextEnd; contextIdx++)
                        {
                            const String *const lineText = strTrim(strDup(strLstGet(lineTextList, contextIdx - 1)));

                            if (strEmpty(lineText) || strEqZ(lineText, "{") || strEqZ(lineText, "}"))
                                contextBegin = contextIdx + 1;
                            else
                                break;
                        }
                    }

                    for (unsigned int contextIdx = contextBegin; contextIdx <= contextEnd; contextIdx++)
                    {
                        strCatFmt(
                            result,
                            TEST_CVG_HTML_RPT_LINE_PRE "%u" TEST_CVG_HTML_RPT_BRANCH_COVERED TEST_CVG_HTML_RPT_CODE "%s"
                            TEST_CVG_HTML_RPT_LINE_POST,
                            contextIdx, strZ(strLstGet(lineTextList, contextIdx - 1)));
                    }

                    // Output coverage
                    strCatFmt(result, TEST_CVG_HTML_RPT_LINE_PRE "%u", line->no);

                    if (!branchCovered)
                    {
                        strCatFmt(
                            result, TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_PRE "%s" TEST_CVG_HTML_RPT_BRANCH_UNCOVERED_POST,
                            strZ(branchStr));
                    }
                    else
                        strCatZ(result, TEST_CVG_HTML_RPT_BRANCH_COVERED);

                    if (line->hit == 0)
                        strCatZ(result, TEST_CVG_HTML_RPT_CODE_UNCOVERED);
                    else
                        strCatZ(result, TEST_CVG_HTML_RPT_CODE);

                    strCatFmt(result, "%s" TEST_CVG_HTML_RPT_LINE_POST, strZ(strLstGet(lineTextList, line->no - 1)));

                    lineLast = line->no;

                    // Add after context
                    contextBegin = line->no + 1;
                    contextEnd = line->no <= strLstSize(lineTextList) - 5 ? line->no + 5 : strLstSize(lineTextList);

                    if (lineIdx < lstSize(file->lineList) - 1)
                    {
                        const TestCoverageLine *const lineNext = lstGet(file->lineList, lineIdx + 1);

                        if (contextEnd >= lineNext->no)
                            contextEnd = lineNext->no - 1;
                    }

                    for (unsigned int contextIdx = contextEnd; contextIdx >= contextBegin; contextIdx--)
                    {
                        const String *const lineText = strTrim(strDup(strLstGet(lineTextList, contextIdx - 1)));

                        if (strEmpty(lineText) || strEqZ(lineText, "{") || strEqZ(lineText, "}"))
                            contextEnd = contextIdx - 1;
                        else
                            break;
                    }

                    for (unsigned int contextIdx = contextBegin; contextIdx <= contextEnd; contextIdx++)
                    {
                        strCatFmt(
                            result,
                            TEST_CVG_HTML_RPT_LINE_PRE "%u" TEST_CVG_HTML_RPT_BRANCH_COVERED TEST_CVG_HTML_RPT_CODE "%s"
                            TEST_CVG_HTML_RPT_LINE_POST,
                            contextIdx, strZ(strLstGet(lineTextList, contextIdx - 1)));
                    }

                    lineLast = contextEnd;
                }

                strCatZ(result, TEST_CVG_HTML_RPT_POST);
            }
        }
        MEM_CONTEXT_TEMP_END();
    }

    // Finalize report
    strCatZ(result, TEST_CVG_HTML_POST);

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Coverage summary
***********************************************************************************************************************************/
#define TEST_CVG_SUMMARY                                                                                                           \
    "<table-row>\n"                                                                                                                \
    "    <table-cell>%s</table-cell>\n"                                                                                            \
    "    <table-cell>%s</table-cell>\n"                                                                                            \
    "    <table-cell>%s</table-cell>\n"                                                                                            \
    "    <table-cell>%s</table-cell>\n"                                                                                            \
    "</table-row>\n"

typedef struct TestCvgSummaryModule
{
    const String *name;                                             // Name of coverage module
    unsigned int functionHit;                                       // Functions hit
    unsigned int functionTotal;                                     // Total functions
    unsigned int branchHit;                                         // Branches hit
    unsigned int branchTotal;                                       // Total branches
    unsigned int lineHit;                                           // Lines hit
    unsigned int lineTotal;                                         // Total lines
} TestCvgSummaryModule;

// Helper to render hit/total and percentage
static String *
testCvgSummaryValue(const unsigned int hit, const unsigned int total)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(UINT, hit);
        FUNCTION_LOG_PARAM(UINT, total);
    FUNCTION_LOG_END();

    // ASSERT(hit <= total);

    String *const result = strNew();

    // If total zero
    if (total == 0)
    {
        strCatZ(result, "---");
    }
    // Else render value
    else
    {
        strCatFmt(result, "%u/%u (", hit, total);

        if (hit == total)
            strCatZ(result, "100.0");
        else
            strCatFmt(result, "%.2f", (float)hit * 100 / (float)total);

        strCatZ(result, "%)");
    }

    FUNCTION_LOG_RETURN(STRING, result);
}

static String *
testCvgSummary(const TestCoverage *const coverage)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(TEST_COVERAGE, coverage);
    FUNCTION_LOG_END();

    ASSERT(coverage != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Track totals
        unsigned int functionHit = 0;
        unsigned int functionTotal = 0;
        unsigned int branchHit = 0;
        unsigned int branchTotal = 0;
        unsigned int lineHit = 0;
        unsigned int lineTotal = 0;

        // Aggregate modules
        List *const moduleList = lstNewP(sizeof(TestCvgSummaryModule), .comparator = lstComparatorStr);

        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            const TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);

            // Filter out anything beginning with test/ or doc/
            if (strBeginsWithZ(file->name, "test/") || strBeginsWithZ(file->name, "doc/"))
                continue;

            const String *const moduleName = strSub(strPath(file->name), sizeof("src/") - 1);

            TestCvgSummaryModule *module = lstFind(moduleList, &moduleName);

            if (module == NULL)
            {
                const TestCvgSummaryModule moduleNew = {.name = moduleName};
                lstAdd(moduleList, &moduleNew);

                module = lstFind(moduleList, &moduleName);
                ASSERT(module != NULL);
            }

            module->functionHit += file->functionHit;
            functionHit += file->functionHit;
            module->functionTotal += file->functionTotal;
            functionTotal += file->functionTotal;
            module->branchHit += file->branchHit;
            branchHit += file->branchHit;
            module->branchTotal += file->branchTotal;
            branchTotal += file->branchTotal;
            module->lineHit += file->lineHit;
            lineHit += file->lineHit;
            module->lineTotal += file->lineTotal;
            lineTotal += file->lineTotal;
        }

        lstSort(moduleList, sortOrderAsc);

        // Output modules
        for (unsigned int moduleIdx = 0; moduleIdx < lstSize(moduleList); moduleIdx++)
        {
            const TestCvgSummaryModule *const module = lstGet(moduleList, moduleIdx);

            strCatFmt(
                result, TEST_CVG_SUMMARY "\n", strZ(module->name),
                strZ(testCvgSummaryValue(module->functionHit, module->functionTotal)),
                strZ(testCvgSummaryValue(module->branchHit, module->branchTotal)),
                strZ(testCvgSummaryValue(module->lineHit, module->lineTotal)));
        }

        strCatFmt(
            result, TEST_CVG_SUMMARY, "TOTAL", strZ(testCvgSummaryValue(functionHit, functionTotal)),
            strZ(testCvgSummaryValue(branchHit, branchTotal)), strZ(testCvgSummaryValue(lineHit, lineTotal)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
int
testCvgGenerate(
    const String *const pathRepo, const String *const pathTest, const String *const vm, const bool coverageSummary,
    const StringList *const moduleList)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
        FUNCTION_LOG_PARAM(STRING, pathTest);
        FUNCTION_LOG_PARAM(STRING, vm);
        FUNCTION_LOG_PARAM(BOOL, coverageSummary);
        FUNCTION_LOG_PARAM(STRING_LIST, moduleList);
    FUNCTION_LOG_END();

    ASSERT(pathRepo != NULL);
    ASSERT(pathTest != NULL);
    ASSERT(vm != NULL);
    ASSERT(moduleList != NULL);

    int result = 0;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get test module list
        const TestDef testDef = testDefParse(storagePosixNewP(pathRepo));

        // Build a list of covered code modules
        StringList *const coverageList = strLstNew();

        for (unsigned int moduleIdx = 0; moduleIdx < strLstSize(moduleList); moduleIdx++)
        {
            const String *const moduleName = strLstGet(moduleList, moduleIdx);
            const TestDefModule *const module = lstFind(testDef.moduleList, &moduleName);
            CHECK_FMT(ParamInvalidError, module != NULL, "'%s' is not a valid test", strZ(moduleName));

            for (unsigned int coverageIdx = 0; coverageIdx < lstSize(module->coverageList); coverageIdx++)
            {
                const TestDefCoverage *const coverage = lstGet(module->coverageList, coverageIdx);

                if (coverage->coverable)
                    strLstAddIfMissing(coverageList, coverage->name);
            }
        }

        // Remove code modules that require an untested module for coverage
        for (unsigned int moduleIdx = 0; moduleIdx < lstSize(testDef.moduleList); moduleIdx++)
        {
            const TestDefModule *const module = lstGet(testDef.moduleList, moduleIdx);

            for (unsigned int coverageIdx = 0; coverageIdx < lstSize(module->coverageList); coverageIdx++)
            {
                const TestDefCoverage *const coverage = lstGet(module->coverageList, coverageIdx);

                if (strLstExists(coverageList, coverage->name) && !strLstExists(moduleList, module->name))
                {
                    LOG_WARN_FMT("module '%s' did not have all tests run required for coverage", strZ(coverage->name));
                    strLstRemove(coverageList, coverage->name);
                }
            }
        }

        // Update code module names as they will appear in coverage
        StringList *const coverageModList = strLstNew();

        for (unsigned int coverageIdx = 0; coverageIdx < strLstSize(coverageList); coverageIdx++)
        {
            String *coverageName = strLstGet(coverageList, coverageIdx);

            if (strBeginsWithZ(coverageName, "test/"))
                coverageName = strNewFmt("test/src/%s.c", strZ(strSub(coverageName, 5)));
            else if (strBeginsWithZ(coverageName, "doc/"))
                coverageName = strNewFmt("doc/src/%s.c", strZ(strSub(coverageName, 4)));
            else
                coverageName = strNewFmt("src/%s.c", strZ(coverageName));

            if (strEndsWithZ(coverageName, ".vendor.c"))
                coverageName = strNewFmt("%s.inc", strZ(coverageName));

            strLstAdd(coverageModList, coverageName);
        }

        // Combine gcc json files
        const Storage *const storageRepo = storagePosixNewP(pathRepo, .write = true);
        const Storage *const storageTest = storagePosixNewP(pathTest);
        const String *const pathCoverageRaw = storagePathP(storageRepo, STRDEF("test/result/coverage/raw"));
        const StringList *const jsonList = storageListP(
            storageRepo, pathCoverageRaw, .errorOnMissing = true, .expression = STRDEF("\\.json$"));
        TestCoverage *const coverage = testCvgNew();

        for (unsigned int jsonIdx = 0; jsonIdx < strLstSize(jsonList); jsonIdx++)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                testCvgMerge(
                    coverage,
                    strNewBuf(
                        storageGetP(
                            storageNewReadP(
                                storageRepo, strNewFmt("%s/%s", strZ(pathCoverageRaw), strZ(strLstGet(jsonList, jsonIdx)))))),
                    coverageModList);
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Coverage exceptions
        String *const regExpBranchStr = strNewFmt(
            "\\s{4}[A-Z][A-Z0-9_]+\\([^\\?]*\\)|\\s{4}(ASSERT|CHECK|CHECK_FMT|assert|switch\\s)\\(|\\{\\+{0,1}(%s%s)",
            coverageSummary ? "uncoverable_branch" : "uncover(ed|able)_branch", strEqZ(vm, "none") ? "|vm_covered" : "");
        RegExp *const regExpBranch = regExpNew(regExpBranchStr);
        String *const regExpLineStr = strNewFmt(
            "\\{\\+{0,1}(%s%s)[^_]", coverageSummary ? "uncoverable" : "uncover(ed|able)",
            strEqZ(vm, "none") ? "|vm_covered" : "");
        RegExp *const regExpLine = regExpNew(regExpLineStr);
        RegExp *const regExpLog = regExpNew(STRDEF("\\s+FUNCTION_(LOG|TEST)_(VOID|BEGIN|END|PARAM(|_P|_PP))\\("));
        RegExp *const regExpLogReturn = regExpNew(STRDEF("\\s+FUNCTION_(LOG|TEST)_RETURN_VOID\\("));

        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            MEM_CONTEXT_TEMP_BEGIN()
            {
                unsigned int lineIdx = 0;
                const TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);
                const StringList *const lineTextList = strLstNewSplitZ(
                    strNewBuf(storageGetP(storageNewReadP(storageTest, strNewFmt("repo/%s", strZ(file->name))))), "\n");

                while (lineIdx < lstSize(file->lineList))
                {
                    ASSERT(lineIdx < strLstSize(lineTextList));
                    TestCoverageLine *const line = lstGet(file->lineList, lineIdx);

                    // Remove covered lines for debug logging. These are not very interesting for coverage reporting.
                    if (line->hit != 0)
                    {
                        if (regExpMatch(regExpLog, strLstGet(lineTextList, line->no - 1)) ||
                            (regExpMatch(regExpLogReturn, strLstGet(lineTextList, line->no - 1)) &&
                             strEqZ(strLstGet(lineTextList, line->no), "}")))
                        {
                            lstRemoveIdx(file->lineList, lineIdx);
                            continue;
                        }
                    }

                    // If not covered then check for line coverage exceptions
                    if (line->hit == 0 && regExpMatch(regExpLine, strLstGet(lineTextList, line->no - 1)))
                        line->hit = 1;

                    // If not covered then check for branch coverage exceptions
                    if (line->branchList != NULL && regExpMatch(regExpBranch, strLstGet(lineTextList, line->no - 1)))
                    {
                        // Remove branch coverage so it is not reported
                        lstFree(line->branchList);
                        line->branchList = NULL;
                    }

                    lineIdx++;
                }
            }
            MEM_CONTEXT_TEMP_END();
        }

        // Calculate totals
        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);

            file->functionHit = 0;
            file->functionTotal = lstSize(file->functionList);
            file->branchHit = 0;
            file->branchTotal = 0;
            file->lineHit = 0;
            file->lineTotal = lstSize(file->lineList);

            for (unsigned int lineIdx = 0; lineIdx < lstSize(file->lineList); lineIdx++)
            {
                const TestCoverageLine *const line = lstGet(file->lineList, lineIdx);

                if (line->hit > 0)
                    file->lineHit++;

                if (line->branchList != NULL)
                {
                    for (unsigned int branchIdx = 0; branchIdx < lstSize(line->branchList); branchIdx++)
                    {
                        file->branchTotal++;

                        if (*(unsigned int *)lstGet(line->branchList, branchIdx) > 0)
                            file->branchHit++;
                    }
                }
            }

            for (unsigned int functionIdx = 0; functionIdx < lstSize(file->functionList); functionIdx++)
            {
                if (((TestCoverageFunction *)lstGet(file->functionList, functionIdx))->hit > 0)
                    file->functionHit++;
            }
        }

        // Write coverage summary
        if (coverageSummary)
        {
            CHECK(OptionInvalidError, !strEqZ(vm, "none"), "coverage summary must be run in vm");

            storagePutP(
                storageNewWriteP(storageRepo, STRDEF("doc/xml/auto/metric-coverage-report.auto.xml")),
                BUFSTR(testCvgSummary(coverage)));
        }

        // Warn on missing coverage
        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);

            if (file->lineHit != file->lineTotal || file->branchHit != file->branchTotal)
            {
                LOG_WARN_FMT(
                    "module '%s' is not fully covered (%u/%u lines, %u/%u branches)", strZ(file->name), file->lineHit,
                    file->lineTotal, file->branchHit, file->branchTotal);
                result++;
            }
        }

        // Filter out covered lines/branches
        for (unsigned int fileIdx = 0; fileIdx < lstSize(coverage->fileList); fileIdx++)
        {
            const TestCoverageFile *const file = lstGet(coverage->fileList, fileIdx);
            unsigned int lineIdx = 0;

            while (lineIdx < lstSize(file->lineList))
            {
                const TestCoverageLine *const line = lstGet(file->lineList, lineIdx);
                bool covered = line->hit != 0;

                if (covered && line->branchList != NULL)
                {
                    for (unsigned int branchIdx = 0; branchIdx < lstSize(line->branchList); branchIdx++)
                    {
                        if ((*(unsigned int *)lstGet(line->branchList, branchIdx)) == 0)
                        {
                            covered = false;
                            break;
                        }
                    }
                }

                // If line is covered remove it from the report
                if (covered)
                {
                    lstRemoveIdx(file->lineList, lineIdx);
                }
                // Else check the next line
                else
                    lineIdx++;
            }
        }

        // Write coverage report
        storagePutP(
            storageNewWriteP(storageRepo, STRDEF("test/result/coverage/coverage.html")),
            BUFSTR(testCvgReport(coverage, pathTest)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INT, result);
}
