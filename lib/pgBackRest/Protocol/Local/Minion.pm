####################################################################################################################################
# PROTOCOL LOCAL MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::Local::Minion;
use parent 'pgBackRest::Protocol::Command::Minion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::Archive::ArchivePushFile;
use pgBackRest::Backup::File;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::Command::Minion;
use pgBackRest::Protocol::Common::Common;
use pgBackRest::Protocol::Helper;
use pgBackRest::RestoreFile;

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
        $strCommand,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'strCommand'},
        );

    # Init object and store variables
    my $self = $class->SUPER::new(
        CMD_LOCAL, $strCommand, optionGet(OPTION_BUFFER_SIZE), optionGet(OPTION_COMPRESS_LEVEL),
        optionGet(OPTION_COMPRESS_LEVEL_NETWORK), optionGet(OPTION_PROTOCOL_TIMEOUT));
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

    # Create the file object
    my $oFile = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(
            optionGet(OPTION_TYPE), optionGet(OPTION_HOST_ID),
            {iProcessIdx => optionGet(OPTION_PROCESS), strCommand => optionGet(OPTION_COMMAND)})
    );

    # Create anonymous subs for each command
    my $hCommandMap =
    {
        &OP_ARCHIVE_PUSH_FILE => sub {archivePushFile($oFile, @{shift()})},
        &OP_BACKUP_FILE => sub {backupFile($oFile, @{shift()})},
        &OP_RESTORE_FILE => sub {restoreFile($oFile, @{shift()})},

        # To be run after each command to keep the remote alive
        &OP_POST => sub {$oFile->{oProtocol}->keepAlive()},
    };

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'hCommandMap', value => $hCommandMap}
    );
}

1;
