/***********************************************************************************************************************************
Variant List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_VARIANTLIST_H
#define COMMON_TYPE_VARIANTLIST_H

/***********************************************************************************************************************************
Variant list object
***********************************************************************************************************************************/
typedef struct VariantList VariantList;

#include "common/type/stringList.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
// Create empty VariantList
FN_INLINE_ALWAYS VariantList *
varLstNew(void)
{
    return (VariantList *)lstNewP(sizeof(Variant *));
}

// Create VariantList from StringList
FV_EXTERN VariantList *varLstNewStrLst(const StringList *stringList);

// Duplicate a variant list
FV_EXTERN VariantList *varLstDup(const VariantList *source);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// List size
FN_INLINE_ALWAYS unsigned int
varLstSize(const VariantList *const this)
{
    return lstSize((List *)this);
}

// Is the list empty?
FN_INLINE_ALWAYS bool
varLstEmpty(const VariantList *const this)
{
    return varLstSize(this) == 0;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add to list
FN_INLINE_ALWAYS VariantList *
varLstAdd(VariantList *const this, Variant *const data)
{
    lstAdd((List *)this, &data);
    return this;
}

// Get by index
FN_INLINE_ALWAYS Variant *
varLstGet(const VariantList *const this, const unsigned int listIdx)
{
    return *(Variant **)lstGet((List *)this, listIdx);
}

// Move to new parent mem context
FN_INLINE_ALWAYS VariantList *
varLstMove(VariantList *const this, MemContext *const parentNew)
{
    return (VariantList *)lstMove((List *)this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
varLstFree(VariantList *const this)
{
    lstFree((List *)this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_VARIANT_LIST_TYPE                                                                                             \
    VariantList *
#define FUNCTION_LOG_VARIANT_LIST_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "VariantList", buffer, bufferSize)

#endif
