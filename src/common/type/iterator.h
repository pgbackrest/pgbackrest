/***********************************************************************************************************************************
Define the interface for an abstract Iterator and suggest naming conventions for specific iterators.

This interface is inspired by the need to iterate through diverse types of storage directories. It addresses one
part of that problem - a way of iterating through diverse types of containers in general.

= Containers
Containers are iterable sets of items. While containers vary widely in their implementation, as far as wa are concerened,
they have one property in common:
    newItr(container) creates an iterator object for scanning through that particular container.

= Iterators
Containers do not scan across themselves. Iterators do the scanning instead. Once created, an iterator provides two
methods:
    more(iterator) is true if there are more items to scan.
    next(iterator) returns a pointer to the next item to be scanned.

Compared to similar interfaces in Java, a container implements the "Iterable" interface and and iterator implements the
"Iterator" interface. The names are slightly different, but the methods are essentially the same.

Note Iterators and Containers are two separate concepts which are sometimes confused. A container holds a set of items,
while an iterator does a single scan across the items.

= Naming conventions
The method names just mentioned are "short" names. The full names of the methods consist of the
short name prepended to the name of their data type. As an example, iterating through a List object.
  - newListItr(list) creates an iterator object of type "ListItr" for scanning the list.
  - moreListItr(listItr) is true if there are more times to scan in the List
  - nextListItr(listItr) points to the next item in the list.

Normally, pgbackrest appends the method name to the type name, so the next() method would be called listItrNext().
By changing the order - placing the method first rather than last - the C preprocessor is able to generate long method
names by concatenating the short method name with the type name. It is not able to generate long
method names conforming to the usual naming convention.

= Goals
The ultimate task is to create an interface for scanning through directories from different storage managers.
However, this task is just one example of a more general issue - scanning through abstract sets of abjects.
In this file, we address the more general issue.

The goals are as follows:
 - implement an abstract "Container", wrapping an underlying container, which iterates the underlying container.
 - define a uniform set of methods for scanning through various types of containers.
 - maintain performance and memory comparable to scanning through the containers "by hand" (ie not using the interface)
 - create "syntactic sugar" to make it easy to use the new iterators.

 First, here is an example of how the iteration interfaces might be used.

= Example: iterating through a List.

== Without using the iterator interface,
   for (int listIdx=0;  listIdx < lstSize(list); listIdx++) {
       ItemType *item = (ItemType *)lstGet(list, listIdx);
       doSomething(*item)
   }

== Using the newly defined interfaces.
   ListItr itr[1];
   newListItr(itr, list);
   while (moreListItr(itr)) {
       ItemType *item = (ItemType *)nextListItr(itr);
       doSomething(*item);
   }

== Incorporating proposed "syntactic sugar".
    foreach(ItemType, item, List, list)
        doSomething(*item);
    endForeach

== Performance and memory.
All three of the previous examples should have roughly the same performance and memory requirements.
 - iteration variables are "auto" and reside in registers or on the call stack. There is no static or dynamic memory.
 - the iteration methods can be inlined, in which case the C optimizer will generate similar code.

= Abstract Container
A "Container" is a polymorphic wrapper around anything which iterates through items. Like abstract containers in C++,
the container (and its iterator) depend on a jump table pointing to the underlying methods.
Adding a level of indirection has a performance penalty, but the penalty is comparable to any C++ program using abstract
data types.

The following code shows how to construct an abstract Container and iterate through it using "syntactic sugar" macros.

    // Construct an abstract Container which wraps the List.
    Container container = newContainer(List, list);

    // Iterate through the abstract Container just like any other container.
    foreach(ItemType, item, Container, container)
        doSomething(*item)
    endForeach

= Design Compromise
Constructing an iterator for an abstract Container requires allocating memory to hold the underlying iterator.
The Container does not know ahead of time how much memory is needed, so a fully general solution will
require dynamic memory allocation. Dynamic memory introduces a new requirement: the memory must be freed when iteration
is complete. Additionally, since we favor the caller allocating memory, the caller must know how much memory to allocate.

As a compromise to avoid dynamic memory allocation, ContainerItr will include a pre-allocated chunk of
memory. If the underlying iterator doesn't fit, the constructor will throw an assert error.
Note this compromise is not "fixed in stone". In the longer term, it may be necessary to add free() and size() methods
to the interface. The use of syntactic sugar would make it easy to update the iteration code to use these new methods.

The signature of the newItr method was changed to support dynamic memory allocation.  Previously it was
       ListItr itr = newListItr(list);

With the updated signature,
       ListItr *itr =  ...
       newListItr(itr, list);

 Both versions generate equivalent code.

***********************************************************************************************************************************/
#ifndef PGBACKREST_ITERATOR_H
#define PGBACKREST_ITERATOR_H

#include <stdbool.h>
#include "common/assert.h"

// TODO: Temporary. There must be a reason this hasn't been done. Perhaps test mocks need the "attribute()" syntax visible.
//       At any rate, it doesn't belong here and we should move it elsewhere if it turns out to be useful.
#define INLINE __attribute__((always_inline)) static inline



// Define the polymorphic Container wrapper, including the jump table of underlying iterator methods.
typedef struct Container {
    void *subContainer;                                             // Pointer to the underlying container.
    bool (*more)(void *this);                                       // Method to query if there are more items to scan.
    void *(*next)(void *this);                                      // Method to get a pointer to the next item.
    void (*newItr)(void *this, void *container);                    // Method to construct a new iterator "in place".
} Container;


// Data structure for iterating through a Container.
typedef struct ContainerItr {
    Container *container;                                           // The polymorphic container wrapping the underlying container.
    void *subIterator;                                              // An iterator to the underlying container.
    void *preallocatedMemory[126];                                  // Preallocated space to hold the underlyng iterator.
} ContainerItr;                                                     // Total size is 1KB, aligned 8.

/***********************************************************************************************************************************
Create an iterator to scan through the abstract Container.
***********************************************************************************************************************************/
INLINE void
newContainerItr(ContainerItr *this, Container *container)
{
    // Remember the container, mainly to pick up the jump table.
    this->container = container;

    // Use preallocated memory to hold the inner container's iterator.
    //   (newContainer() already verified preallocated is big enough.)
    this->subIterator = (void *)this->preallocatedMemory;

    // Use the allocated memory to construct an inner iterator for the inner container.
    container->newItr(this->subIterator, container->subContainer);
}

// Does the Container have more items?
INLINE bool
moreContainerItr(ContainerItr *this) {return this->container->more(this->subIterator);}

// Point to the next item in the Container.
INLINE void *
nextContainerItr(ContainerItr *this) {return this->container->next(this->subIterator);}


/***********************************************************************************************************************************
Proposed syntactic sugar to make iteration look more like C++ or Python.
Note foreach and endForeach are equivalent to a multi-line "braced" statement.
Thought: replace ContainerType with "typeof(*container)".
***********************************************************************************************************************************/
#define foreach(ItemType, item, ContainerType, container) {                                                                        \
        ContainerType##Itr itr[1];                                                                                                 \
        new##ContainerType##Itr((void*)itr, (void*)container);                                                                     \
        while (more##ContainerType##Itr(itr))                                                                                      \
        {                                                                                                                          \
            ItemType *item = (ItemType*) next##ContainerType##Itr(itr);
#define endForeach                                                                                                                 \
        }                                                                                                                          \
}

/***********************************************************************************************************************************
Proposed syntactic sugar for creating an abstract Container from a regular container.
Does a compile time check to ensure we don't overflow pre-allocated memory in ContainerItr.
Thought: replace "SubType" with "typeof(*subContainer).
***********************************************************************************************************************************/
#define newContainer(SubType, subCntainer) (                                                                                       \
    checkPreallocated(sizeof(SubType##Itr) <= sizeof(((ContainerItr*)0)->preallocatedMemory)),                                     \
    (Container) {                                                                                                                  \
        .subContainer = (void *)subCntainer,                                                                                       \
        .newItr = (void(*)(void*,void*))new##SubType##Itr,                                                                         \
        .more = (bool(*)(void*))more##SubType##Itr,                                                                                \
        .next = (void*(*)(void*))next##SubType##Itr                                                                                \
    }                                                                                                                              \
)

// use static_assert when becomes available in C11. For now, put a wrapper around "CHECK" so it can be used in an expression.
INLINE void checkPreallocated(bool condition) { CHECK(AssertError, condition, "Pre-allocated space in ContainerITR is too small");}

#endif //PGBACKREST_ITERATOR_H
