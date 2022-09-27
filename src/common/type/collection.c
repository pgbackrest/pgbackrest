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
Once created, an iterator provides a single method:
    next(iterator) returns a pointer to the next item to be scanned, returning NULL if no more.

Compared to similar traits in Rust, a collection implements the "Iterable" interface and and iterator implements the
"Iterator" trait. The method names are slightly different, but the functionality is essentially the same.

= Naming conventions for specific collections.  (eg "List")
By convention, Pgbackrest uses camelCase method names, where the "short" method name is appended to the full type name.
For example, the "get" method on a List is called "listGet". (actually, it is "lstGet, but let's ignore that for now)
The C preprocessor isn't able to generate camelCase names, so it requires a small "trick" when generating method names.

For each concrete "MyType" which is going to be abstracted, provide the following macro:
    #define CAMEL_MyType myType

Now the C preprocessor can generate full method names in camelCase.
    METHOD(MyType,Get)   -->   myTypeGet

= Goals
The immediate task is to create an interface for scanning through directories from different storage managers.
However, this task is an example of two more general issues - 1) scanning through abstract sets of abjects,
and even more generally 2) Implementing abstract interfaces automatically.

The goals are as follows:
 - define a uniform set of methods for scanning through various types of collections.
   These are "ItrNew()" to create an iterator, and "IterNext()" to advance to the next item.
 - create "syntactic sugar" to make it easy to use the new iterators.
   This are the "FOREACH"/"ENDFOREACH" macros.
 - implement an abstract "Collection", wrapping an underlying collection, which iterates the underlying collection.
   This is the NEWCOLLECTION() macro.
 - maintain performance and memory comparable to scanning through the collections "by hand" (ie not using the interface)

= Example: iterating through a List.
Here are some examples of how to iterate through a List.

== Without the iterator interface,
   for (int listIdx=0;  listIdx < lstSize(list); listIdx++) {
       ItemType *item = (ItemType *)lstGet(list, listIdx);
       doSomething(*item)
   }

== Incorporating proposed "syntactic sugar".
    ItemType *item;
    FOREACH(item, List, list)
        doSomething(*item);
    ENDFOREACH

== Performance and memory.
 - iterators are allocated dynamically, so there is some performance overhead
 - iterators can be inlined, making the per-loop overhead very low.
 - memory allocated inside A FOREACH loop will be freed as the loop progresses.
 - iterators can be configured with callbacks (eg close a file)

= Abstract Collection
An abstract "Collection" is a polymorphic wrapper around anything which iterates through items. Like abstract collections
in C++ or Rust, a Collection (and its iterator) depend on function pointers to the underlying methods.
Adding a level of indirection has a performance penalty, but the penalty is comparable to any C++ program using abstract
data types.

The following code shows how to construct an abstract Collection and iterate through it using "syntactic sugar" macros.

    // Construct an abstract Collection which wraps the List.
    Collection collection[] = NEWCONTAINER(List, list);

    // Iterate through the abstract Collection just like any other collection.
    FOREACH(ItemType, item, Collection, collection)
        doSomething(*item)
    ENDFOREACH

One new goal is to support a "generator" object, where a program loop is transformed into an iterator. This object
will allow scanning through diverse data structures, say XML documents, without creating intermediate Lists.
***********************************************************************************************************************************/
#include "build.auto.h"  // First include in all C files
#include "common/debug.h"
#include "common/assert.h"
#include "common/type/collection.h"

// Abstract collection of items which can be iterated.
// This is an opaque object with no public fields.
struct Collection
{
    void *subCollection;                                            // Pointer to the underlying collection.
    void *(*newItr)(void *collection);                              // Method to construct a new iterator.
    void *(*next)(void *this);                                      // Method to get a pointer to the next item. NULL if no more.
    //int itrSize;                                                  // Size to allocate for the underlying iterator. (not now)
};

// An iterator though an abstract collection.
typedef struct CollectionItr
{
    void *subIterator;                                              // An iterator to the underlying collection.
    void *(*next)(void *this);                                      // subIterator's "next" method
} CollectionItrPub;

/***********************************************************************************************************************************
Create a new abstract Collection from a specific collection (such as List).
 @param subCollection - the specific collection being presented as a Collection.
 @param newItr - the collections function for creating an iterator.
 @param next - the collection iterqtor method for selecting the next item.
 @return - abstract Collection containing the original one.
***********************************************************************************************************************************/
Collection *collectionNew(void *subCollection, void *(*newItr)(void *), void *(*next)(void *))
{
        FUNCTION_TEST_BEGIN();
            FUNCTION_TEST_PARAM_P(VOID, subCollection);
            FUNCTION_TEST_PARAM(FUNCTIONP, newItr);
            FUNCTION_TEST_PARAM(FUNCTIONP, next);
            FUNCTION_TEST_PARAM(FUNCTIONP, free);
        FUNCTION_TEST_END();
        ASSERT(subCollection != NULL);
        ASSERT(newItr != NULL);
        ASSERT(next != NULL);

        Collection *this = NULL;
        OBJ_NEW_BEGIN(Collection)
        {
            // Allocate memory for the Collection object.
            this = OBJ_NEW_ALLOC();

            // Fill in the fields including the jump table pointers and a new iterator to the subCollection.
            *this = (Collection) {
                .subCollection = subCollection,
                .next = next,
                .newItr = newItr,
            };
        }
        OBJ_NEW_END();

        FUNCTION_TEST_RETURN(COLLECTION, this);
}


/***********************************************************************************************************************************
Construct an iterator to scan through the abstract Collection.
***********************************************************************************************************************************/
CollectionItr *
collectionItrNew(Collection *collection)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(COLLECTION, collection);
    FUNCTION_TEST_END();

    // Start by allocating an iterator to the sub-collection.
    void *subIterator = collection->newItr(collection->subCollection);

    CollectionItr *this = NULL;
    OBJ_NEW_BEGIN(CollectionItr, .childQty=MEM_CONTEXT_QTY_MAX)
    {
        // Allocate memory for the Collection object.
        this = OBJ_NEW_ALLOC();

        // Fill in the fields including the jump table pointers and the iterator to the subCollection.
        *this = (CollectionItr) {
            .next = collection->next,
            .subIterator = subIterator,
        };
    }
    OBJ_NEW_END();

    // Be sure to free the subiterator whenever we free this iterator.
    objMove(subIterator, objMemContext(this));

    FUNCTION_TEST_RETURN(COLLECTION_ITR, this);
}

/***********************************************************************************************************************************
Iterate to the next item in the Collection.
  As a future optimization, consider inlining with the -flto compiler option.
***********************************************************************************************************************************/
void *
collectionItrNext(CollectionItr *this)
{
    return this->next(this->subIterator);
}

// Display an abstract Collection. For now, just the address of the sub-collection.
String *collectionToLog(const Collection *this)
{
    return this == NULL ? strDup(NULL_STR) : strNewFmt("Collection{.subCollection=%p}", this->subCollection);  // TODO: save subContainer's logger.
}
