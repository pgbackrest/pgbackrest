/***********************************************************************************************************************************
Test Restore Command
***********************************************************************************************************************************/
#include "common/compress/gzip/compress.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/io.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "postgres/version.h"
#include "storage/posix/storage.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"
#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Build a simple manifest for testing
***********************************************************************************************************************************/
static Manifest *
testManifestMinimal(const String *label, unsigned int pgVersion, const String *pgPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING, label);
        FUNCTION_HARNESS_PARAM(UINT, pgVersion);
        FUNCTION_HARNESS_PARAM(STRING, pgPath);
    FUNCTION_HARNESS_END();

    ASSERT(label != NULL);
    ASSERT(pgVersion != 0);
    ASSERT(pgPath != NULL);

    Manifest *result = NULL;

    MEM_CONTEXT_NEW_BEGIN("Manifest")
    {
        result = manifestNewInternal();
        result->info = infoNew(NULL);

        result->data.backupLabel = strDup(label);
        result->data.pgVersion = pgVersion;

        if (strEndsWithZ(label, "I"))
            result->data.backupType = backupTypeIncr;
        else if (strEndsWithZ(label, "D"))
            result->data.backupType = backupTypeDiff;
        else
            result->data.backupType = backupTypeFull;

        ManifestTarget targetBase = {.name = MANIFEST_TARGET_PGDATA_STR, .path = pgPath};
        manifestTargetAdd(result, &targetBase);
        ManifestPath pathBase = {.name = pgPath, .group = groupName(), .user = userName()};
        manifestPathAdd(result, &pathBase);
        ManifestFile fileVersion = {
            .name = STRDEF("pg_data/" PG_FILE_PGVERSION), .mode = 0600, .group = groupName(), .user = userName()};
        manifestFileAdd(result, &fileVersion);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_HARNESS_RESULT(MANIFEST, result);
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = strNew("20190509F");
        const String *repoFile1 = strNew("pg_data/testfile");

        // Start a protocol server to test the protocol directly
        Buffer *serverWrite = bufNew(8192);
        IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
        ioWriteOpen(serverWriteIo);

        ProtocolServer *server = protocolServerNew(
            strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

        bufUsedSet(serverWrite, 0);

        // Load Parameters
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        // Create the pg path
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("sparse-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                true, 0x10000000000UL, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "zero sparse 1TB file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("sparse-zero")).size, 0x10000000000UL, "    check size");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("normal-zero"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, NULL),
            true, "zero-length file");
        TEST_RESULT_UINT(storageInfoNP(storagePg(), strNew("normal-zero")).size, 0, "    check size");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a compressed encrypted repo file
        StorageWrite *ceRepoFile = storageNewWriteNP(
            storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s.gz", strPtr(repoFileReferenceFull), strPtr(repoFile1)));
        IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(ceRepoFile));
        ioFilterGroupAdd(filterGroup, gzipCompressNew(3, false));
        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, cipherTypeAes256Cbc, BUFSTRDEF("badpass"), NULL));

        storagePutNP(ceRepoFile, BUFSTRDEF("acefile"));

        TEST_ERROR(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("ffffffffffffffffffffffffffffffffffffffff"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            ChecksumError,
            "error restoring 'normal': actual checksum 'd1cd8a7d11daa26814b93eb604e1d49ab4b43770' does not match expected checksum"
                " 'ffffffffffffffffffffffffffffffffffffffff'");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, true, strNew("normal"), strNew("d1cd8a7d11daa26814b93eb604e1d49ab4b43770"),
                false, 7, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, false, false, strNew("badpass")),
            true, "copy file");

        StorageInfo info = storageInfoNP(storagePg(), strNew("normal"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 7, "    check size");
        TEST_RESULT_UINT(info.mode, 0600, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432154, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("normal"))))), "acefile", "    check contents");

        // -------------------------------------------------------------------------------------------------------------------------
        // Create a repo file
        storagePutNP(
            storageNewWriteNP(
                storageRepoWrite(), strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strPtr(repoFileReferenceFull), strPtr(repoFile1))),
            BUFSTRDEF("atestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta missing");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        size_t oldBufferSize = ioBufferSize();
        ioBufferSizeSet(4);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing");

        ioBufferSizeSet(oldBufferSize);

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            false, "sha1 delta force existing");

        // Change the existing file so it no longer matches by size
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("atestfile2"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, size differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        // Change the existing file so it no longer matches by content
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            true, "sha1 delta existing, content differs");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("delta"))))), "atestfile", "    check contents");

        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF("btestfile"));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432155, true, true, NULL),
            true, "delta force existing, timestamp differs");

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 9, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 1557432153, true, true, NULL),
            true, "delta force existing, timestamp after copy time");

        // Change the existing file to zero-length
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("delta")), BUFSTRDEF(""));

        TEST_RESULT_BOOL(
            restoreFile(
                repoFile1, repoFileReferenceFull, false, strNew("delta"), strNew("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"),
                false, 0, 1557432154, 0600, strNew(testUser()), strNew(testGroup()), 0, true, false, NULL),
            false, "sha1 delta existing, content differs");

        // Check protocol function directly
        // -------------------------------------------------------------------------------------------------------------------------
        VariantList *paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewBool(false));

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":true}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        info = storageInfoNP(storagePg(), strNew("protocol"));
        TEST_RESULT_BOOL(info.exists, true, "    check exists");
        TEST_RESULT_UINT(info.size, 9, "    check size");
        TEST_RESULT_UINT(info.mode, 0677, "    check mode");
        TEST_RESULT_UINT(info.timeModified, 1557432100, "    check time");
        TEST_RESULT_STR(strPtr(info.user), testUser(), "    check user");
        TEST_RESULT_STR(strPtr(info.group), testGroup(), "    check group");
        TEST_RESULT_STR(
            strPtr(strNewBuf(storageGetNP(storageNewReadNP(storagePg(), strNew("protocol"))))), "atestfile", "    check contents");

        paramList = varLstNew();
        varLstAdd(paramList, varNewStrZ("protocol"));
        varLstAdd(paramList, varNewUInt64(9));
        varLstAdd(paramList, varNewUInt64(1557432100));
        varLstAdd(paramList, varNewStrZ("9bc8ab2dda60ef4beed07d1e19ce0676d5edde67"));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, varNewStr(repoFile1));
        varLstAdd(paramList, varNewStr(repoFileReferenceFull));
        varLstAdd(paramList, varNewStrZ("0677"));
        varLstAdd(paramList, varNewStrZ(testUser()));
        varLstAdd(paramList, varNewStrZ(testGroup()));
        varLstAdd(paramList, varNewUInt64(1557432200));
        varLstAdd(paramList, varNewBool(true));
        varLstAdd(paramList, NULL);
        varLstAdd(paramList, varNewBool(false));
        varLstAdd(paramList, NULL);

        TEST_RESULT_BOOL(restoreProtocol(PROTOCOL_COMMAND_RESTORE_FILE_STR, paramList, server), true, "protocol restore file");
        TEST_RESULT_STR(strPtr(strNewBuf(serverWrite)), "{\"out\":false}\n", "    check result");
        bufUsedSet(serverWrite, 0);

        // Check invalid protocol function
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_BOOL(restoreProtocol(strNew(BOGUS_STR), paramList, server), false, "invalid function");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestMap()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F"), PG_VERSION_94, pgPath);

        // Remap data directory
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is not remapped");
        TEST_RESULT_STR_STR(
            manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is not remapped");

        // Now change pg1-path so the data directory gets remapped
        pgPath = strNewFmt("%s/pg2", testPath());

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "base directory is remapped");
        TEST_RESULT_STR_STR(manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, pgPath, "base directory is remapped");
        harnessLogResult(strPtr(strNewFmt("P00   INFO: remap data directory to '%s/pg2'", testPath())));

        // Remap tablespaces
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=bogus=/bogus");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(restoreManifestMap(manifest), TablespaceMapError, "unable to remap invalid tablespace 'bogus'");

        // Add some tablespaces
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_tblspc/1"), .path = STRDEF("/1"), .tablespaceId = 1, .tablespaceName = STRDEF("1"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_tblspc/1"), .destination = STRDEF("/1")});
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_tblspc/2"), .path = STRDEF("/2"), .tablespaceId = 2, .tablespaceName = STRDEF("ts2"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_tblspc/2"), .destination = STRDEF("/2")});

        // Error on different paths
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2");
        strLstAddZ(argList, "--tablespace-map=ts2=/ts2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(
            restoreManifestMap(manifest), TablespaceMapError, "tablespace remapped by name 'ts2' and id 2 with different paths");

        // Remap one tablespace using the id and another with the name
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=1=/1-2");
        strLstAddZ(argList, "--tablespace-map=ts2=/2-2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/1-2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/1-2", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-2", "    check tablespace 1 link");

        harnessLogResult(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/1-2'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-2'");

        // Remap a tablespace using just the id and map the rest with tablespace-map-all
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map=2=/2-3");
        strLstAddZ(argList, "--tablespace-map-all=/all");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all/1", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all/1", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/2-3", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/2-3", "    check tablespace 1 link");

        harnessLogResult(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/2-3'");

        // Remap all tablespaces with tablespace-map-all and update version to 9.2 to test warning
        manifest->data.pgVersion = PG_VERSION_92;

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--tablespace-map-all=/all2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap tablespaces");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/1"))->path, "/all2/1", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/1"))->destination, "/all2/1", "    check tablespace 1 link");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_tblspc/2"))->path, "/all2/ts2", "    check tablespace 1 target");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_tblspc/2"))->destination, "/all2/ts2", "    check tablespace 1 link");

        harnessLogResult(
            "P00   INFO: map tablespace 'pg_tblspc/1' to '/all2/1'\n"
            "P00   INFO: map tablespace 'pg_tblspc/2' to '/all2/ts2'\n"
            "P00   WARN: update pg_tablespace.spclocation with new tablespace locations for PostgreSQL <= 9.2");

        // Remap links
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-map=bogus=bogus");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(restoreManifestMap(manifest), LinkMapError, "unable to remap invalid link 'bogus'");

        // Add some links
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});
        manifestTargetAdd(
            manifest, &(ManifestTarget){.name = STRDEF("pg_data/pg_wal"), .path = STRDEF("/wal"),  .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_wal"), .destination = STRDEF("/wal")});

        // Remap both links
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-map=pg_hba.conf=../conf2/pg_hba2.conf");
        strLstAddZ(argList, "--link-map=pg_wal=/wal2");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remap links");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "    check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "    check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "    check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "    check link path");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "    check link dest");

        harnessLogResult(
            "P00   INFO: map link 'pg_hba.conf' to '../conf2/pg_hba2.conf'\n"
            "P00   INFO: map link 'pg_wal' to '/wal2'");

        // Leave all links as they are
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--link-all");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "leave links as they are");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->path, "../conf2", "    check link path");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))->file, "pg_hba2.conf", "    check link file");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf"))->destination, "../conf2/pg_hba2.conf", "    check link dest");
        TEST_RESULT_STR_Z(manifestTargetFind(manifest, STRDEF("pg_data/pg_wal"))->path, "/wal2", "    check link path");
        TEST_RESULT_STR_Z(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal"))->destination, "/wal2", "    check link dest");

        // Remove all links
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreManifestMap(manifest), "remove all links");
        TEST_ERROR(
            manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf")), AssertError,
            "unable to find 'pg_data/pg_hba.conf' in manifest target list");
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_hba.conf")), AssertError,
            "unable to find 'pg_data/pg_hba.conf' in manifest link list");
        TEST_ERROR(
            manifestTargetFind(manifest, STRDEF("pg_data/pg_wal")), AssertError,
            "unable to find 'pg_data/pg_wal' in manifest target list");
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("pg_data/pg_wal")), AssertError,
            "unable to find 'pg_data/pg_wal' in manifest link list");

        harnessLogResult(
            "P00   WARN: file link 'pg_hba.conf' will be restored as a file at the same location\n"
            "P00   WARN: contents of directory link 'pg_wal' will be restored in a directory at the same location");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreManifestOwner()"))
    {
        userInitInternal();

        const String *pgPath = strNewFmt("%s/pg", testPath());
        // const String *repoPath = strNewFmt("%s/repo", testPath());
        //
        // Owner is not root and all ownership is good
        // -------------------------------------------------------------------------------------------------------------------------
        // StringList *argList = strLstNew();
        // strLstAddZ(argList, "pgbackrest");
        // strLstAddZ(argList, "--stanza=test1");
        // strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        // strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        // strLstAddZ(argList, "restore");
        // harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        //
        // TEST_ERROR_FMT(restoreClean(manifest), PathMissingError, "unable to restore to missing path '%s/pg'", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);
        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with good ownership");

        // Owner is not root but has no user name
        // -------------------------------------------------------------------------------------------------------------------------
        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        userLocalData.groupName = NULL;
        userLocalData.userName = NULL;

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with no username");

        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   WARN: user '%s' in backup manifest mapped to current user\n"
                    "P00   WARN: group '%s' in backup manifest mapped to current group",
                    testUser(), testUser())));

        userInitInternal();

        // Owner is not root and some ownership is bad
        // -------------------------------------------------------------------------------------------------------------------------
        manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        ManifestPath path = {.name = STRDEF("pg_data/bogus_path"), .user = STRDEF("path-user-bogus")};
        manifestPathAdd(manifest, &path);
        ManifestFile file = {.name = STRDEF("pg_data/bogus_file"), .mode = 0600, .group = STRDEF("file-group-bogus")};
        manifestFileAdd(manifest, &file);

        TEST_RESULT_VOID(restoreManifestOwner(manifest), "non-root user with some bad ownership");

        harnessLogResult(
            "P00   WARN: unknown user in backup manifest mapped to current user\n"
            "P00   WARN: user 'path-user-bogus' in backup manifest mapped to current user\n"
            "P00   WARN: unknown group in backup manifest mapped to current group\n"
            "P00   WARN: group 'file-group-bogus' in backup manifest mapped to current group");
    }

    // *****************************************************************************************************************************
    if (testBegin("restoreClean()"))
    {
        userInitInternal();

        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());
        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-21275D"), PG_VERSION_96, pgPath);

        // Missing data directory
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(restoreClean(manifest), PathMissingError, "unable to restore to missing path '%s/pg'", testPath());

        // Directory with bad permissions/mode
        // -------------------------------------------------------------------------------------------------------------------------
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0600);

        userLocalData.userId = getuid() + 1;

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' not owned by current user", testPath());

        userLocalData.userRoot = true;

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        userInitInternal();

        TEST_ERROR_FMT(
            restoreClean(manifest), PathOpenError, "unable to restore to path '%s/pg' without rwx permissions", testPath());

        storagePathRemoveNP(storagePgWrite(), NULL);
        storagePathCreateP(storagePgWrite(), NULL, .mode = 0700);

        // Fail on restore with directory not empty
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);

        TEST_ERROR_FMT(
            restoreClean(manifest), PathNotEmptyError,
            "unable to restore to path '%s/pg' because it contains files\n"
                "HINT: try using --delta if this is what you intended.",
            testPath());

        // Succeed when all directories empty
        // -------------------------------------------------------------------------------------------------------------------------
        storageRemoveNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR);

        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/pg_hba.conf"), .path = STRDEF("../conf"), .file = STRDEF("pg_hba.conf"),
                .type = manifestTargetTypeLink});
        manifestLinkAdd(
            manifest, &(ManifestLink){.name = STRDEF("pg_data/pg_hba.conf"), .destination = STRDEF("../conf/pg_hba.conf")});

        storagePathCreateP(storageTest, STRDEF("conf"), .mode = 0700);

        TEST_RESULT_VOID(restoreClean(manifest), "normal restore");

        // Succeed when all directories empty and ignore recovery.conf
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(restoreClean(manifest), "normal restore no recovery.conf");

        storagePutNP(storageNewWriteNP(storagePgWrite(), PG_FILE_RECOVERYCONF_STR), NULL);
        TEST_RESULT_VOID(restoreClean(manifest), "normal restore ignore recovery.conf");

        // Delta restore allowed
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "--type=preserve");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        storagePathCreateP(storagePgWrite(), STRDEF("global"), .mode = 0700);
        storagePutNP(storageNewWriteNP(storagePgWrite(), STRDEF(PG_FILE_PGVERSION)), NULL);
        storagePutNP(storageNewWriteNP(storageTest, STRDEF("conf/pg_hba.conf")), NULL);

        TEST_RESULT_VOID(restoreClean(manifest), "delta restore");
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdRestore()"))
    {
        const String *pgPath = strNewFmt("%s/pg", testPath());
        const String *repoPath = strNewFmt("%s/repo", testPath());

        // Locality error
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--pg1-host=pg1");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // PGDATA missing
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(cmdRestore(), PathMissingError, "$PGDATA directory '%s/pg' does not exist", testPath());

        // Create PGDATA
        storagePathCreateNP(storagePgWrite(), strNew("pg"));

        // postmaster.pid is present
        // -------------------------------------------------------------------------------------------------------------------------
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("postmaster.pid")), NULL);

        TEST_ERROR_FMT(
            cmdRestore(), PostmasterRunningError,
            "unable to restore while PostgreSQL is running\n"
                "HINT: presence of 'postmaster.pid' in '%s/pg' indicates PostgreSQL is running.\n"
                "HINT: remove 'postmaster.pid' only if PostgreSQL is not running.",
            testPath());

        storageRemoveP(storagePgWrite(), strNew("postmaster.pid"), .errorOnMissing = true);

        // Error when no backups are present then add backups to backup.info
        // -------------------------------------------------------------------------------------------------------------------------
        String *infoBackupStr = strNew
        (
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR), harnessInfoChecksum(infoBackupStr)),
            "write backup info");

        TEST_ERROR_FMT(cmdRestore(), BackupSetInvalidError, "no backup sets to restore");

        infoBackupStr = strNew
        (
            "[backup:current]\n"
            "20161219-212741F={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000007000000000000001C\",\"backup-archive-stop\":\"00000007000000000000001C\","
            "\"backup-info-repo-size\":3159776,\"backup-info-repo-size-delta\":3159776,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":26897030,\"backup-timestamp-start\":1482182846,\"backup-timestamp-stop\":1482182861,"
            "\"backup-type\":\"full\",\"db-id\":1,\"option-archive-check\":true,\"option-archive-copy\":false,"
            "\"option-backup-standby\":false,\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,"
            "\"option-online\":true}\n"
            "20161219-212741F_20161219-212803D={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":\"00000008000000000000001E\",\"backup-archive-stop\":\"00000008000000000000001E\","
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"diff\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "20161219-212741F_20161219-212918I={\"backrest-format\":5,\"backrest-version\":\"2.04\","
            "\"backup-archive-start\":null,\"backup-archive-stop\":null,"
            "\"backup-info-repo-size\":3159811,\"backup-info-repo-size-delta\":15765,\"backup-info-size\":26897030,"
            "\"backup-info-size-delta\":163866,\"backup-prior\":\"20161219-212741F\",\"backup-reference\":[\"20161219-212741F\","
            "\"20161219-212741F_20161219-212803D\"],"
            "\"backup-timestamp-start\":1482182877,\"backup-timestamp-stop\":1482182883,\"backup-type\":\"incr\",\"db-id\":1,"
            "\"option-archive-check\":true,\"option-archive-copy\":false,\"option-backup-standby\":false,"
            "\"option-checksum-page\":false,\"option-compress\":true,\"option-hardlink\":false,\"option-online\":true}\n"
            "\n"
            "[db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=6569239123849665679\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[db:history]\n"
            "1={\"db-catalog-version\":201409291,\"db-control-version\":942,\"db-system-id\":6569239123849665679,"
                "\"db-version\":\"9.4\"}\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageRepoWrite(), INFO_BACKUP_PATH_FILE_STR), harnessInfoChecksum(infoBackupStr)),
            "write backup info");

        // Error on invalid backup set
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=BOGUS");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), BackupSetInvalidError, "backup set BOGUS is not valid");

        // Error on missing backup set
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F_20161219-212803D");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR_FMT(
            cmdRestore(), FileMissingError,
            "unable to load backup manifest file '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest' or"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest.copy':\n"
            "FileMissingError: unable to open missing file"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest' for read\n"
            "FileMissingError: unable to open missing file"
                " '%s/repo/backup/test1/20161219-212741F_20161219-212803D/backup.manifest.copy' for read",
            testPath(), testPath(), testPath(), testPath());

        Manifest *manifest = testManifestMinimal(STRDEF("20161219-212741F_20161219-212918I"), PG_VERSION_94, pgPath);

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" MANIFEST_FILE))));

        // PGDATA directory does not look valid
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--delta");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "restore --delta with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptDelta), false, "--delta set to false");
        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                        " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                        " exist in the destination directories the restore will be aborted.\n"
                    "P00   INFO: restore backup set 20161219-212741F_20161219-212918I",
                testPath())));

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew("backup.manifest")), NULL);
        TEST_RESULT_VOID(cmdRestore(), "restore --delta with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew("backup.manifest"), .errorOnMissing = true);
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");

        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--force");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "restore --force with invalid PGDATA");
        TEST_RESULT_BOOL(cfgOptionBool(cfgOptForce), false, "--force set to false");
        harnessLogResult(
            strPtr(
                strNewFmt(
                    "P00   WARN: --delta or --force specified but unable to find 'PG_VERSION' or 'backup.manifest' in '%s/pg' to"
                        " confirm that this is a valid $PGDATA directory.  --delta and --force have been disabled and if any files"
                        " exist in the destination directories the restore will be aborted.\n"
                    "P00   INFO: restore backup set 20161219-212741F_20161219-212918I",
                testPath())));

        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));
        storagePutNP(storageNewWriteNP(storagePgWrite(), strNew(PG_FILE_PGVERSION)), NULL);
        TEST_RESULT_VOID(cmdRestore(), "restore --force with valid PGDATA");
        storageRemoveP(storagePgWrite(), strNew(PG_FILE_PGVERSION), .errorOnMissing = true);
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");

        // Error on mismatched label
        // -------------------------------------------------------------------------------------------------------------------------
        const String *oldLabel = manifest->data.backupLabel;
        manifest->data.backupLabel = STRDEF("20161219-212741F");

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" MANIFEST_FILE))));

        TEST_ERROR(
            cmdRestore(), FormatError,
            "requested backup '20161219-212741F_20161219-212918I' and manifest label '20161219-212741F' do not match\n"
            "HINT: this indicates some sort of corruption (at the very least paths have been renamed).");

        manifest->data.backupLabel = oldLabel;

        manifestSave(
            manifest,
            storageWriteIo(
                storageNewWriteNP(storageRepoWrite(),
                strNew(STORAGE_REPO_BACKUP "/20161219-212741F_20161219-212918I/" MANIFEST_FILE))));

        // SUCCESS TEST FOR COVERAGE -- WILL BE REMOVED / MODIFIED AT SOME POINT
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s", strPtr(repoPath)));
        strLstAdd(argList, strNewFmt("--pg1-path=%s", strPtr(pgPath)));
        strLstAddZ(argList, "--set=20161219-212741F_20161219-212918I");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");
        harnessLogResult("P00   INFO: restore backup set 20161219-212741F_20161219-212918I");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
