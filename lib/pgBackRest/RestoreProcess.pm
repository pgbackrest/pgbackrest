####################################################################################################################################
# PROTOCOL LOCAL GROUP MODULE
####################################################################################################################################
package pgBackRest::RestoreProcess;
use parent 'pgBackRest::Protocol::LocalProcess';

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use pgBackRest::BackupFile;
use pgBackRest::Common::Log;
use pgBackRest::Config::Config;
use pgBackRest::Protocol::Common;
use pgBackRest::Version;

####################################################################################################################################
# CONSTRUCTOR
####################################################################################################################################
sub new
{
    my $class = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iProcessMax,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->new', \@_,
            {name => 'iProcessMax', trace => true},
        );

    my $self = $class->SUPER::new(BACKUP);
    bless $self, $class;

    $self->hostAdd(1, $iProcessMax);

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self}
    );
}

####################################################################################################################################
# cmdResult
#
# Get the backup file result.
####################################################################################################################################
sub cmdResult
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oLocal,
        $hJob,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->cmdResult', \@_,
            {name => 'oLocal', trace => true},
            {name => 'hJob', trace => true},
        );

    $hJob->{bCopy} = $oLocal->outputRead(true);

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# cmdSend
#
# Send the backup file command.
####################################################################################################################################
sub cmdSend
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oLocal,
        $hJob,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->cmdSend', \@_,
            {name => 'oLocal', trace => true},
            {name => 'hJob', trace => true},
        );

    $oLocal->cmdWrite(OP_RESTORE_FILE, $hJob->{hPayload});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# queueRestore
#
# Queue a file for restore.
####################################################################################################################################
sub queueRestore
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strQueue,
        $strKey,
        $strRepoFile,
        $strDbFile,
        $strReference,
        $lSize,
        $lModificationTime,
        $strMode,
        $strUser,
        $strGroup,
        $strChecksum,
        $bZero,
        $lCopyTimeStart,
        $bDelta,
        $bForce,
        $strBackupPath,
        $bSourceCompression,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->queueBackup', \@_,
            {name => 'strQueue'},
            {name => 'strKey'},
            {name => &OP_PARAM_REPO_FILE},
            {name => &OP_PARAM_DB_FILE},
            {name => &OP_PARAM_REFERENCE, required => false},
            {name => &OP_PARAM_SIZE},
            {name => &OP_PARAM_MODIFICATION_TIME},
            {name => &OP_PARAM_MODE},
            {name => &OP_PARAM_USER},
            {name => &OP_PARAM_GROUP},
            {name => &OP_PARAM_CHECKSUM, required => false},
            {name => &OP_PARAM_ZERO, required => false, default => false},
            {name => &OP_PARAM_COPY_TIME_START},
            {name => &OP_PARAM_DELTA},
            {name => &OP_PARAM_FORCE},
            {name => &OP_PARAM_BACKUP_PATH},
            {name => &OP_PARAM_SOURCE_COMPRESSION},
        );

    $self->queueJob(
        1,
        $strQueue,
        $strKey,
        {
            &OP_PARAM_REPO_FILE => $strRepoFile,
            &OP_PARAM_DB_FILE => $strDbFile,
            &OP_PARAM_REFERENCE => $strReference,
            &OP_PARAM_SIZE => $lSize,
            &OP_PARAM_MODIFICATION_TIME => $lModificationTime,
            &OP_PARAM_MODE => $strMode,
            &OP_PARAM_USER => $strUser,
            &OP_PARAM_GROUP => $strGroup,
            &OP_PARAM_CHECKSUM => $strChecksum,
            &OP_PARAM_ZERO => $bZero,
            &OP_PARAM_COPY_TIME_START => $lCopyTimeStart,
            &OP_PARAM_DELTA => $bDelta,
            &OP_PARAM_FORCE => $bForce,
            &OP_PARAM_BACKUP_PATH => $strBackupPath,
            &OP_PARAM_SOURCE_COMPRESSION => $bSourceCompression,
        });

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
