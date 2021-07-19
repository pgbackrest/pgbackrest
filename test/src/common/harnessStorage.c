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

static bool
storageTestDummyInfoList(
    THIS_VOID, const String *path, StorageInfoLevel level, StorageInfoListCallback callback, void *callbackData,
    StorageInterfaceInfoListParam param)
{
    (void)thisVoid; (void)path; (void)level; (void)callback; (void)callbackData; (void)param; return false;
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
    .infoList = storageTestDummyInfoList,
    .newRead = storageTestDummyNewRead,
    .newWrite = storageTestDummyNewWrite,
    .pathRemove = storageTestDummyPathRemove,
    .remove = storageTestDummyRemove,
};

/**********************************************************************************************************************************/
void
hrnStorageInfoListCallback(void *callbackData, const StorageInfo *info)
{
    HarnessStorageInfoListCallbackData *data = callbackData;

    if (data->rootPathOmit && info->type == storageTypePath && strEq(info->name, DOT_STR))
        return;

    strCatFmt(data->content, "%s {", info->name == NULL ? NULL_Z : strZ(info->name));

    if (info->level > storageInfoLevelExists)
    {
        switch (info->type)
        {
            case storageTypeFile:
            {
                strCatZ(data->content, "file");

                if (info->level >= storageInfoLevelBasic && !data->sizeOmit)
                {
                    uint64_t size = info->size;

                    // If the file is compressed then decompress to get the real size.  Note that only gz is used in unit tests since
                    // it is the only compression type guaranteed to be present.
                    if (data->fileCompressed)
                    {
                        ASSERT(data->storage != NULL);

                        StorageRead *read = storageNewReadP(
                            data->storage,
                            data->path != NULL ? strNewFmt("%s/%s", strZ(data->path), strZ(info->name)) : info->name);
                        ioFilterGroupAdd(ioReadFilterGroup(storageReadIo(read)), decompressFilter(compressTypeGz));
                        size = bufUsed(storageGetP(read));
                    }

                    strCatFmt(data->content, ", s=%" PRIu64, size);
                }

                break;
            }

            case storageTypeLink:
                strCatFmt(data->content, "link, d=%s", strZ(info->linkDestination));
                break;

            case storageTypePath:
                strCatZ(data->content, "path");
                break;

            case storageTypeSpecial:
                strCatZ(data->content, "special");
                break;
        }

        if (info->type != storageTypeSpecial)
        {
            if (info->type != storageTypeLink)
            {
                if (info->level >= storageInfoLevelDetail &&
                    (!data->modeOmit || (info->type == storageTypePath && data->modePath != info->mode) ||
                    (info->type == storageTypeFile && data->modeFile != info->mode)))
                {
                    strCatFmt(data->content, ", m=%04o", info->mode);
                }
            }

            if (info->type == storageTypeFile && info->level >= storageInfoLevelBasic)
            {
                if (!data->timestampOmit)
                    strCatFmt(data->content, ", t=%" PRIu64, (uint64_t)info->timeModified);
            }

            if (info->level >= storageInfoLevelDetail && (!data->userOmit || userId() != info->userId))
            {
                if (info->user != NULL)
                {
                    strCatFmt(data->content, ", u=%s", strZ(info->user));
                }
                else
                {
                    strCatFmt(data->content, ", u=%d", (int)info->userId);
                }
            }

            if (info->level >= storageInfoLevelDetail && (!data->groupOmit || groupId() != info->groupId))
            {
                if (info->group != NULL)
                {
                    strCatFmt(data->content, ", g=%s", strZ(info->group));
                }
                else
                {
                    strCatFmt(data->content, ", g=%d", (int)info->groupId);
                }
            }
        }
    }

    strCatZ(data->content, "}\n");
}

/**********************************************************************************************************************************/
void
testStorageGet(const Storage *const storage, const char *const file, const char *const expected, TestStorageGetParam param)
{
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(file != NULL);

    String *fileFull = storagePathP(storage, STR(file));

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
static void
hrnStorageListCallback(void *list, const StorageInfo *info)
{
    if (!strEq(info->name, DOT_STR))
    {
        MEM_CONTEXT_BEGIN(lstMemContext(list))
        {
            StorageInfo infoCopy = *info;
            infoCopy.name = strDup(infoCopy.name);
            infoCopy.user = strDup(infoCopy.user);
            infoCopy.group = strDup(infoCopy.group);
            infoCopy.linkDestination = strDup(infoCopy.linkDestination);

            lstAdd(list, &infoCopy);
        }
        MEM_CONTEXT_END();
    }
}

void
hrnStorageList(const Storage *const storage, const char *const path, const char *const expected, const HrnStorageListParam param)
{
    // Log list test
    hrnTestResultBegin(__func__, false);

    ASSERT(storage != NULL);
    ASSERT(storagePathExistsP(storage, path == NULL ? NULL : STR(path)));

    const String *const pathFull = storagePathP(storage, path == NULL ? NULL : STR(path));
    printf("list%s contents of '%s'", param.remove ? "/remove": "", strZ(pathFull));
    hrnTestResultComment(param.comment);

    // Generate a list of files/paths/etc
    List *list = lstNewP(sizeof(StorageInfo));

    storageInfoListP(
        storage, pathFull, hrnStorageListCallback, list, .sortOrder = sortOrderAsc, .recurse = !param.noRecurse,
        .expression = param.expression != NULL ? STR(param.expression) : NULL);

    // Remove files if requested
    if (param.remove)
    {
        for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
        {
            const StorageInfo *const info = lstGet(list, listIdx);

            // Only remove at the top level since path remove will recurse
            if (strChr(info->name, '/') == -1)
            {
                // Remove a path recursively
                if (info->type == storageTypePath)
                {
                    storagePathRemoveP(
                        storage, strNewFmt("%s/%s", path, strZ(info->name)), .errorOnMissing = true, .recurse = true);
                }
                // Remove file, link, or special
                else
                    storageRemoveP(storage, strNewFmt("%s/%s", path, strZ(info->name)), .errorOnMissing = true);
            }
        }
    }

    // Return list for comparison
    StringList *listStr = strLstNew();

    for (unsigned int listIdx = 0; listIdx < lstSize(list); listIdx++)
    {
        const StorageInfo *const info = lstGet(list, listIdx);

        switch (info->type)
        {
            case storageTypeFile:
                strLstAdd(listStr, info->name);
                break;

            case storageTypeLink:
                strLstAdd(listStr, strNewFmt("%s>", strZ(info->name)));
                break;

            case storageTypePath:
                strLstAdd(listStr, strNewFmt("%s/", strZ(info->name)));
                break;

            case storageTypeSpecial:
                strLstAdd(listStr, strNewFmt("%s*", strZ(info->name)));
                break;
        }
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
    String *fileStr = strNewZ(file);
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
        strCatFmt(filter, "%stime[%" PRIu64 "]",  strEmpty(filter) ? "" : "/", (uint64_t)param.timeModified);

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
