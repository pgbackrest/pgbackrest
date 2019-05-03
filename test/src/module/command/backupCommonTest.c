/***********************************************************************************************************************************
Test Common Functions and Definitions for Backup and Expire Commands
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "storage/posix/storage.h"

#include <common/regExp.h>

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // *****************************************************************************************************************************
    if (testBegin("backupRegExp()"))
    {
        String *full = strNew("20181119-152138F");
        String *incr = strNew("20181119-152138F_20181119-152152I");
        String *diff = strNew("20181119-152138F_20181119-152152D");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(
            backupRegExpP(0),
            AssertError, "assertion 'param.full || param.differential || param.incremental' failed");

        // -------------------------------------------------------------------------------------------------------------------------
	    String *filter = backupRegExpP(.full = true);
        TEST_RESULT_STR(strPtr(filter), "^[0-9]{8}\\-[0-9]{6}F$", "full backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "    does not exactly match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "    does not exactly match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    exactly matches full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true, .incremental = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}I){0,1}$", "full and optional incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "    match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "    does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("12341234-123123F_12341234-123123IG")), false, "    does not match with trailing character");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("A12341234-123123F_12341234-123123I")), false, "    does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true, .differential = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}D){0,1}$", "full and optional diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "    does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "    match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.full = true,  .incremental = true, .differential = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F(\\_[0-9]{8}\\-[0-9]{6}(D|I)){0,1}$", "full, optional diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "    match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "    match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), true, "    match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.incremental = true, .differential = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}(D|I)$", "diff and incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "   match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "   match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("12341234-123123F_12341234-123123DG")), false, "   does not match with trailing character");
        TEST_RESULT_BOOL(
            regExpMatchOne(
                filter, strNew("A12341234-123123F_12341234-123123I")), false, "   does not match with leading character");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.incremental = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}I$", "incr backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), true, "   match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), false, "   does not match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");

        // -------------------------------------------------------------------------------------------------------------------------
        filter = backupRegExpP(.differential = true);
        TEST_RESULT_STR(
            strPtr(filter),
            "^[0-9]{8}\\-[0-9]{6}F\\_[0-9]{8}\\-[0-9]{6}D$", "diff backup regex with anchors");
        TEST_RESULT_BOOL(regExpMatchOne(filter, incr), false, "   does not match incr");
        TEST_RESULT_BOOL(regExpMatchOne(filter, diff), true, "   match diff");
        TEST_RESULT_BOOL(regExpMatchOne(filter, full), false, "   does not match full");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
