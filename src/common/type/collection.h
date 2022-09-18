/***********************************************************************************************************************************
Define the interface for an abstract, iterable Collection and provide "syntactic sugar" for scanning across collections which
implement the "newIter()" method. For information on how to create an "Iterable" collection, see the "Iterator.c" file.
These interfaces are inspired by Rust's Collection and Iterable traits.

Using "syntactic sugar" to scan a list,
    FOREACH(ItemType, item, List, list)
        doSomething(*item);
    ENDFOREACH;

This file also defines an abstract Collection type, which is a wrapper around any iterable collection.
Once wrapped, the Collection can be passed around and the users don't need to know the details of what type
of collection is inside.

    // Construct an abstract Collection which wraps the List.
    Collection *collection = NEWCOLLECTION(List, list);

    // Iterate through the abstract Collection just like any other collection.
    FOREACH(ItemType, item, Collection, collection)
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

// Data structure for iterating through an abstract Collection.
// To support inlining, the fields are public, otherwise this is an opaque object.
typedef struct CollectionItr CollectionItr;
typedef struct CollectionItrPub
{
    void *subIterator;                                               // An iterator to the underlying collection.
    void *(*next)(void *this);                                       // subIterator's "next" method
    void (*free)(void *this);                                        // subIterator's "free" method.
} CollectionItrPub;

#define CAMEL_Collection collection

/***********************************************************************************************************************************
Construct an abstract Collection from a concrete collection (eg. List)
Use a macro front end so we can support polymorphic collections.
Note we depend on casting between compatible function pointers where return value and arguments must also be compatible.
    (void *  and struct * are compatible)
***********************************************************************************************************************************/
#define NEWCOLLECTION(SubType, subCollection)                                                                                      \
    collectionNew(                                                                                                                 \
        subCollection,                                               /* The collection we are wrapping */                          \
        (void *(*)(void*))METHOD(SubType, ItrNew),                   /* Get an iterator to the collection  */                      \
        (void *(*)(void*))METHOD(SubType, ItrNext),                  /* Get next item using the iterator  */                       \
        (void (*)(void *))METHOD(SubType, ItrFree)                   /* Free the iterator */                                       \
    )
Collection *collectionNew(void *subCollection, void *(*newItr)(void*), void *(*next)(void*), void (*free)(void*));

// Construct an iterator to scan through the abstract Collection.
CollectionItr *collectionItrNew(Collection *collection);

/***********************************************************************************************************************************
Point to the next item in the Collection, returning NULL if there are no more.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void *
collectionItrNext(CollectionItr *this)
{
    // Invoke next() on the subiterator.
    return THIS_PUB(CollectionItr)->next(THIS_PUB(CollectionItr)->subIterator);
}

/***********************************************************************************************************************************
Destroy the iterator, freeing any resources which were allocated.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS
void collectionItrFree(CollectionItr *this)
{
    objFree(THIS_PUB(CollectionItr)->subIterator);
    objFree(this);
}

// Syntactic sugar to get the function name from the object type and the short method name.
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
Syntactic sugar to make iteration looks more like C++ or Python.
Note foreach and endForeach are block macros, so the overall foreach/endForeach must be terminated with a semicolon.
This version is slower because it catches and cleans up after exceptions.
***********************************************************************************************************************************/
#define FOREACH(ItemType, item, CollectionType, collection)                                                                        \
    do {                                                                                                                           \
        CollectionType##Itr *FOREACH_itr = METHOD(CollectionType, ItrNew)(collection);                                             \
        void (*FOREACH_free)(CollectionType##Itr*) = METHOD(CollectionType, ItrFree);                                              \
        ItemType *item;                                                                                                            \
        TRY_BEGIN()                                                                                                                \
        {                                                                                                                          \
            while ( (item = METHOD(CollectionType,ItrNext)(FOREACH_itr)) != NULL)                                                  \
            {
#define ENDFOREACH                                                                                                                 \
            }                                                                                                                      \
        }                                                                                                                          \
        FINALLY()                                                                                                                  \
        {                                                                                                                          \
            FOREACH_free(FOREACH_itr);                                                                                             \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    } while (0)

/***********************************************************************************************************************************
Syntactic sugar for iteration. This is a proposed (future) version is optimized for the case when 1) no exceptions can occur, or
2) no "break" statements, and 3) the destruct method does not need to be called after an exception
(say dynamic memory is freed automatically).

Not sure if we want to keep this, since most of the simple cases will be Lists and we can use the list specific "foreach()" macro.
***********************************************************************************************************************************/
#define FOREACH_SIMPLE(item, CollectionType, collection)                                                                           \
    for (                                                                                                                          \
        CollectionType##Itr *FOREACH_itr = METHOD(CollectionType, ItrNew)(collection);                                             \
        (item = METHOD(CollectionType,ItrNext)(FOREACH_itr)) != NULL || (METHOD(CollectionType,ItrFree)(_itr), false);             \
        (void)0                                                                                                                    \
    )

/***********************************************************************************************************************************
Macros for function logging.
***********************************************************************************************************************************/

#define FUNCTION_LOG_COLLECTION_TYPE                                                                                               \
    Collection *
#define FUNCTION_LOG_COLLECTION_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, collectionToLog, buffer, bufferSize)
String *collectionToLog(const Collection *this);

#define FUNCTION_LOG_ITERATOR_TYPE                                                                                                 \
    void *
#define FUNCTION_LOG_ITERATOR_FORMAT(value, buffer, bufferSize)                                                                    \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, iteratorToLog, buffer, bufferSize)
String *iteratorToLog(const void *this);

#endif //COMMON_TYPE_COLLECTION_H
