/***********************************************************************************************************************************
Key Value Handler
***********************************************************************************************************************************/
#include "build.auto.h"

#include <limits.h>

#include "common/debug.h"
#include "common/type/keyValue.h"
#include "common/type/list.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Contains information about the key value store
***********************************************************************************************************************************/
struct KeyValue
{
    KeyValuePub pub;                                                // Publicly accessible variables
    List *list;                                                     // List of keys/values
};

/***********************************************************************************************************************************
Contains information about an individual key/value pair
***********************************************************************************************************************************/
typedef struct KeyValuePair
{
    Variant *key;                                                   // The key
    Variant *value;                                                 // The value (this may be NULL)
} KeyValuePair;

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvNew(void)
{
    FUNCTION_TEST_VOID();

    OBJ_NEW_BEGIN(KeyValue, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (KeyValue)
        {
            .pub =
            {
                .keyList = varLstNew(),
            },
            .list = lstNewP(sizeof(KeyValuePair)),
        };
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(KEY_VALUE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvDup(const KeyValue *const source)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, source);
    FUNCTION_TEST_END();

    ASSERT(source != NULL);

    KeyValue *const this = kvNew();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // Duplicate all key/values
        for (unsigned int listIdx = 0; listIdx < lstSize(source->list); listIdx++)
        {
            const KeyValuePair *const sourcePair = (const KeyValuePair *)lstGet(source->list, listIdx);
            lstAdd(this->list, &(KeyValuePair){.key = varDup(sourcePair->key), .value = varDup(sourcePair->value)});
        }

        // Duplicate key list
        this->pub.keyList = varLstDup(kvKeyList(source));
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(KEY_VALUE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN unsigned int
kvGetIdx(const KeyValue *const this, const Variant *const key)
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
        const KeyValuePair *const pair = (const KeyValuePair *)lstGet(this->list, listIdx);

        // Break if the key matches
        if (varEq(key, pair->key))
        {
            result = listIdx;
            break;
        }
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

/***********************************************************************************************************************************
Internal put key/value pair

Handles the common logic for the external put functions. The correct mem context should be set before calling this function.
***********************************************************************************************************************************/
static void
kvPutInternal(KeyValue *const this, const Variant *const key, Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    FUNCTION_AUDIT_HELPER();

    // Find the key
    const unsigned int listIdx = kvGetIdx(this, key);

    // If the key was not found then add it
    if (listIdx == KEY_NOT_FOUND)
    {
        // Copy the pair
        const KeyValuePair pair = {.key = varDup(key), .value = value};

        // Add to the list
        lstAdd(this->list, &pair);

        // Add to the key list
        varLstAdd(this->pub.keyList, varDup(key));
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

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvPut(KeyValue *const this, const Variant *const key, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        kvPutInternal(this, key, varDup(value));
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(KEY_VALUE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvAdd(KeyValue *const this, const Variant *const key, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        // Find the key
        const unsigned int listIdx = kvGetIdx(this, key);

        // If the key was not found then add it
        if (listIdx == KEY_NOT_FOUND)
        {
            kvPutInternal(this, key, varDup(value));
        }
        // Else create or add to the variant list
        else
        {
            KeyValuePair *const pair = (KeyValuePair *)lstGet(this->list, listIdx);

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
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(KEY_VALUE, this);
}

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvPutKv(KeyValue *const this, const Variant *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    KeyValue *const result = kvNew();

    MEM_CONTEXT_OBJ_BEGIN(this)
    {
        kvPutInternal(this, key, varNewKv(result));
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(KEY_VALUE, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const Variant *
kvGet(const KeyValue *const this, const Variant *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    Variant *result = NULL;

    // Find the key
    const unsigned int listIdx = kvGetIdx(this, key);

    if (listIdx != KEY_NOT_FOUND)
        result = ((KeyValuePair *)lstGet(this->list, listIdx))->value;

    FUNCTION_TEST_RETURN(VARIANT, result);
}

/**********************************************************************************************************************************/
FN_EXTERN const Variant *
kvGetDefault(const KeyValue *const this, const Variant *const key, const Variant *const defaultValue)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
        FUNCTION_TEST_PARAM(VARIANT, defaultValue);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    // Find the key
    const unsigned int listIdx = kvGetIdx(this, key);

    // If key not found then return default, else return the value
    if (listIdx == KEY_NOT_FOUND)
        FUNCTION_TEST_RETURN_CONST(VARIANT, defaultValue);

    FUNCTION_TEST_RETURN(VARIANT, ((KeyValuePair *)lstGet(this->list, listIdx))->value);
}

/**********************************************************************************************************************************/
FN_EXTERN VariantList *
kvGetList(const KeyValue *const this, const Variant *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    // Get the value
    const Variant *const value = kvGet(this, key);

    // Return the list if it already is one
    if (value != NULL && varType(value) == varTypeVariantList)
        FUNCTION_TEST_RETURN(VARIANT_LIST, varLstDup(varVarLst(value)));

    // Create a list to return
    VariantList *result;
    VariantList *const list = varLstNew();

    MEM_CONTEXT_OBJ_BEGIN(list)
    {
        result = varLstAdd(list, varDup(value));
    }
    MEM_CONTEXT_OBJ_END();

    FUNCTION_TEST_RETURN(VARIANT_LIST, result);
}

/**********************************************************************************************************************************/
FN_EXTERN KeyValue *
kvRemove(KeyValue *const this, const Variant *const key)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(KEY_VALUE, this);
        FUNCTION_TEST_PARAM(VARIANT, key);
    FUNCTION_TEST_END();

    ASSERT(this != NULL);
    ASSERT(key != NULL);

    // Find the key
    const unsigned int listIdx = kvGetIdx(this, key);

    // If the key was found, remove it
    if (listIdx != KEY_NOT_FOUND)
    {
        // Free the key/value being removed and remove from the list
        const KeyValuePair *const pair = (KeyValuePair *)lstGet(this->list, listIdx);

        varFree(pair->key);
        varFree(pair->value);
        lstRemoveIdx(this->list, listIdx);

        // Remove from the key list (index must be the same as the key/value list)
        ASSERT(varEq(key, varLstGet(this->pub.keyList, listIdx)));

        varFree(varLstGet(this->pub.keyList, listIdx));
        lstRemoveIdx((List *)this->pub.keyList, listIdx);
    }

    FUNCTION_TEST_RETURN(KEY_VALUE, this);
}
