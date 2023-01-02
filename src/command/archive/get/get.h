/***********************************************************************************************************************************
Archive Get Command
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_GET_H
#define COMMAND_ARCHIVE_GET_GET_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get an archive file from the repository (WAL segment, history file, etc.)
FN_EXTERN int cmdArchiveGet(void);

// Async version of archive get that runs in parallel for performance
FN_EXTERN void cmdArchiveGetAsync(void);

#endif
