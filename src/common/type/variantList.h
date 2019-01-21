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
Functions
***********************************************************************************************************************************/
VariantList *varLstNew(void);
VariantList *varLstNewStrLst(const StringList *stringList);
VariantList *varLstDup(const VariantList *source);
VariantList *varLstAdd(VariantList *this, Variant *data);
Variant *varLstGet(const VariantList *this, unsigned int listIdx);
VariantList *varLstMove(VariantList *this, MemContext *parentNew);
unsigned int varLstSize(const VariantList *this);
void varLstFree(VariantList *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_VARIANT_LIST_TYPE                                                                                             \
    VariantList *
#define FUNCTION_LOG_VARIANT_LIST_FORMAT(value, buffer, bufferSize)                                                                \
    objToLog(value, "VariantList", buffer, bufferSize)

#define FUNCTION_LOG_CONST_VARIANT_LIST_TYPE                                                                                       \
    const VariantList *
#define FUNCTION_LOG_CONST_VARIANT_LIST_FORMAT(value, buffer, bufferSize)                                                          \
    objToLog(value, "VariantList", buffer, bufferSize)

#endif
