/***********************************************************************************************************************************
Storage XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "storage/helper.h"

typedef struct StorageXs
{
    MemContext *memContext;
    const Storage *pxPayload;
} StorageXs, *pgBackRest__LibC__Storage;
