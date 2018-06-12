/***********************************************************************************************************************************
Archive Common
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_COMMON_H
#define COMMAND_ARCHIVE_COMMON_H

#include <sys/types.h>

/***********************************************************************************************************************************
Archive mode enum

Used for functions that are common to both archive-push and archive-get so they can tailor their behavior to the command being run.
***********************************************************************************************************************************/
typedef enum
{
    archiveModePush,
    archiveModeGet,
} ArchiveMode;

#include "common/type/stringList.h"

/***********************************************************************************************************************************
WAL segment constants
***********************************************************************************************************************************/
// Only match on a WAL segment without checksum appended
#define WAL_SEGMENT_REGEXP                                          "^[0-F]{24}$"

// Defines the size of standard WAL segment name -- this should never changed
#define WAL_SEGMENT_NAME_SIZE                                       ((uint)24)

// Default size of a WAL segment
#define WAL_SEGMENT_DEFAULT_SIZE                                    ((size_t)(16 * 1024 * 1024))

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool confessOnError);

String *walSegmentNext(const String *walSegment, size_t walSegmentSize, uint pgVersion);
StringList *walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, uint pgVersion, uint range);

#endif
