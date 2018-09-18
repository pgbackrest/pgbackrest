/***********************************************************************************************************************************
Test PostgreSQL Info
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
    if (testBegin("pgVersionMap()"))
    {
        TEST_RESULT_INT(pgVersionMap( 833, 200711281), PG_VERSION_83, "   check version 8.3");
        TEST_RESULT_INT(pgVersionMap( 843, 200904091), PG_VERSION_84, "   check version 8.4");
        TEST_RESULT_INT(pgVersionMap( 903, 201008051), PG_VERSION_90, "   check version 9.0");
        TEST_RESULT_INT(pgVersionMap( 903, 201105231), PG_VERSION_91, "   check version 9.1");
        TEST_RESULT_INT(pgVersionMap( 922, 201204301), PG_VERSION_92, "   check version 9.2");
        TEST_RESULT_INT(pgVersionMap( 937, 201306121), PG_VERSION_93, "   check version 9.3");
        TEST_RESULT_INT(pgVersionMap( 942, 201409291), PG_VERSION_94, "   check version 9.4");
        TEST_RESULT_INT(pgVersionMap( 942, 201510051), PG_VERSION_95, "   check version 9.5");
        TEST_RESULT_INT(pgVersionMap( 960, 201608131), PG_VERSION_96, "   check version 9.6");
        TEST_RESULT_INT(pgVersionMap(1002, 201707211), PG_VERSION_10, "   check version 10");
        TEST_RESULT_INT(pgVersionMap(1100, 201806231), PG_VERSION_11, "   check version 11");

        // -------------------------------------------------------------------------------------------------------------------------
        #define MAP_ERROR                                                                                                          \
            "unexpected control version = %d and catalog version = 0\n"                                                            \
            "HINT: is this version of PostgreSQL supported?"

        TEST_ERROR_FMT(pgVersionMap(   0, 0), VersionNotSupportedError, MAP_ERROR,    0);
        TEST_ERROR_FMT(pgVersionMap( 833, 0), VersionNotSupportedError, MAP_ERROR,  833);
        TEST_ERROR_FMT(pgVersionMap( 843, 0), VersionNotSupportedError, MAP_ERROR,  843);
        TEST_ERROR_FMT(pgVersionMap( 903, 0), VersionNotSupportedError, MAP_ERROR,  903);
        TEST_ERROR_FMT(pgVersionMap( 922, 0), VersionNotSupportedError, MAP_ERROR,  922);
        TEST_ERROR_FMT(pgVersionMap( 937, 0), VersionNotSupportedError, MAP_ERROR,  937);
        TEST_ERROR_FMT(pgVersionMap( 942, 0), VersionNotSupportedError, MAP_ERROR,  942);
        TEST_ERROR_FMT(pgVersionMap( 960, 0), VersionNotSupportedError, MAP_ERROR,  960);
        TEST_ERROR_FMT(pgVersionMap(1002, 0), VersionNotSupportedError, MAP_ERROR, 1002);
        TEST_ERROR_FMT(pgVersionMap(1100, 0), VersionNotSupportedError, MAP_ERROR, 1100);
    }

    // *****************************************************************************************************************************
    if (testBegin("pgVersionFromStr() and pgVersionToStr()"))
    {
        TEST_ERROR(pgVersionFromStr(strNew("9.3.4")), AssertError, "version 9.3.4 format is invalid");
        TEST_ERROR(pgVersionFromStr(strNew("abc")), AssertError, "version abc format is invalid");
        TEST_ERROR(pgVersionFromStr(NULL), AssertError, "function debug assertion 'version != NULL' failed");

        TEST_RESULT_INT(pgVersionFromStr(strNew("10")), PG_VERSION_10, "valid pg version 10");
        TEST_RESULT_INT(pgVersionFromStr(strNew("9.6")), 90600, "valid pg version 9.6");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR(strPtr(pgVersionToStr(PG_VERSION_11)), "11", "infoPgVersionToString 11");
        TEST_RESULT_STR(strPtr(pgVersionToStr(PG_VERSION_96)), "9.6", "infoPgVersionToString 9.6");
        TEST_RESULT_STR(strPtr(pgVersionToStr(83456)), "8.34", "infoPgVersionToString 83456");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlInfo()"))
    {
        String *controlFile = strNew(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);
        PgControlFile control = {.systemId = 0xFACEFACE, .controlVersion = 833, .catalogVersion = 200711281};
        Buffer *controlBuffer = bufNew(512);
        memset(bufPtr(controlBuffer), 0, bufSize(controlBuffer));
        memcpy(bufPtr(controlBuffer), &control, sizeof(PgControlFile));
        bufUsedSet(controlBuffer, bufSize(controlBuffer));
        storagePutNP(storageNewWriteNP(storageTest, controlFile), controlBuffer);

        PgControlInfo info = {0};
        TEST_ASSIGN(info, pgControlInfo(strNew(testPath())), "get control info");
        TEST_RESULT_INT(info.systemId, 0xFACEFACE, "   check system id");
        TEST_RESULT_INT(info.controlVersion, 833, "   check control version");
        TEST_RESULT_INT(info.catalogVersion, 200711281, "   check catalog version");
        TEST_RESULT_INT(info.version, PG_VERSION_83, "   check version");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
