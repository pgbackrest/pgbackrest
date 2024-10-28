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
#include "command/control/stop.h"
#include "common/debug.h"
#include "common/lock.h"
#include "common/type/convert.h"
#include "config/config.h"
#include "storage/helper.h"
#include "storage/storage.h"
#include "storage/storage.intern.h"

/**********************************************************************************************************************************/
FN_EXTERN void
cmdStop(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const stopFile = lockStopFileName(cfgOptionStrNull(cfgOptStanza));

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
                const String *const lockPath = cfgOptionStr(cfgOptLockPath);
                const String *const stanzaPrefix =
                    cfgOptionTest(cfgOptStanza) ? strNewFmt("%s-", strZ(cfgOptionStr(cfgOptStanza))) : NULL;
                const StringList *const lockPathFileList = strLstSort(
                    storageListP(storageLocal(), lockPath, .errorOnMissing = true), sortOrderAsc);
                List *const pidList = lstNewP(sizeof(int), .comparator = lstComparatorInt);

                // Find each lock file and send term signals to the processes
                for (unsigned int lockPathFileIdx = 0; lockPathFileIdx < strLstSize(lockPathFileList); lockPathFileIdx++)
                {
                    const String *const lockFile = strLstGet(lockPathFileList, lockPathFileIdx);

                    // Skip any file that is not a lock file. Skip lock files for other stanzas if a stanza is provided.
                    if (!strEndsWithZ(lockFile, LOCK_FILE_EXT) || (stanzaPrefix != NULL && !strBeginsWith(lockFile, stanzaPrefix)))
                        continue;

                    // Read the lock file
                    const LockReadResult lockResult = lockReadP(lockFile, .remove = true);

                    // If we cannot read the lock file for any reason then warn and continue to next file
                    if (lockResult.status != lockReadStatusValid)
                    {
                        LOG_WARN_FMT("unable to read lock file %s/%s", strZ(lockPath), strZ(lockFile));
                        continue;
                    }

                    // If the process has already been killed then skip it. This is possible because a single process may be holding
                    // multiple lock files.
                    if (lstFind(pidList, &lockResult.data.processId) != NULL)
                        continue;

                    lstAdd(pidList, &lockResult.data.processId);

                    // The lock file is valid so that means there is a running process -- send a term signal to the process
                    if (kill(lockResult.data.processId, SIGTERM) != 0)
                        LOG_WARN_FMT("unable to send term signal to process %d", lockResult.data.processId);
                    else
                        LOG_INFO_FMT("sent term signal to process %d", lockResult.data.processId);
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
