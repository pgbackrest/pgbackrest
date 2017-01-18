####################################################################################################################################
# PROTOCOL REMOTE MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::RemoteMinion;
use parent 'pgBackRest::Protocol::CommonMinion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use File::Basename qw(dirname);

use pgBackRest::BackupFile;
use pgBackRest::Common::Log;
use pgBackRest::Archive::ArchiveGet;
use pgBackRest::Archive::ArchivePush;
use pgBackRest::Check::Check;
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
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA, false),
        optionGet(OPTION_REPO_PATH, false),
        $self
    );

    my $oArchivePush = new pgBackRest::Archive::ArchivePush();
    my $oArchiveGet = new pgBackRest::Archive::ArchiveGet();
    my $oCheck = new pgBackRest::Check::Check();
    my $oInfo = new pgBackRest::Info();
    my $oDb = new pgBackRest::Db();

    # Create anonymous subs for each command
    my $hCommandMap =
    {
        # ArchiveGet commands
        &OP_ARCHIVE_GET_ARCHIVE_ID => sub {$oArchiveGet->getArchiveId($oFile)},
        &OP_ARCHIVE_GET_CHECK => sub {$oArchiveGet->getCheck($oFile, @{shift()})},

        # ArchivePush commands
        &OP_ARCHIVE_PUSH_CHECK => sub {$oArchivePush->pushCheck($oFile, @{shift()})},

        # Check commands
        &OP_CHECK_BACKUP_INFO_CHECK => sub {$oCheck->backupInfoCheck($oFile, @{shift()})},

        # Db commands
        &OP_DB_CONNECT => sub {$oDb->connect()},
        &OP_DB_EXECUTE_SQL => sub {$oDb->executeSql(@{shift()})},
        &OP_DB_INFO => sub {$oDb->info(@{shift()})},

        # File commands
        &OP_FILE_COPY => sub {my $rParam = shift; $oFile->copy(PATH_ABSOLUTE, shift(@{$rParam}), PATH_ABSOLUTE, @{$rParam})},
        &OP_FILE_COPY_IN => sub {my $rParam = shift; $oFile->copy(PIPE_STDIN, shift(@{$rParam}), PATH_ABSOLUTE, @{$rParam})},
        &OP_FILE_COPY_OUT => sub {my $rParam = shift; $oFile->copy(PATH_ABSOLUTE, shift(@{$rParam}), PIPE_STDOUT, @{$rParam})},
        &OP_FILE_EXISTS => sub {$oFile->exists(PATH_ABSOLUTE, @{shift()})},
        &OP_FILE_LIST => sub {$oFile->list(PATH_ABSOLUTE, @{shift()})},
        &OP_FILE_MANIFEST => sub {$oFile->manifest(PATH_ABSOLUTE, @{shift()})},
        &OP_FILE_PATH_CREATE => sub {$oFile->pathCreate(PATH_ABSOLUTE, @{shift()})},
        &OP_FILE_WAIT => sub {$oFile->wait(PATH_ABSOLUTE, @{shift()})},

        # Info commands
        &OP_INFO_STANZA_LIST => sub {$oInfo->stanzaList($oFile, @{shift()})},
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCommandMap', value => $hCommandMap}
    );
}

1;
