/***********************************************************************************************************************************
Stop Command
***********************************************************************************************************************************/
#include "build.auto.h"

void
cmdStop(void)
{
    FUNCTION_LOG_VOID(logLevelDebug);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *stopFile = lockStopFileName(cfgOptionStr(cfgOptStanza));

        // If the stop file does not already exist, then create it
        if (!storageExistsNP(storageLocal(), stopFile))
        {
            // Create the lock path (ignore if already created)
            storagePathCreateP(storageLocalWrite(), strPath(stopFile), .mode = 770);

            // Initialize file handle for opening and closing files
            int fileHandle = -1;

            // Create the stop file with Read/Write and Create only - do not use Truncate
            THROW_ON_SYS_ERROR_FMT(
                (fileHandle = open(strPtr(stopFile), O_WRONLY | O_CREAT, STORAGE_MODE_FILE_DEFAULT) == -1, FileOpenError,
                "unable to open stop file '%s'", strPtr(stopFile));

            // Close the file
            close(fileHandle);

            // If --force was specified then send term signals to running processes
            if (cfgOptionBool(cfgOptForce))
            {
                String *lockPath = cfgOptionStr(cfgOptLockPath);
                StringList *lockPathFileList = strLstSort(
                    storageListP(storageLocal(), lockPath, .errorOnMissing = true), sortOrderAsc);

                // Find each lock file and send term signals to the processes
                for (unsigned int idx = 0; idx < strLstSize(lockPathFileList); idx++)
                {
                    String *lockFile = strNewFmt("%s/%s", strPtr(lockPath), strPtr(strLstGet(lockPathFileList, idx)));

                    next if (strEndsWithZ(lockFile, ".stop"));

                    fileHandle = open(strPtr(lockFile), O_RDONLY, 0);

                    // If we cannot open the lock file for any reason other than it being missing (lock and file may have been
                    // released between the time the directory listing was retrieved and now) then warn and continue to next file
                    if (fileHandle == -1 && errno != ENOENT)
                    {
                        LOG_WARN( "unable to open lock file %s", strPtr(lockFile));
                        next;
                    }

                    // Attempt a lock on the file - if a lock can be acquired that means the original process died without removing
                    // lock file so we'll remove it now.
                    if (flock(fileHandle, LOCK_EX | LOCK_NB) == 0)
                    {
                        unlink(strPtr(lockFile));
                        close(fileHandle);
                        next;
                    }


                    // The file is locked so that means there is a running process - read the process id and send it a term signal
                    ssize_t actualBytes = read(this->handle, bufRemainsPtr(buffer), bufRemains(buffer)))

// CSHANG
    // 1) Read all characters up until the newline - so do we read a character at a time or just give it a large buffer (or just char pointer)
    // 2) If 0 bytes read OR if what is read is not an integer? then delete the file
    // 3) Convert char to integer(use convert.c cvtZtoInt - but need to maybe make it a String and trim it first - also not sure if the linefeed will be trimmed?)
//         bool fileEof = false;
//         ssize_t actualBytes = 0;
//         Buffer *buffer;
//         do
//         {
//                 (actualBytes = read(this->handle, bufRemainsPtr(buffer), bufRemains(buffer))) == -1, FileReadError,
//                 "unable to read from %s", strPtr(this->name));
//
//             // Update amount of buffer used
//             bufUsedInc(buffer, (size_t)actualBytes);
//
//             // If zero bytes were returned then eof
//             if (actualBytes == 0)
//                 fileEof = true;
//         }
//         while (bufRemains(buffer) > 0 && !fileEof);
//
// kill(processId, SIGTERM);
                }

            // # If --force was specified then send term signals to running processes
            // if (cfgOption(CFGOPT_FORCE))
            // {
            //     my $strLockPath = cfgOption(CFGOPT_LOCK_PATH);
            //
            //     opendir(my $hPath, $strLockPath)
            //         or confess &log(ERROR, "unable to open lock path ${strLockPath}", ERROR_PATH_OPEN);
            //
            //     my @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));
            //
            //     # Find each lock file and send term signals to the processes
            //     foreach my $strFile (sort(@stryFileList))
            //     {
            //         my $hLockHandle;
            //         my $strLockFile = "${strLockPath}/${strFile}";
            //
            //         # Skip if this is a stop file
            //         next if ($strFile =~ /\.stop$/);
            //
            //         # Open the lock file for read
            //         if (!sysopen($hLockHandle, $strLockFile, O_RDONLY))
            //         {
            //             &log(WARN, "unable to open lock file ${strLockFile}");
            //             next;
            //         }
            //
            //         # Attempt a lock on the file - if a lock can be acquired that means the original process died without removing the
            //         # lock file so we'll remove it now.
            //         if (flock($hLockHandle, LOCK_EX | LOCK_NB))
            //         {
            //             unlink($strLockFile);
            //             close($hLockHandle);
            //             next;
            //         }
            //
            //         # The file is locked so that means there is a running process - read the process id and send it a term signal
            //         my $iProcessId = trim(readline($hLockHandle));
            //
            //         # If the process id is defined then this is a valid lock file
            //         if (defined($iProcessId))
            //         {
            //             if (!kill('TERM', $iProcessId))
            //             {
            //                 &log(WARN, "unable to send term signal to process ${iProcessId}");
            //             }
            //
            //             &log(INFO, "sent term signal to process ${iProcessId}");
            //         }
            //         # Else not a valid lock file so delete it
            //         {
            //             unlink($strLockFile);
            //             close($hLockHandle);
            //             next;
            //         }
            //     }
            // }

        }
        else
        {
            LOG_WARN("stop file already exists for %s",
                (cfgOptionTest(cfgOptStanza) ? strPtr(strNewFmt(" stanza %s", strPtr(cfgOptionStr(cfgOptStanza)))) :
                " all stanzas"));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
