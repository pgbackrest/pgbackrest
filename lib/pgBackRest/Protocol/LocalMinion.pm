####################################################################################################################################
# PROTOCOL LOCAL MINION MODULE
####################################################################################################################################
package pgBackRest::Protocol::LocalMinion;
use parent 'pgBackRest::Protocol::CommonMinion';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::BackupFile;
use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::File;
use pgBackRest::Protocol::Common;
use pgBackRest::Protocol::CommonMinion;
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
    $self->{oFile} = new pgBackRest::File
    (
        optionGet(OPTION_STANZA),
        optionGet(OPTION_REPO_PATH),
        protocolGet(optionGet(OPTION_TYPE), optionGet(OPTION_HOST_ID), {iProcessIdx => optionGet(OPTION_PROCESS)})
    );

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

    # Backup a file
    my $bProcess = true;

    if ($strCommand eq OP_BACKUP_FILE)
    {
        my ($iCopyResult, $lCopySize, $lRepoSize, $strCopyChecksum) = backupFile(
            $self->{oFile},
            $self->paramGet(OP_PARAM_DB_FILE),
            $self->paramGet(OP_PARAM_REPO_FILE),
            $self->paramGet(OP_PARAM_DESTINATION_COMPRESS),
            $self->paramGet(OP_PARAM_CHECKSUM, false),
            $self->paramGet(OP_PARAM_MODIFICATION_TIME),
            $self->paramGet(OP_PARAM_SIZE),
            $self->paramGet(OP_PARAM_IGNORE_MISSING, false));

        $self->outputWrite(
            $iCopyResult . ($iCopyResult != BACKUP_FILE_SKIP ? "\t${lCopySize}\t${lRepoSize}\t${strCopyChecksum}" : ''));
    }
    elsif ($strCommand eq OP_RESTORE_FILE)
    {
        $self->outputWrite(
            restoreFile(
                $self->{oFile},
                $self->paramGet(&OP_PARAM_REPO_FILE),
                $self->paramGet(&OP_PARAM_DB_FILE),
                $self->paramGet(&OP_PARAM_REFERENCE, false),
                $self->paramGet(&OP_PARAM_SIZE),
                $self->paramGet(&OP_PARAM_MODIFICATION_TIME),
                $self->paramGet(&OP_PARAM_MODE),
                $self->paramGet(&OP_PARAM_USER),
                $self->paramGet(&OP_PARAM_GROUP),
                $self->paramGet(&OP_PARAM_CHECKSUM, false),
                $self->paramGet(&OP_PARAM_ZERO, false),
                $self->paramGet(&OP_PARAM_COPY_TIME_START),
                $self->paramGet(&OP_PARAM_DELTA),
                $self->paramGet(&OP_PARAM_FORCE),
                $self->paramGet(&OP_PARAM_BACKUP_PATH),
                $self->paramGet(&OP_PARAM_SOURCE_COMPRESSION)));
    }
    # Command not processed
    else
    {
        $bProcess = false;
    }

    # Send keep alive to remote
    $self->{oFile}->{oProtocol}->keepAlive();

    # Command processed
    return $bProcess;
}

1;
