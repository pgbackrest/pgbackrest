/***********************************************************************************************************************************
Info Helper Harness

The info helper caches info until the configuration is loaded again. Tests often change info files between statements and then run a
command again with the same configuration, e.g. to test a mismatched or missing info file. The command would then get the info
cached by an earlier statement rather than the changed file. To prevent this, cached info is cleared at the start of each test
statement and each statement loads the info files from the repo, the same as a new process would. The info objects are not freed
until the configuration is loaded again, so a test can still use info it got in an earlier statement.

Within a statement the cache works as it does in production, e.g. expire run by backup uses the info cached by backup. Each time
cached info is accessed it is compared with the info file. A mismatch means the file was changed outside the info helper or the
cached info was changed and not saved before it was accessed again. In either case the command is working with info that does not
match the repo and a test may still pass, so the mismatch is an error.
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_INFO_HELPER_H
#define TEST_HARNESS_INFO_HELPER_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Enable/disable clearing cached info at the start of each test statement (enabled by default)
void hrnInfoHelperStatementClearSet(bool enabled);

#endif
