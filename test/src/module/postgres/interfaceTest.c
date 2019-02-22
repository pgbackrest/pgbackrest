/***********************************************************************************************************************************
Test PostgreSQL Interface
***********************************************************************************************************************************/
#include "storage/driver/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storageDriverPosixInterface(
        storageDriverPosixNew(strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL));

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
    if (testBegin("pgControlFromBuffer() and pgControlFromFile()"))
    {
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
        TEST_ERROR(pgControlTestToBuffer((PgControl){.version = 0}), AssertError, "invalid version 0");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_11, .systemId = 0xFACEFACE, .walSegmentSize = 1024 * 1024}));

        PgControl info = {0};
        TEST_ASSIGN(info, pgControlFromFile(strNew(testPath())), "get control info v11");
        TEST_RESULT_INT(info.systemId, 0xFACEFACE, "   check system id");
        TEST_RESULT_INT(info.controlVersion, 1100, "   check control version");
        TEST_RESULT_INT(info.catalogVersion, 201809051, "   check catalog version");
        TEST_RESULT_INT(info.version, PG_VERSION_11, "   check version");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_93, .walSegmentSize = 1024 * 1024}));

        TEST_ERROR(
            pgControlFromFile(strNew(testPath())), FormatError,
            "wal segment size is 1048576 but must be 16777216 for PostgreSQL <= 10");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_95, .pageSize = 32 * 1024}));

        TEST_ERROR(pgControlFromFile(strNew(testPath())), FormatError, "page size is 32768 but must be 8192");

        //--------------------------------------------------------------------------------------------------------------------------
        storagePutNP(
            storageNewWriteNP(storageTest, controlFile),
            pgControlTestToBuffer((PgControl){.version = PG_VERSION_83, .systemId = 0xEFEFEFEFEF}));

        TEST_ASSIGN(info, pgControlFromFile(strNew(testPath())), "get control info v83");
        TEST_RESULT_INT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_INT(info.controlVersion, 833, "   check control version");
        TEST_RESULT_INT(info.catalogVersion, 200711281, "   check catalog version");
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

    FUNCTION_HARNESS_RESULT_VOID();
}
