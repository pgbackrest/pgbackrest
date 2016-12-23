####################################################################################################################################
# BACKUP COMMON MODULE
####################################################################################################################################
package pgBackRest::BackupCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename;

use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;

####################################################################################################################################
# Latest backup link constant
####################################################################################################################################
use constant LINK_LATEST                                            => OPTION_DEFAULT_RESTORE_SET;
    push @EXPORT, qw(LINK_LATEST);

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

    # Standard regexp to match date and time formattting
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
        $lTimestampStop
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::backupLabelFormat', \@_,
            {name => 'strType', trace => true},
            {name => 'strBackupLabelLast', required => false, trace => true},
            {name => 'lTimestampStop', trace => true}
        );

    # Full backup label
    my $strBackupLabel;

    if ($strType eq BACKUP_TYPE_FULL)
    {
        # Last backup label must not be defined
        if (defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must not be defined when strType = '${strType}'");
        }

        # Format the timestamp and add the full indicator
        $strBackupLabel = timestampFileFormat(undef, $lTimestampStop) . 'F';
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
        $strBackupLabel .= '_' . timestampFileFormat(undef, $lTimestampStop);

        # Add the diff indicator
        if ($strType eq BACKUP_TYPE_DIFF)
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

1;
