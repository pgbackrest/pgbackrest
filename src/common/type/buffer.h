/***********************************************************************************************************************************
Buffer Handler
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_BUFFER_H
#define COMMON_TYPE_BUFFER_H

/***********************************************************************************************************************************
Buffer object
***********************************************************************************************************************************/
typedef struct Buffer Buffer;

#include "common/memContext.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *bufNew(size_t size);
Buffer *bufNewStr(const String *string);
Buffer *bufCat(Buffer *this, const Buffer *cat);
bool bufEq(const Buffer *this, const Buffer *compare);
Buffer *bufMove(Buffer *this, MemContext *parentNew);
Buffer *bufResize(Buffer *this, size_t size);
size_t bufSize(const Buffer *this);
unsigned char *bufPtr(const Buffer *this);
void bufFree(Buffer *this);

#endif
