####################################################################################################################################
# COMMON LOCK MODULE
####################################################################################################################################
package BackRest::Common::Lock;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:DEFAULT :flock);
use File::Basename qw(dirname);

use lib dirname($0) . '/../lib';
use BackRest::Common::Exception;
use BackRest::Common::Log;
use BackRest::Config::Config;

####################################################################################################################################
# Global lock type and handle
####################################################################################################################################
my $strCurrentLockType;
my $strCurrentLockFile;
my $hCurrentLockHandle;

####################################################################################################################################
# lockPathName
#
# Get the base path where the lock file will be stored.
####################################################################################################################################
sub lockPathName
{
    my $strRepoPath = shift;

    return "${strRepoPath}/lock";
}

####################################################################################################################################
# lockFileName
#
# Get the lock file name.
####################################################################################################################################
sub lockFileName
{
    my $strLockType = shift;
    my $strStanza = shift;
    my $strRepoPath = shift;

    return lockPathName($strRepoPath) . "/${strStanza}-${strLockType}.lock";
}

####################################################################################################################################
# lockAcquire
#
# Attempt to acquire the specified lock.  If the lock is taken by another process return false, else take the lock and return true.
####################################################################################################################################
sub lockAcquire
{
    my $strLockType = shift;
    my $bFailOnNoLock = shift;

    # Cannot proceed if a lock is currently held
    if (defined($strCurrentLockType))
    {
        confess &lock(ASSERT, "${strCurrentLockType} lock is already held");
    }

    # Create the lock path if it does not exist.  Use 770 so that members of the group can run read-only processes.
    if (! -e lockPathName(optionGet(OPTION_REPO_PATH)))
    {
        mkdir (lockPathName(optionGet(OPTION_REPO_PATH)), 0770)
            or confess &log(ERROR, 'unable to create lock path ' . lockPathName(optionGet(OPTION_REPO_PATH)), ERROR_PATH_CREATE);
    }

    # Attempt to open the lock file
    $strCurrentLockFile = lockFileName($strLockType, optionGet(OPTION_STANZA), optionGet(OPTION_REPO_PATH));

    sysopen($hCurrentLockHandle, $strCurrentLockFile, O_WRONLY | O_CREAT, 0660)
        or confess &log(ERROR, "unable to open lock file ${strCurrentLockFile}", ERROR_FILE_OPEN);

    # Attempt to lock the lock file
    if (!flock($hCurrentLockHandle, LOCK_EX | LOCK_NB))
    {
        close($hCurrentLockHandle);

        if (!defined($bFailOnNoLock) || $bFailOnNoLock)
        {
            confess &log(ERROR, "unable to acquire ${strLockType} lock", ERROR_LOCK_ACQUIRE);
        }

        return false;
    }

    # Set current lock type so we know we have a lock
    $strCurrentLockType = $strLockType;

    # Lock was successful
    return true;
}

push @EXPORT, qw(lockAcquire);

####################################################################################################################################
# lockRelease
####################################################################################################################################
sub lockRelease
{
    my $strLockType = shift;

    # Fail if there is no lock
    if (!defined($strCurrentLockType))
    {
        confess &log(ASSERT, 'no lock is currently held');
    }

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

push @EXPORT, qw(lockRelease);

1;
