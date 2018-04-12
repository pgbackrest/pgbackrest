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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# lockPathCreate
#
# Create the lock path if it does not exist.
####################################################################################################################################
sub lockPathCreate
{
    storageLocal()->pathCreate(cfgOption(CFGOPT_LOCK_PATH), {strMode => '770', bIgnoreExists => true, bCreateParent => true});
}

####################################################################################################################################
# lockStopFileName
#
# Get the stop file name.
####################################################################################################################################
sub lockStopFileName
{
    my $strStanza = shift;

    return cfgOption(CFGOPT_LOCK_PATH) . (defined($strStanza) ? "/${strStanza}" : '/all') . '.stop';
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
    my $strStopFile = lockStopFileName(cfgOption(CFGOPT_STANZA, false));

    # If the stop file already exists then warn
    if (-e $strStopFile)
    {
        &log(WARN, 'stop file already exists' .
                   (cfgOptionTest(CFGOPT_STANZA) ? ' for stanza ' . cfgOption(CFGOPT_STANZA) : ' for all stanzas'));
        return false;
    }

    # Create the stop file
    sysopen(my $hStopHandle, $strStopFile, O_WRONLY | O_CREAT, oct(640))
        or confess &log(ERROR, "unable to open stop file ${strStopFile}", ERROR_FILE_OPEN);
    close($hStopHandle);

    # If --force was specified then send term signals to running processes
    if (cfgOption(CFGOPT_FORCE))
    {
        my $strLockPath = cfgOption(CFGOPT_LOCK_PATH);

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
            my $iProcessId = trim(readline($hLockHandle));

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
    my
    (
        $strOperation,
        $bStanzaStopRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::lockStopTest', \@_,
            {name => 'bStanzaStopRequired', optional => true, default => false}
        );

    my $bStopExists = false;

    # Check the stanza first if it is specified
    if (cfgOptionTest(CFGOPT_STANZA))
    {
        # Generate the stop file name
        my $strStopFile = lockStopFileName(cfgOption(CFGOPT_STANZA));

        if (-e $strStopFile)
        {
            # If the stop file exists and is required then set the flag to true
            if ($bStanzaStopRequired)
            {
                $bStopExists = true;
            }
            else
            {
                confess &log(ERROR, 'stop file exists for stanza ' . cfgOption(CFGOPT_STANZA), ERROR_STOP);
            }
        }
    }

    # If not looking for a specific stanza stop file, then check all stanzas
    if (!$bStanzaStopRequired)
    {
        # Now check all stanzas
        my $strStopFile = lockStopFileName();

        if (-e $strStopFile)
        {
            confess &log(ERROR, 'stop file exists for all stanzas', ERROR_STOP);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'bStopExists', value => $bStopExists}
    );
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
    my $strStopFile = lockStopFileName(cfgOption(CFGOPT_STANZA, false));

    # If the stop file doesn't exist then warn
    if (!-e $strStopFile)
    {
        &log(WARN, 'stop file does not exist' . (cfgOptionTest(CFGOPT_STANZA) ? ' for stanza ' . cfgOption(CFGOPT_STANZA) : ''));
        return false;
    }

    # Remove the stop file
    unlink($strStopFile)
        or confess &log(ERROR, "unable to remove ${strStopFile}: $!");

    return true;
}

push @EXPORT, qw(lockStart);

1;
