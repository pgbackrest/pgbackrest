/***********************************************************************************************************************************
Test PostgreSQL Interface
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("pgVersionFromStr() and pgVersionToStr()"))
    {
        TEST_ERROR(pgVersionFromStr(strNew("9.3.4")), AssertError, "version 9.3.4 format is invalid");
        TEST_ERROR(pgVersionFromStr(strNew("abc")), AssertError, "version abc format is invalid");
        TEST_ERROR(pgVersionFromStr(NULL), AssertError, "assertion 'version != NULL' failed");

        TEST_RESULT_INT(pgVersionFromStr(strNew("10")), PG_VERSION_10, "valid pg version 10");
        TEST_RESULT_INT(pgVersionFromStr(strNew("9.6")), 90600, "valid pg version 9.6");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(pgVersionToStr(PG_VERSION_11)), "11", "infoPgVersionToString 11");
        TEST_RESULT_STR(strPtr(pgVersionToStr(PG_VERSION_96)), "9.6", "infoPgVersionToString 9.6");
        TEST_RESULT_STR(strPtr(pgVersionToStr(83456)), "8.34", "infoPgVersionToString 83456");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlVersion() and pgCatalogVersion()"))
    {
        TEST_ERROR(pgControlVersion(70300), AssertError, "invalid PostgreSQL version 70300");
        TEST_RESULT_UINT(pgControlVersion(PG_VERSION_83), 833, "8.3 control version");
        TEST_RESULT_UINT(pgControlVersion(PG_VERSION_11), 1100, "11 control version");

        TEST_ERROR(pgCatalogVersion(70900), AssertError, "invalid PostgreSQL version 70900");
        TEST_RESULT_UINT(pgCatalogVersion(PG_VERSION_83), 200711281, "8.3 catalog version");
        TEST_RESULT_UINT(pgCatalogVersion(PG_VERSION_11), 201809051, "11 catalog version");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlFromBuffer() and pgControlFromFile()"))
    {
        // Sanity test to ensure PG_VERSION_MAX has been updated
        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(pgInterface[0].version, PG_VERSION_MAX, "check max version");

        //--------------------------------------------------------------------------------------------------------------------------
        String *controlFile = strNew(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);

        // Create a bogus control file
        Buffer *result = bufNew(PG_CONTROL_SIZE);
        memset(bufPtr(result), 0, bufSize(result));
        bufUsedSet(result, bufSize(result));
        ((PgControlCommon *)bufPtr(result))->controlVersion = 501;
        ((PgControlCommon *)bufPtr(result))->catalogVersion = 19780101;

        TEST_ERROR(
            pgControlFromBuffer(result), VersionNotSupportedError,
            "unexpected control version = 501 and catalog version = 19780101\nHINT: is this version of PostgreSQL supported?");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR(pgControlTestToBuffer((PgControl){.version = 0}), AssertError, "invalid PostgreSQL version 0");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACE, .walSegmentSize = 1024 * 1024}));

        PgControl info = {0};
        TEST_ASSIGN(info, pgControlFromFile(storageTest, strNew(testPath())), "get control info v11");
        TEST_RESULT_INT(info.systemId, 0xFACEFACE, "   check system id");
        TEST_RESULT_INT(info.version, PG_VERSION_11, "   check version");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .walSegmentSize = 1024 * 1024}));

        TEST_ERROR(
            pgControlFromFile(storageTest, strNew(testPath())), FormatError,
            "wal segment size is 1048576 but must be 16777216 for PostgreSQL <= 10");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_95, .pageSize = 32 * 1024}));

        TEST_ERROR(pgControlFromFile(storageTest, strNew(testPath())), FormatError, "page size is 32768 but must be 8192");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_83, .systemId = 0xEFEFEFEFEF}));

        TEST_ASSIGN(info, pgControlFromFile(storageTest, strNew(testPath())), "get control info v83");
        TEST_RESULT_INT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_INT(info.version, PG_VERSION_83, "   check version");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgWalName()"))
    {
        TEST_RESULT_STR(strPtr(pgWalName(PG_VERSION_96)), "xlog", "check xlog name");
        TEST_RESULT_STR(strPtr(pgWalName(PG_VERSION_10)), "wal", "check wal name");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgWalFromBuffer() and pgWalFromFile()"))
    {
        String *walFile = strNewFmt("%s/0000000F0000000F0000000F", testPath());

        // Create a bogus control file, initially not in long format)
        // --------------------------------------------------------------------------------------------------------------------------
        Buffer *result = bufNew((size_t)16 * 1024 * 1024);
        memset(bufPtr(result), 0, bufSize(result));
        bufUsedSet(result, bufSize(result));
        ((PgWalCommon *)bufPtr(result))->magic = 777;

        TEST_ERROR(pgWalFromBuffer(result), FormatError, "first page header in WAL file is expected to be in long format");

        // Add the long flag so that the version will now error
        // --------------------------------------------------------------------------------------------------------------------------
        ((PgWalCommon *)bufPtr(result))->flag = PG_WAL_LONG_HEADER;

        TEST_ERROR(
            pgWalFromBuffer(result), VersionNotSupportedError,
            "unexpected WAL magic 777\n"
                "HINT: is this version of PostgreSQL supported?");

        //--------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        TEST_ERROR(pgWalTestToBuffer((PgWal){.version = 0}, result), AssertError, "invalid PostgreSQL version 0");

        //--------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_11, .systemId = 0xECAFECAF}, result);
        storagePutNP(storageNewWriteNP(storageTest, walFile), result);

        PgWal info = {0};
        TEST_ASSIGN(info, pgWalFromFile(walFile), "get wal info v11");
        TEST_RESULT_INT(info.systemId, 0xECAFECAF, "   check system id");
        TEST_RESULT_INT(info.version, PG_VERSION_11, "   check version");

        //--------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        pgWalTestToBuffer((PgWal){.version = PG_VERSION_83, .systemId = 0xEAEAEAEA}, result);
        storagePutNP(storageNewWriteNP(storageTest, walFile), result);

        TEST_ASSIGN(info, pgWalFromFile(walFile), "get wal info v8.3");
        TEST_RESULT_INT(info.systemId, 0xEAEAEAEA, "   check system id");
        TEST_RESULT_INT(info.version, PG_VERSION_83, "   check version");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlToLog()"))
    {
        PgControl pgControl =
        {
            .version = PG_VERSION_11,
            .systemId = 0xEFEFEFEFEF,
            .walSegmentSize= 16 * 1024 * 1024,
            .pageChecksum = true
        };

        TEST_RESULT_STR(
            strPtr(pgControlToLog(&pgControl)),
            "{version: 110000, systemId: 1030522662895, walSegmentSize: 16777216, pageChecksum: true}", "check log");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgWalToLog()"))
    {
        PgWal pgWal =
        {
            .version = PG_VERSION_10,
            .systemId = 0xFEFEFEFEFE
        };

        TEST_RESULT_STR(
            strPtr(pgWalToLog(&pgWal)),
            "{version: 100000, systemId: 1095199817470}", "check log");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
