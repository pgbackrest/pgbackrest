/***********************************************************************************************************************************
Test Manifest Info Handler
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("infoManifestNew()"))
    {
        // Make sure the size of structs doesn't change without us knowing about it
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(sizeof(InfoManifestFile), TEST_64BIT() ? 88 : 64, "check size of InfoManifestFile");

        // Create pg directory and generate minimal manifest
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storageTest, strNew("pg"), .mode = 0700, .noParentCreate = true);

        Storage *storagePg = storagePosixNew(
            strNewFmt("%s/pg", testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, false, NULL);
        Storage *storagePgWrite =  storagePosixNew(
            strNewFmt("%s/pg", testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

        storagePut(storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400), BUFSTRDEF("9.4"));
        storagePathCreateP(storagePgWrite, strNew(PG_PREFIX_PGSQLTMP), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PREFIX_PGSQLTMP "2"), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGDYNSHMEM), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGNOTIFY), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGREPLSLOT), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSERIAL), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSNAPSHOTS), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSTATTMP), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSUBTRANS), .mode = 0700, .noParentCreate = true);

        InfoManifest *manifest = NULL;
        TEST_ASSIGN(manifest, infoManifestNew(storagePg, PG_VERSION_90), "create manifest");
        // !!! CHECK THE MANIFEST HERE TO DETERMINE IF EXCLUSIONS ARE WORKING
        (void)manifest;

        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_GLOBAL), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_GLOBAL "/1"), .mode = 0700, .noParentCreate = true);
        THROW_ON_SYS_ERROR(
            system(strPtr(strNewFmt("ln -s /tmp %s/pg/pg_test", testPath()))) != 0, FileWriteError, "unable to create link");

        TEST_ASSIGN(manifest, infoManifestNew(storagePg, PG_VERSION_94), "create manifest");
    }
}
