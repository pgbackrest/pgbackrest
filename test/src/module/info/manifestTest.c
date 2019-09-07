/***********************************************************************************************************************************
Test Manifest Handler
***********************************************************************************************************************************/
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "info/infoBackup.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    Storage *storageTest = storagePosixNew(
        strNew(testPath()), STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL);

    // *****************************************************************************************************************************
    if (testBegin("struct sizes"))
    {
        // Make sure the size of structs doesn't change without us knowing about it
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(sizeof(ManifestLoadFound), TEST_64BIT() ? 1 : 1, "check size of ManifestLoadFound");
        TEST_RESULT_UINT(sizeof(ManifestPath), TEST_64BIT() ? 32 : 16, "check size of ManifestPath");
        TEST_RESULT_UINT(sizeof(ManifestFile), TEST_64BIT() ? 120 : 92, "check size of ManifestFile");
    }

    // *****************************************************************************************************************************
    if (testBegin("manifestNewLoad() and manifestSave()"))
    {
        Manifest *manifest = NULL;

        // Manifest with minimal features
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *contentLoad = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190808-163540F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-compress=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "master=true\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");

        TEST_ERROR(
            manifestTargetFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest target list");
        TEST_RESULT_STRZ(manifestData(manifest)->backupLabel, "20190808-163540F", "    check manifest data");

        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/pg/base"), NULL), "    update target no change");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/path2"), NULL), "    update target");
        TEST_RESULT_STRZ(
            manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, "/path2", "    check target path");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/pg/base"), NULL), "    fix target path");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STRSTR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        // Manifest with all features
        // -------------------------------------------------------------------------------------------------------------------------
        contentLoad = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-archive-start=\"000000030000028500000089\"\n"
            "backup-archive-stop=\"000000030000028500000089\"\n"
            "backup-label=\"20190818-084502F_20190820-084502D\"\n"
            "backup-lsn-start=\"285/89000028\"\n"
            "backup-lsn-stop=\"285/89001F88\"\n"
            "backup-prior=\"20190818-084502F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-backup-standby=false\n"
            "option-buffer-size=16384\n"
            "option-checksum-page=true\n"
            "option-compress=false\n"
            "option-compress-level=3\n"
            "option-compress-level-network=3\n"
            "option-delta=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "option-process-max=32\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"
            "pg_data/pg_stat={\"path\":\"../pg_stat\",\"type\":\"link\"}\n"
            "pg_tblspc/1={\"path\":\"/tblspc/ts1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"
            "\n"
            "[db]\n"
            "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"
            "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"
                ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"
                ",\"timestamp\":1565282115}\n"
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"
                ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"
                ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"
                ",\"timestamp\":1565282114}\n"
            "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "master=false\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:link]\n"
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"
            "pg_data/postgresql.conf={\"destination\":\"../pg_config/postgresql.conf\",\"group\":false,\"user\":\"user1\"}\n"
            "\n"
            "[target:link:default]\n"
            "group=\"group1\"\n"
            "user=false\n"
            "\n"
            "[target:path]\n"
            "pg_data={\"user\":\"user2\"}\n"
            "pg_data/base={\"group\":\"group2\"}\n"
            "pg_data/base/16384={\"mode\":\"0750\"}\n"
            "pg_data/base/32768={}\n"
            "pg_data/base/65536={\"user\":false}\n"
            "\n"
            "[target:path:default]\n"
            "group=false\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");

        TEST_RESULT_STRZ(manifestPgPath(STRDEF("pg_data")), NULL, "check pg_data path");
        TEST_RESULT_STRZ(manifestPgPath(STRDEF("pg_data/PG_VERSION")), "PG_VERSION", "check pg_data path/file");
        TEST_RESULT_STRZ(manifestPgPath(STRDEF("pg_tblspc/1")), "pg_tblspc/1", "check pg_tblspc path/file");

        const ManifestLink *link = NULL;
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest link list");
        TEST_ASSIGN(link, manifestLinkFind(manifest, STRDEF("pg_data/pg_stat")), "find link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat")), "    no update");
        TEST_RESULT_STRZ(link->destination, "../pg_stat", "    check link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat2")), "    update");
        TEST_RESULT_STRZ(link->destination, "../pg_stat2", "    check link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat")), "    fix link destination");

        const ManifestTarget *target = NULL;
        TEST_ASSIGN(target, manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf")), "find target");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, target->name, target->path, STRDEF("pg_hba2.conf")), "    update target file");
        TEST_RESULT_STRZ(target->file, "pg_hba2.conf", "    check target file");
        TEST_RESULT_VOID(manifestTargetUpdate(manifest, target->name, target->path, STRDEF("pg_hba.conf")), "    fix target file");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STRSTR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        TEST_RESULT_VOID(manifestLinkRemove(manifest, STRDEF("pg_data/pg_stat")), "remove link");
        TEST_ERROR(
            manifestLinkRemove(manifest, STRDEF("pg_data/pg_stat")), AssertError,
            "unable to remove 'pg_data/pg_stat' from manifest link list");

        TEST_RESULT_VOID(manifestTargetRemove(manifest, STRDEF("pg_data/pg_hba.conf")), "remove target");
        TEST_ERROR(
            manifestTargetRemove(manifest, STRDEF("pg_data/pg_hba.conf")), AssertError,
            "unable to remove 'pg_data/pg_hba.conf' from manifest target list");
    }

    // *****************************************************************************************************************************
    if (testBegin("infoBackupLoadFile() and infoBackupSaveFile()"))
    {
        Manifest *manifest = NULL;

        TEST_ERROR_FMT(
            manifestLoadFile(storageTest, MANIFEST_FILE_STR, cipherTypeNone, NULL), FileMissingError,
            "unable to load backup manifest file '%s/backup.manifest' or '%s/backup.manifest.copy':\n"
            "FileMissingError: unable to open missing file '%s/backup.manifest' for read\n"
            "FileMissingError: unable to open missing file '%s/backup.manifest.copy' for read",
            testPath(), testPath(), testPath(), testPath());

        // Also use this test to check that extra sections/keys are ignored using coverage.
        // -------------------------------------------------------------------------------------------------------------------------
        const Buffer *content = harnessInfoChecksumZ
        (
            "[backup]\n"
            "backup-label=\"20190808-163540F\"\n"
            "backup-timestamp-copy-start=1565282141\n"
            "backup-timestamp-start=1565282140\n"
            "backup-timestamp-stop=1565282142\n"
            "backup-type=\"full\"\n"
            "ignore-key=ignore-value\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "ignore-key=ignore-value\n"
            "\n"
            "[backup:option]\n"
            "ignore-key=ignore-value\n"
            "option-archive-check=true\n"
            "option-archive-copy=true\n"
            "option-compress=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"
            "\n"
            "[ignore-section]\n"
            "ignore-key=ignore-value\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "ignore-key=ignore-value\n"
            "master=true\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:link:default]\n"
            "ignore-key=ignore-value\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "ignore-key=ignore-value\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, strNew(MANIFEST_FILE INFO_COPY_EXT)), content), "write copy");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(MANIFEST_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");

        storageRemoveP(storageTest, strNew(MANIFEST_FILE INFO_COPY_EXT), .errorOnMissing = true);

        TEST_RESULT_VOID(
            storagePutNP(storageNewWriteNP(storageTest, MANIFEST_FILE_STR), content), "write main");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(MANIFEST_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");
    }

    // Load/save a larger manifest to test performance and memory usage.  The default sizing is for a "typical" cluster but this can
    // be scaled to test larger cluster sizes.
    // *****************************************************************************************************************************
    if (testBegin("manifestNewLoad()/manifestSave() performance"))
    {
        Manifest *manifest = NULL;

        // Manifest with all features
        // -------------------------------------------------------------------------------------------------------------------------
        String *manifestStr = strNew
        (
            "[backup]\n"
            "backup-label=\"20190818-084502F_20190820-084502D\"\n"
            "backup-prior=\"20190818-084502F\"\n"
            "backup-timestamp-copy-start=1566290707\n"
            "backup-timestamp-start=1566290702\n"
            "backup-timestamp-stop=1566290710\n"
            "backup-type=\"diff\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201809051\n"
            "db-control-version=1100\n"
            "db-id=2\n"
            "db-system-id=6689162560678426440\n"
            "db-version=\"11\"\n"
            "\n"
            "[backup:option]\n"
            "option-archive-check=true\n"
            "option-archive-copy=false\n"
            "option-backup-standby=false\n"
            "option-buffer-size=4194304\n"
            "option-checksum-page=true\n"
            "option-compress=true\n"
            "option-compress-level=9\n"
            "option-compress-level-network=3\n"
            "option-delta=false\n"
            "option-hardlink=false\n"
            "option-online=false\n"
            "option-process-max=2\n"
            "\n"
            "[backup:target]\n"
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n");

        for (unsigned int linkIdx = 0; linkIdx < 1; linkIdx++)
            strCatFmt(manifestStr, "pg_data/pg_stat%u={\"path\":\"../pg_stat\",\"type\":\"link\"}\n", linkIdx);

        strCat(
            manifestStr,
            "\n"
            "[target:file]\n");

        for (unsigned int fileIdx = 0; fileIdx < 10; fileIdx++)
            strCatFmt(
                manifestStr,
                "pg_data/base/16384/%u={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":16384"
                    ",\"timestamp\":1565282114}\n",
                16384 + fileIdx);

        strCat(
            manifestStr,
            "\n"
            "[target:file:default]\n"
            "group=\"postgres\"\n"
            "master=false\n"
            "mode=\"0600\"\n"
            "user=\"postgres\"\n"
            "\n"
            "[target:link]\n"
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"
            "\n"
            "[target:link:default]\n"
            "group=\"postgres\"\n"
            "user=\"postgres\"\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "pg_data/base={}\n"
            "pg_data/base/1={}\n"
            "pg_data/base/13124={}\n"
            "pg_data/base/13125={}\n"
            "pg_data/base/16391={}\n"
            "pg_data/global={}\n"
            "pg_data/pg_commit_ts={}\n"
            "pg_data/pg_dynshmem={}\n"
            "pg_data/pg_logical={}\n"
            "pg_data/pg_logical/mappings={}\n"
            "pg_data/pg_logical/snapshots={}\n"
            "pg_data/pg_multixact={}\n"
            "pg_data/pg_multixact/members={}\n"
            "pg_data/pg_multixact/offsets={}\n"
            "pg_data/pg_notify={}\n"
            "pg_data/pg_replslot={}\n"
            "pg_data/pg_serial={}\n"
            "pg_data/pg_snapshots={}\n"
            "pg_data/pg_stat={}\n"
            "pg_data/pg_stat_tmp={}\n"
            "pg_data/pg_subtrans={}\n"
            "pg_data/pg_tblspc={}\n"
            "pg_data/pg_twophase={}\n"
            "pg_data/pg_wal={}\n"
            "pg_data/pg_wal/archive_status={}\n"
            "pg_data/pg_xact={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"postgres\"\n"
            "mode=\"0700\"\n"
            "user=\"postgres\"\n"
        );

        const Buffer *contentLoad = harnessInfoChecksum(manifestStr);

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");
        (void)manifest;

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");

        if (!bufEq(contentSave, contentLoad))
        {
            TEST_RESULT_VOID(                                                               // {uncovered - only for debugging}
                storagePutNP(storageNewWriteNP(storageTest, strNew(MANIFEST_FILE ".expected")), contentLoad),
                "write expected manifest");
            TEST_RESULT_VOID(                                                               // {uncovered - only for debugging}
                storagePutNP(storageNewWriteNP(storageTest, strNew(MANIFEST_FILE ".actual")), contentSave),
                "write actual manifest");
        }

        TEST_RESULT_BOOL(bufEq(contentSave, contentLoad), true, "   check save");
    }
}
