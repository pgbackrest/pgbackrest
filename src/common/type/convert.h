/***********************************************************************************************************************************
Convert Base Data Types
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
