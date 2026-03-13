/***********************************************************************************************************************************
Repository Put Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <unistd.h>

#include "command/repo/common.h"
#include "common/compress/helper.h"
#include "common/debug.h"
#include "common/io/fdRead.h"
#include "common/io/filter/size.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "common/type/string.h"
#include "config/config.h"
#include "info/manifest.h"
#include "storage/helper.h"

static String *
composeDestinationPath(const String *backupLabel, const String *fileName)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, backupLabel);
        FUNCTION_LOG_PARAM(STRING, fileName);
    FUNCTION_LOG_END();

    String *const result = strNewFmt(STORAGE_REPO_BACKUP "/%s/%s", strZ(backupLabel), strZ(fileName));

    FUNCTION_LOG_RETURN(STRING, result);
}

/***********************************************************************************************************************************
Write source IO to destination file
***********************************************************************************************************************************/
static void
storagePushProcess(const String *file, CompressType compressType, int compressLevel)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, file);
        FUNCTION_LOG_PARAM(ENUM, compressType);
        FUNCTION_LOG_PARAM(INT, compressLevel);
    FUNCTION_LOG_END();

    // Ensure that the file exists and readable
    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Normalize source file path
        // Get current working dir
        char currentWorkDir[1024];
        THROW_ON_SYS_ERROR(getcwd(currentWorkDir, sizeof(currentWorkDir)) == NULL, FormatError, "unable to get cwd");

        String *sourcePath = strPathAbsolute(file, strNewZ(currentWorkDir));

        // Repository Path Formation

        const String *backupLabel = cfgOptionStr(cfgOptSet);

        String *destFilename = strCat(strNew(), strFileName(file));
        compressExtCat(destFilename, compressType);

        String *destPath = composeDestinationPath(backupLabel, destFilename);

        // Is path valid for repo?
        destPath = repoPathIsValid(destPath);

        bool repoChecksum = false;
        const Storage *storage = storageRepoWrite();
        const StorageWrite *const destination = storageNewWriteP(storage, destPath);

        IoRead *const sourceIo = storageReadIo(storageNewReadP(storageLocal(), sourcePath));
        IoWrite *const destinationIo = storageWriteIo(destination);

        IoFilterGroup *const readFilterGroup = ioReadFilterGroup(sourceIo);
        IoFilterGroup *const writeFilterGroup = ioWriteFilterGroup(destinationIo);

        const String *manifestFileName = strNewFmt(STORAGE_REPO_BACKUP "/%s/" BACKUP_MANIFEST_FILE, strZ(backupLabel));
        Manifest *manifest = manifestLoadFile(
            storage, manifestFileName,
            cipherTypeNone, NULL);

        // Add SHA1 filter
        ioFilterGroupAdd(writeFilterGroup, cryptoHashNew(hashTypeSha1));

        // Add compression
        if (compressType != compressTypeNone)
        {
            ioFilterGroupAdd(
                writeFilterGroup,
                compressFilterP(compressType, compressLevel));

            repoChecksum = true;
        }

        // Capture checksum of file stored in the repo if filters that modify the output have been applied
        if (repoChecksum)
            ioFilterGroupAdd(writeFilterGroup, cryptoHashNew(hashTypeSha1));

        // Add size filter last to calculate repo size
        ioFilterGroupAdd(writeFilterGroup, ioSizeNew());

        // Add size filter last to calculate source file size
        ioFilterGroupAdd(readFilterGroup, ioSizeNew());

        // Open source and destination
        ioReadOpen(sourceIo);
        ioWriteOpen(destinationIo);

        // Copy data from source to destination
        ioCopyP(sourceIo, destinationIo);

        // Close the source and destination
        ioReadClose(sourceIo);
        ioWriteClose(destinationIo);

        // Use base path to set ownership and mode
        const ManifestPath *const basePath = manifestPathFind(manifest, MANIFEST_TARGET_PGDATA_STR);

        // Add to manifest
        uint64_t rsize = pckReadU64P(ioFilterGroupResultP(readFilterGroup, SIZE_FILTER_TYPE));
        uint64_t wsize = pckReadU64P(ioFilterGroupResultP(writeFilterGroup, SIZE_FILTER_TYPE));
        ManifestFile customFile =
        {
            .name = destFilename,
            .mode = basePath->mode & (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
            .size = wsize,
            .sizeOriginal = rsize,
            .sizeRepo = wsize,
            .timestamp = time(NULL),
            .checksumSha1 = bufPtr(pckReadBinP(ioFilterGroupResultP(writeFilterGroup, CRYPTO_HASH_FILTER_TYPE, .idx = 0))),
        };

        if (repoChecksum)
        {
            PackRead *packRead = ioFilterGroupResultP(writeFilterGroup, CRYPTO_HASH_FILTER_TYPE, .idx = 1);
            ASSERT(packRead != NULL);
            customFile.checksumRepoSha1 = bufPtr(pckReadBinP(packRead));
        }

        bool found = false;

        for (unsigned int fileIdx = 0; fileIdx < manifestCustomFileTotal(manifest); fileIdx++)
        {
            ManifestFile manifestFile = manifestCustomFile(manifest, fileIdx);

            if (strCmp(manifestFile.name, destFilename) == 0){
                manifestFile.mode = customFile.mode;
                manifestFile.size = customFile.size;
                manifestFile.sizeOriginal = customFile.sizeOriginal;
                manifestFile.sizeRepo = customFile.sizeRepo;
                manifestFile.timestamp = customFile.timestamp;
                manifestFile.checksumSha1 = customFile.checksumSha1;

                manifestCustomFileUpdate(manifest, &manifestFile);
                found = true;
                break;
            }
        }

        if (!found)
            manifestCustomFileAdd(manifest, &customFile);

        // Save manifest
        IoWrite *const manifestWrite = storageWriteIo(
            storageNewWriteP(
                storageRepoWrite(),
                manifestFileName
                ));

        manifestSave(manifest, manifestWrite);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStoragePush(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const StringList *params = cfgCommandParam();

        if (strLstSize(params) != 1)
            THROW(ParamInvalidError, "exactly one parameter is required");

        String *filename = strLstGet(params, 0);

        // Check that the filename does not end with a slash
        if (strEndsWith(filename, FSLASH_STR))
            THROW(ParamInvalidError, "file parameter should not end with a slash");

        LOG_INFO_FMT(
            "push file %s to the archive.",
            strZ(filename));

        storagePushProcess(filename,
                           compressTypeEnum(cfgOptionStrId(cfgOptCompressType)),
                           cfgOptionInt(cfgOptCompressLevel));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
