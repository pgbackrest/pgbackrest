/***********************************************************************************************************************************
Calculate Most Common Value
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_MCV_H
#define COMMON_TYPE_MCV_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define MOST_COMMON_VALUE_TYPE                                      MostCommonValue
#define MOST_COMMON_VALUE_PREFIX                                    MostCommonValue

typedef struct MostCommonValue MostCommonValue;

#include "common/type/stringList.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
MostCommonValue *mcvNew(void);
MostCommonValue *mcvUpdate(MostCommonValue *this, Variant *value);
const Variant *mcvResult(MostCommonValue *this);
void mcvFree(MostCommonValue *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MOST_COMMON_VALUE_TYPE                                                                                        \
    MostCommonValue *
#define FUNCTION_LOG_MOST_COMMON_VALUE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "MostCommonValue", buffer, bufferSize)

#endif
