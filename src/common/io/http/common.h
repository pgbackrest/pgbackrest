/***********************************************************************************************************************************
HTTP Common
***********************************************************************************************************************************/
#ifndef COMMON_IO_HTTP_COMMON_H
#define COMMON_IO_HTTP_COMMON_H

#include <time.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Convert HTTP date to time_t and vice versa
FV_EXTERN time_t httpDateToTime(const String *lastModified);
FV_EXTERN String *httpDateFromTime(time_t time);

// Encode string to conform with URI specifications. If a path is being encoded then / characters won't be encoded.
FV_EXTERN String *httpUriEncode(const String *uri, bool path);

// Decode string that conforms to URI specifications
FV_EXTERN String *httpUriDecode(const String *uri);

#endif
