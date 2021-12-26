/***********************************************************************************************************************************
Tape Archive

!!!
***********************************************************************************************************************************/
#ifndef STORAGE_TAR_H
#define STORAGE_TAR_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct TarHeader TarHeader;

#include "common/io/write.h"
#include "common/type/param.h"
#include "common/type/object.h"
#include "common/type/string.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
typedef struct TarHeaderNewParam
{
    VAR_PARAM_HEADER;
    const String *name;                                             // Name
	uint64_t size;                                                  // Size
	time_t timeModified;                                            // Time modified
	mode_t mode;                                                    // Mode
    uid_t userId;                                                   // User id
    const String *user;                                             // User name
    gid_t groupId;                                                  // Group id
    const String *group;                                            // Group name
} TarHeaderNewParam;

#define tarHdrNewP(...)                                                                                                            \
    tarHdrNew((TarHeaderNewParam){VAR_PARAM_INIT, __VA_ARGS__})

TarHeader *tarHdrNew(TarHeaderNewParam param);

/***********************************************************************************************************************************
Getters/setters
***********************************************************************************************************************************/
typedef struct TarHeaderPub
{
    const String *name;                                             // Name
	uint64_t size;                                                  // Size
} TarHeaderPub;

// Name
__attribute__((always_inline)) static inline const String *
tarHdrName(const TarHeader *const this)
{
    return THIS_PUB(TarHeader)->name;
}

// Size
__attribute__((always_inline)) static inline uint64_t
tarHdrSize(const TarHeader *const this)
{
    return THIS_PUB(TarHeader)->size;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Write tar header
void tarHdrWrite(const TarHeader *this, IoWrite *write);

// Write file padding
void tarHdrWritePadding(const TarHeader *this, IoWrite *write);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
tarHdrFree(TarHeader *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Write final padding at end of tar
void tarWritePadding(IoWrite *write);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *tarHdrToLog(const TarHeader *this);

#define FUNCTION_LOG_TAR_HEADER_TYPE                                                                                               \
    TarHeader *
#define FUNCTION_LOG_TAR_HEADER_FORMAT(value, buffer, bufferSize)                                                                  \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, tarHdrToLog, buffer, bufferSize)

#endif
