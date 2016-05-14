####################################################################################################################################
# ARCHIVE INFO MODULE
####################################################################################################################################
package pgBackRest::ArchiveInfo;
use parent 'pgBackRest::Common::Ini';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename qw(dirname basename);
use File::stat;

use lib dirname($0);
use pgBackRest::Common::Exception;
use pgBackRest::Common::Ini;
use pgBackRest::Common::Log;
use pgBackRest::BackupInfo;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Manifest;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_ARCHIVE_INFO                                        => 'ArchiveInfo';

use constant OP_ARCHIVE_INFO_ARCHIVE_ID                             => OP_ARCHIVE_INFO . "->archiveId";
use constant OP_ARCHIVE_INFO_CHECK                                  => OP_ARCHIVE_INFO . "->check";
use constant OP_ARCHIVE_INFO_NEW                                    => OP_ARCHIVE_INFO . "->new";

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
            OP_ARCHIVE_INFO_NEW, \@_,
            {name => 'strArchiveClusterPath'},
            {name => 'bRequired', default => false}
        );

    # Build the archive info path/file name
    my $strArchiveInfoFile = "${strArchiveClusterPath}/" . ARCHIVE_INFO_FILE;
    my $bExists = -e $strArchiveInfoFile ? true : false;

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
# Check archive info and make it is compatible.
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
            OP_ARCHIVE_INFO_CHECK, \@_,
            {name => 'strDbVersion'},
            {name => 'ullDbSysId'}
        );

    my $bSave = false;

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
            $strError = "WAL segment system-id ${ullDbSysId} does not match archive system-id " .
                        $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID);
        }

        if (defined($strError))
        {
            confess &log(ERROR, "${strError}\nHINT: are you archiving to the correct stanza?", ERROR_ARCHIVE_MISMATCH);
        }
    }
    # Else create the info file from the current WAL segment
    else
    {
        my $iDbId = 1;

        # Fill db section
        $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_SYSTEM_ID, undef, $ullDbSysId);
        $self->set(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION, undef, $strDbVersion);
        $self->numericSet(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID, undef, $iDbId);

        # Fill db history
        $self->numericSet(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbId, INFO_ARCHIVE_KEY_DB_ID, $ullDbSysId);
        $self->set(INFO_ARCHIVE_SECTION_DB_HISTORY, $iDbId, INFO_ARCHIVE_KEY_DB_VERSION, $strDbVersion);

        $bSave = true;
    }

    # Save if changes have been made
    if ($bSave)
    {
        $self->save();
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
# Get the archive id.
####################################################################################################################################
sub archiveId
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation
    ) =
        logDebugParam
    (
        OP_ARCHIVE_INFO_ARCHIVE_ID
    );

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strArchiveId', value => $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_VERSION) . "-" .
                                          $self->get(INFO_ARCHIVE_SECTION_DB, INFO_ARCHIVE_KEY_DB_ID)}
    );
}

1;
