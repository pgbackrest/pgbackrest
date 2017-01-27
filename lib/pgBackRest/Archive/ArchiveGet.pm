####################################################################################################################################
# ARCHIVE GET MODULE
####################################################################################################################################
package pgBackRest::Archive::ArchiveGet;
use parent 'pgBackRest::Archive::Archive';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(SEEK_CUR O_RDONLY O_WRONLY O_CREAT);
use File::Basename qw(dirname basename);
use Scalar::Util qw(blessed);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Lock;
use pgBackRest::Common::Log;
use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Archive::ArchiveInfo;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->process');

    # Make sure the archive file is defined
    if (!defined($ARGV[1]))
    {
        confess &log(ERROR, 'WAL segment not provided', ERROR_PARAM_REQUIRED);
    }

    # Make sure the destination file is defined
    if (!defined($ARGV[2]))
    {
        confess &log(ERROR, 'WAL segment destination not provided', ERROR_PARAM_REQUIRED);
    }

    # Info for the Postgres log
    &log(INFO, 'get WAL segment ' . $ARGV[1]);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $self->get($ARGV[1], $ARGV[2]), trace => true}
    );
}

####################################################################################################################################
# get
####################################################################################################################################
sub get
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strSourceArchive,
        $strDestinationFile
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->get', \@_,
            {name => 'strSourceArchive'},
            {name => 'strDestinationFile'}
        );

    lockStopTest();

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(BACKUP)
    );

    # Construct absolute path to the WAL file when it is relative
    $strDestinationFile = walPath($strDestinationFile, optionGet(OPTION_DB_PATH, false), commandGet());

    # Get the wal segment filename
    my ($strArchiveId, $strArchiveFile) = $self->getCheck(
        $oFile, undef, undef, walIsSegment($strSourceArchive) ? $strSourceArchive : undef);

    if (!defined($strArchiveFile) && !walIsSegment($strSourceArchive) &&
        $oFile->exists(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strSourceArchive}"))
    {
        $strArchiveFile = $strSourceArchive;
    }

    # If there are no matching archive files then there are two possibilities:
    # 1) The end of the archive stream has been reached, this is normal and a 1 will be returned
    # 2) There is a hole in the archive stream and a hard error should be returned.  However, holes are possible due to async
    #    archiving - so when to report a hole?  Since a hard error will cause PG to terminate, for now treat as case #1.
    my $iResult = 0;

    if (!defined($strArchiveFile))
    {
        &log(INFO, "unable to find ${strSourceArchive} in the archive");

        $iResult = 1;
    }
    else
    {
        # Determine if the source file is already compressed
        my $bSourceCompressed = $strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$" ? true : false;

        # Copy the archive file to the requested location
        $oFile->copy(PATH_BACKUP_ARCHIVE, "${strArchiveId}/${strArchiveFile}",  # Source file
                     PATH_DB_ABSOLUTE, $strDestinationFile,                     # Destination file
                     $bSourceCompressed,                                        # Source compression based on detection
                     false);                                                    # Destination is not compressed
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iResult', value => $iResult}
    );
}

####################################################################################################################################
# getArchiveId
#
# CAUTION: Only to be used by commands where the DB Version and DB System ID are not important such that the db-path is not valid
# for the command  (i.e. expire command). Since this function will not check validity of the database version call getCheck()
# instead.
####################################################################################################################################
sub getArchiveId
{
    my $self = shift;
    my $oFile = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->getArchiveId');

    my $strArchiveId;

    if ($oFile->isRemote(PATH_BACKUP_ARCHIVE))
    {
        $strArchiveId = $oFile->{oProtocol}->cmdExecute(OP_ARCHIVE_GET_ARCHIVE_ID, undef, true);
    }
    else
    {
        $strArchiveId = (new pgBackRest::Archive::ArchiveInfo($oFile->pathGet(PATH_BACKUP_ARCHIVE), true))->archiveId();
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $strArchiveId, trace => true}
    );
}

1;
