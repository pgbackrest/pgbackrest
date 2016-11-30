####################################################################################################################################
# PROTOCOL REMOTE MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::RemoteMinion;
use parent 'pgBackRest::Protocol::CommonMinion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Archive;
use pgBackRest::Config::Config;
use pgBackRest::Db;
use pgBackRest::File;
use pgBackRest::Info;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::CommonMinion;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;                  # Class name

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,                                # Command the master process is running
        $iBufferMax,                                # Maximum buffer size
        $iCompressLevel,                            # Set compression level
        $iCompressLevelNetwork,                     # Set compression level for network only compression,
        $iProtocolTimeout                           # Protocol timeout
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strCommand'},
            {name => 'iBufferMax'},
            {name => 'iCompressLevel'},
            {name => 'iCompressNetworkLevel'},
            {name => 'iProtocolTimeout'}
        );

    # Init object and store variables
    my $self = $class->SUPER::new(CMD_REMOTE, $strCommand, $iBufferMax, $iCompressLevel,
                                  $iCompressLevelNetwork, $iProtocolTimeout);
    bless $self, $class;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# init
####################################################################################################################################
sub init
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->init');

    # Create objects
    $self->{oFile} = new pgBackRest::File
    (
        optionGet(OPTION_STANZA, false),
        optionGet(OPTION_REPO_PATH, false),
        $self
    );

    $self->{oArchive} = new pgBackRest::Archive();
    $self->{oInfo} = new pgBackRest::Info();
    $self->{oJSON} = JSON::PP->new();
    $self->{oDb} = new pgBackRest::Db();

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# commandProcess
####################################################################################################################################
sub commandProcess
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->commandProcess', \@_,
            {name => 'strCommand', trace => true},
        );

    # Copy file
    if ($strCommand eq OP_FILE_COPY || $strCommand eq OP_FILE_COPY_IN || $strCommand eq OP_FILE_COPY_OUT)
    {
        my $bResult;
        my $strChecksum;
        my $iFileSize;

        # Copy a file locally
        if ($strCommand eq OP_FILE_COPY)
        {
            ($bResult, $strChecksum, $iFileSize) = $self->{oFile}->copy(
                PATH_ABSOLUTE, $self->paramGet('source_file'),
                PATH_ABSOLUTE, $self->paramGet('destination_file'),
                $self->paramGet('source_compressed'),
                $self->paramGet('destination_compress'),
                $self->paramGet('ignore_missing_source', false),
                undef,
                $self->paramGet('mode', false),
                $self->paramGet('destination_path_create') ? 'Y' : 'N',
                $self->paramGet('user', false),
                $self->paramGet('group', false),
                $self->paramGet('append_checksum', false));
        }
        # Copy a file from STDIN
        elsif ($strCommand eq OP_FILE_COPY_IN)
        {
            ($bResult, $strChecksum, $iFileSize) = $self->{oFile}->copy(
                PIPE_STDIN, undef,
                PATH_ABSOLUTE, $self->paramGet('destination_file'),
                $self->paramGet('source_compressed'),
                $self->paramGet('destination_compress'),
                undef, undef,
                $self->paramGet('mode', false),
                $self->paramGet('destination_path_create'),
                $self->paramGet('user', false),
                $self->paramGet('group', false),
                $self->paramGet('append_checksum', false));
        }
        # Copy a file to STDOUT
        elsif ($strCommand eq OP_FILE_COPY_OUT)
        {
            ($bResult, $strChecksum, $iFileSize) = $self->{oFile}->copy(
                PATH_ABSOLUTE, $self->paramGet('source_file'),
                PIPE_STDOUT, undef,
                $self->paramGet('source_compressed'),
                $self->paramGet('destination_compress'));
        }

        $self->outputWrite(
            ($bResult ? 'Y' : 'N') . " " . (defined($strChecksum) ? $strChecksum : '?') . " " .
            (defined($iFileSize) ? $iFileSize : '?'));
    }
    # List files in a path
    elsif ($strCommand eq OP_FILE_LIST)
    {
        my $strOutput;

        foreach my $strFile ($self->{oFile}->list(
            PATH_ABSOLUTE, $self->paramGet('path'), $self->paramGet('expression', false),
            $self->paramGet('sort_order'), $self->paramGet('ignore_missing')))
        {
            $strOutput .= (defined($strOutput) ? "\n" : '') . $strFile;
        }

        $self->outputWrite($strOutput);
    }
    # Create a path
    elsif ($strCommand eq OP_FILE_PATH_CREATE)
    {
        $self->{oFile}->pathCreate(PATH_ABSOLUTE, $self->paramGet('path'), $self->paramGet('mode', false));
        $self->outputWrite();
    }
    # Check if a file/path exists
    elsif ($strCommand eq OP_FILE_EXISTS)
    {
        $self->outputWrite($self->{oFile}->exists(PATH_ABSOLUTE, $self->paramGet('path')) ? 'Y' : 'N');
    }
    # Wait
    elsif ($strCommand eq OP_FILE_WAIT)
    {
        $self->outputWrite($self->{oFile}->wait(PATH_ABSOLUTE, $self->paramGet('wait')));
    }
    # Generate a manifest
    elsif ($strCommand eq OP_FILE_MANIFEST)
    {
        my $hManifest = $self->{oFile}->manifest(PATH_ABSOLUTE, $self->paramGet('path'));
        my $strOutput = "name\ttype\tuser\tgroup\tmode\tmodification_time\tinode\tsize\tlink_destination";

        foreach my $strName (sort(keys(%{$hManifest})))
        {
            $strOutput .=
                "\n${strName}\t" .
                $hManifest->{$strName}{type} . "\t" .
                (defined($hManifest->{$strName}{user}) ? $hManifest->{$strName}{user} : "") . "\t" .
                (defined($hManifest->{$strName}{group}) ? $hManifest->{$strName}{group} : "") . "\t" .
                (defined($hManifest->{$strName}{mode}) ? $hManifest->{$strName}{mode} : "") . "\t" .
                (defined($hManifest->{$strName}{modification_time}) ?
                    $hManifest->{$strName}{modification_time} : "") . "\t" .
                (defined($hManifest->{$strName}{inode}) ? $hManifest->{$strName}{inode} : "") . "\t" .
                (defined($hManifest->{$strName}{size}) ? $hManifest->{$strName}{size} : "") . "\t" .
                (defined($hManifest->{$strName}{link_destination}) ?
                    $hManifest->{$strName}{link_destination} : "");
        }

        $self->outputWrite($strOutput);
    }
    # Archive push checks
    elsif ($strCommand eq OP_ARCHIVE_PUSH_CHECK)
    {
        my ($strArchiveId, $strChecksum) = $self->{oArchive}->pushCheck(
            $self->{oFile},
            $self->paramGet('wal-segment'),
            $self->paramGet('partial'),
            undef,
            $self->paramGet('db-version'),
            $self->paramGet('db-sys-id'));

        $self->outputWrite("${strArchiveId}\t" . (defined($strChecksum) ? $strChecksum : 'Y'));
    }
    elsif ($strCommand eq OP_ARCHIVE_GET_BACKUP_INFO_CHECK)
    {
        $self->outputWrite(
            $self->{oArchive}->getBackupInfoCheck(
                $self->{oFile},
                $self->paramGet('db-version'),
                $self->paramGet('db-control-version'),
                $self->paramGet('db-catalog-version'),
                $self->paramGet('db-sys-id')));
    }
    elsif ($strCommand eq OP_ARCHIVE_GET_CHECK)
    {
        $self->outputWrite(
            $self->{oArchive}->getCheck(
                $self->{oFile},
                $self->paramGet('db-version'),
                $self->paramGet('db-sys-id')));
    }
    elsif ($strCommand eq OP_ARCHIVE_GET_ARCHIVE_ID)
    {
        $self->outputWrite($self->{oArchive}->getArchiveId($self->{oFile}));
    }
    # Info list stanza
    elsif ($strCommand eq OP_INFO_STANZA_LIST)
    {
        $self->outputWrite($self->{oJSON}->encode($self->{oInfo}->stanzaList($self->{oFile}, $self->paramGet('stanza', false))));
    }
    elsif ($strCommand eq OP_DB_INFO)
    {
        my ($strDbVersion, $iControlVersion, $iCatalogVersion, $ullDbSysId) = $self->{oDb}->info($self->paramGet('db-path'));

        $self->outputWrite("${strDbVersion}\t${iControlVersion}\t${iCatalogVersion}\t${ullDbSysId}");
    }
    elsif ($strCommand eq OP_DB_CONNECT)
    {
        $self->outputWrite($self->{oDb}->connect());
    }
    elsif ($strCommand eq OP_DB_EXECUTE_SQL)
    {
        $self->outputWrite(
            $self->{oDb}->executeSql(
                $self->paramGet('script'),
                $self->paramGet('ignore-error', false),
                $self->paramGet('result', false)));
    }
    # Command not processed
    else
    {
        return false;
    }

    # Command processed
    return true;
}

1;
