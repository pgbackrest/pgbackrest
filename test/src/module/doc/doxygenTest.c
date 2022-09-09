/***********************************************************************************************************************************
Test Doxygen documentation.
***********************************************************************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

// File paths used in the tests.  (and reused)
const char *sampleFile = TEST_PATH "/sample.c";                     // The test data file.
const char *expectedFile = TEST_PATH "/expected.c";                  // The expected result file.
const char *actualFile = TEST_PATH "/actual.c";                      // The actual output file
const char *diffFile = TEST_PATH "/sample.diff";                     // The differences between expected and actual file.

// Where to build doxyCfilter.
const char *doxyCfilter = TEST_PATH "/doxyCfilter";                 // Where doxyfilter should be built.
const char *doxyCfilterL = HRN_PATH_REPO "/doc/doxygen/doxyCfilter.l";  // Flex source code for the filter.
const char *doxyCfilterC = TEST_PATH "/doxyCfilter.c";              // The intermediate "C" file.

/***********************************************************************************************************************************
Open a test file
***********************************************************************************************************************************/
static int
testFileOpen(const char *fileName, int flags, int mode)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, fileName);
    FUNCTION_HARNESS_PARAM(INT, flags);
    FUNCTION_HARNESS_PARAM(INT, mode);

    FUNCTION_HARNESS_ASSERT(fileName != NULL);
    FUNCTION_HARNESS_END();

    int result = open(fileName, flags, mode);

    THROW_ON_SYS_ERROR_FMT(result == -1, FileOpenError, "unable to open test file '%s'", fileName);

    FUNCTION_HARNESS_RETURN(INT, result);
}


/***********************************************************************************************************************************
Load file contents into a buffer
***********************************************************************************************************************************/
static void
testFileLoad(const char *fileName, char *buffer, size_t bufferSize)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, fileName);
    FUNCTION_HARNESS_PARAM_P(CHARDATA, buffer);
    FUNCTION_HARNESS_PARAM(SIZE, bufferSize);

    FUNCTION_HARNESS_ASSERT(fileName != NULL);
    FUNCTION_HARNESS_ASSERT(buffer != NULL);
    FUNCTION_HARNESS_END();

    buffer[0] = 0;

    int fd = testFileOpen(fileName, O_RDONLY, 0);

    size_t totalBytes = 0;
    ssize_t actualBytes = 0;

    do
    {
        THROW_ON_SYS_ERROR_FMT(
                (actualBytes = read(fd, buffer, bufferSize - totalBytes)) == -1, FileOpenError, "unable to read file '%s'",
                fileName);

        totalBytes += (size_t)actualBytes;
    }
    while (actualBytes != 0);

    THROW_ON_SYS_ERROR_FMT(close(fd) == -1, FileOpenError, "unable to close file '%s'", fileName);

    // Remove final linefeed
    buffer[totalBytes - 1] = 0;

    FUNCTION_HARNESS_RETURN_VOID();
}


/***********************************************************************************************************************************
Write the string to a test file
***********************************************************************************************************************************/
static void
testFileSave(const char* fileName, const char *contents)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, fileName);
    FUNCTION_HARNESS_PARAM_P(CHARDATA, contents);

    FUNCTION_HARNESS_ASSERT(fileName != NULL);
    FUNCTION_HARNESS_ASSERT(contents != NULL);
    FUNCTION_HARNESS_END();

    int fd = testFileOpen(fileName, O_WRONLY|O_CREAT|O_TRUNC, 0777);

    size_t totalBytes = strlen(contents);
    ssize_t actualBytes = write(fd, contents, totalBytes);

    THROW_ON_SYS_ERROR_FMT(actualBytes == -1, FileOpenError, "unable to read file '%s'", fileName);
    ASSERT((ssize_t)totalBytes == actualBytes);

    THROW_ON_SYS_ERROR_FMT(close(fd) == -1, FileOpenError, "unable to close file '%s'", fileName);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Build the doxyCfilter by running flex and cc directly.  A bit of a hack.
***********************************************************************************************************************************/
static void
buildFilter(void)
{
    char cmd[32*1024];

    // Build the filter in the test directory. Assumes flex and cc are in the path.
    snprintf(cmd, sizeof(cmd), "flex -o %s %s; cc %s -lfl -o %s", doxyCfilterC, doxyCfilterL, doxyCfilterC, doxyCfilter);
    system(cmd);

    snprintf(cmd, sizeof(cmd), "echo /*/libfl.* /*/*/libfl.* /*/*/*/libfl.* >&2");  // where is the library on fedora 36?
    system(cmd);
}

/***********************************************************************************************************************************
Run doxy filter and compare actual results to expected results.
***********************************************************************************************************************************/
static void
testDoxyFilter(const char *sample, const char *expected, const char *description)
{
    FUNCTION_HARNESS_BEGIN();
    FUNCTION_HARNESS_PARAM(STRINGZ, sample);
    FUNCTION_HARNESS_PARAM(STRINGZ, expected);

    FUNCTION_HARNESS_ASSERT(sample != NULL);
    FUNCTION_HARNESS_ASSERT(expected != NULL);
    FUNCTION_HARNESS_END();

    // Save the expected results.
    testFileSave(sampleFile, sample);
    testFileSave(expectedFile, expected);

    // Run the filter to create the actual file output.
    char buffer[32*1024];
    snprintf(buffer, sizeof(buffer), "%s  %s  > %s", doxyCfilter, sampleFile, actualFile);
    system(buffer);

    // Generate differences for debugging.
    snprintf(buffer, sizeof(buffer), "diff %s %s > %s", expectedFile, actualFile, diffFile);
    system(buffer);

    // Check to see if there were any differences.
    testFileLoad(diffFile, buffer, sizeof(buffer));

    TEST_RESULT_Z("", buffer, description);

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void) {
    FUNCTION_HARNESS_VOID();

    /*******************************************************************************************************************************
    Validate filter output given various inputs.
    *******************************************************************************************************************************/
    if (testBegin("doxy_filter")) {

        // First, build the filter.
        TEST_RESULT_VOID(buildFilter(), "building doxyCfilter");

        // Do some quick sanity tests.
        testDoxyFilter("", "", "empty C file");
        testDoxyFilter("/**/\n", "/** @file */\n", "block comment at start of file");
        testDoxyFilter("//\n", "/// @file \n", "line comment at start of file");
        testDoxyFilter("\nint x; // following\n", "\nint x; ///<  following\n", "following comment");

        // A more elaborate kitchen sink test.
        const char *sample =
"/*----------------------------------------------------------------------------------------------------------------------------------\n"
"file doxygen comment. Block comment with custom banner consisting of dashes.\n"
"Similar to what Postgres uses.\n"
"- this single banner character should not be stripped,\n"
"- but two-or-more in a row (eg.\"--\") will be stripped.\n"
"----------------------------------------------------------------------------------------------------------------------------------*/\n"
"\n"
"/************************************************************************\n"
" * leading docgen comment. This is a more typical Javadoc banner style.\n"
" * Does doxygen add it to the file comment or to the struct comment?\n"
" **************************************************************************/\n"
"\n"
"// leading doxygen comment. This comment will only apply to the first forward reference.\n"
"// leading doxygen comment. Should we group adjacent statements together so this comment applies to both?\n"
"        struct forward; // trailing doxygen comment.\n"
"        void proc(void); // trailing doxygen comment.\n"
"\n"
"#include <stdio.h>\n"
"#include \"example.h\"\n"
"\n"
"/* leading doxygen comment. A macro which does nothing. */\n"
"#define DUMMY_MACRO 0\n"
"\n"
"// leading doxygen comment.  Multiline define.\n"
"#define MultiLine  \\\n"
"     {{{{{{{{{((((((( This could really mess up the level of braces if it were parsed. \\\n"
"     but of course we skip over it so we should be OK.\n"
"\n"
"        int value = 0;  // trailing doxygen comment.\n"
"\n"
"// leading doxygen comment. We want to document the something field.\n"
"        typedef struct Foo {\n"
"            int something;  // trailing doxygen comment.\n"
"        } Foo; // trailing doxygen comment.\n"
"\n"
"// leading doxygen comment. Describe the main function\n"
"        int main()\n"
"        {  // NOT a doxygen comment.\n"
"\n"
"            printf(\"Hello, World!\\n\"); // NOT a doxygen comment.\n"
"\n"
"            int a = 5; int b = 6;  // NOT a doxygen comment. Multi statements on single line.\n"
"\n"
"            /* NOT a doxygen comment. */\n"
"            return 0;\n"
"\n"
"        } // trailing doxygen comment.\n"
"\n"
"\n"
"/* Leading doxygen comment. */\n"
"        struct XXX *proc(struct XXX *bar) {\n"
"            int xxx;  // Not a doxygen comment.\n"
"            struct YYY {\n"
"                /* Not a doxygen comment. Doxygen only used for globals. */\n"
"                int x;\n"
"            }; // Not a doxygen comment. */\n"
"        } // Trailing doxygen comment.\n"
"\n";

        const char * expected =
"/**\n"
"file doxygen comment. Block comment with custom banner consisting of dashes.\n"
"Similar to what Postgres uses.\n"
"- this single banner character should not be stripped,\n"
"- but two-or-more in a row (eg.\"\") will be stripped.\n"
" @file */\n"
"\n"
"/*************************************************************************\n"
" * leading docgen comment. This is a more typical Javadoc banner style.\n"
" * Does doxygen add it to the file comment or to the struct comment?\n"
" **************************************************************************/\n"
"\n"
"/// leading doxygen comment. This comment will only apply to the first forward reference.\n"
"/// leading doxygen comment. Should we group adjacent statements together so this comment applies to both?\n"
"        struct forward; ///<  trailing doxygen comment.\n"
"        void proc(void); ///<  trailing doxygen comment.\n"
"\n"
"#include <stdio.h>\n"
"#include \"example.h\"\n"
"\n"
"/** leading doxygen comment. A macro which does nothing. */\n"
"#define DUMMY_MACRO 0\n"
"\n"
"/// leading doxygen comment.  Multiline define.\n"
"#define MultiLine  \\\n"
"     {{{{{{{{{((((((( This could really mess up the level of braces if it were parsed. \\\n"
"     but of course we skip over it so we should be OK.\n"
"\n"
"        int value = 0;  ///<  trailing doxygen comment.\n"
"\n"
"/// leading doxygen comment. We want to document the something field.\n"
"        typedef struct Foo {\n"
"            int something;  ///<  trailing doxygen comment.\n"
"        } Foo; ///<  trailing doxygen comment.\n"
"\n"
"/// leading doxygen comment. Describe the main function\n"
"        int main()\n"
"        {  // NOT a doxygen comment.\n"
"\n"
"            printf(\"Hello, World!\\n\"); // NOT a doxygen comment.\n"
"\n"
"            int a = 5; int b = 6;  // NOT a doxygen comment. Multi statements on single line.\n"
"\n"
"            /* NOT a doxygen comment. */\n"
"            return 0;\n"
"\n"
"        } ///<  trailing doxygen comment.\n"
"\n"
"\n"
"/** Leading doxygen comment. */\n"
"        struct XXX *proc(struct XXX *bar) {\n"
"            int xxx;  // Not a doxygen comment.\n"
"            struct YYY {\n"
"                /* Not a doxygen comment. Doxygen only used for globals. */\n"
"                int x;\n"
"            }; // Not a doxygen comment. */\n"
"        } ///<  Trailing doxygen comment.\n"
"\n";

        testDoxyFilter(sample, expected, "kitchen sink test of doxyCfilter");
    }
    FUNCTION_HARNESS_RETURN_VOID();
}
