/***********************************************************************************************************************************
Key Value Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>

#include "common/debug.h"
#include "common/memContext.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "common/type/object.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constant to indicate key not found
***********************************************************************************************************************************/
#define KEY_NOT_FOUND                                               UINT_MAX

/***********************************************************************************************************************************
Contains information about the key value store
***********************************************************************************************************************************/
struct KeyValue
{
    MemContext *memContext;                                         // Mem context for the store
    List *list;                                                     // List of keys/values
    VariantList *keyList;                                           // List of keys
};

OBJECT_DEFINE_MOVE(KEY_VALUE);
OBJECT_DEFINE_FREE(KEY_VALUE);

/***********************************************************************************************************************************
Contains information about an individual key/value pair
***********************************************************************************************************************************/
typedef struct KeyValuePair
{
    Variant *key;                                                   // The key
    Variant *value;                                                 // The value (this may be NULL)
} KeyValuePair;

/***********************************************************************************************************************************
Create a new key/value store
***********************************************************************************************************************************/
KeyValue *
kvNew(void)
{
    FUNCTION_TEST_VOID();

    KeyValue *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("KeyValue")
    {
        // Allocate state and set context
        this = memNew(sizeof(KeyValue));

        *this = (KeyValue)
        {
            .memContext = MEM_CONTEXT_NEW(),
            .list = lstNew(sizeof(KeyValuePair)),
            .keyList = varLstNew(),
        };
    }
    MEM_CONTEXT_NEW_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Duplicate key/value store
***********************************************************************************************************************************/
KeyValue *
kvDup(const KeyValue *source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, source);
    FUNCTION_TEST_END();

    ASSERT(source != NULL);

    KeyValue *this = kvNew();

    // Duplicate all key/values
    for (unsigned int listIdx = 0; listIdx < lstSize(source->list); listIdx++)
    {
        const KeyValuePair *sourcePair = (const KeyValuePair *)lstGet(source->list, listIdx);

        // Copy the pair
        KeyValuePair pair;
        pair.key = varDup(sourcePair->key);
        pair.value = varDup(sourcePair->value);

        // Add to the list
        lstAdd(this->list, &pair);
    }

    this->keyList = varLstDup(source->keyList);

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Get key index if it exists
***********************************************************************************************************************************/
static unsigned int
kvGetIdx(const KeyValue *this, const Variant *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    // Search for the key
    unsigned int result = KEY_NOT_FOUND;

    for (unsigned int listIdx = 0; listIdx < lstSize(this->list); listIdx++)
    {
        const KeyValuePair *pair = (const KeyValuePair *)lstGet(this->list, listIdx);

        // Break if the key matches
        if (varEq(key, pair->key))
        {
            result = listIdx;
            break;
        }
    }

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Does the key exist (even if the value is NULL)
***********************************************************************************************************************************/
bool
kvKeyExists(const KeyValue *this, const Variant *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    FUNCTION_TEST_RETURN(kvGetIdx(this, key) != KEY_NOT_FOUND);
}

/***********************************************************************************************************************************
Get list of keys
***********************************************************************************************************************************/
const VariantList *
kvKeyList(const KeyValue *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);

    FUNCTION_TEST_RETURN(this->keyList);
}

/***********************************************************************************************************************************
Internal put key/value pair

Handles the common logic for the external put functions. The correct mem context should be set before calling this function.
***********************************************************************************************************************************/
static void
kvPutInternal(KeyValue *this, const Variant *key, Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    // Find the key
    unsigned int listIdx = kvGetIdx(this, key);

    // If the key was not found then add it
    if (listIdx == KEY_NOT_FOUND)
    {
        // Copy the pair
        KeyValuePair pair;
        pair.key = varDup(key);
        pair.value = value;

        // Add to the list
        lstAdd(this->list, &pair);

        // Add to the key list
        varLstAdd(this->keyList, varDup(key));
    }
    // Else update it
    else
    {
        KeyValuePair *pair = (KeyValuePair *)lstGet(this->list, listIdx);

        if (pair->value != NULL)
            varFree(pair->value);

        pair->value = value;
    }

    FUNCTION_TEST_RETURN_VOID();
}

/***********************************************************************************************************************************
Put key/value pair
***********************************************************************************************************************************/
KeyValue *
kvPut(KeyValue *this, const Variant *key, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        kvPutInternal(this, key, varDup(value));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Add value to key -- if the key does not exist then this works the same as kvPut()
***********************************************************************************************************************************/
KeyValue *
kvAdd(KeyValue *this, const Variant *key, const Variant *value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        // Find the key
        unsigned int listIdx = kvGetIdx(this, key);

        // If the key was not found then add it
        if (listIdx == KEY_NOT_FOUND)
        {
            kvPutInternal(this, key, varDup(value));
        }
        // Else create or add to the variant list
        else
        {
            KeyValuePair *pair = (KeyValuePair *)lstGet(this->list, listIdx);

            if (pair->value == NULL)
                pair->value = varDup(value);
            else if (varType(pair->value) != varTypeVariantList)
            {
                Variant *valueList = varNewVarLst(varLstNew());
                varLstAdd(varVarLst(valueList), pair->value);
                varLstAdd(varVarLst(valueList), varDup(value));
                pair->value = valueList;
            }
            else
                varLstAdd(varVarLst(pair->value), varDup(value));
        }
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(this);
}

/***********************************************************************************************************************************
Put key/value store

If this is called on an existing key it will replace the key with an empty kev/value store, even if the key already contains a
key/value store.
***********************************************************************************************************************************/
KeyValue *
kvPutKv(KeyValue *this, const Variant *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    KeyValue *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        result = kvNew();
        kvPutInternal(this, key, varNewKv(result));
    }
    MEM_CONTEXT_END();

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get a value using the key
***********************************************************************************************************************************/
const Variant *
kvGet(const KeyValue *this, const Variant *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    Variant *result = NULL;

    // Find the key
    unsigned int listIdx = kvGetIdx(this, key);

    if (listIdx != KEY_NOT_FOUND)
        result = ((KeyValuePair *)lstGet(this->list, listIdx))->value;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get a value using the key and return a default if not found
***********************************************************************************************************************************/
const Variant *
kvGetDefault(const KeyValue *this, const Variant *key, const Variant *defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    const Variant *result = NULL;

    // Find the key
    unsigned int listIdx = kvGetIdx(this, key);

    // If key not found then return default, else return the value
    if (listIdx == KEY_NOT_FOUND)
        result = defaultValue;
    else
        result = ((KeyValuePair *)lstGet(this->list, listIdx))->value;

    FUNCTION_TEST_RETURN(result);
}

/***********************************************************************************************************************************
Get a value as a list (even if there is only one value) using the key
***********************************************************************************************************************************/
VariantList *
kvGetList(const KeyValue *this, const Variant *key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    VariantList *result = NULL;

    // Get the value
    const Variant *value = kvGet(this, key);

    // Convert the value to a list if it is not already one
    if (value != NULL && varType(value) == varTypeVariantList)
        result = varLstDup(varVarLst(value));
    else
        result = varLstAdd(varLstNew(), varDup(value));

    FUNCTION_TEST_RETURN(result);
}
