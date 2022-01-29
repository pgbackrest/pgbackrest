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

            // Create the stop file with Read/Write and Create only - do not use Truncate
            int fd = -1;
            THROW_ON_SYS_ERROR_FMT(
                ((fd = open(strZ(stopFile), O_WRONLY | O_CREAT, STORAGE_MODE_FILE_DEFAULT)) == -1), FileOpenError,
                "unable to open stop file '%s'", strZ(stopFile));

            // Close the file
            close(fd);

            // If --force was specified then send term signals to running processes
            if (cfgOptionBool(cfgOptForce))
            {
                const String *lockPath = cfgOptionStr(cfgOptLockPath);
                StringList *lockPathFileList = strLstSort(
                    storageListP(storageLocal(), lockPath, .errorOnMissing = true), sortOrderAsc);

                // Find each lock file and send term signals to the processes
                for (unsigned int lockPathFileIdx = 0; lockPathFileIdx < strLstSize(lockPathFileList); lockPathFileIdx++)
                {
                    String *lockFile = strNewFmt("%s/%s", strZ(lockPath), strZ(strLstGet(lockPathFileList, lockPathFileIdx)));

                    // Skip any file that is not a lock file
                    if (!strEndsWithZ(lockFile, LOCK_FILE_EXT))
                        continue;

                    // If we cannot open the lock file for any reason then warn and continue to next file
                    if ((fd = open(strZ(lockFile), O_RDONLY, 0)) == -1)
                    {
                        LOG_WARN_FMT("unable to open lock file %s", strZ(lockFile));
                        continue;
                    }

                    // Attempt a lock on the file - if a lock can be acquired that means the original process died without removing
                    // the lock file so remove it now
                    if (flock(fd, LOCK_EX | LOCK_NB) == 0)
                    {
                        unlink(strZ(lockFile));
                        close(fd);
                        continue;
                    }

                    // The file is locked so that means there is a running process - read the process id and send it a term signal
                    const pid_t processId = lockReadData(lockFile, fd).processId;

                    // If the process id is defined then assume this is a valid lock file
                    if (processId != 0)
                    {
                        if (kill(processId, SIGTERM) != 0)
                            LOG_WARN_FMT("unable to send term signal to process %d", processId);
                        else
                            LOG_INFO_FMT("sent term signal to process %d", processId);
                    }
                    else
                    {
                        unlink(strZ(lockFile));
                        close(fd);
                    }
                }
            }
        }
        else
        {
            LOG_WARN_FMT(
                "stop file already exists for %s",
                cfgOptionTest(cfgOptStanza) ? strZ(strNewFmt("stanza %s", strZ(cfgOptionDisplay(cfgOptStanza)))) : "all stanzas");
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
