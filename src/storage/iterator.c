/***********************************************************************************************************************************
Storage Iterator
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/list.h"
#include "storage/iterator.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageIterator
{
    void *driver;                                                   // Storage driver
    const String *path;                                             // Path to iterate
    StorageInfoLevel level;                                         // Info level
    // bool errorOnMissing;                                            // Error when path is missing
    // bool nullOnMissing;                                             // Return NULL on missing
    bool recurse;                                                   // Recurse into paths
    SortOrder sortOrder;                                            // Sort order
    const String *expression;                                       // Match expression
    RegExp *regExp;                                                 // Parsed match expression

    List *list;                                                     // Info list
    unsigned int listIdx;                                           // Current index in info list
};

/**********************************************************************************************************************************/
typedef struct StorageIterData
{
    List *list;                                                     // List to collect info in
    RegExp *regExp;                                                 // Compiled filter for names
    bool recurse;                                                   // Should we recurse?
    const String *pathSub;                                          // Path below the top-level path (starts as NULL)
} StorageIterData;

static void
storageItrCallback(void *data, const StorageInfo *info)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM_P(VOID, data);
        FUNCTION_LOG_PARAM(STORAGE_INFO, info);
    FUNCTION_TEST_END();

    // Skip .
    if (strEq(info->name, DOT_STR))
        FUNCTION_TEST_RETURN_VOID();

    // Add info if there is no expression or the expression matches
    const StorageIterData *const iterData = data;
    const String *const pathFile = iterData->pathSub == NULL ?
        strDup(info->name) : strNewFmt("%s/%s", strZ(iterData->pathSub), strZ(info->name));

    if ((info->type == storageTypePath && iterData->recurse) ||
        (iterData->regExp == NULL || regExpMatch(iterData->regExp, pathFile)))
    {
        MEM_CONTEXT_OBJ_BEGIN(iterData->list)
        {
            StorageInfo *const infoCopy = lstAdd(iterData->list, info);

            infoCopy->name = strDup(pathFile);
            infoCopy->user = strDup(info->user);
            infoCopy->group = strDup(info->group);
            infoCopy->linkDestination = strDup(info->linkDestination);
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_TEST_RETURN_VOID();
}

StorageIterator *
storageItrNew(
    void *const driver, const String *const path, const StorageInfoLevel level, const bool errorOnMissing, const bool nullOnMissing,
    const bool recurse, const SortOrder sortOrder, const String *const expression)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM_P(VOID, driver);
        FUNCTION_LOG_PARAM(STRING, path);
        FUNCTION_LOG_PARAM(ENUM, level);
        FUNCTION_LOG_PARAM(BOOL, errorOnMissing);
        FUNCTION_LOG_PARAM(BOOL, nullOnMissing);
        FUNCTION_LOG_PARAM(BOOL, recurse);
        FUNCTION_LOG_PARAM(ENUM, sortOrder);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    StorageIterator *this = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        OBJ_NEW_BEGIN(StorageIterator, .childQty = MEM_CONTEXT_QTY_MAX)
        {
            // Create object
            this = OBJ_NEW_ALLOC();

            *this = (StorageIterator)
            {
                .driver = driver,
                .path = strDup(path),
                .level = level,
                // .errorOnMissing = errorOnMissing,
                // .nullOnMissing = nullOnMissing,
                .recurse = recurse,
                .sortOrder = sortOrder,
                .expression = strDup(expression),
                .list = lstNewP(sizeof(StorageInfo), .comparator = lstComparatorStr),
            };

            if (this->expression != NULL)
                this->regExp = regExpNew(this->expression);

            MEM_CONTEXT_TEMP_BEGIN()
            {
                StorageIterData iterData = {.list = this->list, .recurse = this->recurse, .regExp = this->regExp};

                if (storageInterfaceInfoListP(
                        this->driver, path, this->level, storageItrCallback, &iterData, .expression = this->expression))
                {
                    if (this->recurse)
                    {
                        unsigned int listIdx = 0;

                        while (listIdx < lstSize(this->list))
                        {
                            const StorageInfo *const info = lstGet(this->list, listIdx);

                            if (info->type == storageTypePath)
                            {
                                iterData.pathSub = info->name;

                                storageInterfaceInfoListP(
                                    this->driver, strNewFmt("%s/%s", strZ(path), strZ(iterData.pathSub)), this->level,
                                    storageItrCallback, &iterData, .expression = this->expression);

                                if (iterData.regExp != NULL && !regExpMatch(iterData.regExp, info->name))
                                {
                                    lstRemoveIdx(this->list, listIdx);
                                    continue;
                                }
                            }

                            listIdx++;
                        }
                    }

                    if (this->sortOrder != sortOrderNone)
                        lstSort(this->list, this->sortOrder);
                }
                else
                {
                    if (errorOnMissing)
                        THROW_FMT(PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, strZ(path));

                    if (nullOnMissing)
                        this = NULL;
                }
            }
            MEM_CONTEXT_TEMP_END();
        }
        OBJ_NEW_END();

        storageItrMove(this, memContextPrior());
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(STORAGE_ITERATOR, this);
}

/**********************************************************************************************************************************/
bool
storageItrMore(StorageIterator *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, this);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(BOOL, this->listIdx < lstSize(this->list));
}

/**********************************************************************************************************************************/
StorageInfo storageItrNext(StorageIterator *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, this);
    FUNCTION_TEST_END();

    const StorageInfo *const result = lstGet(this->list, this->listIdx);
    this->listIdx++;

    FUNCTION_TEST_RETURN(STORAGE_INFO, *result);
}

/**********************************************************************************************************************************/
String *
storageItrToLog(const StorageIterator *const this)
{
    String *const result = strNewFmt("{list: %s}", strZ(lstToLog(this->list)));
    // String *result = strNewFmt(
    //     "{used: %zu, size: %zu%s", bufUsed(this), bufSize(this),
    //     bufSizeLimit(this) ? zNewFmt(", sizeAlloc: %zu}", bufSizeAlloc(this)) : "}");

    return result;
}
