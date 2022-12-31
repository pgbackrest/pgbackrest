/***********************************************************************************************************************************
Archive Push Command
***********************************************************************************************************************************/
#ifndef COMMAND_ARCHIVE_PUSH_PUSH_H
#define COMMAND_ARCHIVE_PUSH_PUSH_H

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Push a WAL segment to the repository
FV_EXTERN void cmdArchivePush(void);

// Async version of archive push that runs in parallel for performance
FV_EXTERN void cmdArchivePushAsync(void);

#endif
