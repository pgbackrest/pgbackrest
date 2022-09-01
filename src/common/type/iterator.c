/***********************************************************************************************************************************
This file contains more detailed documentation on iterators. It is of interest when creating a new iterator, where
the comments in the header file are geared to using existing iterators.

This interface is inspired by the need to iterate through diverse types of storage directories. It addresses one
part of that problem - a way of iterating through diverse types of containers in general. It takes two steps.
 1) It defines methods which can efficiently scan through specific containers.
 2) It defines an abstract Container which can scan through an underlying container without knowing the details
    of the container.

= Container Properties
Containers are iterable sets of items. While containers vary widely in their implementation, as far as wa are concerened,
they have one property in common:
    newItr(container) creates an iterator object for scanning through that particular container.

= Iterator Properties
Containers do not scan across themselves. Iterators do the scanning instead. Once created, an iterator provides two
methods:
    more(iterator) is true if there are more items to scan.
    next(iterator) returns a pointer to the next item to be scanned.

And for cleaning up:
    destruct(iterator) release any resources (eg. memory) acquired when creating the iterator.

Compared to similar interfaces in Java, a container implements the "Iterable" interface and and iterator implements the
"Iterator" interface. The method names are slightly different, but the functionality is essentially the same.

= Naming conventions for specific containers.  (eg "List")
The method names just mentioned are "short" names. The full names of the methods consist of the
short name prepended to the name of their data type. As an example, iterating through a List object.
  - newListItr(listItr, list) initializes an iterator for scanning the list.
  - moreListItr(listItr) is true if there are more times to scan in the List
  - nextListItr(listItr) points to the next item in the list.
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
 - define a uniform set of methods for scanning through various types of containers.
 - maintain performance and memory comparable to scanning through the containers "by hand" (ie not using the interface)
 - create "syntactic sugar" to make it easy to use the new iterators.
 - implement an abstract "Container", wrapping an underlying container, which iterates the underlying container.

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

= Abstract Container
An abstracr "Container" is a polymorphic wrapper around anything which iterates through items. Like abstract containers
in C++, a Container (and its iterator) depend on a jump table pointing to the underlying methods.
Adding a level of indirection has a performance penalty, but the penalty is comparable to any C++ program using abstract
data types.

The following code shows how to construct an abstract Container and iterate through it using "syntactic sugar" macros.

    // Construct an abstract Container which wraps the List.
    Container container[] = NEWCONTAINER(List, list);

    // Iterate through the abstract Container just like any other container.
    FOREACH(ItemType, item, Container, container)
        doSomething(*item)
    ENDFOREACH

= Design Compromise
Constructing an iterator for an abstract Container requires allocating memory to hold the underlying iterator.
The Container does not know ahead of time how much memory is needed, so a fully general solution will
require dynamic memory allocation. Dynamic memory introduces a new requirement: the memory must be freed when iteration
is complete. Additionally, since we favor the caller allocating memory, the caller must know how much memory to allocate.

As a compromise to avoid dynamic memory allocation, ContainerItr will include a pre-allocated chunk of
memory. If the underlying iterator doesn't fit, the constructor will throw an assert error.

The signature of the newItr method was changed to support dynamic memory allocation.  Previously it was
       ListItr itr = newListItr(list);

With the updated signature,
       ListItr *itr =  ...
       newListItr(itr, list);

Both versions generate equivalent code.

UPDATE. Dynamic memory is required for scanning directories, so a "destruct()" method is being added to the interface.
This method allows iterators to free memory which may have been allocated by the "new()" constructor.
The Container type still allocates its own fixed memory, but that can be easily changed. As an optimization, it could
switch to dynamic memory when the static memory is too small.

Dynamic memory will be managed through the allocItr(this, size) and freeItr(this, ptr) methods.
Currently, they are stubs, but they will be updated to invoke malloc() and free(), and eventually to
integrate with memory contexts.

One new goal is to support a "generator" object, where a program loop is transformed into an iterator. This object
will allow scanning through diverse data structures, say XML documents, without creating intermediate Lists.
***********************************************************************************************************************************/
