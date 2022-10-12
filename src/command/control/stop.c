/***********************************************************************************************************************************
Stop Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include "command/control/common.h"
#include "common/debug.h"
#include "common/lock.h"
#include "common/type/convert.h"
#include "config/config.h"
#include "storage/helper.h"
#include "storage/storage.h"
#include "storage/storage.intern.h"

/**********************************************************************************************************************************/
void
cmdStop(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *stopFile = lockStopFileName(cfgOptionStrNull(cfgOptStanza));

        // If the stop file does not already exist, then create it
        if (!storageExistsP(storageLocal(), stopFile))
        {
            // Create the lock path (ignore if already created)
            storagePathCreateP(storageLocalWrite(), strPath(stopFile), .mode = 0770);

            // Create the stop file with without truncating an existing file
            IoWrite *const stopWrite = storageWriteIo(
                storageNewWriteP(storageLocalWrite(), stopFile, .noAtomic = true, .noTruncate = true, .modePath = 0770));
            ioWriteOpen(stopWrite);
            ioWriteClose(stopWrite);

            // If --force was specified then send term signals to running processes
            if (cfgOptionBool(cfgOptForce))
            {
                const String *lockPath = cfgOptionStr(cfgOptLockPath);
                StringList *lockPathFileList = strLstSort(
                    storageListP(storageLocal(), lockPath, .errorOnMissing = true), sortOrderAsc);

                // Find each lock file and send term signals to the processes
                for (unsigned int lockPathFileIdx = 0; lockPathFileIdx < strLstSize(lockPathFileList); lockPathFileIdx++)
                {
                    const String *lockFile = strLstGet(lockPathFileList, lockPathFileIdx);

                    // Skip any file that is not a lock file. Skip lock files for other stanzas if a stanza is provided.
                    if (!strEndsWithZ(lockFile, LOCK_FILE_EXT) ||
                        (cfgOptionTest(cfgOptStanza) &&
                         !strEq(lockFile, lockFileName(cfgOptionStr(cfgOptStanza), lockTypeArchive)) &&
                         !strEq(lockFile, lockFileName(cfgOptionStr(cfgOptStanza), lockTypeBackup))))
                    {
                        continue;
                    }

                    // Read the lock file
                    lockFile = strNewFmt("%s/%s", strZ(lockPath), strZ(lockFile));
                    LockReadResult lockRead = lockReadFileP(lockFile, .remove = true);

                    // If we cannot read the lock file for any reason then warn and continue to next file
                    if (lockRead.status != lockReadStatusValid)
                    {
                        LOG_WARN_FMT("unable to read lock file %s", strZ(lockFile));
                        continue;
                    }

                    // The lock file is valid so that means there is a running process -- send a term signal to the process
                    if (kill(lockRead.data.processId, SIGTERM) != 0)
                        LOG_WARN_FMT("unable to send term signal to process %d", lockRead.data.processId);
                    else
                        LOG_INFO_FMT("sent term signal to process %d", lockRead.data.processId);
                }
            }
        }
        else
        {
            LOG_WARN_FMT(
                "stop file already exists for %s",
                cfgOptionTest(cfgOptStanza) ? zNewFmt("stanza %s", strZ(cfgOptionDisplay(cfgOptStanza))) : "all stanzas");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
