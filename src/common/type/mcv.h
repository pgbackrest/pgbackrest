/***********************************************************************************************************************************
Calculate Most Common Value

Calculate the most common value in a list of variants.  If there is a tie then the first value passed to mcvUpdate() wins.

mcvResult() can be called multiple times because it does not end processing, but there is a cost to calculating the result each time
since it is not stored.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_MCV_H
#define COMMON_TYPE_MCV_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct MostCommonValue MostCommonValue;

#include "common/type/object.h"
#include "common/type/stringList.h"
#include "common/type/variant.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
MostCommonValue *mcvNew(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Update counts for a value
MostCommonValue *mcvUpdate(MostCommonValue *this, const Variant *value);

// Get most common value
const Variant *mcvResult(const MostCommonValue *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
mcvFree(MostCommonValue *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_MOST_COMMON_VALUE_TYPE                                                                                        \
    MostCommonValue *
#define FUNCTION_LOG_MOST_COMMON_VALUE_FORMAT(value, buffer, bufferSize)                                                           \
    objToLog(value, "MostCommonValue", buffer, bufferSize)

#endif
