/***********************************************************************************************************************************
Convert Base Data Types
***********************************************************************************************************************************/
#ifndef COMMON_TYPE_CONVERT_H
#define COMMON_TYPE_CONVERT_H

#include <inttypes.h>
#include <sys/types.h>

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
size_t cvtDoubleToZ(double value, char *buffer, size_t bufferSize);
double cvtZToDouble(const char *value);

size_t cvtIntToZ(int value, char *buffer, size_t bufferSize);
int cvtZToInt(const char *value);

size_t cvtInt64ToZ(int64_t value, char *buffer, size_t bufferSize);
int64_t cvtZToInt64(const char *value);

size_t cvtModeToZ(mode_t value, char *buffer, size_t bufferSize);

size_t cvtSizeToZ(size_t value, char *buffer, size_t bufferSize);

size_t cvtUIntToZ(unsigned int value, char *buffer, size_t bufferSize);

size_t cvtUInt64ToZ(uint64_t value, char *buffer, size_t bufferSize);

size_t cvtBoolToZ(bool value, char *buffer, size_t bufferSize);

#endif
