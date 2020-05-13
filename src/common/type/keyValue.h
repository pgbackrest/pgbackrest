/***********************************************************************************************************************************
Key Value Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_KEYVALUE_H
#define COMMON_TYPE_KEYVALUE_H

/***********************************************************************************************************************************
KeyValue object
***********************************************************************************************************************************/
#define KEY_VALUE_TYPE                                              KeyValue
#define KEY_VALUE_PREFIX                                            kv

typedef struct KeyValue KeyValue;

#include "common/type/variantList.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
KeyValue *kvNew(void);
KeyValue *kvDup(const KeyValue *source);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add value to key -- if the key does not exist then this works the same as kvPut()
KeyValue *kvAdd(KeyValue *this, const Variant *key, const Variant *value);

// List of keys
const VariantList *kvKeyList(const KeyValue *this);

KeyValue *kvMove(KeyValue *this, MemContext *parentNew);

// Put key/value pair
KeyValue *kvPut(KeyValue *this, const Variant *key, const Variant *value);

// Put key/value store. If this is called on an existing key it will replace the key with an empty kev/value store, even if the key
// already contains a key/value store.
KeyValue *kvPutKv(KeyValue *this, const Variant *key);

// Get a value using the key
const Variant *kvGet(const KeyValue *this, const Variant *key);

// Get a value using the key and return a default if not found
const Variant *kvGetDefault(const KeyValue *this, const Variant *key, const Variant *defaultValue);

// Does the key exist (even if the value is NULL)
bool kvKeyExists(const KeyValue *this, const Variant *key);

// Get a value as a list (even if there is only one value) using the key
VariantList *kvGetList(const KeyValue *this, const Variant *key);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void kvFree(KeyValue *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_KEY_VALUE_TYPE                                                                                                \
    KeyValue *
#define FUNCTION_LOG_KEY_VALUE_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "KeyValue", buffer, bufferSize)

#endif
