####################################################################################################################################
# ARCHIVE INFO MODULE
#
# The archive.info file is created when archiving begins. It is located under the stanza directory. The file contains information
# regarding the stanza database version, database WAL segment system id and other information to ensure that archiving is being
# performed on the proper database.
####################################################################################################################################
package pgBackRest::Archive::ArchiveInfo;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;
use Fcntl qw(O_RDONLY);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::Archive::ArchiveCommon;
use pgBackRest::Config::Config;
use pgBackRest::DbVersion;
use pgBackRest::File;
use pgBackRest::FileCommon;
use pgBackRest::InfoCommon;
use pgBackRest::Manifest;

####################################################################################################################################
# File/path constants
####################################################################################################################################
use constant ARCHIVE_INFO_FILE                                      => 'archive.info';
    our @EXPORT = qw(ARCHIVE_INFO_FILE);

####################################################################################################################################
# Archive info constants
####################################################################################################################################
use constant INFO_ARCHIVE_SECTION_DB                                => INFO_BACKUP_SECTION_DB;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB);
use constant INFO_ARCHIVE_SECTION_DB_HISTORY                        => INFO_BACKUP_SECTION_DB_HISTORY;
    push @EXPORT, qw(INFO_ARCHIVE_SECTION_DB_HISTORY);

use constant INFO_ARCHIVE_KEY_DB_VERSION                            => MANIFEST_KEY_DB_VERSION;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_VERSION);
use constant INFO_ARCHIVE_KEY_DB_ID                                 => MANIFEST_KEY_DB_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_ID);
use constant INFO_ARCHIVE_KEY_DB_SYSTEM_ID                          => MANIFEST_KEY_SYSTEM_ID;
    push @EXPORT, qw(INFO_ARCHIVE_KEY_DB_SYSTEM_ID);

####################################################################################################################################
# Global variables
####################################################################################################################################
my $strArchiveInfoMissingMsg =
    ARCHIVE_INFO_FILE . " does not exist but is required to push/get WAL segments\n" .
    "HINT: is archive_command configured in postgresql.conf?\n" .
    "HINT: has a stanza-create been performed?\n" .
    "HINT: use --no-archive-check to disable archive checks during backup if you have an alternate archiving scheme.";

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
            {name => 'bRequired', default => true}
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = fileExists($strArchiveInfoFile);

    if (!$bExists && $bRequired)
    {
        confess &log(ERROR, $strArchiveInfoMissingMsg, ERROR_FILE_MISSING);
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
# not exist an error will occur.
####################################################################################################################################
sub check
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $bRequired,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->check', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'bRequired', default => true},
        );

    # ??? remove bRequired after stanza-upgrade
    if ($bRequired)
    {
        # Confirm the info file exists with the DB section
        $self->confirmExists();
    }

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
# create
#
# Creates the archive.info file. WARNING - this file should only be called from stanza-create.
####################################################################################################################################
sub create
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDbVersion,
        $ullDbSysId,
        $bSave,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->create', \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'},
            {name => 'bSave', default => true},
        );

    # Fill db section and db history section
    $self->dbSectionSet($strDbVersion, $ullDbSysId, $self->dbHistoryIdGet(false));

    if ($bSave)
    {
        $self->save();
    }

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# reconstruct
#
# Reconstruct the info file from the existing directory and files
####################################################################################################################################
sub reconstruct
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oFile,
        $strCurrentDbVersion,
        $ullCurrentDbSysId,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->reconstruct', \@_,
        {name => 'oFile'},
        {name => 'strCurrentDbVersion'},
        {name => 'ullCurrentDbSysId'},
    );

    my $strInvalidFileStructure = undef;

    # Remove any existing DB or History information
    if ($self->test(INFO_ARCHIVE_SECTION_DB))
    {
        $self->remove(INFO_ARCHIVE_SECTION_DB);
    }
    if ($self->test(INFO_ARCHIVE_SECTION_DB_HISTORY))
    {
        $self->remove(INFO_ARCHIVE_SECTION_DB_HISTORY);
    }

    my @stryArchiveId = fileList($self->{strArchiveClusterPath}, REGEX_ARCHIVE_DIR_DB_VERSION, 'forward', true);
    my %hDbHistoryVersion;

    # Get the db-version and db-id (history id) from the upper level directory names, e.g. 9.4-1
    foreach my $strArchiveId (@stryArchiveId)
    {
        my ($strDbVersion, $iDbHistoryId) = split("-", $strArchiveId);
        $hDbHistoryVersion{$iDbHistoryId} = $strDbVersion;
    }

    # Loop through the DBs in the order they were created as indicated by the db-id so that the last one is set in the db section
    foreach my $iDbHistoryId (sort {$a <=> $b} keys %hDbHistoryVersion)
    {
        my $strDbVersion = $hDbHistoryVersion{$iDbHistoryId};

        # Get the name of the first archive directory
        my $strArchiveDir =
            (fileList($self->{strArchiveClusterPath}."/${strVersionDir}", REGEX_ARCHIVE_DIR_WAL, 'forward', true))[0];

        # Continue if any file structure or missing files info
        if (!defined($strArchiveDir))
        {
            $strInvalidFileStructure = "found empty directory " . $self->{strArchiveClusterPath} . "/${strVersionDir}";
            next;
        }

        # ??? Should probably make a function in ArchiveCommon
        my $strArchiveFile =
            (fileList($self->{strArchiveClusterPath} . "/${strVersionDir}/${strArchiveDir}",
            "^[0-F]{24}(\\.partial){0,1}(-[0-f]+){0,1}(\\.$oFile->{strCompressExtension}){0,1}\$", 'forward', true))[0];

        # Continue if any file structure or missing files info
        if (!defined($strArchiveFile))
        {
            $strInvalidFileStructure =
                "found empty directory " . $self->{strArchiveClusterPath} . "/${strVersionDir}/${strArchiveDir}";
            next;
        }

        # Get the full path for the file
        my $strArchiveFilePath = $self->{strArchiveClusterPath}."/${strVersionDir}/${strArchiveDir}/${strArchiveFile}";

        # Get the db-system-id from the WAL file depending on the version of postgres
        my $iSysIdOffset = $strDbVersion >= PG_VERSION_93 ? PG_WAL_SYSTEM_ID_OFFSET_GTE_93 : PG_WAL_SYSTEM_ID_OFFSET_LT_93;

        # If the file is a compressed file, unzip it, else open the first 8KB
        my $tBlock;

        sysopen(my $hFile, $strArchiveFilePath, O_RDONLY)
            or confess &log(ERROR, "unable to open ${strArchiveFilePath}", ERROR_FILE_OPEN);

        if ($strArchiveFile =~ "^.*\.$oFile->{strCompressExtension}\$")
        {
            gunzip $hFile => \$tBlock
                or confess &log(ERROR,
                    "gunzip failed with error: " . $GunzipError .
                    " on file ${strArchiveFilePath}", ERROR_FILE_READ);
        }
        else
        {
            # Read part of the file
            sysread($hFile, $tBlock, 8192) == 8192
                or confess &log(ERROR, "unable to read ${strArchiveFilePath}", ERROR_FILE_READ);
        }

        close($hFile);

        # Get the required data from the file that was pulled into scalar $tBlock
        my ($iMagic, $iFlag, $junk, $ullDbSysId) = unpack('SSa' . $iSysIdOffset . 'Q', $tBlock);

        if (!defined($ullDbSysId))
        {
            confess &log(ERROR, "unable to read database system identifier", ERROR_FILE_READ);
        }

        # ??? For stanza-upgrade, need to test that the history does not already exist
        # Fill db section and db history section
        $self->dbSectionSet($strDbVersion, $ullDbSysId, $iDbHistoryId);
    }

    # If the DB section does not exist, then there were no valid directories to read from so create the DB and History sections.
    if (!$self->test(INFO_ARCHIVE_SECTION_DB))
    {
        $self->create($strCurrentDbVersion, $ullCurrentDbSysId, false);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strInvalidFileStructure', value => $strInvalidFileStructure}
    );
}

####################################################################################################################################
# dbHistoryIdGet
#
# Get the db history ID
####################################################################################################################################
sub dbHistoryIdGet
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFileRequired,
    ) =
        logDebugParam
    (
        __PACKAGE__ . '->dbHistoryIdGet', \@_,
        {name => 'bFileRequired', default => true},
    );

    # Confirm the info file exists if it is required
    if ($bFileRequired)
    {
        $self->confirmExists();
    }

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
# dbHistoryList
#
# Get the data from the db history section.
####################################################################################################################################
sub dbHistoryList
{
    my $self = shift;
    my
    (
        $strOperation,
    ) = logDebugParam
        (
            __PACKAGE__ . '->dbHistoryList',
        );

    my %hDbHash;

    foreach my $iHistoryId ($self->keys(INFO_ARCHIVE_SECTION_DB_HISTORY))
    {
        $hDbHash{$iHistoryId}{&INFO_DB_VERSION} =
            $self->get(INFO_ARCHIVE_SECTION_DB_HISTORY, $iHistoryId, INFO_ARCHIVE_KEY_DB_VERSION);
        $hDbHash{$iHistoryId}{&INFO_SYSTEM_ID} =
            $self->get(INFO_ARCHIVE_SECTION_DB_HISTORY, $iHistoryId, INFO_ARCHIVE_KEY_DB_ID);
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hDbHash', value => \%hDbHash}
    );
}

####################################################################################################################################
# dbSectionSet
#
# Set the db and db:history sections.
####################################################################################################################################
sub dbSectionSet
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
            __PACKAGE__ . '->dbSectionSet', \@_,
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
# confirmExists
#
# Ensure that the archive.info file and the db section exist.
####################################################################################################################################
sub confirmExists
{
    my $self = shift;

    # Confirm the file exists and the DB section is filled out
    if (!$self->test(INFO_ARCHIVE_SECTION_DB) || !$self->{bExists})
    {
        confess &log(ERROR, $strArchiveInfoMissingMsg, ERROR_FILE_MISSING);
    }
}

1;
