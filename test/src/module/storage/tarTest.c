/***********************************************************************************************************************************
Test Tape Archive
***********************************************************************************************************************************/
#include "common/crypto/hash.h"
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
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

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
    if (testBegin("TarHeader"))
    {
        TEST_TITLE("check sizes");

        TEST_RESULT_UINT(sizeof(TarHeaderData), 512, "sizeof TarHeader");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("size errors");

        char nameLong[TAR_HEADER_NAME_SIZE + 1] = {0};
        memset(nameLong, 'a', TAR_HEADER_NAME_SIZE);
        TEST_ERROR_FMT(tarHdrNewP(.name = STR(nameLong)), FormatError, "file name '%s' is too long for the tar format", nameLong);

        char userLong[TAR_HEADER_UNAME_SIZE + 1] = {0};
        memset(userLong, 'u', TAR_HEADER_UNAME_SIZE);
        TEST_ERROR_FMT(
            tarHdrNewP(.name = STRDEF("test"), .user = STR(userLong)), FormatError, "user '%s' is too long for the tar format",
            userLong);

        char groupLong[TAR_HEADER_GNAME_SIZE + 1] = {0};
        memset(groupLong, 'u', TAR_HEADER_GNAME_SIZE);
        TEST_ERROR_FMT(
            tarHdrNewP(.name = STRDEF("test"), .group = STR(groupLong)), FormatError, "group '%s' is too long for the tar format",
            groupLong);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("complete header");

        TarHeader *header = NULL;

        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("test"), .size = UINT64_MAX, .timeModified = 1640460255, .mode = 0640, .userId = 0,
            .user = STRDEF("user"), .groupId = 777777777, .group = STRDEF("group")), "new header");

        TEST_RESULT_STR_Z(tarHdrName(header), "test", "check name");
        TEST_RESULT_UINT(tarHdrSize(header), UINT64_MAX, "check size");

        TEST_RESULT_Z(header->data.name, "test", "check data name");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.size, TAR_HEADER_SIZE_SIZE), UINT64_MAX, "check data size");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mtime, TAR_HEADER_MTIME_SIZE), 1640460255, "check data time");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.mode, TAR_HEADER_MODE_SIZE), 0640, "check data mode");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.uid, TAR_HEADER_UID_SIZE), 0, "check data uid");
        TEST_RESULT_Z(header->data.uname, "user", "check data user");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.gid, TAR_HEADER_GID_SIZE), 777777777, "check data gid");
        TEST_RESULT_Z(header->data.gname, "group", "check data group");
        TEST_RESULT_UINT(tarHdrReadU64(header->data.chksum, TAR_HEADER_CHKSUM_SIZE), 7290, "check data checksum");

        TEST_RESULT_STR_Z(tarHdrToLog(header), "{name: test, size: 18446744073709551615}", "check log");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("multiple files in tar format");

        StorageWrite *tarFile = storageNewWriteP(storageTest, STRDEF("test.tar"));
        IoWrite *write = storageWriteIo(tarFile);
        ioWriteOpen(write);

        char file1[] = "file1test";
        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("file1"), .size = sizeof(file1) - 1, .timeModified = 1640460255, .mode = 0600,
            .userId = TEST_USER_ID, .groupId = TEST_GROUP_ID, ), "file with no user/group");
        TEST_RESULT_VOID(tarHdrWrite(header, write), "write file1 header");
        TEST_RESULT_VOID(ioWriteStr(write, STR(file1)), "write file1 contents");
        TEST_RESULT_VOID(tarHdrWritePadding(header, write), "write file1 padding");

        unsigned char file2[1024];
        memset(file2, 'x', sizeof(file2));

        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("file2"), .size = sizeof(file2), .timeModified = 1640460256, .mode = 0640,
            .userId = 0, .user = STRDEF(TEST_USER), .groupId = 0, .group = STRDEF(TEST_USER)), "file with user/group");
        TEST_RESULT_VOID(tarHdrWrite(header, write), "write file2 header");
        TEST_RESULT_VOID(ioWrite(write, BUF(file2, sizeof(file2))), "write file2 contents");
        TEST_RESULT_VOID(tarHdrWritePadding(header, write), "write file2 padding");

        char file3[] = "file3test";
        TEST_ASSIGN(
            header, tarHdrNewP(.name = STRDEF("file3"), .size = sizeof(file3) - 1, .timeModified = 1640460257, .mode = 0600),
            "file with root user/group");
        TEST_RESULT_VOID(tarHdrWrite(header, write), "write file3 header");
        TEST_RESULT_VOID(ioWriteStr(write, STR(file3)), "write file3 contents");
        TEST_RESULT_VOID(tarHdrWritePadding(header, write), "write file3 padding");

        TEST_RESULT_VOID(tarWritePadding(write), "write tar padding");
        TEST_RESULT_VOID(ioWriteFlush(write), "flush tar file");
        TEST_RESULT_VOID(ioWriteClose(write), "close tar file");

        // Create a directory and extract tar
        storagePathCreateP(storageTest, STRDEF("extract"));
        HRN_SYSTEM("tar -xf \"" TEST_PATH "/test.tar\" -C \"" TEST_PATH "/extract\"");

        // Check tar contents
        TEST_RESULT_UINT(storageInfoP(storageTest, STRDEF("test.tar")).size, 4608, "check tar size");

        HarnessStorageInfoListCallbackData callbackData = {.content = strNew(), .rootPathOmit = true};

        TEST_RESULT_VOID(
            storageInfoListP(storageTest, STRDEF("extract"), hrnStorageInfoListCallback, &callbackData, .sortOrder = sortOrderAsc),
            "tar extract contents");
        TEST_RESULT_STR_Z(
            callbackData.content,
            "file1 {file, s=9, m=0600, t=1640460255, u=" TEST_USER ", g=" TEST_GROUP "}\n"
            "file2 {file, s=1024, m=0640, t=1640460256, u=" TEST_USER ", g=" TEST_GROUP "}\n"
            "file3 {file, s=9, m=0600, t=1640460257, u=" TEST_USER ", g=" TEST_GROUP "}\n",
            "check content");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
