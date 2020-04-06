/***********************************************************************************************************************************
Archive Get Command
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_GET_GET_H
#define COMMAND_ARCHIVE_GET_GET_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Get an archive file from the repository (WAL segment, history file, etc.)
int cmdArchiveGet(void);

// Async version of archive get that runs in parallel for performance
void cmdArchiveGetAsync(void);

#endif
