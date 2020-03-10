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

#include "common/compress/helper.h"
#include "common/type/stringList.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Status file extension constants
***********************************************************************************************************************************/
#define STATUS_EXT_ERROR                                            ".error"
#define STATUS_EXT_ERROR_SIZE                                       (sizeof(STATUS_EXT_ERROR) - 1)

#define STATUS_EXT_OK                                               ".ok"
#define STATUS_EXT_OK_SIZE                                          (sizeof(STATUS_EXT_OK) - 1)

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

// WAL segment directory/file
#define WAL_SEGMENT_DIR_REGEXP                                      "^[0-F]{16}$"
    STRING_DECLARE(WAL_SEGMENT_DIR_REGEXP_STR);
#define WAL_SEGMENT_FILE_REGEXP                                     "^[0-F]{24}-[0-f]{40}" COMPRESS_TYPE_REGEXP "{0,1}$"
    STRING_DECLARE(WAL_SEGMENT_FILE_REGEXP_STR);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Remove errors for an archive file.  This should be done before forking the async process to prevent a race condition where an
// old error may be reported rather than waiting for the async process to succeed or fail.
void archiveAsyncErrorClear(ArchiveMode archiveMode, const String *archiveFile);

bool archiveAsyncStatus(ArchiveMode archiveMode, const String *walSegment, bool throwOnError);
void archiveAsyncStatusOkWrite(ArchiveMode archiveMode, const String *walSegment, const String *warning);
void archiveAsyncStatusErrorWrite(ArchiveMode archiveMode, const String *walSegment, int code, const String *message);

// Execute the async process.  This function will only return in the calling process and the implementation is platform depedent.
void archiveAsyncExec(ArchiveMode archiveMode, const StringList *commandExec);

bool walIsPartial(const String *walSegment);
bool walIsSegment(const String *walSegment);
String *walPath(const String *walFile, const String *pgPath, const String *command);
String *walSegmentFind(const Storage *storage, const String *archiveId, const String *walSegment, TimeMSec timeout);
String *walSegmentNext(const String *walSegment, size_t walSegmentSize, unsigned int pgVersion);
StringList *walSegmentRange(const String *walSegmentBegin, size_t walSegmentSize, unsigned int pgVersion, unsigned int range);

#endif
