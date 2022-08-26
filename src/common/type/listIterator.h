/***********************************************************************************************************************************
Define the interface for an abstract Iterator and suggest naming conventions for specific iterators.

= Iterable Containers
Abstract containers are polymorphic and can represent any type of underlying container.
For example, a Container could represent a List or a sorted heap, and the code iterating through the items
doesn't need to know what the underlying data structure is. The Container doesn't even have to be a container;
it could read items from a file or even generate items "on the fly".

= Iterators
Iterators scan across a container. They are similar to Java Iterators, where each implementation provides
next() and more() methods. (In Java, they are called next() and hasNext().)

An iterator can be scanned once, front to back, and then it is finished.

When creating an iterator for a some type of container, the suggested names consist of adding "Itr" to the container
type being iterated. To ensure flexible use of memory, creating an iterator takes two steps.

   // Create an iterator to scan the list.
   ListItr itr[1];          // Allocate memory for a local iterator.
   ListItrNew(itr, list);   // Initialize the iterator to scan across the list.

    // Here is a plausible way of iterating through a List.
    while (listItrMore(itr))
    {
       ItemType *item = listItrtNext(itr);
       doSomething(*item);
    }

As experimental syntactic sugar, we have macros which resemble C++ or Java iteration.
   foreach(ItemType, item, List, list)
       doSomething(*item);
   endForeach

= Abstract Container
A Container represents any data type which iterates through items.
Containers can consist of Lists, Arrays, and could even include generators which create items "on the fly".
While each type of container has its own unique iterator, the Container type provides a single interface to scan through any
type of container. It doesn't care what the underlying data structure is as long as it can be iterated.

To achieve polymorphism, the Container type includes pointers to the underlying iterator functions.

The following code shows how construct a Container from a List and iterate through it.

    // Define a static (compile time) table pointing to the underlying iteration functions.
    static const ContainerInterface containerIfaceList = { ... };

    // Construct a Container object which wraps a List and invokes the List iterator functions.
    Container container = containerNew(list, containerIfaceList);

    // Now, iterate through the abstract Container just like any other container.
    foreach(ItemType, item, Container, container)
        dosomething(*item)
    endForeach

TODO: resolve memory allocation issues in containerItrNew.
TODO: remove the duplicate titles in this block comment and make more readable.
***********************************************************************************************************************************/
#ifndef PGBACKREST_LISTITERATOR_H
#define PGBACKREST_LISTITERATOR_H

// Question: There must be a reason this hasn't been done. Perhaps test mocks need the "attribute()" syntax visible.
#define INLINE __attribute__("inline only") static inline

// Interface for initializing and accessing an iterator.
typedef struct IteratorInterface {
    bool (*itrMore)(void *this);                    // Function to query if there are more items to scan.
    void *(*itrNext)(void *this);                   // Function to get a pointer to the next item.
    void (*itrNew)(void *this, void *container);    // Function to initialize an iterator "in place" using preallocated memory.
    int itrSize;                                    // Size in bytes of the container's iterator. Used to preallocate memory.
} ContainerInterface;

// Define a polymorphic Container which can wrap any iterable container.
typedef struct Container {
    void *container;             // The underlying container
    IteratorInterface  iface;    // Interface to the container's iterator functions.
} Container;

// Data structure for iterating through a Container.
typedef struct ContainerItr {
    void *container;          // The underlying container. (debug? not needed otherwise)
    void *iterator;           // An iterator to the underlying container.
} ContainerItr;

/***********************************************************************************************************************************
Create an iterator to scan through the abstract Container.
***********************************************************************************************************************************/
INLINE void containerItrNewFunction(ContainerItr* this, Container *container)
{
    // Allocate memory for the underlying iterator.
    // TODO: SERIOUSLY TEMPORARY!  How should we allocate memory for this? Which context?  Do we need to create a destructor as well?
    // TODO: This is where memory contexts come into play. This is a guarenteed memory leak. NOT FOR PRIME TIME!
    // TODO: Even worse, we could have nested iterators, so there is no limit to how much memory would be allocated.
    // TODO:  If we're going to allocate, then we need to free.  Where?
    void *itr = ALLOCATE_MEMORY()

    // Now that we have memory, initialize the underlying iterator.
    container->iface.itrNew(itr, container);

    // Now, initialize the abstract iterator.
    *this = (ContainerItr){.container=container, .iterator=itr};
}

#define containerItrNewMacro(this, container)                                                                                      \
    (ContainerItr){                                                                                                                \
        .container = container,                                                                                                    \
        .iterator = container->iface.iterNew(alloca(container->iface.size), container)                                             \
    }

// Does the Container have more items?
INLINE bool containerItrMore(ContainerItr *this) {return this->container.iface.more(this->itr);}

// Point to the next item in the Container.
INLINE void *containerItrNext(ContainerItr *this) {return this->container.iface.next(this->itr);}

/***********************************************************************************************************************************
Syntactic sugar macro to make scanning with an abstract Iterator look more like C++ or Python.
  Note: "containerPrefix" is the type of the container, with the first letter being lowercase.
  C preprocessor isn't able to make the first letter of the type name lowercase.
***********************************************************************************************************************************/
#define foreach(ItemType, item, containerPrefix, container)                                                                        \
    do {                                                                                                                           \
        ItemType##Itr *itr = containerPrefix#ItrNew(container);                                                                    \
        static ContainerPrefix *mem = 0;                     \
        while (ContainerType##ItrMore(itr))                                                                                        \
        {                                                                                                                          \
            ItemType *item = containerPrefix##ItrNext(itr);
#define endForeach()                                                                                                               \
        }                                                                                                                          \
    } while (0)

/***********************************************************************************************************************************
Create an abstract Container from a specific type of container.
  @param container - the underlying container object
  @param itrNew - pointer to the function for creating an iterator from the container.
  @param itrSize - the size of the iterator object so we can allocate it locally (if desired)
***********************************************************************************************************************************/
INLINE Container containerNew(void *container, void *(*itrNew)(void*), int itrSize)
{
    return (Container){.container=container, .itrNew=itrNew, .itrSize=itrSize};
}

// Concrete iterator for iterating through a list.
typedef struct ListIterator {
    List *list;                      // pointer to the list being scanned.
    int nextIdx;                     // index of the next item to process.
} ListIterator;

/***********************************************************************************************************************************
Given a list, construct an iterator to scan the list.
***********************************************************************************************************************************/
INLINE ListIterator
listItrNew(List *list)
{
    return (ListIterator) {.list = list, .nextIdx = 0};
}

/***********************************************************************************************************************************
Are there more items remaining in the List?
***********************************************************************************************************************************/
INLINE bool
listItrHasNext(ListIterator *this)
{
    return this->nextIdx < lstSize(this->list);
}

/***********************************************************************************************************************************
Advance to the next item in the List and return a pointer to it.
***********************************************************************************************************************************/
INLINE void *
listItrNext(ListIterator *this)
{
    assert(listItrHasNext(this));
    return lstGet(this->list, this->nextIdx++);
}

// Create a jump table with methods to implement the Iterator interface for ListIterator.
static const
IteratorInterface itrInterfaceForListItr = {
        .next = listItrNext,
        .hasNext = listItrHasNext,
};

/***********************************************************************************************************************************
Given a list, construct an abstract Iterator to scan the list.
***********************************************************************************************************************************/
INLINE Iterator newItrFromList(List *list)
{
    // Create a concrete ListIterator.
    ListIterator listItr = lstIteratorNew(list);

    // Package it as an abstract Iterator.
    return castToIterator(listItr, itrInterfaceForListItr);
}

#endif //PGBACKREST_LISTITERATOR_H
