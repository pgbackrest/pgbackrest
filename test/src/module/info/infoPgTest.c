/***********************************************************************************************************************************
Test PostgreSQL Info Handler
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test save callback
***********************************************************************************************************************************/
static void
testInfoBackupSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData)
{
    (void)data;

    if (infoSaveSection(infoSaveData, STRDEF("backup:current"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("backup:current"), STRDEF("20161219-212741F"), STRDEF("{}"));

    if (infoSaveSection(infoSaveData, STRDEF("db:backup"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("db:backup"), STRDEF("key"), STRDEF("\"value\""));

    if (infoSaveSection(infoSaveData, STRDEF("later"), sectionNext))
        infoSaveValue(infoSaveData, STRDEF("later"), STRDEF("key"), STRDEF("\"value\""));
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    // *****************************************************************************************************************************
    if (testBegin("infoPgNew(), infoPgNewInternal(), infoPgSet()"))
    {
        InfoPg *infoPg = NULL;

        TEST_ASSIGN(infoPg, infoPgNew(infoPgBackup, NULL), "infoPgNew(cipherTypeNone, NULL)");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 0, "  0 history");
        TEST_RESULT_STR(infoCipherPass(infoPgInfo(infoPg)), NULL, "  cipherPass NULL");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");

        TEST_ASSIGN(infoPg, infoPgNew(infoPgArchive, strNew("123xyz")), "infoPgNew(cipherTypeAes256Cbc, 123xyz)");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 0, "  0 history");
        TEST_RESULT_STR_Z(infoCipherPass(infoPgInfo(infoPg)), "123xyz", "  cipherPass set");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            infoPg,
            infoPgSet(
                infoPgNew(infoPgArchive, NULL), infoPgArchive, PG_VERSION_94, 6569239123849665679,
                pgCatalogTestVersion(PG_VERSION_94)),
            "infoPgSet - infoPgArchive");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "  1 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        InfoPgData pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 1, "  id set");
        TEST_RESULT_UINT(pgData.systemId, 6569239123849665679, "  system-id set");
        TEST_RESULT_UINT(pgData.version, PG_VERSION_94, "  version set");
        TEST_RESULT_UINT(pgData.catalogVersion, 0, "  catalog version not set for archive");

        TEST_ASSIGN(
            infoPg, infoPgSet(infoPg, infoPgArchive, PG_VERSION_95, 6569239123849665999, pgCatalogTestVersion(PG_VERSION_95)),
            "infoPgSet - infoPgArchive second db");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 2, "  2 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 2, "  current id updated");
        TEST_RESULT_UINT(pgData.systemId, 6569239123849665999, "  system-id updated");
        TEST_RESULT_UINT(pgData.version, PG_VERSION_95, "  version updated");
        TEST_RESULT_UINT(pgData.catalogVersion, 0, "  catalog version not set for archive");
        TEST_RESULT_STR(infoCipherPass(infoPgInfo(infoPg)), NULL, "  cipherPass not set");

        //--------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(
            infoPg,
            infoPgSet(
                infoPgNew(infoPgBackup, strNew("123xyz")), infoPgBackup, PG_VERSION_94, 6569239123849665679,
                pgCatalogTestVersion(PG_VERSION_94)),
            "infoPgSet - infoPgBackup");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "  1 history");
        TEST_RESULT_INT(infoPgDataCurrentId(infoPg), 0, "  0 historyCurrent");
        pgData = infoPgData(infoPg, infoPgDataCurrentId(infoPg));
        TEST_RESULT_INT(pgData.id, 1, "  id set");
        TEST_RESULT_UINT(pgData.systemId, 6569239123849665679, "  system-id set");
        TEST_RESULT_UINT(pgData.version, PG_VERSION_94, "  version set");
        TEST_RESULT_UINT(pgData.catalogVersion, 201409291, "  catalog version updated");
        TEST_RESULT_STR_Z(infoCipherPass(infoPgInfo(infoPg)), "123xyz", "  cipherPass set");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoPgNewLoad(), infoPgFree(), infoPgDataCurrent(), infoPgDataToLog(), infoPgAdd(), infoPgSave()"))
    {
        // Archive info
        //--------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = harnessInfoChecksumZ(
            "[backup:current]\n"
            "20161219-212741F={}\n"
            "\n"
            "[db]\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:backup]\n"
            "key=\"value\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-id\":6569239123849665679,\"db-version\":\"9.4\"}\n"
            "\n"
            "[later]\n"
            "key=\"value\"\n");

        String *callbackContent = strNew("");
        InfoPg *infoPg = NULL;

        TEST_ASSIGN(
            infoPg, infoPgNewLoad(ioBufferReadNew(contentLoad), infoPgArchive, harnessInfoLoadNewCallback, callbackContent),
            "load file");
        TEST_RESULT_STR_Z(
            callbackContent,
            "[backup:current] 20161219-212741F={}\n"
                "[db:backup] key=\"value\"\n"
                "[later] key=\"value\"\n",
            "    check callback content");
        TEST_RESULT_INT(lstSize(infoPg->pub.history), 1, "    history record added");

        InfoPgData pgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgData.id, 1, "    id set");
        TEST_RESULT_UINT(pgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_UINT(pgData.systemId, 6569239123849665679, "    system-id set");
        TEST_RESULT_INT(infoPgDataTotal(infoPg), 1, "    check pg data total");
        TEST_RESULT_STR_Z(infoPgArchiveId(infoPg, 0), "9.4-1", "    check pg archive id");
        TEST_RESULT_STR(infoPgCipherPass(infoPg), NULL, "    no cipher passphrase");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(infoPgSave(infoPg, ioBufferWriteNew(contentSave), testInfoBackupSaveCallback, (void *)1), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        // Backup info
        //--------------------------------------------------------------------------------------------------------------------------
        #define CONTENT_DB                                                                                                         \
            "[db]\n"                                                                                                               \
            "db-catalog-version=201510051\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=2\n"                                                                                                            \
            "db-system-id=6365925855999999999\n"                                                                                   \
            "db-version=\"9.5\"\n"

        #define CONTENT_DB_HISTORY                                                                                                 \
            "\n"                                                                                                                   \
            "[db:history]\n"                                                                                                       \
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"                 \
                "\"db-version\":\"9.4\"}\n"                                                                                        \
            "2={\"db-catalog-version\":201510051,\"db-control-version\":942,\"db-system-id\":6365925855999999999,"                 \
                "\"db-version\":\"9.5\"}\n"

        contentLoad = harnessInfoChecksumZ(
            CONTENT_DB
            "\n"
            "[backup:current]\n"
            "20161219-212741F={}\n"
            CONTENT_DB_HISTORY);

        callbackContent = strNew("");

        TEST_ASSIGN(infoPg, infoPgNewLoad(ioBufferReadNew(contentLoad), infoPgBackup, NULL, NULL), "load file");
        TEST_RESULT_STR_Z(callbackContent, "", "    check callback content");

        TEST_RESULT_INT(infoPgDataTotal(infoPg), 2, "    check pg data total");

        pgData = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgData.id, 2, "    id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_95, "    version set");
        TEST_RESULT_UINT(pgData.systemId, 6365925855999999999, "    system-id set");

        pgData = infoPgData(infoPg, 1);
        TEST_RESULT_INT(pgData.id, 1, "    id set");
        TEST_RESULT_INT(pgData.version, PG_VERSION_94, "    version set");
        TEST_RESULT_UINT(pgData.systemId, 6569239123849665679, "    system-id set");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(infoPgSave(infoPg, ioBufferWriteNew(contentSave), NULL, NULL), "info save");
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(harnessInfoChecksumZ(CONTENT_DB CONTENT_DB_HISTORY)), "   check save");

        // infoPgAdd
        //--------------------------------------------------------------------------------------------------------------------------
        pgData.id = 3;
        pgData.version = PG_VERSION_96;
        pgData.systemId = 6399999999999999999;
        TEST_RESULT_VOID(infoPgAdd(infoPg, &pgData), "infoPgAdd");

        InfoPgData pgDataTest = infoPgDataCurrent(infoPg);
        TEST_RESULT_INT(pgDataTest.id, 3, "    id set");
        TEST_RESULT_INT(pgDataTest.version, PG_VERSION_96, "    version set");
        TEST_RESULT_UINT(pgDataTest.systemId, 6399999999999999999, "    system-id set");

        // infoPgDataToLog
        //--------------------------------------------------------------------------------------------------------------------------
        // test max values
        pgDataTest.id = (unsigned int)4294967295;
        pgDataTest.version = (unsigned int)4294967295;
        pgDataTest.systemId = 18446744073709551615U;
        pgDataTest.catalogVersion = 200101011;

        TEST_RESULT_STR_Z(
            infoPgDataToLog(&pgDataTest),
            "{id: 4294967295, version: 4294967295, systemId: 18446744073709551615, catalogVersion: 200101011}",
            "    check max format");
    }
}
