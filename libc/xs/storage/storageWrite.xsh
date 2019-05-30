/***********************************************************************************************************************************
Storage Write XS Header
***********************************************************************************************************************************/
#include "common/assert.h"
#include "common/memContext.h"
#include "common/type/convert.h"
#include "common/type/json.h"
#include "storage/helper.h"

typedef struct StorageWriteXs
{
    MemContext *memContext;
    const Storage *storage;
    const StorageWrite *write;
} StorageWriteXs, *pgBackRest__LibC__StorageWrite;
