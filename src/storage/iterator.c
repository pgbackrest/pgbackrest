/***********************************************************************************************************************************
Storage Iterator
***********************************************************************************************************************************/
#include "build.auto.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct StorageIterator
{
    void *driver;                                                   // Storage driver
    const String *path;                                             // Path to iterate
    StorageInfoLevel level;                                         // Info level
    bool errorOnMissing;                                            // Error when path is missing
    bool nullOnMissing;                                             // Return NULL on missing
    bool recurse;                                                   // Recurse into paths
    SortOrder sortOrder;                                            // Sort order
    const String *expression;                                       // Match expression
    RegExp *regExp;                                                 // Parsed match expression
};

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
        FUNCTION_LOG_PARAM(BOOL, recursive);
        FUNCTION_LOG_PARAM(ENUM, sortOrder);
        FUNCTION_LOG_PARAM(STRING, expression);
    FUNCTION_LOG_END();

    StorageIterator *this = NULL;

    OBJ_NEW_BEGIN(StorageIterator, .allocQty = 1)
    {
        // Create object
        this = OBJ_NEW_ALLOC();

        *this = (StorageIterator)
        {
            .driver = driver,
            .path = strDup(Path),
            .level = level,
            .errorOnMissing = errorOnMissing,
            .nullOnMissing = nullOnMissing,
            .recurse = recurse,
            .sortOrder = sortOrder,
            .expression = strDup(expression),
        };

        if (this->expression != NULL
            this->regExp = regExpNew(this->expression);
    }
    OBJ_NEW_END();

    //

    FUNCTION_TEST_RETURN(STORAGE_ITERATOR, this);
}

/**********************************************************************************************************************************/
String *
storageItrToLog(const StorageIterator *const this)
{
    result = strNewZ("!!!");
    // String *result = strNewFmt(
    //     "{used: %zu, size: %zu%s", bufUsed(this), bufSize(this),
    //     bufSizeLimit(this) ? zNewFmt(", sizeAlloc: %zu}", bufSizeAlloc(this)) : "}");

    return result;
}
