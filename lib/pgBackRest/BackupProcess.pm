####################################################################################################################################
# PROTOCOL LOCAL GROUP MODULE
####################################################################################################################################
package pgBackRest::BackupProcess;
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
    (my $strOperation) = logDebugParam(__PACKAGE__ . '->new');

    my $self = $class->SUPER::new(DB);
    bless $self, $class;

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

    my $strResult = $oLocal->outputRead(true);
    $hJob->{iCopyResult} = (split("\t", $strResult))[0];

    if ($hJob->{iCopyResult} != BACKUP_FILE_SKIP)
    {
        $hJob->{lCopySize} = (split("\t", $strResult))[1];
        $hJob->{lRepoSize} = (split("\t", $strResult))[2];
        $hJob->{strCopyChecksum} = (split("\t", $strResult))[3];
    }

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

    $oLocal->cmdWrite(OP_BACKUP_FILE, $hJob->{hPayload});

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

####################################################################################################################################
# queueBackup
#
# Queue a file for backup.
####################################################################################################################################
sub queueBackup
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $iHostConfigIdx,
        $strQueue,
        $strKey,
        $strDbFile,
        $strRepoFile,
        $bDestinationCompress,
        $lModificationTime,
        $lSize,
        $strChecksum,
        $bIgnoreMissing,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->queueBackup', \@_,
            {name => 'iHostConfigIdx'},
            {name => 'strQueue'},
            {name => 'strKey'},
            {name => &OP_PARAM_DB_FILE},
            {name => &OP_PARAM_REPO_FILE},
            {name => &OP_PARAM_DESTINATION_COMPRESS},
            {name => &OP_PARAM_MODIFICATION_TIME},
            {name => &OP_PARAM_SIZE},
            {name => &OP_PARAM_CHECKSUM, required => false},
            {name => &OP_PARAM_IGNORE_MISSING, required => false},
        );

    $self->queueJob(
        $iHostConfigIdx,
        $strQueue,
        $strKey,
        {
            &OP_PARAM_DB_FILE => $strDbFile,
            &OP_PARAM_REPO_FILE => $strRepoFile,
            &OP_PARAM_DESTINATION_COMPRESS => $bDestinationCompress,
            &OP_PARAM_CHECKSUM => $strChecksum,
            &OP_PARAM_MODIFICATION_TIME => $lModificationTime,
            &OP_PARAM_SIZE => $lSize,
            &OP_PARAM_IGNORE_MISSING => $bIgnoreMissing,
        });

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
}

1;
