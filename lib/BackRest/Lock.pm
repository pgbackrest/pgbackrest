####################################################################################################################################
# LOCK MODULE
####################################################################################################################################
package BackRest::Lock;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Fcntl qw(:DEFAULT :flock);
use File::Basename qw(dirname);
use Exporter qw(import);

use lib dirname($0) . '/../lib';
use BackRest::Exception;
use BackRest::Config;
use BackRest::Utility;

####################################################################################################################################
# Exported Functions
####################################################################################################################################
our @EXPORT = qw(lockAcquire lockRelease);

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

    # Cannot proceed if a lock is currently held
    if (defined($strCurrentLockType))
    {
        confess &lock(ASSERT, "${strCurrentLockType} lock is already held");
    }

    # Create the lock path if it does not exist
    if (! -e lockPathName(optionGet(OPTION_REPO_PATH)))
    {
        mkdir lockPathName(optionGet(OPTION_REPO_PATH))
            or confess(ERROR, 'unable to create lock path ' . lockPathName(optionGet(OPTION_REPO_PATH)), ERROR_PATH_CREATE);
    }

    # Attempt to open the lock file
    $strCurrentLockFile = lockFileName($strLockType, optionGet(OPTION_STANZA), optionGet(OPTION_REPO_PATH));

    sysopen($hCurrentLockHandle, $strCurrentLockFile, O_WRONLY | O_CREAT)
        or confess &log(ERROR, "unable to open lock file ${strCurrentLockFile}");

    # Attempt to lock the lock file
    if (!flock($hCurrentLockHandle, LOCK_EX | LOCK_NB))
    {
        close($hCurrentLockHandle);
        return false;
    }

    # Set current lock type so we know we have a lock
    $strCurrentLockType = $strLockType;

    # Lock was successful
    return true;
}

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

1;
