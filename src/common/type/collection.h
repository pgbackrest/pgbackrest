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

    // And remember to release it when done.
    objFree(collection);

***********************************************************************************************************************************/
#ifndef COMMON_TYPE_COLLECTION_H
#define COMMON_TYPE_COLLECTION_H

#include <stdbool.h>
#include "common/assert.h"  // Fails to compile by itself.
#include "common/type/object.h"

// A Polymorphic interface for Iterable collections.
typedef struct Collection Collection;

// Data structure for iterating through an abstract Collection.
// To support inlining, the fields are public.
typedef struct CollectionItr CollectionItr;
typedef struct CollectionItrPub
{
    void *subIterator;                                               // An iterator to the underlying collection.
    void *(*next)(void *this);                                       // subIterator's "next" method
    void (*free)(void *this);                                        // subIterator's "free" method.
} CollectionItrPub;


/***********************************************************************************************************************************
Construct an abstract Collection from a concrete collection (eg. List)
Use a macro front end so we can support polymorphic collections.
Note we depend on casting between compatible function pointers where return value and arguments must also be compatible.
    (void *  and struct * are compatible)
***********************************************************************************************************************************/
#define NEWCOLLECTION(SubType, subCollection)                                                                                      \
    collectionNew(                                                                                                                 \
        subCollection,                                              /* The collection we are wrapping */                           \
        (void *(*)(void*))new##SubType##Itr,                        /* Get an iterator to the collection  */                       \
        (void *(*)(void*))next##SubType##Itr,                       /* Get next item using the iterator  */                        \
        (void (*)(void *))free##SubType##Itr                        /* Free the iterator */                                        \
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

// Macros to enable a Collection object as an abstract collection
#define newCollectionItr collectionItrNew
#define nextCollectionItr collectionItrNext
#define freeCollectionItr collectionItrFree

/***********************************************************************************************************************************
Syntactic sugar to make iteration looks more like C++ or Python.
Note foreach and endForeach are block macros, so the overall foreach/endForeach must be terminated with a semicolon.
This version is a big slower because it catches and cleans up after exceptions.
***********************************************************************************************************************************/
#define FOREACH(ItemType, item, CollectionType, collection)                                                                        \
    do {                                                                                                                           \
        CollectionType##Itr *_itr = new##CollectionType##Itr(collection);                                                          \
        void (*_free)(CollectionType##Itr*) = free##CollectionType##Itr;                                                           \
        ItemType *item;                                                                                                            \
        TRY_BEGIN()                                                                                                                \
        {                                                                                                                          \
            while ( (item = next##CollectionType##Itr(_itr)) != NULL)                                                              \
            {
#define ENDFOREACH                                                                                                                 \
            }                                                                                                                      \
        }                                                                                                                          \
        FINALLY()                                                                                                                  \
        {                                                                                                                          \
            _free(_itr);                                                                                                           \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    } while (0)



/***********************************************************************************************************************************
Syntactic sugar for iteration. This is a proposed (future) version is optimized for the case when 1) no exceptions can occur or
2) the destruct method does not need to be called after an exception (say dynamic memory is freed automatically).
***********************************************************************************************************************************/
#define FOREACH_SIMPLE(item, CollectionType, collection)                                                                           \
    for (                                                                                                                          \
        CollectionType##Itr *_itr = new##CollectionType##Itr(collection);                                                          \
        (item = next##CollectionType##Itr(_itr)) != NULL) || (free##CollectionType##Itr(_itr), false))                             \
        (void)0                                                                                                                    \
    )

#define foreach(item, list) \
    for (                                                                                                                          \
        int foreach_idx = 0;                                                                                                                 \
        (item = foreach_idx<lstSize(list)?lstGet(list, foreach_idx):NULL);                                                                           \
        foreach_idx++;                                                                                                                       \
    )
#endif //COMMON_TYPE_COLLECTION_H
