/***********************************************************************************************************************************
Storage Test Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/type/object.h"
#include "common/type/param.h"
#include "common/user.h"
#include "storage/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessStorage.h"
#include "common/harnessTest.h"

/***********************************************************************************************************************************
Dummy functions and interface for constructing test storage drivers
***********************************************************************************************************************************/
static StorageInfo
storageTestDummyInfo(THIS_VOID, const String *file, StorageInfoLevel level, StorageInterfaceInfoParam param)
{
    (void)thisVoid; (void)file; (void)level; (void)param; return (StorageInfo){.exists = false};
}

static StorageList *
storageTestDummyList(THIS_VOID, const String *path, StorageInfoLevel level, StorageInterfaceListParam param)
{
    (void)thisVoid; (void)path; (void)level; (void)param; return false;
}

static StorageRead *
storageTestDummyNewRead(THIS_VOID, const String *file, bool ignoreMissing, StorageInterfaceNewReadParam param)
{
    (void)thisVoid; (void)file; (void)ignoreMissing; (void)param; return NULL;
}

static StorageWrite *
storageTestDummyNewWrite(THIS_VOID, const String *file, StorageInterfaceNewWriteParam param)
{
    (void)thisVoid; (void)file; (void)param; return NULL;
}

static bool
storageTestDummyPathRemove(THIS_VOID, const String *path, bool recurse, StorageInterfacePathRemoveParam param)
{
    (void)thisVoid; (void)path; (void)recurse; (void)param; return false;
}

static void
storageTestDummyRemove(THIS_VOID, const String *file, StorageInterfaceRemoveParam param)
{
    (void)thisVoid; (void)file; (void)param;
}

const StorageInterface storageInterfaceTestDummy =
{
    .info = storageTestDummyInfo,
    .list = storageTestDummyList,
    .newRead = storageTestDummyNewRead,
    .newWrite = storageTestDummyNewWrite,
    .pathRemove = storageTestDummyPathRemove,
    .remove = storageTestDummyRemove,
};

/**********************************************************************************************************************************/
void
testStorageGet(const Storage *const storage, const char *const file, const char *const expected, TestStorageGetParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(file != NULL);

    String *fileFull = strCat(strNew(), storagePathP(storage, STR(file)));

    // Add compression extension if one exists
    compressExtCat(fileFull, param.compressType);

    // Declare an information filter for displaying paramaters to the output
    String *const filter = strNew();

    StorageRead *read = storageNewReadP(storage, fileFull);
    IoFilterGroup *filterGroup = ioReadFilterGroup(storageReadIo(read));

    // Add decrypt filter
    if (param.cipherType != 0 && param.cipherType != cipherTypeNone)
    {
        // Default to main cipher pass
        if (param.cipherPass == NULL)
            param.cipherPass = TEST_CIPHER_PASS;

        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeDecrypt, param.cipherType, BUFSTRZ(param.cipherPass), NULL));

        strCatFmt(filter, "enc[%s,%s] ", strZ(strIdToStr(param.cipherType)), param.cipherPass);
    }

    // Add decompress filter
    if (param.compressType != compressTypeNone)
    {
        ASSERT(param.compressType == compressTypeGz || param.compressType == compressTypeBz2);
        ioFilterGroupAdd(filterGroup, decompressFilter(param.compressType));
    }

    printf("test content of %s'%s'", strEmpty(filter) ? "" : strZ(filter), strZ(fileFull));
    hrnTestResultComment(param.comment);

    hrnTestResultZ(strZ(strNewBuf(storageGetP(read))), expected, harnessTestResultOperationEq);

    if (param.remove)
        storageRemoveP(storage, fileFull, .errorOnMissing = true);
}

/**********************************************************************************************************************************/
void
testStorageExists(const Storage *const storage, const char *const file, const TestStorageExistsParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(file != NULL);

    const String *const fileFull = storagePathP(storage, STR(file));

    printf("file exists '%s'", strZ(fileFull));
    hrnTestResultComment(param.comment);

    hrnTestResultBool(storageExistsP(storage, fileFull), true);

    if (param.remove)
        storageRemoveP(storage, fileFull, .errorOnMissing = true);
}

/**********************************************************************************************************************************/
void
hrnStorageList(const Storage *const storage, const char *const path, const char *const expected, const HrnStorageListParam param)
{
    // Check if paths are supported
    const bool featurePath = storageFeature(storage, storageFeaturePath);

    // Determine sort order
    const SortOrder sortOrder = param.sortOrder == sortOrderNone ? sortOrderAsc : param.sortOrder;

    // Determine level
    StorageInfoLevel level = param.level == storageInfoLevelDefault && !param.levelForce ? storageInfoLevelType : param.level;

    // Log list test
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(!featurePath || storagePathExistsP(storage, path == NULL ? NULL : STR(path)));

    const String *const pathFull = storagePathP(storage, path == NULL ? NULL : STR(path));
    printf("list%s contents of '%s'", param.remove ? "/remove": "", strZ(pathFull));
    hrnTestResultComment(param.comment);

    // Generate a list of files/paths/etc
    StorageList *const list = storageLstNew(level == storageInfoLevelDefault ? storageInfoLevelDetail : level);

    StorageIterator *const storageItr = storageNewItrP(
        storage, pathFull, .recurse = !param.noRecurse, .sortOrder = sortOrder, .level = level,
        .expression = param.expression != NULL ? STR(param.expression) : NULL);

    while (storageItrMore(storageItr))
    {
        StorageInfo info = storageItrNext(storageItr);
        storageLstAdd(list, &info);
    }

    // Remove files if requested
    if (param.remove)
    {
        for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
        {
            const StorageInfo info = storageLstGet(list, listIdx);

            if (strEq(info.name, DOT_STR))
                continue;

            // Only remove at the top level since path remove will recurse
            if (strChr(info.name, '/') == -1)
            {
                // Remove a path recursively
                if (info.type == storageTypePath)
                {
                    storagePathRemoveP(
                        storage, strNewFmt("%s/%s", strZ(pathFull), strZ(info.name)), .errorOnMissing = featurePath,
                        .recurse = true);
                }
                // Remove file, link, or special
                else
                    storageRemoveP(storage, strNewFmt("%s/%s", strZ(pathFull), strZ(info.name)), .errorOnMissing = true);
            }
        }
    }

    // Generate list for comparison
    StringList *listStr = strLstNew();

    if (level == storageInfoLevelDefault)
        level = storageFeature(storage, storageFeatureInfoDetail) ? storageInfoLevelDetail : storageInfoLevelBasic;

    if (param.includeDot)
    {
        StorageInfo info = storageInfoP(storage, pathFull);
        info.name = DOT_STR;

        if (sortOrder == sortOrderAsc)
            storageLstInsert(list, 0, &info);
        else
            storageLstAdd(list, &info);
    }

    for (unsigned int listIdx = 0; listIdx < storageLstSize(list); listIdx++)
    {
        const StorageInfo info = storageLstGet(list, listIdx);
        String *const item = strCat(strNew(), info.name);

        if (strEq(info.name, DOT_STR) && !param.includeDot)
            continue;

        switch (info.type)
        {
            case storageTypeFile:
                break;

            case storageTypeLink:
                strCatZ(item, ">");
                break;

            case storageTypePath:
                strCatZ(item, "/");
                break;

            case storageTypeSpecial:
                strCatZ(item, "*");
                break;
        }

        if (((info.type == storageTypeFile || info.type == storageTypeLink) && level >= storageInfoLevelBasic) ||
            (info.type == storageTypePath && level >= storageInfoLevelDetail))
        {
            strCatZ(item, " {");

            if (info.type == storageTypeFile)
                strCatFmt(item, "s=%" PRIu64 ", t=%" PRId64, info.size, (int64_t)info.timeModified);
            else if (info.type == storageTypeLink)
            {
                const StorageInfo infoLink = storageInfoP(
                    storage,
                    strEq(info.name, DOT_STR) ? pathFull : strNewFmt("%s/%s", strZ(pathFull), strZ(info.name)),
                    .level = storageInfoLevelDetail);

                strCatFmt(item, "d=%s", strZ(infoLink.linkDestination));
            }

            if (level >= storageInfoLevelDetail)
            {
                if (info.type != storageTypePath)
                    strCatZ(item, ", ");

                if (info.user != NULL)
                    strCatFmt(item, "u=%s", strZ(info.user));
                else
                    strCatFmt(item, "u=%d", (int)info.userId);

                if (info.group != NULL)
                    strCatFmt(item, ", g=%s", strZ(info.group));
                else
                    strCatFmt(item, ", g=%d", (int)info.groupId);

                if (info.type != storageTypeLink)
                    strCatFmt(item, ", m=%04o", info.mode);
            }

            strCatZ(item, "}");
        }

        strLstAdd(listStr, item);
    }

    hrnTestResultStringList(listStr, expected, harnessTestResultOperationEq);
}

/**********************************************************************************************************************************/
void
hrnStorageMode(const Storage *const storage, const char *const path, HrnStorageModeParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);

    const char *const pathFull = strZ(storagePathP(storage, path == NULL ? NULL : STR(path)));

    // If no mode specified then default the mode based on the file type
    if (param.mode == 0)
    {
        struct stat statFile;

        THROW_ON_SYS_ERROR_FMT(stat(pathFull, &statFile) == -1, FileOpenError, "unable to stat '%s'", pathFull);

        if (S_ISDIR(statFile.st_mode))
            param.mode = STORAGE_MODE_PATH_DEFAULT;
        else
            param.mode = STORAGE_MODE_FILE_DEFAULT;
    }

    printf("chmod '%04o' on '%s'", param.mode, pathFull);
    hrnTestResultComment(param.comment);

    THROW_ON_SYS_ERROR_FMT(chmod(pathFull, param.mode) == -1, FileModeError, "unable to set mode on '%s'", pathFull);

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStorageMove(
    const Storage *const storage, const char *const fileSource, const char *const fileDest, HrnStorageMoveParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(fileSource != NULL);
    ASSERT(fileDest != NULL);

    const String *const fileSourceStr = storagePathP(storage, STR(fileSource));
    const String *const fileDestStr = storagePathP(storage, STR(fileDest));

    printf("move '%s' to '%s'", strZ(fileSourceStr), strZ(fileDestStr));

    hrnTestResultComment(param.comment);

    // Move (rename) the file
    storageMoveP(storage, storageNewReadP(storage, fileSourceStr), storageNewWriteP(storage,fileDestStr));

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStorageCopy(
    const Storage *const storageSource, const char *const fileSource, const Storage *const storageDest, const char *const fileDest,
    HrnStorageCopyParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storageSource != NULL);
    ASSERT(fileSource != NULL);
    ASSERT(storageDest != NULL);
    ASSERT(fileDest != NULL);

    const String *const fileSourceStr = storagePathP(storageSource, STR(fileSource));
    const String *const fileDestStr = storagePathP(storageDest, STR(fileDest));

    printf("copy '%s' to '%s'", strZ(fileSourceStr), strZ(fileDestStr));

    hrnTestResultComment(param.comment);

    // Copy the file
    storageCopyP(storageNewReadP(storageSource, fileSourceStr), storageNewWriteP(storageDest, fileDestStr));

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStoragePut(
    const Storage *const storage, const char *const file, const Buffer *const buffer, const char *const logPrefix,
    HrnStoragePutParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(file != NULL);

    // Add compression extension to file name
    String *fileStr = strCatZ(strNew(), file);
    compressExtCat(fileStr, param.compressType);

    // Create file
    StorageWrite *destination = storageNewWriteP(storage, fileStr, .modeFile = param.modeFile, .timeModified = param.timeModified);
    IoFilterGroup *filterGroup = ioWriteFilterGroup(storageWriteIo(destination));

    // Declare an information filter for displaying paramaters to the output
    String *const filter = strNew();

    // Add mode to output information filter
    if (param.modeFile != 0)
        strCatFmt(filter, "mode[%04o]", param.modeFile);

    // Add modified time to output information filter
    if (param.timeModified != 0)
        strCatFmt(filter, "%stime[%" PRIu64 "]", strEmpty(filter) ? "" : "/", (uint64_t)param.timeModified);

    // Add compression filter
    if (param.compressType != compressTypeNone)
    {
        ASSERT(param.compressType == compressTypeGz || param.compressType == compressTypeBz2);
        ioFilterGroupAdd(filterGroup, compressFilter(param.compressType, 1));

        strCatFmt(filter, "%scmp[%s]", strEmpty(filter) ? "" : "/", strZ(compressTypeStr(param.compressType)));
    }

    // Add encrypted filter
    if (param.cipherType != 0 && param.cipherType != cipherTypeNone)
    {
        // Default to main cipher pass
        if (param.cipherPass == NULL)
            param.cipherPass = TEST_CIPHER_PASS;

        ioFilterGroupAdd(filterGroup, cipherBlockNew(cipherModeEncrypt, param.cipherType, BUFSTRZ(param.cipherPass), NULL));
    }

    // Add file name
    printf(
        "%s %s%s%s'%s'", logPrefix != NULL ? logPrefix : "put file", buffer == NULL || bufEmpty(buffer) ? "(empty) " : "",
        strZ(filter), strEmpty(filter) ? "" : " ", strZ(storagePathP(storage, fileStr)));
    hrnTestResultComment(param.comment);

    // Put file
    storagePutP(destination, buffer);

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStoragePathCreate(const Storage *const storage, const char *const path, HrnStoragePathCreateParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);

    const String *const pathFull = storagePathP(storage, path == NULL ? NULL : STR(path));

    printf("create path '%s'", strZ(pathFull));

    if (param.mode != 0)
        printf(" mode '%04o'", param.mode);

    hrnTestResultComment(param.comment);

    storagePathCreateP(
        storage, pathFull, .mode = param.mode, .errorOnExists = !param.noErrorOnExists, .noParentCreate = param.noParentCreate);

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStoragePathRemove(const Storage *const storage, const char *const path, HrnStoragePathRemoveParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);

    const String *const pathFull = storagePathP(storage, path == NULL ? NULL : STR(path));

    printf("remove path '%s'", strZ(pathFull));
    hrnTestResultComment(param.comment);

    storagePathRemoveP(storage, pathFull, .recurse = param.recurse, .errorOnMissing = param.errorOnMissing);

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStorageRemove(const Storage *const storage, const char *const file, const HrnStorageRemoveParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(file != NULL);

    printf("remove file '%s'", strZ(storagePathP(storage, STR(file))));
    hrnTestResultComment(param.comment);

    storageRemoveP(storage, STR(file), .errorOnMissing = param.errorOnMissing);

    hrnTestResultEnd();
}

/**********************************************************************************************************************************/
void
hrnStorageTime(const Storage *const storage, const char *const path, const time_t modified, const HrnStorageTimeParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);

    const char *const pathFull = strZ(storagePathP(storage, path == NULL ? NULL : STR(path)));

    printf("time '%" PRId64 "' on '%s'", (int64_t)modified, pathFull);
    hrnTestResultComment(param.comment);

    THROW_ON_SYS_ERROR_FMT(
        utime(pathFull, &((struct utimbuf){.actime = modified, .modtime = modified})) == -1, FileInfoError,
        "unable to set time for '%s'", pathFull);

    hrnTestResultEnd();
}
