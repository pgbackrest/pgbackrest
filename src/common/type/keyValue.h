/***********************************************************************************************************************************
Key Value Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_KEYVALUE_H
#define COMMON_TYPE_KEYVALUE_H

/***********************************************************************************************************************************
KeyValue object
***********************************************************************************************************************************/
typedef struct KeyValue KeyValue;

#include "common/type/object.h"
#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constant to indicate key not found
***********************************************************************************************************************************/
#define KEY_NOT_FOUND                                               UINT_MAX

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN KeyValue *kvNew(void);
FN_EXTERN KeyValue *kvDup(const KeyValue *source);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct KeyValuePub
{
    VariantList *keyList;                                           // List of keys
} KeyValuePub;

// List of keys
FN_INLINE_ALWAYS const VariantList *
kvKeyList(const KeyValue *const this)
{
    return THIS_PUB(KeyValue)->keyList;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add a value to an existing key. If the key does not exist then this works the same as kvPut(). If the key does exist then the
// value is converted to a VariantList with the existing value and the new value as list items. Subsequent calls expand the list.
//
// NOTE: this function is seldom required and kvPut() should be used when a key is expected to have a single value.
FN_EXTERN KeyValue *kvAdd(KeyValue *this, const Variant *key, const Variant *value);

// Move to a new parent mem context
FN_INLINE_ALWAYS KeyValue *
kvMove(KeyValue *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Put key/value pair
FN_EXTERN KeyValue *kvPut(KeyValue *this, const Variant *key, const Variant *value);

// Put key/value store. If this is called on an existing key it will replace the key with an empty kev/value store, even if the key
// already contains a key/value store.
FN_EXTERN KeyValue *kvPutKv(KeyValue *this, const Variant *key);

// Get a value using the key
FN_EXTERN const Variant *kvGet(const KeyValue *this, const Variant *key);

// Get a value using the key and return a default if not found
FN_EXTERN const Variant *kvGetDefault(const KeyValue *this, const Variant *key, const Variant *defaultValue);

// Get key index if it exists
FN_EXTERN unsigned int kvGetIdx(const KeyValue *this, const Variant *key);

// Does the key exist (even if the value is NULL)
FN_INLINE_ALWAYS bool
kvKeyExists(const KeyValue *const this, const Variant *const key)
{
    return kvGetIdx(this, key) != KEY_NOT_FOUND;
}

// Get a value as a list (even if there is only one value) using the key
FN_EXTERN VariantList *kvGetList(const KeyValue *this, const Variant *key);

// Remove a key/value pair
FN_EXTERN KeyValue *kvRemove(KeyValue *this, const Variant *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
kvFree(KeyValue *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_KEY_VALUE_TYPE                                                                                                \
    KeyValue *
#define FUNCTION_LOG_KEY_VALUE_FORMAT(value, buffer, bufferSize)                                                                   \
    objNameToLog(value, "KeyValue", buffer, bufferSize)

#endif
