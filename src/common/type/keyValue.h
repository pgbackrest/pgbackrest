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
KeyValue *kvNew(void);
KeyValue *kvDup(const KeyValue *source);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct KeyValuePub
{
    MemContext *memContext;                                         // Mem context
    VariantList *keyList;                                           // List of keys
} KeyValuePub;

// List of keys
__attribute__((always_inline)) static inline const VariantList *
kvKeyList(const KeyValue *const this)
{
    return THIS_PUB(KeyValue)->keyList;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add value to key -- if the key does not exist then this works the same as kvPut()
KeyValue *kvAdd(KeyValue *this, const Variant *key, const Variant *value);

// Move to a new parent mem context
__attribute__((always_inline)) static inline KeyValue *
kvMove(KeyValue *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Put key/value pair
KeyValue *kvPut(KeyValue *this, const Variant *key, const Variant *value);

// Put key/value store. If this is called on an existing key it will replace the key with an empty kev/value store, even if the key
// already contains a key/value store.
KeyValue *kvPutKv(KeyValue *this, const Variant *key);

// Get a value using the key
const Variant *kvGet(const KeyValue *this, const Variant *key);

// Get a value using the key and return a default if not found
const Variant *kvGetDefault(const KeyValue *this, const Variant *key, const Variant *defaultValue);

// Get key index if it exists
unsigned int kvGetIdx(const KeyValue *this, const Variant *key);

// Does the key exist (even if the value is NULL)
__attribute__((always_inline)) static inline bool
kvKeyExists(const KeyValue *const this, const Variant *const key)
{
    return kvGetIdx(this, key) != KEY_NOT_FOUND;
}

// Get a value as a list (even if there is only one value) using the key
VariantList *kvGetList(const KeyValue *this, const Variant *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
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
    objToLog(value, "KeyValue", buffer, bufferSize)

#endif
