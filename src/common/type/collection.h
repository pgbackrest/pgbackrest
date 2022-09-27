/***********************************************************************************************************************************
Define the interface for an abstract, iterable Collection and provide "syntactic sugar" for scanning across collections which
implement the "IterNew()" method. For information on how to create an "Iterable" collection, see the "Iterator.c" file.
These interfaces are inspired by Rust's Collection and Iterable traits.

Using "syntactic sugar" to scan a list,
    ItemType *item;
    FOREACH(item, List, list)
        doSomething(*item);
    ENDFOREACH;

This file also defines an abstract Collection type, which is a wrapper around any iterable collection.
Once wrapped, the Collection can be passed around and the users don't need to know the details of what type
of collection is inside.

    // Construct an abstract Collection which wraps the List.
    Collection *collection = NEWCOLLECTION(List, list);

    // Iterate through the abstract Collection just like any other collection.
    ItemType *item;
    FOREACH(item, Collection, collection)
        doSomething(*item)
    ENDFOREACH;
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_COLLECTION_H
#define COMMON_TYPE_COLLECTION_H

#include <stdbool.h>
#include "common/assert.h"
#include "common/type/object.h"
#include "common/type/string.h"

// A Polymorphic interface for Iterable collections.
typedef struct Collection Collection;

/***********************************************************************************************************************************
Construct an abstract Collection from a concrete collection (eg. List)
Use a macro front end so we can support polymorphic collections.
Note we depend on casting between compatible function pointers where return value and arguments must also be compatible.
    (void *  and struct * are compatible)
***********************************************************************************************************************************/
#define NEWCOLLECTION(SubType, subCollection)                                                                                      \
    collectionNew(                                                                                                                 \
        subCollection,                                              /* The collection we are wrapping */                           \
        (void *(*)(void*))METHOD(SubType, ItrNew),                  /* Get an iterator to the collection  */                       \
        (void *(*)(void*))METHOD(SubType, ItrNext)                  /* Get next item using the iterator  */                        \
    )

// Helper to construct an abstract collection.
Collection *collectionNew(void *subCollection, void *(*newItr)(void*), void *(*next)(void*));

// Iterator to scan an abstract Collecton.
typedef struct CollectionItr CollectionItr;
#define CAMEL_Collection collection
CollectionItr *collectionItrNew(Collection *collection);
void *collectionItrNext(CollectionItr *this);

/***********************************************************************************************************************************
Syntactic sugar to make iteration looks like C++ or Python.
Note FOREACH and ENDFOREACH are block macros, so the overall pair must be terminated with a semicolon.
The macro uses two nexted memory contexts. The outer context contains the iterator and is freed when the loop exits.
The inner context periodically frees up memory allocated during loop execution to prevent runaway memory leaks.
***********************************************************************************************************************************/
#define FOREACH(item, CollectionType, collection)                                                                                  \
    MEM_CONTEXT_TEMP_BEGIN()                                                                                                       \
        CollectionType##Itr *FOREACH_itr = METHOD(CollectionType, ItrNew)(collection);                                             \
        MEM_CONTEXT_TEMP_RESET_BEGIN()                                                                                             \
            while ( (item = METHOD(CollectionType,ItrNext)(FOREACH_itr)) != NULL)                                                  \
            {                                                                                                                      \
                MEM_CONTEXT_TEMP_RESET(1000);
#define ENDFOREACH                                                                                                                 \
            }                                                                                                                      \
        MEM_CONTEXT_TEMP_END();                                                                                                    \
    MEM_CONTEXT_TEMP_END()

// Syntactic sugar to get the function name from the object type and the short method name.
// Used to generate method names for abstract interfaces.
//    eg.   METHOD(List, Get) -->  listGet
// Note it takes an (obscure) second level of indirection and a "CAMEL_Type" macro to make this work.
#define METHOD(type, method)  JOIN(CAMEL(type), method)
#define JOIN(a,b) JOIN_AGAIN(a,b)
#define JOIN_AGAIN(a, b) a ## b

// We can't really convert to camel case, but we can invoke the symbol CAMEL_<type name> to get it.
// Each of the collection types must provide a CAMEL_* macro to provide the type name in camelCase.
// In the case of iterators, we append Itr to the collection type, so we don't have to create CAMEL_TypeItr.
#define CAMEL(type) CAMEL_##type

/***********************************************************************************************************************************
Macros for function logging.
***********************************************************************************************************************************/
#define FUNCTION_LOG_COLLECTION_TYPE                                                                                               \
    Collection *
#define FUNCTION_LOG_COLLECTION_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, collectionToLog, buffer, bufferSize)
String *collectionToLog(const Collection *this);

#define FUNCTION_LOG_COLLECTION_ITR_TYPE                                                                                           \
    CollectionItr *

#endif //COMMON_TYPE_COLLECTION_H
