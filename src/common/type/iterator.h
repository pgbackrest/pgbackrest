/***********************************************************************************************************************************
Define the interface for an abstract Iterator and provide "syntactic sugar" for scanning across containers which
implement the "Iterable" interface. For information on how to create an "Iterable" container, see the "Iterator.c" file.

For example, to scan a list,
    FOREACH(ItemType, item, List, list)
        doSomething(*item);
    ENDFOREACH;

This file also defines an abstract Container type, which is a wrapper around any other Iterable container.
Once wrapped, the Container can be passed around and the users don't need to know the details of what type
of container is inside.

    // Construct an abstract Container which wraps the List.
    Container container[] = {NEWCONTAINER(List, list)};

    // Iterate through the abstract Container just like any other container.
    FOREACH(ItemType, item, Container, container)
        doSomething(*item)
    ENDFOREACH;

***********************************************************************************************************************************/
#ifndef PGBACKREST_ITERATOR_H
#define PGBACKREST_ITERATOR_H

#include <stdbool.h>
#include "common/assert.h"

// TODO: Temporary. There must be a reason this hasn't been done. Perhaps test mocks need the "attribute()" syntax visible.
//       At any rate, it doesn't belong here and we should move it elsewhere if it turns out to be useful.
#define INLINE __attribute__((always_inline)) static inline

// The basic "Block" macro. Also belongs in a different file.
#define BEGIN do {
#define END   } while (0)

// Define the polymorphic Container wrapper, including the jump table of underlying iterator methods.
typedef struct Container {
    void *subContainer;                                             // Pointer to the underlying container.
    bool (*more)(void *this);                                       // Method to query if there are more items to scan.
    void *(*next)(void *this);                                      // Method to get a pointer to the next item.
    void (*newItr)(void *this, void *container);                    // Method to construct a new iterator "in place".
    void (*destruct)(void *this);                                   // Method to free resources allocated during "newItr()"
} Container;

// Data structure for iterating through a Container.
typedef struct ContainerItr {
    Container *container;                                           // The polymorphic container wrapping the underlying container.
    void *subIterator;                                              // An iterator to the underlying container.
    void *preallocatedMemory[126];                                  // Preallocated space to hold the underlyng iterator.
} ContainerItr;                                                     // Total size is 1KB, aligned 8.

/***********************************************************************************************************************************
Construct an iterator to scan through the abstract Container.
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

/***********************************************************************************************************************************
Does the Container have more items?
***********************************************************************************************************************************/
INLINE bool
moreContainerItr(ContainerItr *this)
{
    return this->container->more(this->subIterator);
}

/***********************************************************************************************************************************
Point to the next item in the Container.
***********************************************************************************************************************************/
INLINE void *
nextContainerItr(ContainerItr *this)
{
    return this->container->next(this->subIterator);
}

/***********************************************************************************************************************************
Destroy the iterator, freeing any resources which were allocated.
***********************************************************************************************************************************/
INLINE void
destructContainerItr(ContainerItr *this)
{
    this->container->destruct(this->subIterator);
}


/***********************************************************************************************************************************
Proposed syntactic sugar to make iteration look more like C++ or Python.
Note foreach and endForeach are equivalent to a multi-line block statement.
With a smart optimizer, the invocation of _destructor could be inlined, but otherwise it is an indirect call.
***********************************************************************************************************************************/
#define FOREACH(ItemType, item, ContainerType, container)                                                                           \
    BEGIN                                                                                                                           \
        ContainerType##Itr _itr[1];                                                                                                 \
        new##ContainerType##Itr(_itr, container);                                                                                   \
        void (*_destructor)(ContainerType##Itr*) = destruct##ContainerType##Itr;                                                    \
        while (more##ContainerType##Itr(_itr))                                                                                      \
        {                                                                                                                           \
            ItemType *item = (ItemType*) next##ContainerType##Itr(_itr);
#define ENDFOREACH                                                                                                                  \
        }                                                                                                                           \
        _destructor(_itr);                                                                                                          \
    END

/***********************************************************************************************************************************
Proposed syntactic sugar for creating an abstract Container from a regular container.
Checks to ensure we won't overflow pre-allocated memory in ContainerItr.
Unlike "block" macros, this macro is actually an expression used to initialize a Container.
***********************************************************************************************************************************/
#define NEWCONTAINER(SubType, subCntainer) (                                                                                       \
    checkItr(subCntainer != NULL, "Attempting to create NEWCONTAINER from NULL"),                                                  \
    checkItr(                                                                                                                      \
        sizeof(SubType##Itr) <= sizeof(((ContainerItr*)0)->preallocatedMemory),                                                    \
        "Pre-allocated space in ContainerItr is too small"                                                                         \
    ),                                                                                                                             \
    (Container) {                                                                                                                  \
        .subContainer = (void *)subCntainer,                                                                                       \
        .newItr = (void(*)(void*,void*))new##SubType##Itr,                                                                         \
        .more = (bool(*)(void*))more##SubType##Itr,                                                                                \
        .next = (void*(*)(void*))next##SubType##Itr,                                                                               \
        .destruct = (void(*)(void*))destruct##SubType##Itr,                                                                        \
    }                                                                                                                              \
)

// Put a wrapper around "CHECK" so it can be used as an expression.
INLINE void
checkItr(bool condition, const char* message) {
    CHECK(AssertError, condition, message);
}

// To be implemented later.
INLINE void *
allocItr(void *this, size_t size)
{
    (void)this;
    (void)size;
    checkItr(false, "Dyanamic memory allocation for iterators not implemented");
    return NULL;
}

// To be implemented later.
INLINE void
freeItr(void *this, void *ptr)
{
    (void)this;
    (void)ptr;
    checkItr(false, "Dyanamic memory allocation for iterators not implemented");
}

// TODO: INLINE belongs elsewhere.
#undef INLINE


#endif //PGBACKREST_ITERATOR_H
