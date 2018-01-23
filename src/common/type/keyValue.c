/***********************************************************************************************************************************
Key Value Handler
***********************************************************************************************************************************/
#include "common/memContext.h"
#include "common/type.h"
#include "common/type/keyValue.h"
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
kvNew()
{
    KeyValue *this = NULL;

    MEM_CONTEXT_NEW_BEGIN("keyValue")
    {
        // Allocate state and set context
        this = memNew(sizeof(KeyValue));
        this->memContext = MEM_CONTEXT_NEW();

        // Initialize list
        this->list = lstNew(sizeof(KeyValuePair));
        this->keyList = varLstNew();
    }
    MEM_CONTEXT_NEW_END();

    return this;
}

/***********************************************************************************************************************************
Duplicate key/value store
***********************************************************************************************************************************/
KeyValue *
kvDup(const KeyValue *source)
{
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

    return this;
}

/***********************************************************************************************************************************
Get key index if it exists
***********************************************************************************************************************************/
static unsigned int
kvGetIdx(const KeyValue *this, const Variant *key)
{
    // Search for the key
    unsigned int listIdx = 0;

    for (; listIdx < lstSize(this->list); listIdx++)
    {
        const KeyValuePair *pair = (const KeyValuePair *)lstGet(this->list, listIdx);

        // Break if the key matches
        if (varEq(key, pair->key))
            break;
    }

    // If key was not found
    if (listIdx == lstSize(this->list))
        return KEY_NOT_FOUND;

    return listIdx;
}

/***********************************************************************************************************************************
Get list of keys
***********************************************************************************************************************************/
const VariantList *
kvKeyList(const KeyValue *this)
{
    return this->keyList;
}

/***********************************************************************************************************************************
Internal put key/value pair

Handles the common logic for the external put functions. The correct mem context should be set before calling this function.
***********************************************************************************************************************************/
static void
kvPutInternal(KeyValue *this, const Variant *key, Variant *value)
{
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
}

/***********************************************************************************************************************************
Put key/value pair
***********************************************************************************************************************************/
KeyValue *
kvPut(KeyValue *this, const Variant *key, const Variant *value)
{
    MEM_CONTEXT_BEGIN(this->memContext)
    {
        kvPutInternal(this, key, varDup(value));
    }
    MEM_CONTEXT_END();

    return this;
}

/***********************************************************************************************************************************
Add value to key -- if the key does not exist then this works the same as kvPut()
***********************************************************************************************************************************/
KeyValue *
kvAdd(KeyValue *this, const Variant *key, const Variant *value)
{
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
                Variant *valueList = varNewVarLstEmpty();
                varLstAdd(varVarLst(valueList), pair->value);
                varLstAdd(varVarLst(valueList), varDup(value));
                pair->value = valueList;
            }
            else
                varLstAdd(varVarLst(pair->value), varDup(value));
        }
    }
    MEM_CONTEXT_END();

    return this;
}

/***********************************************************************************************************************************
Put key/value store

If this is called on an existing key it will replace the key with an empty kev/value store, even if the key already contains a
key/value store.
***********************************************************************************************************************************/
KeyValue *
kvPutKv(KeyValue *this, const Variant *key)
{
    KeyValue *result = NULL;

    MEM_CONTEXT_BEGIN(this->memContext)
    {
        Variant *keyValue = varNewKv();
        result = varKv(keyValue);

        kvPutInternal(this, key, keyValue);
    }
    MEM_CONTEXT_END();

    return result;
}

/***********************************************************************************************************************************
Get a value using the key
***********************************************************************************************************************************/
const Variant *
kvGet(const KeyValue *this, const Variant *key)
{
    Variant *result = NULL;

    // Find the key
    unsigned int listIdx = kvGetIdx(this, key);

    if (listIdx != KEY_NOT_FOUND)
        result = ((KeyValuePair *)lstGet(this->list, listIdx))->value;

    return result;
}

/***********************************************************************************************************************************
Get a value as a list (even if there is only one value) using the key
***********************************************************************************************************************************/
VariantList *
kvGetList(const KeyValue *this, const Variant *key)
{
    VariantList *result = NULL;

    // Get the value
    const Variant *value = kvGet(this, key);

    // Convert the value to a list if it is not already one
    if (value != NULL && varType(value) == varTypeVariantList)
        result = varLstDup(varVarLst(value));
    else
        result = varLstAdd(varLstNew(), varDup(value));

    return result;
}

/***********************************************************************************************************************************
Free the string
***********************************************************************************************************************************/
void
kvFree(KeyValue *this)
{
    memContextFree(this->memContext);
}
