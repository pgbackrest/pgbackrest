/***********************************************************************************************************************************
Test Tape Archive
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    // Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("tarHdrReadU64()"))
    {
        TEST_TITLE("field full of octal digits");

        TEST_RESULT_UINT(tarHdrReadU64("77777777", 8), 077777777, "check octal");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid octal returns invalid result");

        TEST_RESULT_UINT(tarHdrReadU64("8", 1), 0, "invalid octal");
    }

    // *****************************************************************************************************************************
    if (testBegin("tar"))
    {
        TEST_TITLE("size errors");

        char nameLong[TAR_HEADER_DATA_NAME_SIZE + 1] = {0};
        memset(nameLong, 'a', TAR_HEADER_DATA_NAME_SIZE);

        TEST_ERROR_FMT(tarHdrNewP(.name = STR(nameLong)), FormatError, "file name '%s' is too long for the tar format", nameLong);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complete header");

        TarHeader *header = NULL;

        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("test"), .size = UINT64_MAX, .timeModified = 1640460255, .mode = 0640, .userId = 0,
            .groupId = 777777777), "new header");

        TEST_RESULT_STR_Z(tarHdrName(header), "test", "check name");
        TEST_RESULT_UINT(tarHdrSize(header), UINT64_MAX, "check size");

        TEST_RESULT_Z(header->data.name, "test", "check data name");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.size, TAR_HEADER_DATA_SIZE_SIZE), UINT64_MAX, "check data size");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mtime, TAR_HEADER_DATA_MTIME_SIZE), 1640460255, "check data time");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mode, TAR_HEADER_DATA_MODE_SIZE), 0640, "check data mode");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.uid, TAR_HEADER_DATA_UID_SIZE), 0, "check data uid");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.gid, TAR_HEADER_DATA_GID_SIZE), 777777777, "check data gid");

        TEST_RESULT_STR_Z(tarHdrToLog(header), "{name: test, size: 18446744073709551615}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        // TEST_TITLE("check range");

        // char field[12];

        // for (uint64_t idx = 0; idx < 1000000; idx++)
        // {
        //     memset(field, 0, sizeof(field));

        //     tarHdrWriteU64(field, sizeof(field), idx);

        //     if (tarHdrReadU64(field, sizeof(field)) != idx)
        //         THROW_FMT(AssertError, "failed on %" PRIu64, idx);
        // }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
