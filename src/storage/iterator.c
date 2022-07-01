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
    bool recurse;                                                   // Recurse into paths
    SortOrder sortOrder;                                            // Sort order
    const String *expression;                                       // Match expression
    RegExp *regExp;                                                 // Parsed match expression

    List *list;                                                     // List of info lists
    bool returnedNext;                                              // Next info was returned
    StorageInfo infoNext;                                           // Info to be returned by next
    String *nameNext;                                               // Name for next info
};

typedef struct StorageIteratorInfo
{
    String *pathSub;                                                // Subpath
    List *list;                                                     // Info list
    unsigned int listIdx;                                           // Current index in info list
    bool pathContentSkip;                                           // Skip reading path content
} StorageIteratorInfo;

/***********************************************************************************************************************************
Check a path and add it to the stack if it exists and has content
***********************************************************************************************************************************/
typedef struct StorageItrPathAddResult
{
    bool found;                                                     // Was the path found?
    bool content;                                                   // Did the path have content?
} StorageItrPathAddResult;

static StorageItrPathAddResult
storageItrPathAdd(StorageIterator *const this, const String *const pathSub)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, this);
        FUNCTION_LOG_PARAM(STRING, pathSub);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageItrPathAddResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Append subpath to path when required
        const String *const path = pathSub == NULL ? this->path : strNewFmt("%s/%s", strZ(this->path), strZ(pathSub));

        MEM_CONTEXT_OBJ_BEGIN(this->list)
        {
            // Get path content
            StorageIteratorInfo listInfo =
            {
                .list = storageInterfaceListP(this->driver, path, this->level, .expression = this->expression),
            };

            if (listInfo.list != NULL)
            {
                result.found = true;

                // If the path has content
                if (!lstEmpty(listInfo.list))
                {
                    result.content = true;

                    // Put subpath in list context so they get freed together
                    MEM_CONTEXT_OBJ_BEGIN(listInfo.list)
                    {
                        listInfo.pathSub = strDup(pathSub);
                    }
                    MEM_CONTEXT_OBJ_END();

                    // Sort if needed
                    if (this->sortOrder != sortOrderNone)
                        lstSort(listInfo.list, this->sortOrder);

                    // Add path to stack
                    lstAdd(this->list, &listInfo);
                }
            }
        }
        MEM_CONTEXT_OBJ_END();
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_STRUCT(result);
}

/**********************************************************************************************************************************/
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

    ASSERT(driver != NULL);
    ASSERT(path != NULL);
    ASSERT(!recurse || level >= storageInfoLevelType);

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
                .recurse = recurse,
                .sortOrder = sortOrder,
                .expression = strDup(expression),
                .list = lstNewP(sizeof(StorageIteratorInfo)),
                .returnedNext = true,
            };

            // Compile regular expression
            if (this->expression != NULL)
                this->regExp = regExpNew(this->expression);

            // If root path was not found
            if (!storageItrPathAdd(this, NULL).found)
            {
                // Throw an error when requested
                if (errorOnMissing)
                    THROW_FMT(PathMissingError, STORAGE_ERROR_LIST_INFO_MISSING, strZ(this->path));

                // Return NULL when requested
                if (nullOnMissing)
                    this = NULL;
            }
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
        FUNCTION_TEST_PARAM(STORAGE_ITERATOR, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    // If next has not been called then return true
    if (!this->returnedNext)
        FUNCTION_TEST_RETURN(BOOL, true);

    while (lstSize(this->list) != 0)
    {
        StorageIteratorInfo *const listInfo = lstGetLast(this->list);

        for (; listInfo->listIdx < lstSize(listInfo->list); listInfo->listIdx++)
        {
            this->infoNext = *(StorageInfo *)lstGet(listInfo->list, listInfo->listIdx);

            if (listInfo->pathSub != NULL)
            {
                strFree(this->nameNext);

                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    this->nameNext = strNewFmt("%s/%s", strZ(listInfo->pathSub), strZ(this->infoNext.name));
                    this->infoNext.name = this->nameNext;
                }
                MEM_CONTEXT_OBJ_END();
            }

            // !!!
            bool pathFound = false;

            if (this->infoNext.type == storageTypePath && this->recurse && !listInfo->pathContentSkip)
            {
                MEM_CONTEXT_TEMP_BEGIN()
                {
                    const String *const path = strNewFmt("%s/%s", strZ(this->path), strZ(this->infoNext.name));

                    MEM_CONTEXT_OBJ_BEGIN(this->list)
                    {
                        StorageIteratorInfo listInfo =
                        {
                            .list = storageInterfaceListP(this->driver, path, this->level, .expression = this->expression),
                        };

                        if (listInfo.list != NULL && !lstEmpty(listInfo.list))
                        {
                            MEM_CONTEXT_OBJ_BEGIN(listInfo.list)
                            {
                                listInfo.pathSub = strDup(this->infoNext.name);
                            }
                            MEM_CONTEXT_OBJ_END();

                            if (this->sortOrder != sortOrderNone)
                                lstSort(listInfo.list, this->sortOrder);

                            lstAdd(this->list, &listInfo);
                            pathFound = true;
                        }
                    }
                    MEM_CONTEXT_OBJ_END();
                }
                MEM_CONTEXT_TEMP_END();
            }

            if (this->regExp != NULL && !regExpMatch(this->regExp, this->infoNext.name))
            {
                if (pathFound)
                {
                    listInfo->listIdx++;
                    break;
                }

                continue;
            }

            if (pathFound && this->sortOrder == sortOrderDesc)
            {
                listInfo->pathContentSkip = true;
                break;
            }

            // !!! Found
            this->returnedNext = false;
            listInfo->pathContentSkip = false;
            listInfo->listIdx++;

            FUNCTION_TEST_RETURN(BOOL, true);
        }

        // !!!
        if (listInfo->listIdx >= lstSize(listInfo->list))
        {
            lstFree(listInfo->list);
            lstRemoveLast(this->list);
        }
    }

    FUNCTION_TEST_RETURN(BOOL, false);
}

/**********************************************************************************************************************************/
StorageInfo storageItrNext(StorageIterator *const this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STORAGE_ITERATOR, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(!this->returnedNext);

    this->returnedNext = true;

    FUNCTION_TEST_RETURN(STORAGE_INFO, this->infoNext);
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
