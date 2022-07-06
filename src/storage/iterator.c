/***********************************************************************************************************************************
Storage Iterator
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/debug.h"
#include "common/log.h"
#include "common/regExp.h"
#include "common/type/list.h"
#include "storage/iterator.h"
#include "storage/list.h"
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

    List *stack;                                                    // Stack of info lists
    bool returnedNext;                                              // Next info was returned
    StorageInfo infoNext;                                           // Info to be returned by next
    String *nameNext;                                               // Name for next info
};

// Path info list
typedef struct StorageIteratorInfo
{
    String *pathSub;                                                // Subpath
    StorageList *list;                                              // Storage info list
    unsigned int listIdx;                                           // Current index in info list
    bool pathContentSkip;                                           // Skip reading path content
} StorageIteratorInfo;

/***********************************************************************************************************************************
Check a path and add it to the stack if it exists and has content
***********************************************************************************************************************************/
typedef struct StorageItrPathAddResult
{
    bool exists;                                                    // Does the path exist?
    bool content;                                                   // Does the path have content?
} StorageItrPathAddResult;

static StorageItrPathAddResult
storageItrPathAdd(StorageIterator *const this, const String *const pathSub)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STORAGE_ITERATOR, this);
        FUNCTION_LOG_PARAM(STRING, pathSub);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    StorageItrPathAddResult result = {0};

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Get path content
        StorageList *const list = storageInterfaceListP(
            this->driver, pathSub == NULL ? this->path : strNewFmt("%s/%s", strZ(this->path), strZ(pathSub)), this->level,
            .expression = this->expression);

        // If path exists
        if (list != NULL)
        {
            result.exists = true;

            // If the path has content
            if (!storageLstEmpty(list))
            {
                result.content = true;

                // Sort if requested
                if (this->sortOrder != sortOrderNone)
                    storageLstSort(list, this->sortOrder);

                // Add path to top of stack
                MEM_CONTEXT_OBJ_BEGIN(this->stack)
                {
                    OBJ_NEW_BEGIN(StorageIteratorInfo, .childQty = MEM_CONTEXT_QTY_MAX, .allocQty = 1)
                    {
                        StorageIteratorInfo *const listInfo = OBJ_NEW_ALLOC();
                        *listInfo = (StorageIteratorInfo){
                            .pathSub = strDup(pathSub), .list = storageLstMove(list, memContextCurrent())};

                        lstAdd(this->stack, &listInfo);
                    }
                    OBJ_NEW_END();
                }
                MEM_CONTEXT_OBJ_END();
            }
        }
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
                .stack = lstNewP(sizeof(StorageIteratorInfo *)),
                .nameNext = strNew(),
                .returnedNext = true,
            };

            // Compile regular expression
            if (this->expression != NULL)
                this->regExp = regExpNew(this->expression);

            // If root path does not exist
            if (!storageItrPathAdd(this, NULL).exists)
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

    // Search stack for info
    while (lstSize(this->stack) != 0)
    {
        // Always pull from the top of the stack
        StorageIteratorInfo *const listInfo = *(StorageIteratorInfo **)lstGetLast(this->stack);

        // Search list for info
        for (; listInfo->listIdx < storageLstSize(listInfo->list); listInfo->listIdx++)
        {
            // Next info
            this->infoNext = storageLstGet(listInfo->list, listInfo->listIdx);

            // Update info name when in subpath
            if (listInfo->pathSub != NULL)
            {
                strCatFmt(strTrunc(this->nameNext), "%s/%s", strZ(listInfo->pathSub), strZ(this->infoNext.name));

                this->infoNext.name = this->nameNext;
            }

            // Does the path have content?
            const bool pathContent =
                this->infoNext.type == storageTypePath && this->recurse && !listInfo->pathContentSkip &&
                storageItrPathAdd(this, this->infoNext.name).content;

            // Clear path content skip flag if it was set on a previous iteration
            listInfo->pathContentSkip = false;

            // Skip info if it does match the provided expression
            if (this->regExp != NULL && !regExpMatch(this->regExp, this->infoNext.name))
            {
                // Path content may match the expression even if the path does not. Break so the content on the top of the stack
                // will be checked and increment the index so that path will be skipped.
                if (pathContent)
                {
                    listInfo->listIdx++;
                    break;
                }

                // Skip info and continue
                continue;
            }

            // When sort order is descending the path will need to be output after the content. Break so that content gets checked
            // but set a flag so on the next iteration the path will be output but the content skipped.
            if (pathContent && this->sortOrder == sortOrderDesc)
            {
                listInfo->pathContentSkip = true;
                break;
            }

            // Return next info
            this->returnedNext = false;
            listInfo->listIdx++;

            FUNCTION_TEST_RETURN(BOOL, true);
        }

        // If no more info then free the list. This check is required because we may break out of the above loop early.
        if (listInfo->listIdx >= storageLstSize(listInfo->list))
        {
            objFree(listInfo);
            lstRemoveLast(this->stack);
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
    return strNewFmt("{stack: %s}", strZ(lstToLog(this->stack)));
}
