/***********************************************************************************************************************************
Test Backup Manifest Handler
***********************************************************************************************************************************/
#include <unistd.h>

#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "info/infoBackup.h"
#include "storage/posix/storage.h"

#include "common/harnessInfo.h"

/***********************************************************************************************************************************
Special string constants
***********************************************************************************************************************************/
#define SHRUG_EMOJI                                                 "¯\\_(ツ)_/¯"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    Storage *storageTest = storagePosixNewP(strNew(testPath()), .write = true);

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
    if (testBegin("manifestNewBuild()"))
    {
        #define TEST_MANIFEST_HEADER                                                                                               \
            "[backup]\n"                                                                                                           \
            "backup-label=null\n"                                                                                                  \
            "backup-timestamp-copy-start=0\n"                                                                                      \
            "backup-timestamp-start=0\n"                                                                                           \
            "backup-timestamp-stop=0\n"                                                                                            \
            "backup-type=\"full\"\n"

        #define TEST_MANIFEST_DB_83                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=200711281\n"                                                                                       \
            "db-control-version=833\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"8.3\"\n"

        #define TEST_MANIFEST_DB_84                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=200904091\n"                                                                                       \
            "db-control-version=843\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"8.4\"\n"

        #define TEST_MANIFEST_DB_90                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201008051\n"                                                                                       \
            "db-control-version=903\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"9.0\"\n"

        #define TEST_MANIFEST_DB_91                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201105231\n"                                                                                       \
            "db-control-version=903\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"9.1\"\n"

        #define TEST_MANIFEST_DB_92                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201204301\n"                                                                                       \
            "db-control-version=922\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"9.2\"\n"

        #define TEST_MANIFEST_DB_94                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201409291\n"                                                                                       \
            "db-control-version=942\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"9.4\"\n"

        #define TEST_MANIFEST_DB_12                                                                                                \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201909212\n"                                                                                       \
            "db-control-version=1201\n"                                                                                            \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"12\"\n"

        #define TEST_MANIFEST_OPTION_ALL                                                                                           \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=false\n"                                                                                         \
            "option-archive-copy=false\n"                                                                                          \
            "option-checksum-page=false\n"                                                                                         \
            "option-compress=false\n"                                                                                              \
            "option-compress-type=\"none\"\n"                                                                                      \
            "option-hardlink=false\n"                                                                                              \
            "option-online=false\n"

        #define TEST_MANIFEST_OPTION_ARCHIVE                                                                                       \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=false\n"                                                                                         \
            "option-archive-copy=false\n"

        #define TEST_MANIFEST_OPTION_CHECKSUM_PAGE_FALSE                                                                           \
            "option-checksum-page=false\n"

        #define TEST_MANIFEST_OPTION_CHECKSUM_PAGE_TRUE                                                                            \
            "option-checksum-page=true\n"

        #define TEST_MANIFEST_OPTION_ONLINE_FALSE                                                                                  \
            "option-compress=false\n"                                                                                              \
            "option-compress-type=\"none\"\n"                                                                                      \
            "option-hardlink=false\n"                                                                                              \
            "option-online=false\n"

        #define TEST_MANIFEST_OPTION_ONLINE_TRUE                                                                                   \
            "option-compress=false\n"                                                                                              \
            "option-compress-type=\"none\"\n"                                                                                      \
            "option-hardlink=false\n"                                                                                              \
            "option-online=true\n"

        #define TEST_MANIFEST_FILE_DEFAULT_PRIMARY_FALSE                                                                           \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"{[group]}\"\n"                                                                                                \
            "master=false\n"                                                                                                       \
            "mode=\"0400\"\n"                                                                                                      \
            "user=\"{[user]}\"\n"

        #define TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE                                                                            \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"{[group]}\"\n"                                                                                                \
            "master=true\n"                                                                                                        \
            "mode=\"0400\"\n"                                                                                                      \
            "user=\"{[user]}\"\n"

        #define TEST_MANIFEST_LINK_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:link:default]\n"                                                                                              \
            "group=\"{[group]}\"\n"                                                                                                \
            "user=\"{[user]}\"\n"

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=\"{[group]}\"\n"                                                                                                \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"{[user]}\"\n"

        storagePathCreateP(storageTest, strNew("pg"), .mode = 0700, .noParentCreate = true);

        Storage *storagePg = storagePosixNewP(strNewFmt("%s/pg", testPath()));
        Storage *storagePgWrite = storagePosixNewP(strNewFmt("%s/pg", testPath()), .write = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("8.3 with custom exclusions and special file");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("8.3\n"));

        // Create special file
        String *specialFile = strNewFmt("%s/pg/testpipe", testPath());
        TEST_RESULT_INT(
            system(strPtr(strNewFmt("mkfifo -m 666 %s", strPtr(specialFile)))), 0, "create pipe");

        // Files that will always be ignored
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_BACKUPLABELOLD), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_POSTMASTEROPTS), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_POSTMASTERPID), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_RECOVERYCONF), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_RECOVERYDONE), .modeFile = 0400, .timeModified = 1565282101),
            NULL);

        // Directories that will always be ignored
        storagePathCreateP(storagePgWrite, strNew(PG_PREFIX_PGSQLTMP), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, strNew(PG_PREFIX_PGSQLTMP "2"), .mode = 0700, .noParentCreate = true);

        // Directories under which files will be ignored (some depending on the version)
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGDYNSHMEM), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGDYNSHMEM "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282101), NULL);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGNOTIFY), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGNOTIFY "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282102), NULL);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGREPLSLOT), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGREPLSLOT "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282103), NULL);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSERIAL), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGSERIAL "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282104), NULL);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSNAPSHOTS), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGSNAPSHOTS "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282105), BUFSTRDEF("test"));
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGSTATTMP "/" BOGUS_STR), .modeFile = 0640,
           .timeModified = 1565282106), NULL);
        storagePathCreateP(storagePgWrite, strNew(PG_PATH_PGSUBTRANS), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_PATH_PGSUBTRANS "/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282107), NULL);

        // WAL directory not ignored when offline
        storagePathCreateP(storagePgWrite, strNew("pg_xlog"), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew("pg_xlog/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282108), NULL);
        storagePathCreateP(storagePgWrite, strNew("pg_xlog/archive_status"), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew("pg_xlog/archive_status/" BOGUS_STR), .modeFile = 0400,
           .timeModified = 1565282108), NULL);

        // global directory
        storagePathCreateP(storagePgWrite, STRDEF(PG_PATH_GLOBAL), .mode = 0700, .noParentCreate = true);
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGINTERNALINIT)), NULL);
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGINTERNALINIT ".1")), NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGINTERNALINIT ".allow"), .modeFile = 0400,
            .timeModified = 1565282114), NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_GLOBAL "/t1_1"), .modeFile = 0400, .timeModified = 1565282114), NULL);

        // base/1 directory
        storagePathCreateP(storagePgWrite, STRDEF(PG_PATH_BASE), .mode = 0700, .noParentCreate = true);
        storagePathCreateP(storagePgWrite, STRDEF(PG_PATH_BASE "/1"), .mode = 0700, .noParentCreate = true);

        StringList *exclusionList = strLstNew();
        strLstAddZ(exclusionList, PG_PATH_GLOBAL "/" PG_FILE_PGINTERNALINIT);
        strLstAddZ(exclusionList, "bogus");
        strLstAddZ(exclusionList, PG_PATH_BASE "/");
        strLstAddZ(exclusionList, "bogus/");

        Manifest *manifest = NULL;
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_83, false, false, exclusionList, NULL), "build manifest");

        Buffer *contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_83
                TEST_MANIFEST_OPTION_ALL
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/global/pg_internal.init.allow={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/global/t1_1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_dynshmem/BOGUS={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_notify/BOGUS={\"size\":0,\"timestamp\":1565282102}\n"
                "pg_data/pg_replslot/BOGUS={\"size\":0,\"timestamp\":1565282103}\n"
                "pg_data/pg_serial/BOGUS={\"size\":0,\"timestamp\":1565282104}\n"
                "pg_data/pg_snapshots/BOGUS={\"size\":4,\"timestamp\":1565282105}\n"
                "pg_data/pg_stat_tmp/BOGUS={\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282106}\n"
                "pg_data/pg_xlog/BOGUS={\"size\":0,\"timestamp\":1565282108}\n"
                "pg_data/pg_xlog/archive_status/BOGUS={\"size\":0,\"timestamp\":1565282108}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_xlog={}\n"
                "pg_data/pg_xlog/archive_status={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        TEST_RESULT_LOG(
            "P00   INFO: exclude contents of '{[path]}/pg/base' from backup using 'base/' exclusion\n"
            "P00   INFO: exclude '{[path]}/pg/global/pg_internal.init' from backup using 'global/pg_internal.init' exclusion\n"
            "P00   WARN: exclude special file '{[path]}/pg/testpipe' from backup");

        storageRemoveP(storageTest, specialFile, .errorOnMissing = true);

        // Set up for manifestNewBuild tests
        // -------------------------------------------------------------------------------------------------------------------------

        // Temp relations to ignore
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/t1_1"), .modeFile = 0400,
           .timeModified = 1565282113), NULL);
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/t1_1.1"), .modeFile = 0400,
           .timeModified = 1565282113),  NULL);
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/t8888888_8888888_vm"), .modeFile = 0400,
           .timeModified = 1565282113),  NULL);
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/t8888888_8888888_vm.999999"), .modeFile = 0400,
           .timeModified = 1565282113),  NULL);

        // Unlogged relations (pgVersion > 9.1)
        storagePutP(storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // skip file because there is an _init
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555_fsm"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // skip file because there is an _init
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555_vm.1"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // skip file because there is an _init
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555_init"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // do not skip _init
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555_init.1"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // do not skip _init with segment
        storagePutP(
            storageNewWriteP(storagePgWrite, STRDEF(PG_PATH_BASE "/1/555_vm.1_vm"), .modeFile = 0400, .timeModified = 1565282114),
            NULL); // do not skip files that do not have valid endings as we are not sure what they are

        // Config directory and file links
        storagePathCreateP(storageTest, STRDEF("config"), .mode = 0700);
        THROW_ON_SYS_ERROR(
            symlink("../config/postgresql.conf", strPtr(strNewFmt("%s/pg/postgresql.conf", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(storageTest, strNew("config/postgresql.conf"), .modeFile = 0400, .timeModified = 1565282116),
            BUFSTRDEF("POSTGRESQLCONF"));
        THROW_ON_SYS_ERROR(
            symlink("../config/pg_hba.conf", strPtr(strNewFmt("%s/pg/pg_hba.conf", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(storageTest, strNew("config/pg_hba.conf"), .modeFile = 0400, .timeModified = 1565282117),
            BUFSTRDEF("PGHBACONF"));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 8.4, online");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("8.4\n"));

        // Tablespace 1 (old tablespace dir format)
        storagePathCreateP(storageTest, STRDEF("ts/1"), .mode = 0777);
        storagePathCreateP(storageTest, STRDEF("ts/1/1"), .mode = 0700);
        storagePathCreateP(storagePgWrite, MANIFEST_TARGET_PGTBLSPC_STR, .mode = 0700, .noParentCreate = true);
        THROW_ON_SYS_ERROR(
            symlink("../../ts/1", strPtr(strNewFmt("%s/pg/pg_tblspc/1", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/1/16384"), .modeFile = 0400, .timeModified = 1565282115),
            BUFSTRDEF("TESTDATA"));
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/1/t123_123_fsm"), .modeFile = 0400, .timeModified = 1565282115),
            BUFSTRDEF("TEMP_RELATION"));

        // Test manifest - mode stored for shared cluster tablespace dir, pg_xlog contents ignored because online
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_84, true, false, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_84
                TEST_MANIFEST_OPTION_ARCHIVE
                TEST_MANIFEST_OPTION_CHECKSUM_PAGE_FALSE
                TEST_MANIFEST_OPTION_ONLINE_TRUE
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_tblspc/1={\"path\":\"../../ts/1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"master\":true,\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_fsm={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/t1_1={\"size\":0,\"timestamp\":1565282113}\n"
                "pg_data/base/1/t1_1.1={\"size\":0,\"timestamp\":1565282113}\n"
                "pg_data/base/1/t8888888_8888888_vm={\"size\":0,\"timestamp\":1565282113}\n"
                "pg_data/base/1/t8888888_8888888_vm.999999={\"size\":0,\"timestamp\":1565282113}\n"
                "pg_data/global/pg_internal.init.allow={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/global/t1_1={\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_dynshmem/BOGUS={\"master\":true,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_hba.conf={\"master\":true,\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_notify/BOGUS={\"master\":true,\"size\":0,\"timestamp\":1565282102}\n"
                "pg_data/pg_replslot/BOGUS={\"master\":true,\"size\":0,\"timestamp\":1565282103}\n"
                "pg_data/pg_serial/BOGUS={\"master\":true,\"size\":0,\"timestamp\":1565282104}\n"
                "pg_data/pg_snapshots/BOGUS={\"master\":true,\"size\":4,\"timestamp\":1565282105}\n"
                "pg_data/postgresql.conf={\"master\":true,\"size\":14,\"timestamp\":1565282116}\n"
                "pg_tblspc/1/1/16384={\"size\":8,\"timestamp\":1565282115}\n"
                "pg_tblspc/1/1/t123_123_fsm={\"size\":13,\"timestamp\":1565282115}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_FALSE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_tblspc/1={\"destination\":\"../../ts/1\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_xlog={}\n"
                "pg_data/pg_xlog/archive_status={}\n"
                "pg_tblspc={}\n"
                "pg_tblspc/1={\"mode\":\"0777\"}\n"
                "pg_tblspc/1/1={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // Remove directory
        storagePathRemoveP(storageTest, STRDEF("ts/1/1"), .recurse = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 9.0");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("9.0\n"));

        // Make 'pg_xlog/archive_status' a link (if other links in the pg_xlog dir (should not be), they will be followed and added
        // when online but archive_status (and pg_xlog), whether a link of not, will will only be followed if offline)
        storagePathRemoveP(storagePgWrite, strNew("pg_xlog/archive_status"), .recurse = true);
        storagePathCreateP(storageTest, STRDEF("archivestatus"), .mode = 0777);
        THROW_ON_SYS_ERROR(
            symlink("../../archivestatus", strPtr(strNewFmt("%s/pg/pg_xlog/archive_status", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_xlog/archive_status/" BOGUS_STR), .modeFile = 0400, .timeModified = 1565282120),
            BUFSTRDEF("TESTDATA"));

        // Tablespace 1
        storagePathCreateP(storageTest, STRDEF("ts/1/PG_9.0_201008051/1"), .mode = 0700);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/PG_9.0_201008051/1/16384"), .modeFile = 0400, .timeModified = 1565282115),
            BUFSTRDEF("TESTDATA"));
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/PG_9.0_201008051/1/t123_123_fsm"), .modeFile = 0400,
               .timeModified = 1565282115), BUFSTRDEF("IGNORE_TEMP_RELATION"));

        // Add tablespaceList with error (no name)
        VariantList *tablespaceList = varLstNew();
        VariantList *tablespace = varLstNew();
        varLstAdd(tablespace, varNewUInt(2)); // tablespaceId - does not exist so will bypass
        varLstAdd(tablespace, varNewStrZ("tblspc2"));  // tablespaceName
        varLstAdd(tablespaceList, varNewVarLst(tablespace));

        // Test tablespace error
        TEST_ERROR(manifestNewBuild(storagePg, PG_VERSION_90, false, false, NULL, tablespaceList), AssertError,
            "tablespace with oid 1 not found in tablespace map\n"
            "HINT: was a tablespace created or dropped during the backup?");

        // Add a tablespace to tablespaceList that does exist
        tablespace = varLstNew();
        varLstAdd(tablespace, varNewUInt(1)); // tablespaceId - exists so will show up in manifest with tablespaceName
        varLstAdd(tablespace, varNewStrZ("tblspc1"));  // tablespaceName
        varLstAdd(tablespaceList, varNewVarLst(tablespace));

        // Test manifest - temp tables and pg_notify files ignored
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_90, false, false, NULL, tablespaceList), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_90
                TEST_MANIFEST_OPTION_ALL
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog/archive_status={\"path\":\"../../archivestatus\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_tblspc/1={\"path\":\"../../ts/1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"tblspc1\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_fsm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/global/pg_internal.init.allow={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_dynshmem/BOGUS={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_hba.conf={\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_replslot/BOGUS={\"size\":0,\"timestamp\":1565282103}\n"
                "pg_data/pg_serial/BOGUS={\"size\":0,\"timestamp\":1565282104}\n"
                "pg_data/pg_snapshots/BOGUS={\"size\":4,\"timestamp\":1565282105}\n"
                "pg_data/pg_xlog/BOGUS={\"size\":0,\"timestamp\":1565282108}\n"
                "pg_data/pg_xlog/archive_status/BOGUS={\"size\":8,\"timestamp\":1565282120}\n"
                "pg_data/postgresql.conf={\"size\":14,\"timestamp\":1565282116}\n"
                "pg_tblspc/1/PG_9.0_201008051/1/16384={\"master\":false,\"size\":8,\"timestamp\":1565282115}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_tblspc/1={\"destination\":\"../../ts/1\"}\n"
                "pg_data/pg_xlog/archive_status={\"destination\":\"../../archivestatus\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_xlog={}\n"
                "pg_data/pg_xlog/archive_status={\"mode\":\"0777\"}\n"
                "pg_tblspc={}\n"
                "pg_tblspc/1={}\n"
                "pg_tblspc/1/PG_9.0_201008051={}\n"
                "pg_tblspc/1/PG_9.0_201008051/1={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // Remove symlinks and directories
        THROW_ON_SYS_ERROR(
            unlink(strPtr(strNewFmt("%s/pg/pg_tblspc/1", testPath()))) == -1, FileRemoveError, "unable to remove symlink");
        storagePathRemoveP(storageTest, STRDEF("ts/1/PG_9.0_201008051"), .recurse = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 9.1, online");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("9.1\n"));

        // Create a path other than archive_status under pg_xlog for code coverage
        storagePathCreateP(storagePgWrite, strNew("pg_xlog/somepath"), .mode = 0700, .noParentCreate = true);

        // Add data to pg_wal to ensure it is not ignored (online or offline) until >= pgVersion 10 (file in pg_xlog log is ignored)
        storagePathCreateP(storagePgWrite, strNew("pg_wal"), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_wal/000000010000000000000001"), .modeFile = 0400, .timeModified = 1565282120),
            BUFSTRDEF("WALDATA"));

        // Test manifest - temp tables, unlogged tables, pg_serial and pg_xlog files ignored
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_91, true, false, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_91
                TEST_MANIFEST_OPTION_ARCHIVE
                TEST_MANIFEST_OPTION_CHECKSUM_PAGE_FALSE
                TEST_MANIFEST_OPTION_ONLINE_TRUE
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog/archive_status={\"path\":\"../../archivestatus\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555_init={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/global/pg_internal.init.allow={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_dynshmem/BOGUS={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_hba.conf={\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_replslot/BOGUS={\"size\":0,\"timestamp\":1565282103}\n"
                "pg_data/pg_snapshots/BOGUS={\"size\":4,\"timestamp\":1565282105}\n"
                "pg_data/pg_wal/000000010000000000000001={\"size\":7,\"timestamp\":1565282120}\n"
                "pg_data/postgresql.conf={\"size\":14,\"timestamp\":1565282116}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_xlog/archive_status={\"destination\":\"../../archivestatus\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_data/pg_xlog={}\n"
                "pg_data/pg_xlog/archive_status={\"mode\":\"0777\"}\n"
                "pg_data/pg_xlog/somepath={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // Remove pg_xlog and the directory that archive_status link pointed to
        storagePathRemoveP(storagePgWrite, STRDEF("pg_xlog"), .recurse = true);
        storagePathRemoveP(storageTest, STRDEF("archivestatus"), .recurse = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 9.2");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("9.2\n"));

        // create pg_xlog/wal as a link
        storagePathCreateP(storageTest, STRDEF("wal"), .mode = 0700);
        THROW_ON_SYS_ERROR(
            symlink(strPtr(strNewFmt("%s/wal", testPath())), strPtr(strNewFmt("%s/pg/pg_xlog", testPath()))) == -1, FileOpenError,
            "unable to create symlink");

        // Files to conditionally ignore before 9.4
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_POSTGRESQLAUTOCONFTMP), .modeFile = 0400, .timeModified = 1565282101),
            NULL);

        // Test manifest - pg_snapshots files ignored
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_92, false, false, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_92
                TEST_MANIFEST_OPTION_ALL
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog={\"path\":\"{[path]}/wal\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555_init={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/global/pg_internal.init.allow={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_dynshmem/BOGUS={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_hba.conf={\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_replslot/BOGUS={\"size\":0,\"timestamp\":1565282103}\n"
                "pg_data/pg_wal/000000010000000000000001={\"size\":7,\"timestamp\":1565282120}\n"
                "pg_data/postgresql.auto.conf.tmp={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/postgresql.conf={\"size\":14,\"timestamp\":1565282116}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_xlog={\"destination\":\"{[path]}/wal\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_data/pg_xlog={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 9.4, checksum-page");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("9.4\n"));

        // Put a pgcontrol (always master:true)
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL), .modeFile = 0400, .timeModified = 1565282101),
            NULL);

        // pg_clog pgVersion < 10 master:false (pg_xact pgVersion < 10 master:true), pg_multixact always master:false
        storagePathCreateP(storagePgWrite, STRDEF(PG_PATH_PGMULTIXACT), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF(PG_PATH_PGMULTIXACT "/" BOGUS_STR), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePathCreateP(storagePgWrite, STRDEF("pg_clog"), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF("pg_clog/" BOGUS_STR), .modeFile = 0400, .timeModified = 1565282121),
            NULL);
        storagePathCreateP(storagePgWrite, STRDEF("pg_xact"), .mode = 0700, .noParentCreate = true);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF("pg_xact/" BOGUS_STR), .modeFile = 0400, .timeModified = 1565282122),
            NULL);

        // Files to capture in version < 12 but ignore >= 12 (code coverage)
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_RECOVERYSIGNAL), .modeFile = 0400, .timeModified = 1565282101),
            NULL);
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_STANDBYSIGNAL), .modeFile = 0400, .timeModified = 1565282101),
            NULL);

        // Files to capture in version < 9.6 but ignore >= 9.6 (code coverage)
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_BACKUPLABEL), .modeFile = 0400, .timeModified = 1565282101),
            NULL);

        // Tablespace 1
        storagePathCreateP(storageTest, STRDEF("ts/1/PG_9.4_201409291/1"), .mode = 0700);
        THROW_ON_SYS_ERROR(
            symlink("../../ts/1", strPtr(strNewFmt("%s/pg/pg_tblspc/1", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/PG_9.4_201409291/1/16384"), .modeFile = 0400, .timeModified = 1565282115),
            BUFSTRDEF("TESTDATA"));
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/PG_9.4_201409291/1/t123_123_fsm"), .modeFile = 0400,
                .timeModified = 1565282115),
            BUFSTRDEF("IGNORE_TEMP_RELATION"));

        // Add checksum-page files to exclude from checksum-page validation in database relation directories
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF(PG_PATH_BASE "/1/" PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282120),
            NULL);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, STRDEF(PG_PATH_BASE "/1/" PG_FILE_PGFILENODEMAP), .modeFile = 0400, .timeModified = 1565282120),
            NULL);
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/1/PG_9.4_201409291/1/" PG_FILE_PGVERSION), .modeFile = 0400,
                .timeModified = 1565282120),
            NULL);

        // Tablespace 2
        storagePathCreateP(storageTest, STRDEF("ts/2/PG_9.4_201409291/1"), .mode = 0700);
        THROW_ON_SYS_ERROR(
            symlink("../../ts/2", strPtr(strNewFmt("%s/pg/pg_tblspc/2", testPath()))) == -1, FileOpenError,
            "unable to create symlink");
        storagePutP(
            storageNewWriteP(
                storagePgWrite, strNew("pg_tblspc/2/PG_9.4_201409291/1/16385"), .modeFile = 0400, .timeModified = 1565282115),
            BUFSTRDEF("TESTDATA"));

        // Test manifest - pg_dynshmem, pg_replslot and postgresql.auto.conf.tmp files ignored
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_94, false, true, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_94
                TEST_MANIFEST_OPTION_ARCHIVE
                TEST_MANIFEST_OPTION_CHECKSUM_PAGE_TRUE
                TEST_MANIFEST_OPTION_ONLINE_FALSE
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog={\"path\":\"{[path]}/wal\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_tblspc/1={\"path\":\"../../ts/1\",\"tablespace-id\":\"1\",\"tablespace-name\":\"ts1\",\"type\":\"link\"}\n"
                "pg_tblspc/2={\"path\":\"../../ts/2\",\"tablespace-id\":\"2\",\"tablespace-name\":\"ts2\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"master\":true,\"size\":4,\"timestamp\":1565282100}\n"
                "pg_data/backup_label={\"master\":true,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/base/1/555_init={\"checksum-page\":true,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"checksum-page\":true,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"checksum-page\":true,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/PG_VERSION={\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/base/1/pg_filenode.map={\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/global/pg_control={\"master\":true,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/global/pg_internal.init.allow={\"checksum-page\":true,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/pg_clog/BOGUS={\"size\":0,\"timestamp\":1565282121}\n"
                "pg_data/pg_hba.conf={\"master\":true,\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_multixact/BOGUS={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_wal/000000010000000000000001={\"master\":true,\"size\":7,\"timestamp\":1565282120}\n"
                "pg_data/pg_xact/BOGUS={\"master\":true,\"size\":0,\"timestamp\":1565282122}\n"
                "pg_data/postgresql.conf={\"master\":true,\"size\":14,\"timestamp\":1565282116}\n"
                "pg_data/recovery.signal={\"master\":true,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/standby.signal={\"master\":true,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_tblspc/1/PG_9.4_201409291/1/16384={\"checksum-page\":true,\"size\":8,\"timestamp\":1565282115}\n"
                "pg_tblspc/1/PG_9.4_201409291/1/PG_VERSION={\"size\":0,\"timestamp\":1565282120}\n"
                "pg_tblspc/2/PG_9.4_201409291/1/16385={\"checksum-page\":true,\"size\":8,\"timestamp\":1565282115}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_FALSE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_tblspc/1={\"destination\":\"../../ts/1\"}\n"
                "pg_data/pg_tblspc/2={\"destination\":\"../../ts/2\"}\n"
                "pg_data/pg_xlog={\"destination\":\"{[path]}/wal\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_clog={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_multixact={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_data/pg_xact={}\n"
                "pg_data/pg_xlog={}\n"
                "pg_tblspc={}\n"
                "pg_tblspc/1={}\n"
                "pg_tblspc/1/PG_9.4_201409291={}\n"
                "pg_tblspc/1/PG_9.4_201409291/1={}\n"
                "pg_tblspc/2={}\n"
                "pg_tblspc/2/PG_9.4_201409291={}\n"
                "pg_tblspc/2/PG_9.4_201409291/1={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        storageRemoveP(storageTest, STRDEF("pg/pg_tblspc/2"), .errorOnMissing = true);
        storagePathRemoveP(storageTest, STRDEF("ts/2"), .recurse = true);
        storageRemoveP(storagePgWrite, STRDEF(PG_PATH_GLOBAL "/" PG_FILE_PGINTERNALINIT ".allow"), .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest with all features - 12, online");

        // Version
        storagePutP(
            storageNewWriteP(storagePgWrite, strNew(PG_FILE_PGVERSION), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("12\n"));

        // Tablespace link errors when correct verion not found
        TEST_ERROR_FMT(
            manifestNewBuild(storagePg, PG_VERSION_12, false, false, NULL, NULL), FileOpenError,
            "unable to get info for missing path/file '%s/pg/pg_tblspc/1/PG_12_201909212': [2] No such file or directory",
            testPath());

        // Remove the link inside pg/pg_tblspc
        THROW_ON_SYS_ERROR(
            unlink(strPtr(strNewFmt("%s/pg/pg_tblspc/1", testPath()))) == -1, FileRemoveError, "unable to remove symlink");

        // Write a file into the directory pointed to by pg_xlog - contents will not be ignored online or offline
        storagePutP(
            storageNewWriteP(storageTest, strNew("wal/000000020000000000000002"), .modeFile = 0400, .timeModified = 1565282100),
            BUFSTRDEF("OLDWAL"));

        // create pg_xlog/wal as a link
        storagePathCreateP(storageTest, STRDEF("wal"), .mode = 0700);

        // Test manifest - 'pg_data/pg_tblspc' will appear in manifest but 'pg_tblspc' will not (no links). Recovery signal files
        // and backup_label ignored. Old recovery files and pg_xlog are now just another file/directory and will not be ignored.
        // pg_wal contents will be ignored online. pg_clog pgVersion > 10 master:true, pg_xact pgVersion > 10 master:false
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_12, true, false, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_12
                TEST_MANIFEST_OPTION_ARCHIVE
                TEST_MANIFEST_OPTION_CHECKSUM_PAGE_FALSE
                TEST_MANIFEST_OPTION_ONLINE_TRUE
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog={\"path\":\"{[path]}/wal\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":3,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555_init={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/PG_VERSION={\"master\":false,\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/base/1/pg_filenode.map={\"master\":false,\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/global/pg_control={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_clog/BOGUS={\"size\":0,\"timestamp\":1565282121}\n"
                "pg_data/pg_hba.conf={\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_multixact/BOGUS={\"master\":false,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_xact/BOGUS={\"master\":false,\"size\":0,\"timestamp\":1565282122}\n"
                "pg_data/pg_xlog/000000020000000000000002={\"size\":6,\"timestamp\":1565282100}\n"
                "pg_data/postgresql.conf={\"size\":14,\"timestamp\":1565282116}\n"
                "pg_data/recovery.conf={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/recovery.done={\"size\":0,\"timestamp\":1565282101}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_xlog={\"destination\":\"{[path]}/wal\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_clog={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_multixact={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_data/pg_xact={}\n"
                "pg_data/pg_xlog={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("rerun 12, offline");

        // pg_wal not ignored
        TEST_ASSIGN(manifest, manifestNewBuild(storagePg, PG_VERSION_12, false, false, NULL, NULL), "build manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER
                TEST_MANIFEST_DB_12
                TEST_MANIFEST_OPTION_ALL
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"{[path]}/pg\",\"type\":\"path\"}\n"
                "pg_data/pg_hba.conf={\"file\":\"pg_hba.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "pg_data/pg_xlog={\"path\":\"{[path]}/wal\",\"type\":\"link\"}\n"
                "pg_data/postgresql.conf={\"file\":\"postgresql.conf\",\"path\":\"../config\",\"type\":\"link\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/PG_VERSION={\"size\":3,\"timestamp\":1565282100}\n"
                "pg_data/base/1/555_init={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_init.1={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/555_vm.1_vm={\"master\":false,\"size\":0,\"timestamp\":1565282114}\n"
                "pg_data/base/1/PG_VERSION={\"master\":false,\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/base/1/pg_filenode.map={\"master\":false,\"size\":0,\"timestamp\":1565282120}\n"
                "pg_data/global/pg_control={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_clog/BOGUS={\"size\":0,\"timestamp\":1565282121}\n"
                "pg_data/pg_hba.conf={\"size\":9,\"timestamp\":1565282117}\n"
                "pg_data/pg_multixact/BOGUS={\"master\":false,\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/pg_wal/000000010000000000000001={\"size\":7,\"timestamp\":1565282120}\n"
                "pg_data/pg_xact/BOGUS={\"master\":false,\"size\":0,\"timestamp\":1565282122}\n"
                "pg_data/pg_xlog/000000020000000000000002={\"size\":6,\"timestamp\":1565282100}\n"
                "pg_data/postgresql.conf={\"size\":14,\"timestamp\":1565282116}\n"
                "pg_data/recovery.conf={\"size\":0,\"timestamp\":1565282101}\n"
                "pg_data/recovery.done={\"size\":0,\"timestamp\":1565282101}\n"
                TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
                "\n"
                "[target:link]\n"
                "pg_data/pg_hba.conf={\"destination\":\"../config/pg_hba.conf\"}\n"
                "pg_data/pg_xlog={\"destination\":\"{[path]}/wal\"}\n"
                "pg_data/postgresql.conf={\"destination\":\"../config/postgresql.conf\"}\n"
                TEST_MANIFEST_LINK_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                "pg_data/base={}\n"
                "pg_data/base/1={}\n"
                "pg_data/global={}\n"
                "pg_data/pg_clog={}\n"
                "pg_data/pg_dynshmem={}\n"
                "pg_data/pg_multixact={}\n"
                "pg_data/pg_notify={}\n"
                "pg_data/pg_replslot={}\n"
                "pg_data/pg_serial={}\n"
                "pg_data/pg_snapshots={}\n"
                "pg_data/pg_stat_tmp={\"mode\":\"0750\"}\n"
                "pg_data/pg_subtrans={}\n"
                "pg_data/pg_tblspc={}\n"
                "pg_data/pg_wal={}\n"
                "pg_data/pg_xact={}\n"
                "pg_data/pg_xlog={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");


        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on link to pg_data");

        THROW_ON_SYS_ERROR(
            symlink(strPtr(strNewFmt("%s/pg/base", testPath())), strPtr(strNewFmt("%s/pg/link", testPath()))) == -1,
            FileOpenError, "unable to create symlink");

        TEST_ERROR(
            manifestNewBuild(storagePg, PG_VERSION_94, false, false, NULL, NULL), LinkDestinationError,
            hrnReplaceKey("link 'link' destination '{[path]}/pg/base' is in PGDATA"));

        THROW_ON_SYS_ERROR(
            unlink(strPtr(strNewFmt("%s/pg/link", testPath()))) == -1, FileRemoveError, "unable to remove symlink");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on path in pg_tblspc");

        storagePathCreateP(storagePgWrite, strNew(MANIFEST_TARGET_PGTBLSPC "/somedir"), .mode = 0700, .noParentCreate = true);

        TEST_ERROR(
            manifestNewBuild(storagePg, PG_VERSION_94, false, false, NULL, NULL), LinkExpectedError,
            "'pg_data/pg_tblspc/somedir' is not a symlink - pg_tblspc should contain only symlinks");

        storagePathRemoveP(storagePgWrite, strNew(MANIFEST_TARGET_PGTBLSPC "/somedir"));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on file in pg_tblspc");

        storagePutP(storageNewWriteP(storagePgWrite, strNew(MANIFEST_TARGET_PGTBLSPC "/somefile")), NULL);

        TEST_ERROR(
            manifestNewBuild(storagePg, PG_VERSION_94, false, false, NULL, NULL), LinkExpectedError,
            "'pg_data/pg_tblspc/somefile' is not a symlink - pg_tblspc should contain only symlinks");

        storageRemoveP(storagePgWrite, strNew(MANIFEST_TARGET_PGTBLSPC "/somefile"));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on link that points to nothing");

        THROW_ON_SYS_ERROR(
            symlink("../bogus-link", strPtr(strNewFmt("%s/pg/link-to-link", testPath()))) == -1, FileOpenError,
            "unable to create symlink");

        TEST_ERROR(
            manifestNewBuild(storagePg, PG_VERSION_94, false, true, NULL, NULL), FileOpenError,
            hrnReplaceKey("unable to get info for missing path/file '{[path]}/pg/link-to-link': [2] No such file or directory"));

        THROW_ON_SYS_ERROR(
            unlink(strPtr(strNewFmt("%s/pg/link-to-link", testPath()))) == -1, FileRemoveError, "unable to remove symlink");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("error on link to a link");

        storagePathCreateP(storageTest, STRDEF("linktestdir"), .mode = 0777);
        THROW_ON_SYS_ERROR(
            symlink(strPtr(strNewFmt("%s/linktestdir", testPath())), strPtr(strNewFmt("%s/linktest", testPath()))) == -1,
            FileOpenError, "unable to create symlink");
        THROW_ON_SYS_ERROR(
            symlink(strPtr(strNewFmt("%s/linktest", testPath())), strPtr(strNewFmt("%s/pg/linktolink", testPath()))) == -1,
            FileOpenError, "unable to create symlink");

        TEST_ERROR_FMT(
            manifestNewBuild(storagePg, PG_VERSION_94, false, false, NULL, NULL), LinkDestinationError,
            "link '%s/pg/linktolink' cannot reference another link '%s/linktest'", testPath(), testPath());

        #undef TEST_MANIFEST_HEADER
        #undef TEST_MANIFEST_DB_83
        #undef TEST_MANIFEST_DB_84
        #undef TEST_MANIFEST_DB_90
        #undef TEST_MANIFEST_DB_91
        #undef TEST_MANIFEST_DB_92
        #undef TEST_MANIFEST_DB_94
        #undef TEST_MANIFEST_DB_12
        #undef TEST_MANIFEST_OPTION_ALL
        #undef TEST_MANIFEST_OPTION_ARCHIVE
        #undef TEST_MANIFEST_OPTION_CHECKSUM_PAGE_FALSE
        #undef TEST_MANIFEST_OPTION_CHECKSUM_PAGE_TRUE
        #undef TEST_MANIFEST_OPTION_ONLINE_FALSE
        #undef TEST_MANIFEST_OPTION_ONLINE_TRUE
        #undef TEST_MANIFEST_FILE_DEFAULT_PRIMARY_FALSE
        #undef TEST_MANIFEST_FILE_DEFAULT_PRIMARY_TRUE
        #undef TEST_MANIFEST_LINK_DEFAULT
        #undef TEST_MANIFEST_PATH_DEFAULT
    }

    // *****************************************************************************************************************************
    if (testBegin("manifestBuildValidate()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("don't check for delta if already enabled and test online timestamp");

        Manifest *manifest = manifestNewInternal();
        manifest->data.backupOptionOnline = true;

        TEST_RESULT_VOID(manifestBuildValidate(manifest, true, 1482182860, false), "validate manifest");
        TEST_RESULT_INT(manifest->data.backupTimestampCopyStart, 1482182861, "check copy start");
        TEST_RESULT_BOOL(varBool(manifest->data.backupOptionDelta), true, "check delta");
        TEST_RESULT_UINT(manifest->data.backupOptionCompressType, compressTypeNone, "check compress");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timestamp in past does not force delta");

        manifest->data.backupOptionOnline = false;

        manifestFileAdd(
            manifest,
            &(ManifestFile){.name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION), .size = 4, .timestamp = 1482182860});

        TEST_RESULT_VOID(manifestBuildValidate(manifest, false, 1482182860, false), "validate manifest");
        TEST_RESULT_INT(manifest->data.backupTimestampCopyStart, 1482182860, "check copy start");
        TEST_RESULT_BOOL(varBool(manifest->data.backupOptionDelta), false, "check delta");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("timestamp in future forces delta");

        TEST_RESULT_VOID(manifestBuildValidate(manifest, false, 1482182859, compressTypeGz), "validate manifest");
        TEST_RESULT_INT(manifest->data.backupTimestampCopyStart, 1482182859, "check copy start");
        TEST_RESULT_BOOL(varBool(manifest->data.backupOptionDelta), true, "check delta");
        TEST_RESULT_UINT(manifest->data.backupOptionCompressType, compressTypeGz, "check compress");

        TEST_RESULT_LOG("P00   WARN: file 'PG_VERSION' has timestamp in the future, enabling delta checksum");
    }

    // *****************************************************************************************************************************
    if (testBegin("manifestBuildIncr()"))
    {
        #define TEST_MANIFEST_HEADER_PRE                                                                                           \
            "[backup]\n"                                                                                                           \
            "backup-label=null\n"                                                                                                  \
            "backup-prior=\"20190101-010101F\"\n"                                                                                  \
            "backup-timestamp-copy-start=0\n"                                                                                      \
            "backup-timestamp-start=0\n"                                                                                           \
            "backup-timestamp-stop=0\n"                                                                                            \
            "backup-type=\"incr\"\n"                                                                                               \
            "\n"                                                                                                                   \
            "[backup:db]\n"                                                                                                        \
            "db-catalog-version=201608131\n"                                                                                       \
            "db-control-version=960\n"                                                                                             \
            "db-id=0\n"                                                                                                            \
            "db-system-id=0\n"                                                                                                     \
            "db-version=\"9.6\"\n"                                                                                                 \
            "\n"                                                                                                                   \
            "[backup:option]\n"                                                                                                    \
            "option-archive-check=false\n"                                                                                         \
            "option-archive-copy=false\n"                                                                                          \
            "option-compress=false\n"                                                                                              \
            "option-compress-type=\"none\"\n"

        #define TEST_MANIFEST_HEADER_POST                                                                                          \
            "option-hardlink=false\n"                                                                                              \
            "option-online=false\n"

        #define TEST_MANIFEST_FILE_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:file:default]\n"                                                                                              \
            "group=\"test\"\n"                                                                                                     \
            "master=false\n"                                                                                                       \
            "mode=\"0600\"\n"                                                                                                      \
            "user=\"test\"\n"

        #define TEST_MANIFEST_PATH_DEFAULT                                                                                         \
            "\n"                                                                                                                   \
            "[target:path:default]\n"                                                                                              \
            "group=\"test\"\n"                                                                                                     \
            "mode=\"0700\"\n"                                                                                                      \
            "user=\"test\"\n"

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta disabled and not enabled during validation");

        Manifest *manifest = manifestNewInternal();
        manifest->info = infoNew(NULL);
        manifest->data.pgVersion = PG_VERSION_96;
        manifest->data.backupOptionDelta = BOOL_FALSE_VAR;

        manifestTargetAdd(manifest, &(ManifestTarget){.name = MANIFEST_TARGET_PGDATA_STR, .path = STRDEF("/pg")});
        manifestPathAdd(
            manifest,
            &(ManifestPath){.name = MANIFEST_TARGET_PGDATA_STR, .mode = 0700, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/BOGUS"), .size = 6, .sizeRepo = 6, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE3"), .size = 0, .sizeRepo = 0, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE4"), .size = 55, .sizeRepo = 55, .timestamp = 1482182861,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});

        Manifest *manifestPrior = manifestNewInternal();
        manifestPrior->data.backupLabel = strNew("20190101-010101F");
        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE3"), .size = 0, .sizeRepo = 0, .timestamp = 1482182860,
               .checksumSha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709"});
        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE4"), .size = 55, .sizeRepo = 55, .timestamp = 1482182860,
               .checksumSha1 = "ccccccccccaaaaaaaaaabbbbbbbbbbdddddddddd"});
        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .checksumSha1 = "aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd"});

        TEST_RESULT_VOID(manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, NULL), "incremental manifest");

        Buffer *contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER_PRE
                "option-delta=false\n"
                TEST_MANIFEST_HEADER_POST
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/BOGUS={\"size\":6,\"timestamp\":1482182860}\n"
                "pg_data/FILE3={\"reference\":\"20190101-010101F\",\"size\":0,\"timestamp\":1482182860}\n"
                "pg_data/FILE4={\"size\":55,\"timestamp\":1482182861}\n"
                "pg_data/PG_VERSION={\"checksum\":\"aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd\",\"reference\":\"20190101-010101F\","
                    "\"size\":4,\"timestamp\":1482182860}\n"
                TEST_MANIFEST_FILE_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta enabled before validation");

        manifest->data.backupOptionDelta = BOOL_TRUE_VAR;
        lstClear(manifest->fileList);
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/" PG_FILE_PGVERSION), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});

        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .reference = STRDEF("20190101-010101F_20190202-010101D"),
               .checksumSha1 = "aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd"});

        TEST_RESULT_VOID(manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, NULL), "incremental manifest");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER_PRE
                "option-delta=true\n"
                TEST_MANIFEST_HEADER_POST
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/FILE1={\"checksum\":\"aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd\","
                    "\"reference\":\"20190101-010101F_20190202-010101D\",\"size\":4,\"timestamp\":1482182860}\n"
                "pg_data/PG_VERSION={\"checksum\":\"aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd\",\"reference\":\"20190101-010101F\","
                    "\"size\":4,\"timestamp\":1482182860}\n"
                TEST_MANIFEST_FILE_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta enabled by timestamp validation and copy checksum error");

        // Clear manifest and add a single file
        manifest->data.backupOptionDelta = BOOL_FALSE_VAR;
        lstClear(manifest->fileList);

        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 4, .sizeRepo = 4, .timestamp = 1482182859,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});

        // Clear prior manifest and add a single file with later timestamp and checksum error
        lstClear(manifestPrior->fileList);

        VariantList *checksumPageErrorList = varLstNew();
        varLstAdd(checksumPageErrorList, varNewUInt(77));
        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .reference = STRDEF("20190101-010101F_20190202-010101D"),
               .checksumSha1 = "aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd", .checksumPage = true, .checksumPageError = true,
               .checksumPageErrorList = checksumPageErrorList});

        TEST_RESULT_VOID(manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, NULL), "incremental manifest");

        TEST_RESULT_LOG("P00   WARN: file 'FILE1' has timestamp earlier than prior backup, enabling delta checksum");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER_PRE
                "option-delta=true\n"
                TEST_MANIFEST_HEADER_POST
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/FILE1={\"checksum\":\"aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd\",\"checksum-page\":false,"
                    "\"checksum-page-error\":[77],\"reference\":\"20190101-010101F_20190202-010101D\",\"size\":4,"
                    "\"timestamp\":1482182859}\n"
                TEST_MANIFEST_FILE_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta enabled by size validation");

        manifest->data.backupOptionDelta = BOOL_FALSE_VAR;
        lstClear(manifest->fileList);
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 6, .sizeRepo = 6, .timestamp = 1482182861,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE2"), .size = 6, .sizeRepo = 6, .timestamp = 1482182860,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});

        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE2"), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .reference = STRDEF("20190101-010101F_20190202-010101D"),
               .checksumSha1 = "ddddddddddbbbbbbbbbbccccccccccaaaaaaaaaa"});

        TEST_RESULT_VOID(
            manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, STRDEF("000000040000000400000004")),
            "incremental manifest");

        TEST_RESULT_LOG("P00   WARN: file 'FILE2' has same timestamp as prior but different size, enabling delta checksum");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER_PRE
                "option-delta=true\n"
                TEST_MANIFEST_HEADER_POST
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/FILE1={\"size\":6,\"timestamp\":1482182861}\n"
                "pg_data/FILE2={\"size\":6,\"timestamp\":1482182860}\n"
                TEST_MANIFEST_FILE_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta enabled by timeline change");

        manifestPrior->data.archiveStop = STRDEF("000000030000000300000003");
        manifest->data.backupOptionDelta = BOOL_FALSE_VAR;

        TEST_RESULT_VOID(
            manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, STRDEF("000000040000000400000004")), "incremental manifest");

        TEST_RESULT_LOG(
            "P00   WARN: a timeline switch has occurred since the 20190101-010101F backup, enabling delta checksum\n"
            "            HINT: this is normal after restoring from backup or promoting a standby.");

        TEST_RESULT_BOOL(varBool(manifest->data.backupOptionDelta), true, "check delta is enabled");

        manifest->data.backupOptionDelta = BOOL_FALSE_VAR;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("delta enabled by online option change");

        manifest->data.backupOptionOnline = BOOL_FALSE_VAR;
        lstClear(manifest->fileList);
        manifestFileAdd(
            manifest,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE1"), .size = 6, .sizeRepo = 6, .timestamp = 1482182861,
               .mode = 0600, .group = STRDEF("test"), .user = STRDEF("test")});

        manifest->data.backupOptionOnline = BOOL_TRUE_VAR;
        manifestFileAdd(
            manifestPrior,
            &(ManifestFile){
               .name = STRDEF(MANIFEST_TARGET_PGDATA "/FILE2"), .size = 4, .sizeRepo = 4, .timestamp = 1482182860,
               .checksumSha1 = "ddddddddddbbbbbbbbbbccccccccccaaaaaaaaaa"});

        TEST_RESULT_VOID(
            manifestBuildIncr(manifest, manifestPrior, backupTypeIncr, STRDEF("000000030000000300000003")), "incremental manifest");

        TEST_RESULT_LOG("P00   WARN: the online option has changed since the 20190101-010101F backup, enabling delta checksum");

        contentSave = bufNew(0);
        TEST_RESULT_VOID(manifestSave(manifest, ioBufferWriteNew(contentSave)), "save manifest");
        TEST_RESULT_STR(
            strNewBuf(contentSave),
            strNewBuf(harnessInfoChecksumZ(hrnReplaceKey(
                TEST_MANIFEST_HEADER_PRE
                "option-delta=true\n"
                "option-hardlink=false\n"
                "option-online=true\n"
                "\n"
                "[backup:target]\n"
                "pg_data={\"path\":\"/pg\",\"type\":\"path\"}\n"
                "\n"
                "[target:file]\n"
                "pg_data/FILE1={\"size\":6,\"timestamp\":1482182861}\n"
                TEST_MANIFEST_FILE_DEFAULT
                "\n"
                "[target:path]\n"
                "pg_data={}\n"
                TEST_MANIFEST_PATH_DEFAULT))),
            "check manifest");

        #undef TEST_MANIFEST_HEADER_PRE
        #undef TEST_MANIFEST_HEADER_POST
        #undef TEST_MANIFEST_FILE_DEFAULT
        #undef TEST_MANIFEST_PATH_DEFAULT
    }

    // *****************************************************************************************************************************
    if (testBegin("manifestNewLoad(), manifestSave(), and manifestBuildComplete()"))
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
            "option-compress-type=\"none\"\n"
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

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(manifest, manifestNewLoad(ioBufferReadNew(contentLoad)), "load manifest");
            TEST_RESULT_VOID(manifestMove(manifest, memContextPrior()), "move manifest");
        }
        MEM_CONTEXT_TEMP_END();

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
        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentLoad), "   check save");

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
            "option-checksum-page=false\n"                                                                                         \
            "option-compress=true\n"                                                                                               \
            "option-compress-level=3\n"                                                                                            \
            "option-compress-level-network=6\n"                                                                                    \
            "option-compress-type=\"gz\"\n"                                                                                        \
            "option-delta=false\n"                                                                                                 \
            "option-hardlink=true\n"                                                                                               \
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
            SHRUG_EMOJI "={\"db-id\":18000,\"db-last-system-id\":12168}\n"

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
            "pg_data/postgresql.conf={\"master\":true,\"size\":4457,\"timestamp\":1565282114}\n"                                   \
            "pg_data/special-@#!$^&*()_+~`{}[]\\:;={\"master\":true,\"mode\":\"0640\",\"size\":0,\"timestamp\":1565282120"         \
                ",\"user\":false}\n"                                                                                               \

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

        TEST_ASSIGN(
            manifest,
            manifestNewLoad(ioBufferReadNew(harnessInfoChecksumZ(
                "[backup]\n"
                "backup-archive-start=\"000000040000028500000089\"\n"
                "backup-archive-stop=\"000000040000028500000089\"\n"
                "backup-label=\"20190818-084502F\"\n"
                "backup-lsn-start=\"300/89000028\"\n"
                "backup-lsn-stop=\"300/89001F88\"\n"
                "backup-prior=\"20190818-084502F\"\n"
                "backup-timestamp-copy-start=1565282141\n"
                "backup-timestamp-start=777\n"
                "backup-timestamp-stop=777\n"
                "backup-type=\"full\"\n"
                "\n"
                "[backup:db]\n"
                "db-catalog-version=201409291\n"
                "db-control-version=942\n"
                "db-id=2\n"
                "db-system-id=2000000000000000094\n"
                "db-version=\"9.4\"\n"
                "\n"
                "[backup:option]\n"
                "option-archive-check=false\n"
                "option-archive-copy=false\n"
                "option-backup-standby=true\n"
                "option-buffer-size=16384\n"
                "option-checksum-page=false\n"
                "option-compress=true\n"
                "option-compress-level=33\n"
                "option-compress-level-network=66\n"
                "option-delta=false\n"
                "option-hardlink=false\n"
                "option-online=false\n"
                "option-process-max=99\n"
                TEST_MANIFEST_TARGET
                "\n"
                "[db]\n"
                "mail={\"db-id\":16456,\"db-last-system-id\":12168}\n"
                "postgres={\"db-id\":12173,\"db-last-system-id\":12168}\n"
                TEST_MANIFEST_FILE
                TEST_MANIFEST_FILE_DEFAULT
                TEST_MANIFEST_LINK
                TEST_MANIFEST_LINK_DEFAULT
                TEST_MANIFEST_PATH
                TEST_MANIFEST_PATH_DEFAULT))),
            "load manifest");

        TEST_RESULT_VOID(manifestBackupLabelSet(manifest, STRDEF("20190818-084502F_20190820-084502D")), "backup label set");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("manifest validation");

        // Munge files to produce errors
        manifestFileUpdate(manifest, STRDEF("pg_data/postgresql.conf"), 4457, 0, NULL, NULL, false, false, NULL);
        manifestFileUpdate(manifest, STRDEF("pg_data/base/32768/33000.32767"), 0, 0, NULL, NULL, true, false, NULL);

        TEST_ERROR(
            manifestValidate(manifest, false), FormatError,
            "manifest validation failed:\n"
            "missing checksum for file 'pg_data/postgresql.conf'");

        TEST_ERROR(
            manifestValidate(manifest, true), FormatError,
            "manifest validation failed:\n"
            "invalid checksum '6e99b589e550e68e934fd235ccba59fe5b592a9e' for zero size file 'pg_data/base/32768/33000.32767'\n"
            "missing checksum for file 'pg_data/postgresql.conf'\n"
            "repo size must be > 0 for file 'pg_data/postgresql.conf'");

        // Undo changes made to files
        manifestFileUpdate(manifest, STRDEF("pg_data/base/32768/33000.32767"), 32768, 32768, NULL, NULL, true, false, NULL);
        manifestFileUpdate(
            manifest, STRDEF("pg_data/postgresql.conf"), 4457, 4457, "184473f470864e067ee3a22e64b47b0a1c356f29", NULL, false,
            false, NULL);

        TEST_RESULT_VOID(manifestValidate(manifest, true), "successful validate");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_VOID(
            manifestBuildComplete(manifest, 0, NULL, NULL, 0, NULL, NULL, 0, 0, NULL, false, false, 0, 0, 0, false, 0, false),
            "manifest complete without db");

        // Create db list
        VariantList *dbList = varLstNew();

        VariantList *dbRow = varLstNew();
        varLstAdd(dbRow, varNewUInt64(12168));
        varLstAdd(dbRow, varNewStrZ("template0"));
        varLstAdd(dbRow, varNewUInt64(12168));
        varLstAdd(dbList, varNewVarLst(dbRow));

        dbRow = varLstNew();
        varLstAdd(dbRow, varNewUInt64(1));
        varLstAdd(dbRow, varNewStrZ("template1"));
        varLstAdd(dbRow, varNewUInt64(12168));
        varLstAdd(dbList, varNewVarLst(dbRow));

        dbRow = varLstNew();
        varLstAdd(dbRow, varNewUInt64(18000));
        varLstAdd(dbRow, varNewStrZ(SHRUG_EMOJI));
        varLstAdd(dbRow, varNewUInt64(12168));
        varLstAdd(dbList, varNewVarLst(dbRow));

        TEST_RESULT_VOID(
            manifestBuildComplete(
                manifest, 1565282140, STRDEF("285/89000028"), STRDEF("000000030000028500000089"), 1565282142,
                STRDEF("285/89001F88"), STRDEF("000000030000028500000089"), 1, 1000000000000000094, dbList,
                true, true, 16384, 3, 6, true, 32, false),
            "manifest complete with db");

        TEST_RESULT_STR_Z(manifestPathPg(STRDEF("pg_data")), NULL, "check pg_data path");
        TEST_RESULT_STR_Z(manifestPathPg(STRDEF("pg_data/PG_VERSION")), "PG_VERSION", "check pg_data path/file");
        TEST_RESULT_STR_Z(manifestPathPg(STRDEF("pg_tblspc/1")), "pg_tblspc/1", "check pg_tblspc path/file");

        TEST_RESULT_STR_Z(manifestCipherSubPass(manifest), NULL, "    check cipher subpass");
        TEST_RESULT_VOID(manifestCipherSubPassSet(manifest, STRDEF("supersecret")), "cipher subpass set");
        TEST_RESULT_STR_Z(manifestCipherSubPass(manifest), "supersecret", "    check cipher subpass");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check that a file link in the parent path of a path link does not conflict");

        manifestTargetAdd(
            manifest, &(ManifestTarget){
               .name = STRDEF("pg_data/test.sh"), .type = manifestTargetTypeLink, .path = STRDEF(".."), .file = STRDEF("test.sh")});
        TEST_RESULT_VOID(manifestLinkCheck(manifest), "successful link check");
        manifestTargetRemove(manifest, STRDEF("pg_data/test.sh"));

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
            manifestFileFind(manifest, STRDEF("pg_data/special-@#!$^&*()_+~`{}[]\\:;"))->name,
            "pg_data/special-@#!$^&*()_+~`{}[]\\:;", "find special file");
        TEST_ASSIGN(file, manifestFileFindDefault(manifest, STRDEF("bogus"), NULL), "manifestFileFindDefault()");
        TEST_RESULT_PTR(file, NULL, "    return default NULL");

        TEST_RESULT_VOID(
            manifestFileUpdate(manifest, STRDEF("pg_data/postgresql.conf"), 4457, 4457, "", NULL, false, false, NULL),
            "update file");
        TEST_RESULT_VOID(
            manifestFileUpdate(manifest, STRDEF("pg_data/postgresql.conf"), 4457, 4457, NULL, varNewStr(NULL), false, false, NULL),
            "update file");

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

        Buffer *contentCompare = harnessInfoChecksumZ
        (
            TEST_MANIFEST_HEADER
            TEST_MANIFEST_TARGET
            "\n"
            "[cipher]\n"
            "cipher-pass=\"supersecret\"\n"
            TEST_MANIFEST_DB
            TEST_MANIFEST_FILE
            TEST_MANIFEST_FILE_DEFAULT
            TEST_MANIFEST_LINK
            TEST_MANIFEST_LINK_DEFAULT
            TEST_MANIFEST_PATH
            TEST_MANIFEST_PATH_DEFAULT
        );

        TEST_RESULT_STR(strNewBuf(contentSave), strNewBuf(contentCompare), "   check save");

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

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("load validation errors");

        TEST_ERROR(
            manifestNewLoad(ioBufferReadNew(BUFSTRDEF("[target:file]\npg_data/bogus={}"))), FormatError,
            "missing timestamp for file 'pg_data/bogus'");
        TEST_ERROR(
            manifestNewLoad(ioBufferReadNew(BUFSTRDEF("[target:file]\npg_data/bogus={\"timestamp\":0}"))), FormatError,
            "missing size for file 'pg_data/bogus'");
    }

    // *****************************************************************************************************************************
    if (testBegin("manifestLoadFile(), manifestFree()"))
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
            "ignore-key=\"ignore-value\"\n"
            "\n"
            "[backup:db]\n"
            "db-catalog-version=201409291\n"
            "db-control-version=942\n"
            "db-id=1\n"
            "db-system-id=1000000000000000094\n"
            "db-version=\"9.4\"\n"
            "ignore-key=\"ignore-value\"\n"
            "\n"
            "[backup:option]\n"
            "ignore-key=\"ignore-value\"\n"
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
            "ignore-key=\"ignore-value\"\n"
            "\n"
            "[target:file]\n"
            "pg_data/PG_VERSION={\"checksum\":\"184473f470864e067ee3a22e64b47b0a1c356f29\",\"size\":4,\"timestamp\":1565282114}\n"
            "\n"
            "[target:file:default]\n"
            "group=\"group1\"\n"
            "ignore-key=\"ignore-value\"\n"
            "master=true\n"
            "mode=\"0600\"\n"
            "user=\"user1\"\n"
            "\n"
            "[target:link:default]\n"
            "ignore-key=\"ignore-value\"\n"
            "\n"
            "[target:path]\n"
            "pg_data={}\n"
            "\n"
            "[target:path:default]\n"
            "group=\"group1\"\n"
            "ignore-key=\"ignore-value\"\n"
            "mode=\"0700\"\n"
            "user=\"user1\"\n"
        );

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, strNew(BACKUP_MANIFEST_FILE INFO_COPY_EXT)), content), "write copy");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(BACKUP_MANIFEST_FILE), cipherTypeNone, NULL), "load copy");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");
        TEST_RESULT_STR_Z(manifestData(manifest)->backrestVersion, PROJECT_VERSION, "    check backrest version");

        storageRemoveP(storageTest, strNew(BACKUP_MANIFEST_FILE INFO_COPY_EXT), .errorOnMissing = true);

        TEST_RESULT_VOID(
            storagePutP(storageNewWriteP(storageTest, BACKUP_MANIFEST_FILE_STR), content), "write main");
        TEST_ASSIGN(manifest, manifestLoadFile(storageTest, STRDEF(BACKUP_MANIFEST_FILE), cipherTypeNone, NULL), "load main");
        TEST_RESULT_UINT(manifestData(manifest)->pgSystemId, 1000000000000000094, "    check file loaded");

        TEST_RESULT_VOID(manifestFree(manifest), "free manifest");
        TEST_RESULT_VOID(manifestFree(NULL), "free null manifest");
    }
}
