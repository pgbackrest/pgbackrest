/***********************************************************************************************************************************
Storage Read XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "storage/helper.h"

typedef struct StorageReadXs
{
    MemContext *memContext;
    const Storage *storage;
    const StorageRead *read;
} StorageReadXs, *pgBackRest__LibC__StorageRead;
