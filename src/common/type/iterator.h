/***********************************************************************************************************************************
Define the interface for an abstract Iterator and suggest naming conventions for specific iterators.

= Iterable Containers
Containers are iterable sets of items.
For example, a Container could represent a List or a sorted heap, and the code iterating through the items
doesn't need to know what the underlying data structure is. The Container doesn't even have to be a container;
it could read items from a file or even generate items "on the fly".

= Iterators
Iterators scan across a container. They are similar to Java Iterators, where each implementation provides
next() and more() methods. (In Java, they are called next() and hasNext().)

An iterator can be scanned once, front to back, and then it is finished.

= Naming conventions
When creating an iterator for a some type of container, the suggested names consist of adding "Itr" to the container
type being iterated. Methods operating on the iterator have the method name prefixed to the type. All method names are
lowerCamelCase and type names are UpperCamelCase. This arrangement allows macros to construct names based on the ContainerType and method name, without
messing up the CamelCase name of the container or iterator types.

Here is an example of iterating through a List.
   // Create an iterator to scan the list.
   ListItr itr[1] = newListItr(list);

    // While there are more items in the list,
    while (moreListItr(itr))
    {
       ItemType *item = nextListItr(itr);

       // Process the item
       doSomething(*item);
    }

    // Free any internal memory used by the iterator.
    freeListItr(itr)

Note the new and free methods only allocate and free dynamic memory used internally to the iterator. Iterators
which do not use internal dynamic memory are allocated as local variables and do not need to be freed.
(Change names to construct and destruct?)

As experimental syntactic sugar, we have macros which resemble C++ or Java iteration. To scan a List,
   foreach(ItemType, item, List, list)
       doSomething(*item);
   endForeach

= Abstract Container
A Container is a wrapper around any data type which iterates through items.
Containers can consist of Lists, Arrays, and could even include generators which create items "on the fly".
While each type of container has its own unique iterator, the Container type provides a single interface to scan through any
type of container. It doesn't care what the underlying data structure is as long as it can be iterated.

To achieve polymorphism, the Container type includes pointers to the underlying iterator functions.

The following code shows how construct a Container from a List and iterate through it.

    // Define a static (compile time) table pointing to the underlying iteration functions.
    // This table will probably be defined in a .h file.
    static const ContainerInterface containerIfaceList = { ... };

    // Construct a Container object which wraps a List and invokes the List iterator functions.
    Container container = containerNew(list, containerIfaceList);

    // Now, iterate through the abstract Container just like any other container.
    foreach(ItemType, item, Container, container)
        dosomething(*item)
    endForeach

= Design Decision
When allocating an iterator with itrNew(), there are two approaches to managing memory. 1) the caller allocates memory
and itrNew() returns the initialized structure, or 2) itrNew() allocates memory and returns a pointer.
The advantage of 1) is iterators of fixed size reside fully on the stack and the space is reused
automatically. There is no need to "free" the memory.  The downside of 1) is it doesn't work for the polymorphic container.
The polymorphic iterator must allocate memory dynamically during initialization, and the memory must be "freed()"
when no longer needed.

Compromise solution
 - Return a fixed size structure rather than a pointer to allocated memory.
 - For the variable size Container iterator, add a query function sizeItr(subcontainer) do all iterators informing the
   Container iterator how much memory must be allocated.  (Where should memory be allocated from?)
 - Create a "free()" method to free the memory, which will be a noop() for fixed size iterators.

Temporary solution for exploration:
 - For the generic Container, preallocate memory in the ContainerItr structure to hold the sub iterator.
 - assert and die if the preallocated memory is not big enough.

TODO: resolve memory allocation issues in containerItrNew.
TODO: remove the duplicate titles in this block comment and make more readable.
***********************************************************************************************************************************/
#ifndef PGBACKREST_ITERATOR_H
#define PGBACKREST_ITERATOR_H

// TODO: Temporary. There must be a reason this hasn't been done. Perhaps test mocks need the "attribute()" syntax visible.
//       At any rate, it doesn't belong here and we should "undef" it at the end of this file.
#define INLINE __attribute__("inline only") static inline

// Interface for initializing and accessing an iterator.
typedef struct IteratorInterface {
    bool (*more)(void *this);                                       // Function to query if there are more items to scan.
    void *(*next)(void *this);                                      // Function to get a pointer to the next item.
    void (*new)(void *container);                                   // Function to initialize an iterator "in place".
    void (*free)(void *this);                                       // Free any resources when iterator no longer needed.
    size_t (*size)(void *this);                                     // How much memory will the iterator need?
} ContainerInterface;

// Define a polymorphic Container which can wrap any iterable container.
typedef struct Container {
    void *subContainer;                                             // The underlying container
    IteratorInterface  iface;                                       // Interface to the container's iterator functions.
} Container;

// Data structure for iterating through a Container.
typedef struct ContainerItr {
    void *container;                                                // The polymorphic container wrapping the underlying container.
    void *subIterator;                                              // An iterator to the underlying container.
    void *allocatedMemory[1024];                                    // TODO: TEMPORARY: preallocated space to hold the underlyng iterator.
} ContainerItr;

/***********************************************************************************************************************************
Create an iterator to scan through the abstract Container.
***********************************************************************************************************************************/
INLINE ContainerItr newContainerItr(Container *container)
{
    // Allocate memory for the sub-iterator, after requesting its size.
    // TODO: SERIOUSLY TEMPORARY!  How should we allocate memory for this? Which context?  We need to create a destructor as well?
    // TODO:    This hack avoids dynamic allocation and "free", up to the point the sub iterator is bigger than allocatedMemory.
    size_t subSize = container->iface.iterSize(container->subContainer);
    void *subIterator = container->allocatedMemory;
    assert(subSize <= sizeof(container->allocatedMemory));

    // Now that we have memory, initialize the underlying iterator.
    *subIterator = container->iface.itrNew(container);

    // Finally, initialize ourselves, the abstract iterator.
    return (ContainerItr){
            .container=container,
            .subIterator=subIterator;
    };
}

// Does the Container have more items?
INLINE bool moreContainerItr(ContainerItr *this) {return this->container.iface.more(this->itr);}

// Point to the next item in the Container.
INLINE void *nextContainerItr(ContainerItr *this) {return this->container.iface.next(this->itr);}

INLINE void *sizeContainerItr() {return sizeof(ContainerItr)-sizeof(ContainerItr.allocatedMemory) +

/***********************************************************************************************************************************
Experimental syntactic sugar macro to make scanning with an iterator look more like C++ or Python.
Note: "containerType" is "ContainerType", with the first letter being lowercase.
C preprocessor isn't able to make the first letter of the type name lowercase, so these macros aren't going to work as is.
Need a different naming convention, say next##ContainerType or ContainerType##_next.
***********************************************************************************************************************************/
#define foreach(ItemType, item, container, ContainerType)                                                                          \
    do {                                                                                                                           \
        ContainerType##Itr itr[1] = containerType##ItrNew(container);                                                              \
        while (containerType##ItrMore(itr))                                                                                        \
        {                                                                                                                          \
            ItemType *item = containerType##Next(itr);
#define endForeach()                                                                                                               \
        }                                                                                                                          \
        containerType##ItrFree(itr);                                                                                             \
    } while (0)

#endif //PGBACKREST_ITERATOR_H
