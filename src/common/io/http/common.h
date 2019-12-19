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

String *httpUriEncode(const String *uri, bool path);

#endif
