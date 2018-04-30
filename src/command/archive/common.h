/***********************************************************************************************************************************
Archive Common
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_COMMON_H
#define COMMAND_ARCHIVE_COMMON_H

#include <sys/types.h>

#include "common/type/stringList.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool archiveAsyncStatus(const String *walSegment, bool confessOnError);
String *walSegmentNext(const String *walSegment, size_t walSegmentSize, uint pgVersion);
StringList *walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, uint pgVersion, uint range);

#endif
