####################################################################################################################################
# COMMON LOCK MODULE
####################################################################################################################################
package pgBackRest::Common::Lock;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:DEFAULT :flock);
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::FileCommon;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_LOCK                                                => 'Common:::Lock';

use constant OP_LOCK_ACQUIRE                                        => OP_LOCK . "::lockAquire";
use constant OP_LOCK_RELEASE                                        => OP_LOCK . "::lockRelease";
use constant OP_LOCK_STOP_TEST                                      => OP_LOCK . "::lockStopTest";

####################################################################################################################################
# Global lock type and handle
####################################################################################################################################
my $strCurrentLockType;
my $strCurrentLockFile;
my $hCurrentLockHandle;

####################################################################################################################################
# lockFileName
#
# Get the lock file name.
####################################################################################################################################
sub lockFileName
{
    my $strLockType = shift;
    my $strStanza = shift;
    my $bRemote = shift;
    my $iProcessIdx = shift;

    return optionGet(OPTION_LOCK_PATH) . '/' . (defined($strStanza) ? $strStanza : 'global') . "_${strLockType}" .
           (defined($bRemote) && $bRemote ? '_remote' : '') .
           (defined($iProcessIdx) ? "-${iProcessIdx}" : '') . '.lock';
}

####################################################################################################################################
# lockPathCreate
#
# Create the lock path if it does not exist.
####################################################################################################################################
sub lockPathCreate
{
    filePathCreate(optionGet(OPTION_LOCK_PATH), '770');
}

####################################################################################################################################
# lockAcquire
#
# Attempt to acquire the specified lock.  If the lock is taken by another process return false, else take the lock and return true.
####################################################################################################################################
sub lockAcquire
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strLockType,
        $bFailOnNoLock,
        $bRemote,
        $iProcessIdx
    ) =
        logDebugParam
        (
            OP_LOCK_ACQUIRE, \@_,
            {name => 'strLockType'},
            {name => 'bFailOnNoLock', default => true},
            {name => 'bRemote', default => false},
            {name => 'iProcessIdx', required => false}
        );

    my $bResult = true;

    # Acquire if locking is enabled
    if (!optionTest(OPTION_LOCK) || optionGet(OPTION_LOCK))
    {
        $bResult = false;

        # Cannot proceed if a lock is currently held
        if (defined($strCurrentLockType))
        {
            confess &lock(ASSERT, "${strCurrentLockType} lock is already held");
        }

        # Check if processes are currently stopped
        lockStopTest();

        # Create the lock path
        lockPathCreate();

        # Attempt to open the lock file
        $strCurrentLockFile = lockFileName($strLockType, optionGet(OPTION_STANZA, false), $bRemote, $iProcessIdx);

        sysopen($hCurrentLockHandle, $strCurrentLockFile, O_WRONLY | O_CREAT, oct(640))
            or confess &log(ERROR, "unable to open lock file ${strCurrentLockFile}", ERROR_FILE_OPEN);

        # Attempt to lock the lock file
        if (!flock($hCurrentLockHandle, LOCK_EX | LOCK_NB))
        {
            close($hCurrentLockHandle);

            if (!defined($bFailOnNoLock) || $bFailOnNoLock)
            {
                confess &log(ERROR, "unable to acquire ${strLockType} lock", ERROR_LOCK_ACQUIRE);
            }
        }
        else
        {
            # Write pid into the lock file.  This is used stop terminate processes on a stop --force.
            syswrite($hCurrentLockHandle, "$$\n")
                or confess(ERROR, "unable to write process id into lock file ${strCurrentLockFile}", ERROR_FILE_WRITE);

            # Set current lock type so we know we have a lock
            $strCurrentLockType = $strLockType;

            # Lock was successful
            $bResult = true;
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bResult', value => $bResult}
    );
}

push @EXPORT, qw(lockAcquire);

####################################################################################################################################
# lockRelease
####################################################################################################################################
sub lockRelease
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFailOnNoLock
    ) =
        logDebugParam
        (
            OP_LOCK_RELEASE, \@_,
            {name => 'bFailOnNoLock', default => true}
        );

    # Release if locking is enabled
    if (!optionTest(OPTION_LOCK) || optionGet(OPTION_LOCK))
    {
        # Fail if there is no lock
        if (!defined($strCurrentLockType))
        {
            if ($bFailOnNoLock)
            {
                confess &log(ASSERT, 'no lock is currently held');
            }
        }
        else
        {
            # # Fail if the lock being released is not the one held
            # if ($strLockType ne $strCurrentLockType)
            # {
            #     confess &log(ASSERT, "cannot remove lock ${strLockType} since ${strCurrentLockType} is currently held");
            # }

            # Remove the file
            unlink($strCurrentLockFile);
            close($hCurrentLockHandle);

            # Undef lock variables
            undef($strCurrentLockType);
            undef($hCurrentLockHandle);
        }
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

push @EXPORT, qw(lockRelease);

####################################################################################################################################
# lockStopFileName
#
# Get the stop file name.
####################################################################################################################################
sub lockStopFileName
{
    my $strStanza = shift;

    return optionGet(OPTION_LOCK_PATH) . (defined($strStanza) ? "/${strStanza}" : '/all') . '.stop';
}

####################################################################################################################################
# lockStop
#
# Write a stop file that will prevent backrest processes from running.  Optionally attempt to kill any processes that are currently
# running.
####################################################################################################################################
sub lockStop
{
    # Create the lock path
    lockPathCreate();

    # Generate the stop file name
    my $strStopFile = lockStopFileName(optionGet(OPTION_STANZA, false));

    # If the stop file already exists then warn
    if (-e $strStopFile)
    {
        &log(WARN, 'stop file already exists' .
                   (optionTest(OPTION_STANZA) ? ' for stanza ' . optionGet(OPTION_STANZA) : ' for all stanzas'));
        return false;
    }

    # Create the stop file
    sysopen(my $hStopHandle, $strStopFile, O_WRONLY | O_CREAT, oct(640))
        or confess &log(ERROR, "unable to open stop file ${strStopFile}", ERROR_FILE_OPEN);
    close($hStopHandle);

    # If --force was specified then send term signals to running processes
    if (optionGet(OPTION_FORCE))
    {
        my $strLockPath = optionGet(OPTION_LOCK_PATH);

        opendir(my $hPath, $strLockPath)
            or confess &log(ERROR, "unable to open lock path ${strLockPath}", ERROR_PATH_OPEN);

        my @stryFileList = grep(!/^(\.)|(\.\.)$/i, readdir($hPath));

        # Find each lock file and send term signals to the processes
        foreach my $strFile (sort(@stryFileList))
        {
            my $hLockHandle;
            my $strLockFile = "${strLockPath}/${strFile}";

            # Skip if this is a stop file
            next if ($strFile =~ /\.stop$/);
            #
            # # Skip if this is a thread lock file (we only send TERMs to the main process)
            # next if ($strFile =~ /\-[0-9]+\.lock$/);

            # Open the lock file for read
            if (!sysopen($hLockHandle, $strLockFile, O_RDONLY))
            {
                &log(WARN, "unable to open lock file ${strLockFile}");
                next;
            }

            # Attempt a lock on the file - if a lock can be acquired that means the original process died without removing the
            # lock file so we'll remove it now.
            if (flock($hLockHandle, LOCK_EX | LOCK_NB))
            {
                unlink($strLockFile);
                close($hLockHandle);
                next;
            }

            # The file is locked so that means there is a running process - read the process id and send it a term signal
            my $iProcessId = readline($hLockHandle);

            # If the process id is defined then this is a valid lock file
            if (defined($iProcessId))
            {
                if (!kill('TERM', $iProcessId))
                {
                    &log(WARN, "unable to send term signal to process ${iProcessId}");
                }

                &log(INFO, "sent term signal to process ${iProcessId}");
            }
            # Else not a valid lock file so delete it
            {
                unlink($strLockFile);
                close($hLockHandle);
                next;
            }
        }
    }

    return true;
}

push @EXPORT, qw(lockStop);

####################################################################################################################################
# lockStopTest
#
# Test if a stop file exists for the current stanza or all stanzas.
####################################################################################################################################
sub lockStopTest
{
    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(OP_LOCK_STOP_TEST);

    # Check the stanza first if it is specified
    if (optionTest(OPTION_STANZA))
    {
        # Generate the stop file name
        my $strStopFile = lockStopFileName(optionGet(OPTION_STANZA));

        if (-e $strStopFile)
        {
            confess &log(ERROR, 'stop file exists for stanza ' . optionGet(OPTION_STANZA), ERROR_STOP);
        }
    }

    # Now check all stanzas
    my $strStopFile = lockStopFileName();

    if (-e $strStopFile)
    {
        confess &log(ERROR, 'stop file exists for all stanzas', ERROR_STOP);
    }

    # Return from function and log return values if any
    logDebugReturn($strOperation);
}

push @EXPORT, qw(lockStopTest);

####################################################################################################################################
# lockStart
#
# Remove the stop file so processes can run.
####################################################################################################################################
sub lockStart
{
    # Generate the stop file name
    my $strStopFile = lockStopFileName(optionGet(OPTION_STANZA, false));

    # If the stop file doesn't exist then warn
    if (!-e $strStopFile)
    {
        &log(WARN, 'stop file does not exist' . (optionTest(OPTION_STANZA) ? ' for stanza ' . optionGet(OPTION_STANZA) : ''));
        return false;
    }

    # Remove the stop file
    unlink($strStopFile)
        or confess &log(ERROR, "unable to remove ${strStopFile}: $!");

    return true;
}

push @EXPORT, qw(lockStart);

1;
