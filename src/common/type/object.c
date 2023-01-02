/***********************************************************************************************************************************
Object Helper Macros and Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/type/object.h"

/**********************************************************************************************************************************/
FN_EXTERN void *
objMove(THIS_VOID, MemContext *parentNew)
{
    if (thisVoid != NULL)
        memContextMove(memContextFromAllocExtra(thisVoid), parentNew);

    return thisVoid;
}

/**********************************************************************************************************************************/
FN_EXTERN void *
objMoveContext(THIS_VOID, MemContext *parentNew)
{
    if (thisVoid != NULL)
        memContextMove(*(MemContext **)thisVoid, parentNew);

    return thisVoid;
}

/**********************************************************************************************************************************/
FN_EXTERN void
objFree(THIS_VOID)
{
    if (thisVoid != NULL)
        memContextFree(memContextFromAllocExtra(thisVoid));
}

/**********************************************************************************************************************************/
FN_EXTERN void
objFreeContext(THIS_VOID)
{
    if (thisVoid != NULL)
        memContextFree(*(MemContext **)thisVoid);
}
