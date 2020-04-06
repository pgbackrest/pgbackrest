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
VariantList *varLstNew(void);

// Create VariantList from StringList
VariantList *varLstNewStrLst(const StringList *stringList);

// Duplicate a variant list
VariantList *varLstDup(const VariantList *source);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Add to list
VariantList *varLstAdd(VariantList *this, Variant *data);

// Get by index
Variant *varLstGet(const VariantList *this, unsigned int listIdx);

// Move to new parent mem context
VariantList *varLstMove(VariantList *this, MemContext *parentNew);

// List size
unsigned int varLstSize(const VariantList *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void varLstFree(VariantList *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_VARIANT_LIST_TYPE                                                                                             \
    VariantList *
#define FUNCTION_LOG_VARIANT_LIST_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "VariantList", buffer, bufferSize)

#endif
