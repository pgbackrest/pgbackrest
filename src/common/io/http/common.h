/***********************************************************************************************************************************
Http Common

Http common functions.
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_COMMON_H
#define COMMON_IO_HTTP_COMMON_H

#include <time.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert Last-Modified header to time_t
time_t httpLastModifiedToTime(const String *lastModified);

// Encode string to conform with URI specifications. If a path is being encoded then / characters won't be encoded.
String *httpUriEncode(const String *uri, bool path);

#endif
