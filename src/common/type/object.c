/***********************************************************************************************************************************
Object Helper Macros and Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/type/object.h"

/**********************************************************************************************************************************/
FN_EXTERN void *
objMove(THIS_VOID, MemContext *const parentNew)
{
    if (thisVoid != NULL)
        memContextMove(memContextFromAllocExtra(thisVoid), parentNew);

    return thisVoid;
}

/**********************************************************************************************************************************/
FN_EXTERN void *
objMoveToInterface(THIS_VOID, void *const interfaceVoid, const MemContext *const current)
{
    return objMemContext(thisVoid) != current ? objMove(thisVoid, objMemContext(interfaceVoid)) : thisVoid;
}

/**********************************************************************************************************************************/
FN_EXTERN void
objFree(THIS_VOID)
{
    if (thisVoid != NULL)
        memContextFree(memContextFromAllocExtra(thisVoid));
}
