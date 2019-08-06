/***********************************************************************************************************************************
Test Restore Command
***********************************************************************************************************************************/
#include "common/compress/gzip/compress.h"
#include "common/crypto/cipherBlock.h"
#include "common/io/io.h"
#include "common/io/bufferRead.h"
#include "common/io/bufferWrite.h"
#include "storage/posix/storage.h"
#include "storage/helper.h"

#include "common/harnessConfig.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Start a protocol server to test the protocol directly
    Buffer *serverWrite = bufNew(8192);
    IoWrite *serverWriteIo = ioBufferWriteNew(serverWrite);
    ioWriteOpen(serverWriteIo);

    ProtocolServer *server = protocolServerNew(
        strNew("test"), strNew("test"), ioBufferReadNew(bufNew(0)), serverWriteIo);

    bufUsedSet(serverWrite, 0);

    // *****************************************************************************************************************************
    if (testBegin("restoreFile()"))
    {
        const String *repoFileReferenceFull = strNew("20190509F");
        const String *repoFile1 = strNew("pg_data/testfile");

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
    if (testBegin("cmdRestore()"))
    {
        // Locality error
        // -------------------------------------------------------------------------------------------------------------------------
        StringList *argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "--pg1-host=pg1");
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_ERROR(cmdRestore(), HostInvalidError, "restore command must be run on the PostgreSQL host");

        // SUCCESS TEST FOR COVERAGE -- WILL BE REMOVED / MODIFIED AT SOME POINT
        // -------------------------------------------------------------------------------------------------------------------------
        argList = strLstNew();
        strLstAddZ(argList, "pgbackrest");
        strLstAddZ(argList, "--stanza=test1");
        strLstAdd(argList, strNewFmt("--repo1-path=%s/repo", testPath()));
        strLstAdd(argList, strNewFmt("--pg1-path=%s/pg", testPath()));
        strLstAddZ(argList, "restore");
        harnessCfgLoad(strLstSize(argList), strLstPtr(argList));

        TEST_RESULT_VOID(cmdRestore(), "successful restore");
    }

    FUNCTION_HARNESS_RESULT_VOID();
}
