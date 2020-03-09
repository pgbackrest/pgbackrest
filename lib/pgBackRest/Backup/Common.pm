####################################################################################################################################
# BACKUP COMMON MODULE
####################################################################################################################################
package pgBackRest::Backup::Common;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename;

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Common::Wait;
use pgBackRest::Manifest;
use pgBackRest::Storage::Helper;

####################################################################################################################################
# Latest backup link constant
####################################################################################################################################
use constant LINK_LATEST                                            => 'latest';
    push @EXPORT, qw(LINK_LATEST);

use constant CFGOPTVAL_BACKUP_TYPE_FULL                             => 'full';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_FULL);
use constant CFGOPTVAL_BACKUP_TYPE_DIFF                             => 'diff';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_DIFF);
use constant CFGOPTVAL_BACKUP_TYPE_INCR                             => 'incr';
    push @EXPORT, qw(CFGOPTVAL_BACKUP_TYPE_INCR);

####################################################################################################################################
# backupRegExpGet
#
# Generate a regexp depending on the backups that need to be found.
####################################################################################################################################
sub backupRegExpGet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFull,
        $bDifferential,
        $bIncremental,
        $bAnchor
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupRegExpGet', \@_,
            {name => 'bFull', default => false},
            {name => 'bDifferential', default => false},
            {name => 'bIncremental', default => false},
            {name => 'bAnchor', default => true}
        );

    # One of the types must be selected
    if (!($bFull || $bDifferential || $bIncremental))
    {
        confess &log(ASSERT, 'at least one backup type must be selected');
    }

    # Standard regexp to match date and time formatting
    my $strDateTimeRegExp = "[0-9]{8}\\-[0-9]{6}";
    # Start the expression with the anchor if requested, date/time regexp and full backup indicator
    my $strRegExp = ($bAnchor ? '^' : '') . $strDateTimeRegExp . 'F';

    # Add the diff and/or incr expressions if requested
    if ($bDifferential || $bIncremental)
    {
        # If full requested then diff/incr is optional
        if ($bFull)
        {
            $strRegExp .= "(\\_";
        }
        # Else diff/incr is required
        else
        {
            $strRegExp .= "\\_";
        }

        # Append date/time regexp for diff/incr
        $strRegExp .= $strDateTimeRegExp;

        # Filter on both diff/incr
        if ($bDifferential && $bIncremental)
        {
            $strRegExp .= '(D|I)';
        }
        # Else just diff
        elsif ($bDifferential)
        {
            $strRegExp .= 'D';
        }
        # Else just incr
        else
        {
            $strRegExp .= 'I';
        }

        # If full requested then diff/incr is optional
        if ($bFull)
        {
            $strRegExp .= '){0,1}';
        }
    }

    # Append the end anchor if requested
    $strRegExp .= $bAnchor ? "\$" : '';

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strRegExp', value => $strRegExp}
    );
}

push @EXPORT, qw(backupRegExpGet);

####################################################################################################################################
# backupLabelFormat
#
# Format the label for a backup.
####################################################################################################################################
sub backupLabelFormat
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strType,
        $strBackupLabelLast,
        $lTimestampStart
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupLabelFormat', \@_,
            {name => 'strType', trace => true},
            {name => 'strBackupLabelLast', required => false, trace => true},
            {name => 'lTimestampTart', trace => true}
        );

    # Full backup label
    my $strBackupLabel;

    if ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL)
    {
        # Last backup label must not be defined
        if (defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must not be defined when strType = '${strType}'");
        }

        # Format the timestamp and add the full indicator
        $strBackupLabel = timestampFileFormat(undef, $lTimestampStart) . 'F';
    }
    # Else diff or incr label
    else
    {
        # Last backup label must be defined
        if (!defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must be defined when strType = '${strType}'");
        }

        # Get the full backup portion of the last backup label
        $strBackupLabel = substr($strBackupLabelLast, 0, 16);

        # Format the timestamp
        $strBackupLabel .= '_' . timestampFileFormat(undef, $lTimestampStart);

        # Add the diff indicator
        if ($strType eq CFGOPTVAL_BACKUP_TYPE_DIFF)
        {
            $strBackupLabel .= 'D';
        }
        # Else incr indicator
        else
        {
            $strBackupLabel .= 'I';
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackupLabel', value => $strBackupLabel, trace => true}
    );
}

push @EXPORT, qw(backupLabelFormat);

####################################################################################################################################
# backupLabel
#
# Get unique backup label.
####################################################################################################################################
sub backupLabel
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $oStorageRepo,
        $strRepoBackupPath,
        $strType,
        $strBackupLabelLast,
        $lTimestampStart
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupLabelFormat', \@_,
            {name => 'oStorageRepo', trace => true},
            {name => 'strRepoBackupPath', trace => true},
            {name => 'strType', trace => true},
            {name => 'strBackupLabelLast', required => false, trace => true},
            {name => 'lTimestampStart', trace => true}
        );

    # Create backup label
    my $strBackupLabel = backupLabelFormat($strType, $strBackupLabelLast, $lTimestampStart);

    # Make sure that the timestamp has not already been used by a prior backup.  This is unlikely for online backups since there is
    # already a wait after the manifest is built but it's still possible if the remote and local systems don't have synchronized
    # clocks.  In practice this is most useful for making offline testing faster since it allows the wait after manifest build to
    # be skipped by dealing with any backup label collisions here.
    if ($oStorageRepo->list(
        $strRepoBackupPath,
             {strExpression =>
                ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL ? '^' : '_') . timestampFileFormat(undef, $lTimestampStart) .
                ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL ? 'F' : '(D|I)$')}) ||
        $oStorageRepo->list(
            "${strRepoBackupPath}/" . PATH_BACKUP_HISTORY . '/' . timestampFormat('%4d', $lTimestampStart),
             {strExpression =>
                ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL ? '^' : '_') . timestampFileFormat(undef, $lTimestampStart) .
                ($strType eq CFGOPTVAL_BACKUP_TYPE_FULL ? 'F' : '(D|I)\.manifest\.gz$'),
                bIgnoreMissing => true}))
    {
        waitRemainder();
        $strBackupLabel = backupLabelFormat($strType, $strBackupLabelLast, time());
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'strBackupLabel', value => $strBackupLabel, trace => true}
    );
}

push @EXPORT, qw(backupLabel);

1;
