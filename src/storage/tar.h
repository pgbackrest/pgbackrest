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
Destructor
***********************************************************************************************************************************/
__attribute__((always_inline)) static inline void
tarHdrFree(TarHeader *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
String *tarHdrToLog(const TarHeader *this);

#define FUNCTION_LOG_HTTP_URL_TYPE                                                                                                 \
    TarHeader *
#define FUNCTION_LOG_HTTP_URL_FORMAT(value, buffer, bufferSize)                                                                    \
    FUNCTION_LOG_STRING_OBJECT_FORMAT(value, tarHdrToLog, buffer, bufferSize)

#endif
