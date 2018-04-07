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
VariantList *varLstNew();
VariantList *varLstNewStrLst(const StringList *stringList);
VariantList *varLstDup(const VariantList *source);
VariantList *varLstAdd(VariantList *this, Variant *data);
Variant *varLstGet(const VariantList *this, unsigned int listIdx);
unsigned int varLstSize(const VariantList *this);
void varLstFree(VariantList *this);

#endif
