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
__attribute__((always_inline)) static inline VariantList *
varLstNew(void)
{
    return (VariantList *)lstNewP(sizeof(Variant *));
}

// Create VariantList from StringList
VariantList *varLstNewStrLst(const StringList *stringList);

// Duplicate a variant list
VariantList *varLstDup(const VariantList *source);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
// List size
__attribute__((always_inline)) static inline unsigned int
varLstSize(const VariantList *const this)
{
    return lstSize((List *)this);
}

// Is the list empty?
__attribute__((always_inline)) static inline bool
varLstEmpty(const VariantList *const this)
{
    return varLstSize(this) == 0;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add to list
__attribute__((always_inline)) static inline VariantList *
varLstAdd(VariantList *const this, Variant *const data)
{
    lstAdd((List *)this, &data);
    return this;
}

// Get by index
__attribute__((always_inline)) static inline Variant *
varLstGet(const VariantList *const this, const unsigned int listIdx)
{
    return *(Variant **)lstGet((List *)this, listIdx);
}

// Move to new parent mem context
__attribute__((always_inline)) static inline VariantList *
varLstMove(VariantList *const this, MemContext *const parentNew)
{
    return (VariantList *)lstMove((List *)this, parentNew);
}

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
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
