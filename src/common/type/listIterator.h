/***********************************************************************************************************************************
Define the interface for an abstract Iterator and provide concrete iterators for specific containers.

Abstract Iterators are used when the underlying containers are polymorphic.
For example, the code iterating through the items doesn't know if the items come from a List, are being
read from file, or if they were generated "on the fly"

An iterator can be scanned once, front to back, and then it is finished..

Iterators are modelled after the Java Iterator interface, where each implementation provides
next() and hasNext() methods.

Here is the suggested way of using a concrete iterator, in this case a List iterator. The C optimizer should generate efficient code.
    ItrList itr = itrListNew(list);
    while (itrListHasNext(itr))
    {
       MyType *item = itrListNext(itr);
       doSomething(*item);
    }

An alternate way, using an abstract iterator and some sugar macros, is more in the spirit of C++ or Java iterators.
   foreach(MyType *, item, itrListAbstract(list))
       doSomething(*item);
   endForeach
Note these macros are experimental and do not conform to coding standards.

There is a small performance penalty with dynamic dispatch. It probably won't be optimized away.
TODO: Define an "Iterable" interface to simplify the examples even more.
***********************************************************************************************************************************/
#ifndef PGBACKREST_LISTITERATOR_H
#define PGBACKREST_LISTITERATOR_H

// Question: There must be a reason this hasn't been done already. Perhaps the testing mocks need the "attribute()" syntax visible.
#define INLINE __attribute__("inline only") static inline

// Assemble the methods of the Iterator interface into a single table.
typedef struct IteratorInterface {
    bool (*hasNext)(void *this);  // Does the iterator have more items?
    void *(*next)(void *this);     // Advance to the next item and fetch it.
} IteratorInterface;

// Define a polymorphic Iterator which, under the hood, invokes the underlying object's methods.
typedef struct Iterator {
    IteratorInterface interface;    // The methods which implement the Iterator interface.
    void * iterator;                // The underlying object providing iteration.
} Iterator;

// Methods for the polymorphic Iterator.
INLINE bool itrHasNext(IteratorInterface *this)  {return this->hasNext((void*)this);} // Does the iterator have more items?
INLINE void *itrNext(IteratorInterface *this) {return this->next((void*)this);}       // Advance to the next item and fetch pointer.

// Sugar macro to make scanning with an abstract Iterator look more like C++ or Python.
#define foreach(type, localVar, iterator)                                                                                          \
    do {                                                                                                                           \
        Iterator *itr = &(iterator);                                                                                               \
        while (itrHasNext(itr))                                                                                                    \
        {                                                                                                                          \
            type localVar = itr->next();
#define endForeach()                                                                                                               \
        }                                                                                                                          \
    } while (0)


/***********************************************************************************************************************************
Cast a concrete iterator to an abstract Iterator. This procedure is similar to a c++ conversion constructor.
  @param concreteIterator - an object which can be iterated.
  @param IteratorInterface - A jump table of methods implementing the Iterator interface for the object.
  @return Iterator - an abstract object which implements the Iterator interface.
***********************************************************************************************************************************/
INLINE Iterator castToIterator(void *concreteIterator, IteratorInterface *interface)
{
    return (Iterator){.interface=interface, .iterator=concreteIterator};
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
