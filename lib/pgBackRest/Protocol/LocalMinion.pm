####################################################################################################################################
# PROTOCOL LOCAL MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::LocalMinion;
use parent 'pgBackRest::Protocol::CommandMinion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::BackupFile;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::CommandMinion;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::Protocol;
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
        protocolGet(optionGet(OPTION_TYPE), optionGet(OPTION_HOST_ID), {iProcessIdx => optionGet(OPTION_PROCESS)})
    );

    # Create anonymous subs for each command
    my $hCommandMap =
    {
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
