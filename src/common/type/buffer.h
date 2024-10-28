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
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Buffer *bufNew(size_t size);

// Create a new buffer from a C buffer
FN_EXTERN Buffer *bufNewC(const void *buffer, size_t size);

// Create a new buffer from a string encoded with the specified type
FN_EXTERN Buffer *bufNewDecode(EncodingType type, const String *string);

// Duplicate a buffer
FN_EXTERN Buffer *bufDup(const Buffer *buffer);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct BufferPub
{
    size_t sizeAlloc;                                               // Allocated size of the buffer
    size_t size;                                                    // Reported size of the buffer
    bool sizeLimit;                                                 // Is the size limited to make the buffer appear smaller?
    size_t used;                                                    // Amount of buffer used
    unsigned char *buffer;                                          // Buffer
} BufferPub;

// Amount of the buffer actually used. This will be updated automatically when possible but if the buffer is modified by using
// bufPtr() then the user is responsible for updating used.
FN_INLINE_ALWAYS size_t
bufUsed(const Buffer *const this)
{
    return THIS_PUB(Buffer)->used;
}

// Is the buffer empty?
FN_INLINE_ALWAYS bool
bufEmpty(const Buffer *const this)
{
    return bufUsed(this) == 0;
}

// Buffer size
FN_INLINE_ALWAYS size_t
bufSize(const Buffer *const this)
{
    return THIS_PUB(Buffer)->size;
}

// Is the buffer full?
FN_INLINE_ALWAYS bool
bufFull(const Buffer *const this)
{
    return bufUsed(this) == bufSize(this);
}

// Buffer pointer
FN_INLINE_ALWAYS unsigned char *
bufPtr(Buffer *const this)
{
    return THIS_PUB(Buffer)->buffer;
}

// Const buffer pointer
FN_INLINE_ALWAYS const unsigned char *
bufPtrConst(const Buffer *const this)
{
    return THIS_PUB(Buffer)->buffer;
}

// Remaining space in the buffer
FN_INLINE_ALWAYS size_t
bufRemains(const Buffer *const this)
{
    return bufSize(this) - bufUsed(this);
}

// Pointer to remaining buffer space (after used space)
FN_INLINE_ALWAYS unsigned char *
bufRemainsPtr(Buffer *const this)
{
    return bufPtr(this) + bufUsed(this);
}

// Allocated buffer size. This may be different from bufSize() if a limit has been set.
FN_INLINE_ALWAYS size_t
bufSizeAlloc(const Buffer *const this)
{
    return THIS_PUB(Buffer)->sizeAlloc;
}

// Is the size limited to make the buffer appear smaller?
FN_INLINE_ALWAYS bool
bufSizeLimit(const Buffer *const this)
{
    return THIS_PUB(Buffer)->sizeLimit;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Append the contents of another buffer
FN_EXTERN Buffer *bufCat(Buffer *this, const Buffer *cat);

// Append a C buffer
FN_EXTERN Buffer *bufCatC(Buffer *this, const unsigned char *cat, size_t catOffset, size_t catSize);

// Append a subset of another buffer
FN_EXTERN Buffer *bufCatSub(Buffer *this, const Buffer *cat, size_t catOffset, size_t catSize);

// Are two buffers equal?
FN_EXTERN bool bufEq(const Buffer *this, const Buffer *compare);

// Find a buffer in another buffer
typedef struct BufFindParam
{
    VAR_PARAM_HEADER;
    const unsigned char *begin;                                     // Begin find from this address
} BufFindParam;

#define bufFindP(this, find, ...)                                                                                                     \
    bufFind(this, find, (BufFindParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN const unsigned char *bufFind(const Buffer *this, const Buffer *find, BufFindParam param);

// Move to a new parent mem context
FN_INLINE_ALWAYS Buffer *
bufMove(Buffer *const this, MemContext *const parentNew)
{
    return objMove(this, parentNew);
}

// Resize the buffer
FN_EXTERN Buffer *bufResize(Buffer *this, size_t size);

// Manage buffer limits
FN_EXTERN void bufLimitClear(Buffer *this);
FN_EXTERN void bufLimitSet(Buffer *this, size_t limit);

FN_EXTERN void bufUsedInc(Buffer *this, size_t inc);
FN_EXTERN void bufUsedSet(Buffer *this, size_t used);
FN_EXTERN void bufUsedZero(Buffer *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
bufFree(Buffer *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for constant buffers

Frequently used constant buffers can be declared with these macros at compile time rather than dynamically at run time.

Note that buffers created in this way are declared as const so can't be modified or freed by the buf*() methods. Casting to Buffer *
will result in a segfault.

By convention all buffer constant identifiers are appended with _BUF.
***********************************************************************************************************************************/
// This struct must be kept in sync with BufferPub (except for const qualifiers)
typedef struct BufferPubConst
{
    size_t sizeAlloc;                                               // Allocated size of the buffer
    size_t size;                                                    // Reported size of the buffer
    bool sizeLimit;                                                 // Is the size limited to make the buffer appear smaller?
    size_t used;                                                    // Amount of buffer used
    const unsigned char *buffer;                                    // Buffer
} BufferPubConst;

// Create a buffer constant inline from an unsigned char[]
#define BUF(bufferParam, sizeParam)                                                                                                \
    ((const Buffer *)&(const BufferPubConst){.size = sizeParam, .used = sizeParam, .buffer = (const unsigned char *)bufferParam})

// Create a buffer constant inline from a non-constant zero-terminated string
#define BUFSTRZ(stringz)                                                                                                           \
    BUF((const unsigned char *)stringz, strlen(stringz))

// Create a buffer constant inline from a String
#define BUFSTR(string)                                                                                                             \
    BUF((const unsigned char *)strZ(string), strSize(string))

// Create a buffer constant inline from a constant zero-terminated string
#define BUFSTRDEF(stringdef)                                                                                                       \
    BUF((const unsigned char *)stringdef, (sizeof(stringdef) - 1))

// Used to define buffer constants that will be externed using BUFFER_DECLARE(). Must be used in a .c file.
#define BUFFER_EXTERN(name, ...)                                                                                                   \
    static const uint8_t name##_RAW[] = {__VA_ARGS__};                                                                             \
    VR_EXTERN_DEFINE const Buffer *const name = BUF(name##_RAW, sizeof(name##_RAW));

// Used to define String Buffer constants that will be externed using BUFFER_DECLARE(). Must be used in a .c file.
#define BUFFER_STRDEF_EXTERN(name, string)                                                                                         \
    VR_EXTERN_DEFINE const Buffer *const name = BUFSTRDEF(string)

// Used to define String Buffer constants that will be local to the .c file. Must be used in a .c file.
#define BUFFER_STRDEF_STATIC(name, string)                                                                                         \
    static const Buffer *const name = BUFSTRDEF(string)

// Used to declare externed Buffer constants defined with BUFFER*EXTERN(). Must be used in a .h file.
#define BUFFER_DECLARE(name)                                                                                                       \
    VR_EXTERN_DECLARE const Buffer *const name

/***********************************************************************************************************************************
Constant buffers that are generally useful
***********************************************************************************************************************************/
BUFFER_DECLARE(BRACEL_BUF);
BUFFER_DECLARE(BRACER_BUF);
BUFFER_DECLARE(BRACKETL_BUF);
BUFFER_DECLARE(BRACKETR_BUF);
BUFFER_DECLARE(COMMA_BUF);
BUFFER_DECLARE(EQ_BUF);
BUFFER_DECLARE(LF_BUF);
BUFFER_DECLARE(QUOTED_BUF);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
FN_EXTERN void bufToLog(const Buffer *this, StringStatic *debugLog);

#define FUNCTION_LOG_BUFFER_TYPE                                                                                                   \
    Buffer *
#define FUNCTION_LOG_BUFFER_FORMAT(value, buffer, bufferSize)                                                                      \
    FUNCTION_LOG_OBJECT_FORMAT(value, bufToLog, buffer, bufferSize)

#endif
