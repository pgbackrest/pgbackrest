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

// Define the polymorphic Collection wrapper, including the jump table of underlying iterator methods.
// Since we are inlining the methods, there are no private fields.
typedef struct Collection Collection;
typedef struct CollectionPub {
    void *subCollection;                                            // Pointer to the underlying collection.
    void *(*next)(void *this);                                      // Method to get a pointer to the next item. NULL if no more.
    void (*newItr)(void *this, void *collection);                   // Method to construct a new iterator "in place".
    void (*free)(void *this);                                       // Method to free resources allocated during "newItr()"
    int itrSize;                                                    // Size to allocate for the underlying iterator.
} CollectionPub;

// Data structure for iterating through a Collection.
// Again, to support inlining, there are no private fields.
typedef struct CollectionItr CollectionItr;
typedef struct CollectionItrPub
{
    Collection *collection;                                         // The abstract Collection object.
    void *subIterator;                                              // An iterator to the underlying collection.
} CollectionItrPub;


/***********************************************************************************************************************************
Construct an abstract Collection from a concrete collection (eg. List)
Use a macro front end so we can support polymorphic collections.
***********************************************************************************************************************************/
#define NEWCOLLECTION(SubType, subCollection)                                                                                      \
    collectionNew(subCollection, newItr##SubType, next##SubType, free##SubType)
Collection *collectionNew(void *subCollection, void *(*newItr)(void*), void *(*next)(void*), void (*free)(void*));

// Construct an iterator to scan through the abstract Collection.
Collection *collectionItrNew(Collection *collection);

/***********************************************************************************************************************************
Point to the next item in the Collection, returning NULL if there are no more.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void *
collectionItrNext(CollectionItr *this)
{
    // Point to the underlying collection's iterator.
    void *subIterator = THIS_PUB(CollectionItr)->subIterator;

    // Point to the "next" method of the iterator.
    void *(*next)(void *) = THIS_PUB(CollectionItr)->collection->pub.next;

    // Get the next item from the underlying collection, if any.
    return next(subIterator);
}

/***********************************************************************************************************************************
Destroy the iterator, freeing any resources which were allocated.
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
collectionItrFree(CollectionItr *this)
{
    objFree(THIS_PUB(CollectionItr)->subIterator);
    objFree(this);
}

// Macros to enable a Collection object as an abstract collection
#define newCollectionItr(collection) collectionItrNew(collection)
#define nextCollectionItr(itr) collectionItrNext(itr)
#define freeCollectionItr(itr) collectionItrFree(itr)

/***********************************************************************************************************************************
Syntactic sugar to make iteration looks more like C++ or Python.
Note foreach and endForeach are block macros, so the overall foreach/endForeach must be terminated with a semicolon.
This version is a big slower because it catches and cleans up after exceptions.
***********************************************************************************************************************************/
#define FOREACH(ItemType, item, CollectionType, collection)                                                                        \
    do {                                                                                                                        \
        CollectionType##Itr *_itr = new##CollectionType##Itr(_itr, collection);                                                    \
        void (*_free)(CollectionType##Itr*) = free##CollectionType##Itr;                                                 \
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
            _free(_itr);                                                                                                     \
        }                                                                                                                          \
        TRY_END();                                                                                                                 \
    while (0)



/***********************************************************************************************************************************
Syntactic sugar for iteration. This is a proposed (future) version is optimized for the case when 1) no exceptions can occur or
2) the destruct method does not need to be called after an exception (say dynamic memory is freed automatically).
***********************************************************************************************************************************/
#define FOREACH_SIMPLE(item, CollectionType, collection)                                                                           \
    for (                                                                                                                          \
        CollectionType##Itr *_itr = new##CollectionType##Itr(_itr, collection);                                                    \
        (item = next##CollectionType##Itr(_itr)) != NULL) || (free##CollectionType##Itr(_itr), false))                             \
        (void)0                                                                                                                    \
    )


#endif //PGBACKREST_COLLECTION_H
