/***********************************************************************************************************************************
Test PostgreSQL Info
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    Storage *storageTest = storageNewP(strNew(testPath()), .write = true);

    // -----------------------------------------------------------------------------------------------------------------------------
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
    }

    // -----------------------------------------------------------------------------------------------------------------------------
    if (testBegin("pgControlInfo()"))
    {
        String *controlFile = strNew(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL);
        PgControlFile control = {.systemId = 0xFACEFACE, .controlVersion = 833, .catalogVersion = 200711281};
        storagePutNP(storageNewWriteNP(storageTest, controlFile), bufNewC(sizeof(PgControlFile), &control));

        PgControlInfo info = {0};
        TEST_ASSIGN(info, pgControlInfo(strNew(testPath())), "get control info");
        TEST_RESULT_INT(info.systemId, 0xFACEFACE, "   check system id");
        TEST_RESULT_INT(info.controlVersion, 833, "   check control version");
        TEST_RESULT_INT(info.catalogVersion, 200711281, "   check catalog version");
        TEST_RESULT_INT(info.version, PG_VERSION_83, "   check version");
    }
}
