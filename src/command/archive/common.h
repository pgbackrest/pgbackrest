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
#include "storage/storage.h"

/***********************************************************************************************************************************
WAL segment constants
***********************************************************************************************************************************/
// Extension for partial segments
#define WAL_SEGMENT_PARTIAL_EXT                                     ".partial"

// Match when the first 24 characters match a WAL segment
#define WAL_SEGMENT_PREFIX_REGEXP                                   "^[0-F]{24}"

// Match on a WAL segment without checksum appended
#define WAL_SEGMENT_REGEXP                                          WAL_SEGMENT_PREFIX_REGEXP "$"
    STRING_DECLARE(WAL_SEGMENT_REGEXP_STR);

// Match on a WAL segment with partial allowed
#define WAL_SEGMENT_PARTIAL_REGEXP                                  WAL_SEGMENT_PREFIX_REGEXP "(\\.partial){0,1}$"
    STRING_DECLARE(WAL_SEGMENT_PARTIAL_REGEXP_STR);

// Defines the size of standard WAL segment name -- hopefully this won't change
#define WAL_SEGMENT_NAME_SIZE                                       ((unsigned int)24)

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool confessOnError);

bool walIsPartial(const String *walSegment);
bool walIsSegment(const String *walSegment);
String *walSegmentFind(const Storage *storage, const String *archiveId, const String *walSegment);
String *walSegmentNext(const String *walSegment, size_t walSegmentSize, unsigned int pgVersion);
StringList *walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, unsigned int pgVersion, unsigned int range);

#endif
