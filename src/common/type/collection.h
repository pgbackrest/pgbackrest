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
#ifndef PGBACKREST_COLLECTION_H
#define PGBACKREST_COLLECTION_H

#include <stdbool.h>
#include "common/assert.h"

// TODO: Temporary. There must be a reason this hasn't been done. Perhaps test mocks need the "attribute()" syntax visible.
//       At any rate, it doesn't belong here and we should move it elsewhere if it turns out to be useful.
#define INLINE __attribute__((always_inline)) static inline

// The basic "Block" macros. Also belong in a different file.
#define BEGIN do {
#define END   } while (0)

// Define the polymorphic Collection wrapper, including the jump table of underlying iterator methods.
typedef struct Collection {
    void *subCollection;                                             // Pointer to the underlying collection.
    void *(*next)(void *this);                                      // Method to get a pointer to the next item. NULL if no more.
    void (*newItr)(void *this, void *collection);                    // Method to construct a new iterator "in place".
    void (*destruct)(void *this);                                   // Method to free resources allocated during "newItr()"
} Collection;

// Data structure for iterating through a Collection.
typedef struct CollectionItr {
    Collection *collection;                                           // The polymorphic collection wrapping the underlying collection.
    void *subIterator;                                              // An iterator to the underlying collection.
    void *preallocatedMemory[126];                                  // Preallocated space to hold the underlyng iterator.
} CollectionItr;                                                     // Total size is 1KB, aligned 8.

/***********************************************************************************************************************************
Construct an iterator to scan through the abstract Collection.
***********************************************************************************************************************************/
INLINE void
newCollectionItr(CollectionItr *this, Collection *collection)
{
    // Remember the collection, mainly to pick up the jump table.
    this->collection = collection;

    // Use preallocated memory to hold the inner collection's iterator.
    //   (newCollection() already verified preallocated is big enough.)
    this->subIterator = (void *)this->preallocatedMemory;

    // Use the allocated memory to construct an inner iterator for the inner collection.
    collection->newItr(this->subIterator, collection->subCollection);
}


/***********************************************************************************************************************************
Point to the next item in the Collection, returning NULL if there are no more.
***********************************************************************************************************************************/
INLINE void *
nextCollectionItr(CollectionItr *this)
{
    return this->collection->next(this->subIterator);
}

/***********************************************************************************************************************************
Destroy the iterator, freeing any resources which were allocated.
***********************************************************************************************************************************/
INLINE void
destructCollectionItr(CollectionItr *this)
{
    this->collection->destruct(this->subIterator);
}


/***********************************************************************************************************************************
Proposed syntactic sugar to make iteration look more like C++ or Python.
Note foreach and endForeach are equivalent to a multi-line block statement.
With a smart optimizer, the invocation of _destructor could be inlined, but otherwise it is an indirect call.
***********************************************************************************************************************************/
#define FOREACH(ItemType, item, CollectionType, collection)                                                                        \
    BEGIN                                                                                                                          \
        CollectionType##Itr _itr[1];                                                                                               \
        new##CollectionType##Itr(_itr, collection);                                                                                \
        void (*_destructor)(CollectionType##Itr*) = destruct##CollectionType##Itr;                                                 \
        ItemType *item;                                                                                                            \
        while ( (item = next##CollectionType##Itr(_itr)) != NULL)                                                                               \
        {
#define ENDFOREACH                                                                                                                 \
        }                                                                                                                          \
        _destructor(_itr);                                                                                                         \
    END

/***********************************************************************************************************************************
Proposed syntactic sugar for creating an abstract Collection from a regular collection.
Checks to ensure we won't overflow pre-allocated memory in CollectionItr.
Unlike "block" macros, this macro is actually an expression returning a pointer to a local (auto) Collection.
***********************************************************************************************************************************/
#define NEWCOLLECTION(SubType, subCntainer) (                                                                                      \
    checkItr(subCntainer != NULL, "Attempting to create NEWCOLLECTION from NULL"),                                                 \
    checkItr(                                                                                                                      \
        sizeof(SubType##Itr) <= sizeof(((CollectionItr*)0)->preallocatedMemory),                                                   \
        "Pre-allocated space in CollectionItr is too small"                                                                        \
    ),                                                                                                                             \
    &(Collection) {                                                                                                                 \
        .subCollection = (void *)subCntainer,                                                                                      \
        .newItr = (void(*)(void*,void*))new##SubType##Itr,                                                                         \
        .next = (void*(*)(void*))next##SubType##Itr,                                                                               \
        .destruct = (void(*)(void*))destruct##SubType##Itr,                                                                        \
    }                                                                                                                              \
)

// Put a wrapper around "CHECK" so it can be used as an expression.
INLINE void
checkItr(bool condition, const char* message) {
    CHECK(AssertError, condition, message);
}

// TODO: INLINE belongs elsewhere.
#undef INLINE


#endif //PGBACKREST_COLLECTION_H
