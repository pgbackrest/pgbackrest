/***********************************************************************************************************************************
Variant List Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_VARIANT_LIST_H
#define COMMON_TYPE_VARIANT_LIST_H

/***********************************************************************************************************************************
Variant list type
***********************************************************************************************************************************/
typedef struct VariantList VariantList;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
#include "common/type/variant.h"

VariantList *varLstNew();
VariantList *varLstNewStrLst(const StringList *stringList);
VariantList *varLstDup(const VariantList *source);
VariantList *varLstAdd(VariantList *this, Variant *data);
Variant *varLstGet(const VariantList *this, unsigned int listIdx);
unsigned int varLstSize(const VariantList *this);
void varLstFree(VariantList *this);

#endif
