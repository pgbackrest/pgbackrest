/***********************************************************************************************************************************
Object Helper Macros and Functions
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/type/object.h"

/**********************************************************************************************************************************/
void *
objMove(THIS_VOID, MemContext *parentNew)
{
    if (thisVoid != NULL)
        memContextMove(memContextFromAllocExtra(thisVoid), parentNew);

    return thisVoid;
}

/**********************************************************************************************************************************/
void *
objMoveContext(THIS_VOID, MemContext *parentNew)
{
    if (thisVoid != NULL)
        memContextMove(*(MemContext **)thisVoid, parentNew);

    return thisVoid;
}

/**********************************************************************************************************************************/
void
objFree(THIS_VOID)
{
    if (thisVoid != NULL)
        memContextFree(memContextFromAllocExtra(thisVoid));
}

/**********************************************************************************************************************************/
void
objFreeContext(THIS_VOID)
{
    if (thisVoid != NULL)
        memContextFree(*(MemContext **)thisVoid);
}
