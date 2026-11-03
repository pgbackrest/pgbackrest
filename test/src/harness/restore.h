/***********************************************************************************************************************************
Harness for Restore Testing
***********************************************************************************************************************************/
#ifndef TEST_HARNESS_RESTORE_H
#define TEST_HARNESS_RESTORE_H

#include <stdbool.h>
#include <sys/types.h>

#include "common/macro.h"

/***********************************************************************************************************************************
Script individual filesystem operations without replacing the restore helper or coordinator
***********************************************************************************************************************************/
#ifdef HAVE_SYNCFS

typedef enum
{
    hrnRestoreSyncOpOpen,
    hrnRestoreSyncOpStat,
    hrnRestoreSyncOpSync,
    hrnRestoreSyncOpClose,
} HrnRestoreSyncOp;

typedef struct HrnRestoreSyncScript
{
    HrnRestoreSyncOp op;                                            // Operation expected
    const char *path;                                               // Expected path for open
    int fd;                                                         // Descriptor returned by open or passed to other operations
    dev_t device;                                                   // Device returned by fstat
    bool file;                                                      // Expect a regular file descriptor rather than a directory
    bool writeOnly;                                                 // Expect non-truncating write access for a write-only file
    int errNo;                                                      // Return -1 with this errno when nonzero
    void (*callback)(void);                                         // Observe actual restore state at this operation
} HrnRestoreSyncScript;

#define HRN_RESTORE_SYNC_SCRIPT_SET(...)                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnRestoreSyncScript HRN_RESTORE_SYNC_SCRIPT_SET_script[] = {__VA_ARGS__};                                           \
        hrnRestoreSyncScriptSet(                                                                                                   \
            HRN_RESTORE_SYNC_SCRIPT_SET_script, LENGTH_OF(HRN_RESTORE_SYNC_SCRIPT_SET_script));                                    \
    }                                                                                                                              \
    while (0)

void hrnRestoreSyncScriptSet(const HrnRestoreSyncScript *script, unsigned int scriptSize);
bool hrnRestoreSyncScriptDone(void);
void hrnRestoreSyncScriptClear(void);

#endif

/***********************************************************************************************************************************
Check synchronization on writers for this absolute destination path. Pass NULL to disable observation. The path must remain valid
until the next call. Only writers created by the restore implementation are observed.
***********************************************************************************************************************************/
void hrnRestoreWriteSyncSet(const char *file, bool syncFile);
unsigned int hrnRestoreWriteSyncTotal(void);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Restore the database from the test backup and handle all the required locking. The restore configuration must already be loaded.
void hrnCmdRestore(void);

#endif
