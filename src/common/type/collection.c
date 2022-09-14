/***********************************************************************************************************************************
This file contains more detailed documentation on iterators. It is of interest when creating a new iterator, where
the comments in the header file are geared to using existing iterators.

This interface is inspired by the need to iterate through diverse types of storage directories. It addresses one
part of that problem - a way of iterating through diverse types of collections in general. It takes two steps.
 1) It defines methods which can efficiently scan through specific collections.
 2) It defines an abstract Collection which can scan through an underlying collection without knowing the details
    of the collection.

= Collection Properties
Collections are iterable sets of items. While collections vary widely in their implementation, as far as wa are concerened,
they have one property in common:
    newItr(collection) creates an iterator object for scanning through that particular collection.

= Iterator Properties
Once created, an iterator provides two methods:
    next(iterator) returns a pointer to the next item to be scanned, returning NULL if no more.
    destruct(iterator) clean up and release any resources (eg.dynamkc memory) ownded by the iterator.

Compared to similar traits in Rust, a collection implements the "Iterable" interface and and iterator implements the
"Iterator" trait. The method names are slightly different, but the functionality is essentially the same.

= Naming conventions for specific collections.  (eg "List")
The method names just mentioned are "short" names. The full names of the methods consist of the
short name prepended to the name of their data type. As an example, iterating through a List object.
  - newListItr(listItr, list) initializes an iterator for scanning the list.
  - nextListItr(listItr) points to the next item in the list, returning NULL if none.
  - destructListItr(listItr) deconstructs the iterator

Normally, pgbackrest appends the method name to the type name, so the next() method would be called listItrNext().
By changing the order - placing the method first rather than last - the C preprocessor is able to generate long method
names by concatenating the short method name with the type name. It is not able to generate long
method names with the usual conventions.

= Goals
The desired task is to create an interface for scanning through directories from different storage managers.
However, this task is just one example of a more general issue - scanning through abstract sets of abjects.
In this file, we address the more general issue.

The goals are as follows:
 - define a uniform set of methods for scanning through various types of collections.
 - maintain performance and memory comparable to scanning through the collections "by hand" (ie not using the interface)
 - create "syntactic sugar" to make it easy to use the new iterators.
 - implement an abstract "Collection", wrapping an underlying collection, which iterates the underlying collection.

= Example: iterating through a List.
Here are some examples of how to iterate through a List.

== Without the iterator interface,
   for (int listIdx=0;  listIdx < lstSize(list); listIdx++) {
       ItemType *item = (ItemType *)lstGet(list, listIdx);
       doSomething(*item)
   }

== Using the newly defined interface.
This is admittedly awkward, especially for scanning a List.
   ListItr itr[1];
   newListItr(itr, list);
   while (moreListItr(itr)) {
       ItemType *item = (ItemType *)nextListItr(itr);
       doSomething(*item);
   }
   destructListItr(itr);

== Incorporating proposed "syntactic sugar".
    FOREACH(ItemType, item, List, list)
        doSomething(*item);
    ENDFOREACH

== Performance and memory.
All three of the previous examples should have roughly the same performance and memory requirements.
 - iteration variables are "auto" and reside in registers or on the call stack. There is no dynamic memory.
 - the iteration methods can be inlined, in which case the C optimizer will generate similar code.

= Abstract Collection
An abstracr "Collection" is a polymorphic wrapper around anything which iterates through items. Like abstract collections
in C++, a Collection (and its iterator) depend on a jump table pointing to the underlying methods.
Adding a level of indirection has a performance penalty, but the penalty is comparable to any C++ program using abstract
data types.

The following code shows how to construct an abstract Collection and iterate through it using "syntactic sugar" macros.

    // Construct an abstract Collection which wraps the List.
    Collection collection[] = NEWCONTAINER(List, list);

    // Iterate through the abstract Collection just like any other collection.
    FOREACH(ItemType, item, Collection, collection)
        doSomething(*item)
    ENDFOREACH

= Design Compromise
Constructing an iterator for an abstract Collection requires allocating memory to hold the underlying iterator.
The Collection does not know ahead of time how much memory is needed, so a fully general solution will
require dynamic memory allocation. Dynamic memory introduces a new requirement: the memory must be freed when iteration
is complete. Additionally, since we favor the caller allocating memory, the caller must know how much memory to allocate.

As a compromise to avoid dynamic memory allocation, CollectionItr will include a pre-allocated chunk of
memory. If the underlying iterator doesn't fit, the constructor will throw an assert error.

The signature of the newItr method was changed to support dynamic memory allocation.  Previously it was
       ListItr itr = newListItr(list);

With the updated signature,
       ListItr *itr =  ...
       newListItr(itr, list);

Both versions generate equivalent code.

UPDATE. Dynamic memory is required for scanning directories, so a "destruct()" method is being added to the interface.
This method allows iterators to free memory which may have been allocated by the "new()" constructor.
The Collection type still allocates its own fixed memory, but that can be easily changed. As an optimization, it could
switch to dynamic memory when the static memory is too small.

Dynamic memory will be managed through the allocItr(this, size) and freeItr(this, ptr) methods.
Currently, they are stubs, but they will be updated to invoke malloc() and free(), and eventually to
integrate with memory contexts.

One new goal is to support a "generator" object, where a program loop is transformed into an iterator. This object
will allow scanning through diverse data structures, say XML documents, without creating intermediate Lists.
***********************************************************************************************************************************/

#include "common/type/collection.h"

// A Private iterator where all fields are public.
struct CollectionItr
{
    CollectionItrPub pub;
};

/***********************************************************************************************************************************
Construct an iterator to scan through the abstract Collection.
***********************************************************************************************************************************/
Collection *
collectionItrNew(Collection *collection, void *(*newItr)(void *), void *(*next)(void *), void (*free)(void *))
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(POINTER, collection);
    FUNCTION_TEST_END();

    CollectionItr *this = NULL;
    OBJ_NEW_BEGIN(CollectionItr)
    {
        // Allocate memory for the Collection object.
        this = OBJ_NEW_ALLOC();

        // Fill in the fields including the jump table pointers and a new iterator to the subCollection.
        *this = (CollectionItr) {
            .next = = next,
            .free = free,
            .collection = collection,
            .subIterator = newItr(collection->subCollection);
        }
    }
    OBJ_NEW_END();

    FUNCTION_TEST_RETURN(POINTER, this);    // TODO: Create display types.
}

void
collectionItrFree(CollectionItr *this)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(POINTER, this);
    FUNCTION_TEST_END();

    // Free the underlying iterator, then free ourselves.
    objFree(THIS_PUB(CollectionItr)->subIterator);
    objFree(this);

    FUNCTION_TEST_RETURN_VOID();
}
