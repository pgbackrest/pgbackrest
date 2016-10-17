####################################################################################################################################
# ARCHIVE INFO MODULE
#
# The archive.info file is created when archiving begins. It is located under the stanza directory. The file contains information
# regarding the stanza database version, database WAL segment system id and other information to ensure that archiving is being
# performed on the proper database.
####################################################################################################################################
package pgBackRest::ArchiveInfo;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;

use IO::Uncompress::Gunzip qw(gunzip $GunzipError) ;

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::BackupInfo;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::Manifest;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant ARCHIVE_INFO_FILE                                      => 'archive.info';
    our @EXPORT = qw(ARCHIVE_INFO_FILE);

####################################################################################################################################
# Backup info Constants
####################################################################################################################################
use constant INFO_ARCHIVE_SECTION_DB                                => INFO_BACKUP_SECTION_DB;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);
use constant INFO_ARCHIVE_SECTION_DB_HISTORY                        => INFO_BACKUP_SECTION_DB_HISTORY;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);

use constant INFO_ARCHIVE_KEY_DB_VERSION                            => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_VERSION);
use constant INFO_ARCHIVE_KEY_DB_ID                                 => INFO_BACKUP_KEY_HISTORY_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_ID);
use constant INFO_ARCHIVE_KEY_DB_SYSTEM_ID                          => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_SYSTEM_ID);

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                              # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strArchiveClusterPath,                     # Backup cluster path
        $bRequired                                  # Is archive info required?
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strArchiveClusterPath'},
            {name => 'bRequired', default => false}
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = fileExists($strArchiveInfoFile);

    if (!$bExists && $bRequired)
    {
        confess &log(ERROR, ARCHIVE_INFO_FILE . " does not exist but is required to get WAL segments\n" .
                     "HINT: is archive_command configured in postgresql.conf?\n" .
                     "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving" .
                     " scheme.", ERROR_FILE_MISSING);
    }

    # Init object and store variables
    my $self = $class->SUPER::new($strArchiveInfoFile, $bExists);

    $self->{bExists} = $bExists;
    $self->{strArchiveClusterPath} = $strArchiveClusterPath;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# check
#
# Check archive info file and make sure it is compatible with the current version of the database for the stanza. If the file does
# not exist it will be created with the values passed.
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'}
        );

    if ($self->test(INFO_ARCHIVE_SECTION_DB))
    {
        my $strError = undef;

        if (!$self->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion))
        {
            $strError = "WAL segment version ${strDbVersion} does not match archive version " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION);
        }

        if (!$self->test(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId))
        {
            $strError = (defined($strError) ? ($strError . "\n") : "") .
                        "WAL segment system-id ${ullDbSysId} does not match archive system-id " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID);
        }

        if (defined($strError))
        {
            confess &log(ERROR, "${strError}\nHINT: are you archiving to the correct stanza?", ERROR_ARCHIVE_MISMATCH);
        }
    }
    # Else create the info file from the parameters passed which are usually derived from the current WAL segment
    else
    {
        $self->fileCreateUpdate($strDbVersion, $ullDbSysId, $self->getDbHistoryId());
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $self->archiveId()}
    );
}

####################################################################################################################################
# archiveId
#
# Get the archive id which is a combination of the DB version and the db-id setting (e.g. 9.4-1)
####################################################################################################################################
sub archiveId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->archiveId');

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION) . "-" .
                                          $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID)}
    );
}

####################################################################################################################################
# getDbHistoryId
#
# Get the db history ID
####################################################################################################################################
sub getDbHistoryId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->getDbHistoryId');

    # If the DB section does not exist, initialize the history to one, else return the latest ID
    my $iDbHistoryId = (!$self->test(INFO_ARCHIVE_SECTION_DB))
                        ? 1 : $self->numericGet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'iDbHistoryId', value => $iDbHistoryId}
    );
}

####################################################################################################################################
# setDb
#
# Set the db and db:history sections.
####################################################################################################################################
sub setDb
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $iDbHistoryId,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->setDb', \@_,
            {name => 'strDbVersion', trace => true},
            {name => 'ullDbSysId', trace => true},
            {name => 'iDbHistoryId', trace => true}
        );

    # Fill db section
    $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId);
    $self->set(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion);
    $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID, undef, $iDbHistoryId);

    # Fill db history
    $self->numericSet(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_ID, $ullDbSysId);
    $self->set(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbHistoryId, INFO_ARCHIVE_KEY_DB_VERSION, $strDbVersion);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# fileCreateUpdate
#
# Creates or updates the archive.info file.
####################################################################################################################################
sub fileCreateUpdate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $iDbHistoryId
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->fileCreateUpdate', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'iDbHistoryId'}
        );

    # Fill db section and db history section
    $self->setDb($strDbVersion, $ullDbSysId, $iDbHistoryId);

    $self->save();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# validate
#
# Determines the existence of the archive.info file and if it is missing but subdirectories exist, it will attempt to recreate it.
####################################################################################################################################
sub validate
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCompressExtension,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->validate', \@_,
            {name => 'strCompressExtension'}
        );

# CSHANG TODO:
# *  get a list of the directories in the archive/<stanza> -- the above regex gets this list and nothing more:
#     DIR= 9.4-1
#     DIR= 9.5-10
#     DIR= 10.1-3
# * split the directories to version and history
# * unpack the first file into memory and extract the system id
# * update the archive info file if necessary (figure out criteria for doing so)

    # If the archive directory does not exist, it will throw and error and exit, but if it does exist and the archive.info file is
    # missing then create a valid archive info from the directories.
    if (fileExists($self->{strArchiveClusterPath}) && !$self->{bExists})
    {
        # Get the upper level directory names, e.g. 9.4-1
        foreach my $strVersionDir (fileList($self->{strArchiveClusterPath}, '^[0-9]+\.[0-9]+-[0-9]+$'))
        {
            # Get the db-version and db-id (history id) from the directory name
            my ($strDbVersion, $strDbId) = split("-", $strVersionDir);

# CSHANG can there be 000000010000000000000001-220ff32d22aa232cb1904504871678045a786cbe.SOME_OTHER_EXTENTION? I saw something about partial but not sure what that's all about. Also, what if async archiving is on? the WAL goes to a spool dir first before getting to the dir, so if we did an upgrade and the archive.info was missing an archiving is happening but not to us yet....
            # Get the name of the first archive directory
            my $strArchiveDir = (fileList($self->{strArchiveClusterPath}."/${strVersionDir}", '^[0-F]{16}$'))[0];

#CSHANG or should I be using "^[0-F]{24}(\\.partial){0,1}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$"
            # Get the name of the first archive file
            my $strArchiveFile =
                (fileList($self->{strArchiveClusterPath}."/${strVersionDir}/${strArchiveDir}",'^[0-F]{24}-[0-f]{40}.*'))[0];

            # Get the full path for the file
            my $strArchiveFilePath = $self->{strArchiveClusterPath}."/${strVersionDir}/${strArchiveDir}/${strArchiveFile}";

# CSHANG I'm not sure I like this because it appears we can't just read out the first X number of bytes. 
            my $tBlock;
            if ($strArchiveFile =~ "\.$strCompressExtension")
            {
                # can't limit with InputLength - it requires you know the size of the compressed data stream
                # gunzip $strArchiveFilePath => \$tBlock, InputLength => 100
                #     or confess "gunzip failed: $GunzipError\n";
                gunzip $strArchiveFilePath => \$tBlock
                    or confess "gunzip failed: $GunzipError\n";
            }

# CSHANG may need to put these constants in a common location
# my $iSysIdOffset = $strDbVersion >= PG_VERSION_93 ? PG_WAL_SYSTEM_ID_OFFSET_GTE_93 : PG_WAL_SYSTEM_ID_OFFSET_LT_93;
            my $iSysIdOffset = $strDbVersion >= '9.3' ? 20 : 12;
            my ($iMagic, $iFlag, $junk, $ullDbSysId) = unpack('SSa' . $iSysIdOffset . 'Q', $tBlock);

            if (!defined($ullDbSysId))
            {
                confess &log(ERROR, "unable to read database system identifier");
            }

    print "strDbVersion=$strDbVersion, ullDbSysId=$ullDbSysId, strDbId=$strDbId\n";

            $self->fileCreateUpdate($strDbVersion, $ullDbSysId, $strDbId);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}


1;
