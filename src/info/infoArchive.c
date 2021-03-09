/***********************************************************************************************************************************
Archive Info Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "common/crypto/cipherBlock.h"
#include "common/debug.h"
#include "common/log.h"
#include "common/ini.h"
#include "common/io/bufferWrite.h"
#include "common/io/io.h"
#include "common/memContext.h"
#include "common/type/object.h"
#include "info/infoArchive.h"
#include "info/infoPg.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(INFO_ARCHIVE_PATH_FILE_STR,                           INFO_ARCHIVE_PATH_FILE);
STRING_EXTERN(INFO_ARCHIVE_PATH_FILE_COPY_STR,                      INFO_ARCHIVE_PATH_FILE_COPY);

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct InfoArchive
{
    MemContext *memContext;                                         // Mem context
    InfoPg *infoPg;                                                 // Contents of the DB data
};

OBJECT_DEFINE_MOVE(INFO_ARCHIVE);
OBJECT_DEFINE_FREE(INFO_ARCHIVE);

/***********************************************************************************************************************************
Internal constructor
***********************************************************************************************************************************/
static InfoArchive *
infoArchiveNewInternal(void)
{
    FUNCTION_TEST_VOID();

    InfoArchive *this = memNew(sizeof(InfoArchive));

    *this = (InfoArchive)
    {
        .memContext = memContextCurrent(),
    };

    FUNCTION_TEST_RETURN(this);
}

/**********************************************************************************************************************************/
InfoArchive *
infoArchiveNew(unsigned int pgVersion, uint64_t pgSystemId, const String *cipherPassSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
        FUNCTION_TEST_PARAM(STRING, cipherPassSub);
    FUNCTION_LOG_END();

    ASSERT(pgVersion > 0 && pgSystemId > 0);

    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoArchive")
    {
        this = infoArchiveNewInternal();

        // Initialize the pg data
        this->infoPg = infoPgNew(infoPgArchive, cipherPassSub);
        infoArchivePgSet(this, pgVersion, pgSystemId);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/**********************************************************************************************************************************/
InfoArchive *
infoArchiveNewLoad(IoRead *read)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(IO_READ, read);
    FUNCTION_LOG_END();

    ASSERT(read != NULL);

    InfoArchive *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("InfoArchive")
    {
        this = infoArchiveNewInternal();
        this->infoPg = infoPgNewLoad(read, infoPgArchive, NULL, NULL);
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/**********************************************************************************************************************************/
const String *
infoArchiveIdHistoryMatch(
    const InfoArchive *this, const unsigned int historyId, const unsigned int pgVersion, const uint64_t pgSystemId)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(UINT, historyId);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    String *archiveId = NULL;
    InfoPg *infoPg = infoArchivePg(this);

    // Search the history list, from newest to oldest
    for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
    {
        InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);

        // If there is an exact match with the history, system and version then get the archiveId and stop
        if (historyId == pgDataArchive.id && pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
        {
            archiveId = infoPgArchiveId(infoPg, pgIdx);
            break;
        }
    }

    // If there was not an exact match, then search for the first matching database system-id and version
    if (archiveId == NULL)
    {
        for (unsigned int pgIdx = 0; pgIdx < infoPgDataTotal(infoPg); pgIdx++)
        {
            InfoPgData pgDataArchive = infoPgData(infoPg, pgIdx);

            if (pgSystemId == pgDataArchive.systemId && pgVersion == pgDataArchive.version)
            {
                archiveId = infoPgArchiveId(infoPg, pgIdx);
                break;
            }
        }
    }

    // If the archive id has not been found, then error
    if (archiveId == NULL)
    {
        THROW_FMT(
            ArchiveMismatchError,
            "unable to retrieve the archive id for database version '%s' and system-id '%" PRIu64 "'",
            strZ(pgVersionToStr(pgVersion)), pgSystemId);
    }

    FUNCTION_LOG_RETURN(STRING, archiveId);
}

/***********************************************************************************************************************************
Save to file
***********************************************************************************************************************************/
static void
infoArchiveSave(InfoArchive *this, IoWrite *write)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(IO_WRITE, write);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(write != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        infoPgSave(infoArchivePg(this), write, NULL, NULL);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
infoArchiveId(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoPgArchiveId(this->infoPg, infoPgDataCurrentId(this->infoPg)));
}

/**********************************************************************************************************************************/
const String *
infoArchiveCipherPass(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(infoPgCipherPass(this->infoPg));
}

/**********************************************************************************************************************************/
InfoPg *
infoArchivePg(const InfoArchive *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(INFO_ARCHIVE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->infoPg);
}

/**********************************************************************************************************************************/
InfoArchive *
infoArchivePgSet(InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, this);
        FUNCTION_LOG_PARAM(UINT, pgVersion);
        FUNCTION_LOG_PARAM(UINT64, pgSystemId);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    this->infoPg = infoPgSet(this->infoPg, infoPgArchive, pgVersion, pgSystemId, 0);

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, this);
}

/**********************************************************************************************************************************/
typedef struct InfoArchiveLoadFileData
{
    MemContext *memContext;                                         // Mem context
    const Storage *storage;                                         // Storage to load from
    const String *fileName;                                         // Base filename
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    InfoArchive *infoArchive;                                       // Loaded infoArchive object
} InfoArchiveLoadFileData;

static bool
infoArchiveLoadFileCallback(void *data, unsigned int try)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(UINT, try);
    FUNCTION_LOG_END();

    ASSERT(data != NULL);

    InfoArchiveLoadFileData *loadData = (InfoArchiveLoadFileData *)data;
    bool result = false;

    if (try < 2)
    {
        // Construct filename based on try
        const String *fileName = try == 0 ? loadData->fileName : strNewFmt("%s" INFO_COPY_EXT, strZ(loadData->fileName));

        // Attempt to load the file
        IoRead *read = storageReadIo(storageNewReadP(loadData->storage, fileName));
        cipherBlockFilterGroupAdd(ioReadFilterGroup(read), loadData->cipherType, cipherModeDecrypt, loadData->cipherPass);

        MEM_CONTEXT_BEGIN(loadData->memContext)
        {
            loadData->infoArchive = infoArchiveNewLoad(read);
            result = true;
        }
        MEM_CONTEXT_END();
    }

    FUNCTION_LOG_RETURN(BOOL, result);
}

InfoArchive *
infoArchiveLoadFile(const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    InfoArchiveLoadFileData data =
    {
        .memContext = memContextCurrent(),
        .storage = storage,
        .fileName = fileName,
        .cipherType = cipherType,
        .cipherPass = cipherPass,
    };

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *fileNamePath = strZ(storagePathP(storage, fileName));

        TRY_BEGIN()
        {
            infoLoad(
                strNewFmt("unable to load info file '%s' or '%s" INFO_COPY_EXT "'", fileNamePath, fileNamePath),
                infoArchiveLoadFileCallback, &data);
        }
        CATCH_ANY()
        {
            THROWP_FMT(
                errorType(),
                "%s\n"
                "HINT: archive.info cannot be opened but is required to push/get WAL segments.\n"
                "HINT: is archive_command configured correctly in postgresql.conf?\n"
                "HINT: has a stanza-create been performed?\n"
                "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.",
                errorMessage());
        }
        TRY_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(INFO_ARCHIVE, data.infoArchive);
}

/**********************************************************************************************************************************/
void
infoArchiveSaveFile(
    InfoArchive *infoArchive, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(INFO_ARCHIVE, infoArchive);
        FUNCTION_LOG_PARAM(STORAGE, storage);
        FUNCTION_LOG_PARAM(STRING, fileName);
        FUNCTION_LOG_PARAM(ENUM, cipherType);
        FUNCTION_TEST_PARAM(STRING, cipherPass);
    FUNCTION_LOG_END();

    ASSERT(infoArchive != NULL);
    ASSERT(storage != NULL);
    ASSERT(fileName != NULL);
    ASSERT((cipherType == cipherTypeNone && cipherPass == NULL) || (cipherType != cipherTypeNone && cipherPass != NULL));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Write output into a buffer since it needs to be saved to storage twice
        Buffer *buffer = bufNew(ioBufferSize());
        IoWrite *write = ioBufferWriteNew(buffer);
        cipherBlockFilterGroupAdd(ioWriteFilterGroup(write), cipherType, cipherModeEncrypt, cipherPass);
        infoArchiveSave(infoArchive, write);

        // Save the file and make a copy
        storagePutP(storageNewWriteP(storage, fileName), buffer);
        storagePutP(storageNewWriteP(storage, strNewFmt("%s" INFO_COPY_EXT, strZ(fileName))), buffer);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
