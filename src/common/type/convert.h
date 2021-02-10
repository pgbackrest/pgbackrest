/***********************************************************************************************************************************
Convert C Types

Contains conversions to/from native C types. Conversions of project types should be defined in their respective type modules.
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_CONVERT_H
#define COMMON_TYPE_CONVERT_H

#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>

/***********************************************************************************************************************************
Required buffer sizes
***********************************************************************************************************************************/
#define CVT_BOOL_BUFFER_SIZE                                        6
#define CVT_BASE10_BUFFER_SIZE                                      64

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert char to zero-terminated string
size_t cvtCharToZ(char value, char *buffer, size_t bufferSize);

// Convert double to zero-terminated string and vice versa
size_t cvtDoubleToZ(double value, char *buffer, size_t bufferSize);
double cvtZToDouble(const char *value);

// Convert int to zero-terminated string and vice versa
size_t cvtIntToZ(int value, char *buffer, size_t bufferSize);
int cvtZToInt(const char *value);
int cvtZToIntBase(const char *value, int base);

// Convert int64 to zero-terminated string and vice versa
size_t cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize);
int64_t cvtZToInt64(const char *value);
int64_t cvtZToInt64Base(const char *value, int base);

// Convert int32/64 to uint32/64 using zigzag encoding and vice versa. Zigzag encoding places the sign in the least significant bit
// so that signed and unsigned values alternate, e.g. 0 = 0, -1 = 1, 1 = 2, -2 = 3, 2 = 4, -3 = 5, 3 = 6, etc. This moves as many
// bits as possible into the low order bits which is good for other types of encoding, e.g. base-128. See
// http://neurocline.github.io/dev/2015/09/17/zig-zag-encoding.html for details.
__attribute__((always_inline)) static inline uint32_t
cvtInt32ToZigZag(int32_t value)
{
    return ((uint32_t)value << 1) ^ (uint32_t)(value >> 31);
}

__attribute__((always_inline)) static inline int32_t
cvtInt32FromZigZag(uint32_t value)
{
    return (int32_t)((value >> 1) ^ (~(value & 1) + 1));
}

__attribute__((always_inline)) static inline uint64_t
cvtInt64ToZigZag(int64_t value)
{
    return ((uint64_t)value << 1) ^ (uint64_t)(value >> 63);
}

__attribute__((always_inline)) static inline int64_t
cvtInt64FromZigZag(uint64_t value)
{
    return (int64_t)((value >> 1) ^ (~(value & 1) + 1));
}

// Convert mode to zero-terminated string and vice versa
size_t cvtModeToZ(mode_t value, char *buffer, size_t bufferSize);
mode_t cvtZToMode(const char *value);

// Convert size/ssize to zero-terminated string
size_t cvtSizeToZ(size_t value, char *buffer, size_t bufferSize);
size_t cvtSSizeToZ(ssize_t value, char *buffer, size_t bufferSize);

// Convert time_t to zero-terminated string
size_t cvtTimeToZ(time_t value, char *buffer, size_t bufferSize);

// Convert uint to zero-terminated string and vice versa
size_t cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize);
unsigned int cvtZToUInt(const char *value);
unsigned int cvtZToUIntBase(const char *value, int base);

// Convert uint64 to zero-terminated string and visa versa
size_t cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize);
uint64_t cvtZToUInt64(const char *value);
uint64_t cvtZToUInt64Base(const char *value, int base);

// Convert boolean to zero-terminated string. Use cvtBoolToConstZ() whenever possible since it is more efficient.
size_t cvtBoolToZ(bool value, char *buffer, size_t bufferSize);
const char *cvtBoolToConstZ(bool value);

#endif
