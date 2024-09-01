/***********************************************************************************************************************************
Convert C Types

Contains conversions to/from native C types. Conversions of project types should be defined in their respective type modules.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_CONVERT_H
#define COMMON_TYPE_CONVERT_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "common/assert.h"
#include "common/type/param.h"

/***********************************************************************************************************************************
Required buffer sizes
***********************************************************************************************************************************/
#define CVT_BOOL_BUFFER_SIZE                                        6
#define CVT_BASE10_BUFFER_SIZE                                      64
#define CVT_VARINT128_BUFFER_SIZE                                   10

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert char to zero-terminated string
FN_INLINE_ALWAYS size_t
cvtCharToZ(const char value, char *const buffer, const size_t bufferSize)
{
    ASSERT_INLINE(buffer != NULL);
    ASSERT_INLINE(bufferSize >= 2);

    return (size_t)snprintf(buffer, bufferSize, "%c", value);
}

// Convert double to zero-terminated string
FN_EXTERN size_t cvtDoubleToZ(double value, char *buffer, size_t bufferSize);

// Convert int to zero-terminated string and vice versa
FN_EXTERN size_t cvtIntToZ(int value, char *buffer, size_t bufferSize);
FN_EXTERN int cvtZToInt(const char *value);
FN_EXTERN int cvtZToIntBase(const char *value, int base);
FN_EXTERN int cvtZSubNToIntBase(const char *value, size_t offset, size_t size, int base);

FN_INLINE_ALWAYS int
cvtZSubNToInt(const char *const value, const size_t offset, const size_t size)
{
    return cvtZSubNToIntBase(value, offset, size, 10);
}

// Convert int64 to zero-terminated string and vice versa
FN_EXTERN size_t cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize);
FN_EXTERN int64_t cvtZToInt64(const char *value);
FN_EXTERN int64_t cvtZToInt64Base(const char *value, int base);
FN_EXTERN int64_t cvtZSubNToInt64Base(const char *value, size_t offset, size_t size, int base);

FN_INLINE_ALWAYS int64_t
cvtZSubNToInt64(const char *const value, const size_t offset, const size_t size)
{
    return cvtZSubNToInt64Base(value, offset, size, 10);
}

// Convert int32/64 to uint32/64 using zigzag encoding and vice versa. Zigzag encoding places the sign in the least significant bit
// so that signed and unsigned values alternate, e.g. 0 = 0, -1 = 1, 1 = 2, -2 = 3, 2 = 4, -3 = 5, 3 = 6, etc. This moves as many
// bits as possible into the low order bits which is good for other types of encoding, e.g. base-128. See
// http://neurocline.github.io/dev/2015/09/17/zig-zag-encoding.html for details.
FN_INLINE_ALWAYS uint32_t
cvtInt32ToZigZag(const int32_t value)
{
    return ((uint32_t)value << 1) ^ (uint32_t)(value >> 31);
}

FN_INLINE_ALWAYS int32_t
cvtInt32FromZigZag(const uint32_t value)
{
    return (int32_t)((value >> 1) ^ (~(value & 1) + 1));
}

FN_INLINE_ALWAYS uint64_t
cvtInt64ToZigZag(const int64_t value)
{
    return ((uint64_t)value << 1) ^ (uint64_t)(value >> 63);
}

FN_INLINE_ALWAYS int64_t
cvtInt64FromZigZag(const uint64_t value)
{
    return (int64_t)((value >> 1) ^ (~(value & 1) + 1));
}

// Convert mode to zero-terminated string and vice versa
FN_EXTERN size_t cvtModeToZ(mode_t value, char *buffer, size_t bufferSize);
FN_EXTERN mode_t cvtZToMode(const char *value);

// Convert size/ssize to zero-terminated string
FN_EXTERN size_t cvtSizeToZ(size_t value, char *buffer, size_t bufferSize);

// Convert time_t to zero-terminated string
typedef struct CvtTimeToZParam
{
    VAR_PARAM_HEADER;
    bool utc;                                                       // Use UTC instead of local time?
} CvtTimeToZParam;

#define cvtTimeToZP(format, value, buffer, bufferSize, ...)                                                                        \
    cvtTimeToZ(format, value, buffer, bufferSize, (CvtTimeToZParam){VAR_PARAM_INIT, __VA_ARGS__})

FN_EXTERN FN_STRFTIME(1) size_t cvtTimeToZ(
    const char *format, time_t value, char *buffer, size_t bufferSize, CvtTimeToZParam param);

// Convert zero-terminated string with the following format to time_t:
// ^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}((\,|\.)[0-9]{1,6})?((\+|\-)[0-9]{2}(:?)([0-9]{2})?)?$
FN_EXTERN time_t cvtZToTime(const char *time);

// Convert uint to zero-terminated string and vice versa
FN_EXTERN size_t cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize);
FN_EXTERN unsigned int cvtZToUInt(const char *value);
FN_EXTERN unsigned int cvtZToUIntBase(const char *value, int base);
FN_EXTERN unsigned int cvtZSubNToUIntBase(const char *value, size_t offset, size_t size, int base);

FN_INLINE_ALWAYS unsigned int
cvtZSubNToUInt(const char *const value, const size_t offset, const size_t size)
{
    return cvtZSubNToUIntBase(value, offset, size, 10);
}

// Convert uint64 to zero-terminated string and vice versa
FN_EXTERN size_t cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize);
FN_EXTERN uint64_t cvtZToUInt64(const char *value);
FN_EXTERN uint64_t cvtZToUInt64Base(const char *value, int base);
FN_EXTERN uint64_t cvtZSubNToUInt64Base(const char *value, size_t offset, size_t size, int base);

FN_INLINE_ALWAYS uint64_t
cvtZSubNToUInt64(const char *const value, const size_t offset, const size_t size)
{
    return cvtZSubNToUInt64Base(value, offset, size, 10);
}

// Convert uint64 to base-128 varint and vice versa
FN_EXTERN void cvtUInt64ToVarInt128(uint64_t value, uint8_t *buffer, size_t *bufferPos, size_t bufferSize);
FN_EXTERN uint64_t cvtUInt64FromVarInt128(const uint8_t *buffer, size_t *bufferPos, size_t bufferSize);

// Convert boolean to zero-terminated string. Use cvtBoolToConstZ() whenever possible since it is more efficient.
FN_EXTERN size_t cvtBoolToZ(bool value, char *buffer, size_t bufferSize);
FN_EXTERN const char *cvtBoolToConstZ(bool value);

#endif
