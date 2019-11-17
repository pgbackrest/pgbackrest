/***********************************************************************************************************************************
Test Backup Manifest Handler
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
            "[cipher]\n"
            "cipher-pass=\"somepass\"\n"
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
        TEST_RESULT_STR_Z(manifestData(manifest)->backupLabel, "20190808-163540F", "    check manifest data");

        TEST_RESULT_STR_Z(manifestCipherSubPass(manifest), "somepass", "    check cipher subpass");

        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/pg/base"), NULL), "    update target no change");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/path2"), NULL), "    update target");
        TEST_RESULT_STR_Z(
            manifestTargetFind(manifest, MANIFEST_TARGET_PGDATA_STR)->path, "/path2", "    check target path");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, MANIFEST_TARGET_PGDATA_STR, STRDEF("/pg/base"), NULL), "    fix target path");

        Buffer *contentSave = bufNew(0);

        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        // Manifest with all features
        // -------------------------------------------------------------------------------------------------------------------------
        #define TEST_MANIFEST_HEADER                                                                                               \
            "[backup]\n"                                                                                                           \
            "backup-archive-start=\"000000030000028500000089\"\n"                                                                  \
            "backup-archive-stop=\"000000030000028500000089\"\n"                                                                   \
            "backup-label=\"20190818-084502F_20190820-084502D\"\n"                                                                 \
            "backup-lsn-start=\"285/89000028\"\n"                                                                                  \
            "backup-lsn-stop=\"285/89001F88\"\n"                                                                                   \
            "backup-prior=\"20190818-084502F\"\n"                                                                                  \
            "backup-timestamp-copy-start=1565282141\n"                                                                             \
            "backup-timestamp-start=1565282140\n"                                                                                  \
            "backup-timestamp-stop=1565282142\n"                                                                                   \
            "backup-type=\"full\"\n"                                                                                               \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201409291\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=1\n"                                                                                                            \
            "db-system-id=1000000000000000094\n"                                                                                   \
            "db-version=\"9.4\"\n"                                                                                                 \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=true\n"                                                                                          \
            "option-archive-copy=true\n"                                                                                           \
            "option-backup-standby=false\n"                                                                                        \
            "option-buffer-size=16384\n"                                                                                           \
            "option-checksum-page=true\n"                                                                                          \
            "option-compress=false\n"                                                                                              \
            "option-compress-level=3\n"                                                                                            \
            "option-compress-level-network=3\n"                                                                                    \
            "option-delta=false\n"                                                                                                 \
            "option-hardlink=false\n"                                                                                              \
            "option-online=false\n"                                                                                                \
            "option-process-max=32\n"

        #define TEST_MANIFEST_TARGET                                                                                               \
            "\n"                                                                                                                   \
            "[backup:target]\n"                                                                                                    \
            "pg_data={\"path\":\"/pg/base\",\"type\":\"path\"}\n"                                                                  \
            "pg_data/base/1={\"path\":\"../../base-1\",\"type\":\"link\"}\n"                                                       \
            "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"                         \
            "pg_data/pg_stat={\"path\":\"../pg_stat\",\"type\":\"link\"}\n"                                                        \
            "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../pg_config\",\"type\":\"link\"}\n"                 \
            "pg_tblspc/1={\"path\":\"/tblspc/ts1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"

        #define TEST_MANIFEST_DB                                                                                                   \
            "\n"                                                                                                                   \
            "[db]\n"                                                                                                               \
            "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"                                                                 \
            "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"                                                             \
            "template0={\"db-id\":12168,\"db-last-system-id\":12168}\n"                                                            \
            "template1={\"db-id\":1,\"db-last-system-id\":12168}\n"                                                                \

        #define TEST_MANIFEST_FILE                                                                                                 \
            "\n"                                                                                                                   \
            "[target:file]\n"                                                                                                      \
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"master\":true"                        \
                ",\"reference\":\"20190818-084502F_20190819-084506D\",\"size\":4,\"timestamp\":1565282114}\n"                      \
            "pg_data/base/16384/17000={\"checksum\":\"e0101dd8ffb910c9c202ca35b5f828bcb9697bed\",\"checksum-page\":false"          \
                ",\"checksum-page-error\":[1],\"repo-size\":4096,\"size\":8192,\"timestamp\":1565282114}\n"                        \
            "pg_data/base/16384/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"group\":false,\"size\":4"  \
                ",\"timestamp\":1565282115}\n"                                                                                     \
            "pg_data/base/32768/33000={\"checksum\":\"7a16d165e4775f7c92e8cdf60c0af57313f0bf90\",\"checksum-page\":true"           \
                ",\"reference\":\"20190818-084502F\",\"size\":1073741824,\"timestamp\":1565282116}\n"                              \
            "pg_data/base/32768/33000.32767={\"checksum\":\"6e99b589e550e68e934fd235ccba59fe5b592a9e\",\"checksum-page\":true"     \
                ",\"reference\":\"20190818-084502F\",\"size\":32768,\"timestamp\":1565282114}\n"                                   \
            "pg_data/postgresql.conf={\"checksum\":\"6721d92c9fcdf4248acff1f9a1377127d9064807\",\"master\":true,\"size\":4457"     \
                ",\"timestamp\":1565282114}\n"                                                                                     \
            "pg_data/special={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120,\"user\":false}\n"

        #define TEST_MANIFEST_FILE_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "master=false\n"                                                                                                       \
            "mode=\"0600\"\n"                                                                                                      \
            "user=\"user1\"\n"

        #define TEST_MANIFEST_LINK                                                                                                 \
            "\n"                                                                                                                   \
            "[target:link]\n"                                                                                                      \
            "pg_data/pg_stat={\"destination\":\"../pg_stat\"}\n"                                                                   \
            "pg_data/postgresql.conf={\"destination\":\"../pg_config/postgresql.conf\",\"group\":false,\"user\":\"user1\"}\n"

        #define TEST_MANIFEST_LINK_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:link:default]\n"                                                                                              \
            "group=\"group1\"\n"                                                                                                   \
            "user=false\n"

        #define TEST_MANIFEST_PATH                                                                                                 \
            "\n"                                                                                                                   \
            "[target:path]\n"                                                                                                      \
            "pg_data={\"user\":\"user2\"}\n"                                                                                       \
            "pg_data/base={\"group\":\"group2\"}\n"                                                                                \
            "pg_data/base/16384={\"mode\":\"0750\"}\n"                                                                             \
            "pg_data/base/32768={}\n"                                                                                              \
            "pg_data/base/65536={\"user\":false}\n"

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=false\n"                                                                                                        \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"user1\"\n"

        contentLoad = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");

        TEST_RESULT_STR_Z(manifestPgPath(STRDEF("pg_data")), NULL, "check pg_data path");
        TEST_RESULT_STR_Z(manifestPgPath(STRDEF("pg_data/PG_VERSION")), "PG_VERSION", "check pg_data path/file");
        TEST_RESULT_STR_Z(manifestPgPath(STRDEF("pg_tblspc/1")), "pg_tblspc/1", "check pg_tblspc path/file");

        TEST_RESULT_PTR(manifestCipherSubPass(manifest), NULL, "    check cipher subpass");

        // Absolute target paths
        TEST_RESULT_STR_Z(manifestTargetPath(manifest, manifestTargetBase(manifest)), "/pg/base", "base target path");
        TEST_RESULT_STR_Z(
            manifestTargetPath(manifest, manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf"))), "/pg/pg_config",
            "relative file link target path");
        TEST_RESULT_STR_Z(
            manifestTargetPath(manifest, manifestTargetFind(manifest, STRDEF("pg_data/pg_stat"))), "/pg/pg_stat",
            "relative path link target path");
        TEST_RESULT_STR_Z(
            manifestTargetPath(manifest, manifestTargetFind(manifest, STRDEF("pg_data/base/1"))), "/pg/base-1",
            "relative path link target path");

        // Link check
        TEST_RESULT_VOID(manifestLinkCheck(manifest), "successful link check");
        manifestTargetAdd(
            manifest, &(ManifestTarget){
                .name = STRDEF("pg_data/base/2"), .type = manifestTargetTypeLink, .path = STRDEF("../../base-1/base-2")});
        TEST_ERROR(
            manifestLinkCheck(manifest), LinkDestinationError,
            "link 'base/2' (/pg/base-1/base-2) destination is a subdirectory of or the same directory as"
                " link 'base/1' (/pg/base-1)");
        manifestTargetRemove(manifest, STRDEF("pg_data/base/2"));

        // ManifestFile getters
        const ManifestFile *file = NULL;
        TEST_ERROR(
            manifestFileFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest file list");
        TEST_ASSIGN(file, manifestFileFind(manifest, STRDEF("pg_data/PG_VERSION")), "manifestFileFind()");
        TEST_RESULT_STR_Z(file->name, "pg_data/PG_VERSION", "    find file");
        TEST_RESULT_STR_Z(
            manifestFileFindDefault(manifest, STRDEF("bogus"), file)->name, "pg_data/PG_VERSION",
            "manifestFileFindDefault() - return default");
        TEST_RESULT_STR_Z(
            manifestFileFindDefault(manifest, STRDEF("pg_data/special"), file)->name, "pg_data/special",
            "manifestFileFindDefault() - return found");
        TEST_ASSIGN(file, manifestFileFindDefault(manifest, STRDEF("bogus"), NULL), "manifestFileFindDefault()");
        TEST_RESULT_PTR(file, NULL, "    return default NULL");

        // ManifestDb getters
        const ManifestDb *db = NULL;
        TEST_ERROR(
            manifestDbFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest db list");
        TEST_ASSIGN(db, manifestDbFind(manifest, STRDEF("postgres")), "manifestDbFind()");
        TEST_RESULT_STR_Z(db->name, "postgres", "    check name");
        TEST_RESULT_STR_Z(
            manifestDbFindDefault(manifest, STRDEF("bogus"), db)->name, "postgres", "manifestDbFindDefault() - return default");
        TEST_RESULT_UINT(
            manifestDbFindDefault(manifest, STRDEF("template0"), db)->id, 12168, "manifestDbFindDefault() - return found");
        TEST_ASSIGN(db, manifestDbFindDefault(manifest, STRDEF("bogus"), NULL), "manifestDbFindDefault()");
        TEST_RESULT_PTR(db, NULL, "    return default NULL");

        // ManifestLink getters
        const ManifestLink *link = NULL;
        TEST_ERROR(
            manifestLinkFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest link list");
        TEST_ASSIGN(link, manifestLinkFind(manifest, STRDEF("pg_data/pg_stat")), "find link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat")), "    no update");
        TEST_RESULT_STR_Z(link->destination, "../pg_stat", "    check link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat2")), "    update");
        TEST_RESULT_STR_Z(link->destination, "../pg_stat2", "    check link");
        TEST_RESULT_VOID(manifestLinkUpdate(manifest, STRDEF("pg_data/pg_stat"), STRDEF("../pg_stat")), "    fix link destination");
        TEST_RESULT_STR_Z(
            manifestLinkFindDefault(manifest, STRDEF("bogus"), link)->name, "pg_data/pg_stat",
            "manifestLinkFindDefault() - return default");
        TEST_RESULT_STR_Z(
            manifestLinkFindDefault(manifest, STRDEF("pg_data/postgresql.conf"), link)->destination, "../pg_config/postgresql.conf",
            "manifestLinkFindDefault() - return found");
        TEST_ASSIGN(link, manifestLinkFindDefault(manifest, STRDEF("bogus"), NULL), "manifestLinkFindDefault()");
        TEST_RESULT_PTR(link, NULL, "    return default NULL");

        // ManifestPath getters
        const ManifestPath *path = NULL;
        TEST_ERROR(
            manifestPathFind(manifest, STRDEF("bogus")), AssertError, "unable to find 'bogus' in manifest path list");
        TEST_ASSIGN(path, manifestPathFind(manifest, STRDEF("pg_data")), "manifestPathFind()");
        TEST_RESULT_STR_Z(path->name, "pg_data", "    check path");
        TEST_RESULT_STR_Z(
            manifestPathFindDefault(manifest, STRDEF("bogus"), path)->name, "pg_data",
            "manifestPathFindDefault() - return default");
        TEST_RESULT_STR_Z(
            manifestPathFindDefault(manifest, STRDEF("pg_data/base"), path)->group, "group2",
            "manifestPathFindDefault() - return found");
        TEST_ASSIGN(path, manifestPathFindDefault(manifest, STRDEF("bogus"), NULL), "manifestPathFindDefault()");
        TEST_RESULT_PTR(path, NULL, "    return default NULL");

        const ManifestTarget *target = NULL;
        TEST_ASSIGN(target, manifestTargetFind(manifest, STRDEF("pg_data/pg_hba.conf")), "find target");
        TEST_RESULT_VOID(
            manifestTargetUpdate(manifest, target->name, target->path, STRDEF("pg_hba2.conf")), "    update target file");
        TEST_RESULT_STR_Z(target->file, "pg_hba2.conf", "    check target file");
        TEST_RESULT_VOID(manifestTargetUpdate(manifest, target->name, target->path, STRDEF("pg_hba.conf")), "    fix target file");

        contentSave = bufNew(0);

        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

        TEST_RESULT_VOID(manifestFileRemove(manifest, STRDEF("pg_data/PG_VERSION")), "remove file");
        TEST_ERROR(
            manifestFileRemove(manifest, STRDEF("pg_data/PG_VERSION")), AssertError,
            "unable to remove 'pg_data/PG_VERSION' from manifest file list");

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
    if (testBegin("manifestLoadFile()"))
    {
        Manifest *manifest = NULL;

        TEST_ERROR_FMT(
            manifestLoadFile(storageTest, BACKUP_MANIFEST_FILE_STR, cipherTypeNone, NULL), FileMissingError,
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
            storagePutP(storageNewWriteP(storageTest, strNew(BACKUP_MANIFEST_FILE INFO_COPY_EXT)), content), "write copy");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(BACKUP_MANIFEST_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");
        TEST_RESULT_STR(strPtr(manifestData(manifest)->backrestVersion), PROJECT_VERSION, "    check backrest version");

        storageRemoveP(storageTest, strNew(BACKUP_MANIFEST_FILE INFO_COPY_EXT), .errorOnMissing = true);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, BACKUP_MANIFEST_FILE_STR), content), "write main");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(BACKUP_MANIFEST_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");
    }
}
