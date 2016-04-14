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

use lib dirname($0);
use pgBackRest::Common::Log;
use pgBackRest::Common::String;
use pgBackRest::Config::Config;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_BACKUP_COMMON                                       => 'BackupCommon';

use constant OP_BACKUP_COMMON_REG_EXP_GET                           => OP_BACKUP_COMMON . '::backupRegExpGet';
use constant OP_BACKUP_COMMON_BACKUP_LABEL_FORMAT                   => OP_BACKUP_COMMON . '::backupLabelFormat';

####################################################################################################################################
# backupRegExpGet - Generate a regexp depending on the backups that need to be found
####################################################################################################################################
sub backupRegExpGet
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $bFull,
        $bDifferential,
        $bIncremental
    ) =
        logDebugParam
        (
            OP_BACKUP_COMMON_REG_EXP_GET, \@_,
            {name => 'bFull', default => false},
            {name => 'bDifferential', default => false},
            {name => 'bIncremental', default => false}
        );

    if (!$bFull && !$bDifferential && !$bIncremental)
    {
        confess &log(ASSERT, 'one parameter must be true');
    }

    my $strDateTimeRegExp = "[0-9]{8}\\-[0-9]{6}";
    my $strRegExp = '^';

    if ($bFull || $bDifferential || $bIncremental)
    {
        $strRegExp .= $strDateTimeRegExp . 'F';
    }

    if ($bDifferential || $bIncremental)
    {
        if ($bFull)
        {
            $strRegExp .= "(\\_";
        }
        else
        {
            $strRegExp .= "\\_";
        }

        $strRegExp .= $strDateTimeRegExp;

        if ($bDifferential && $bIncremental)
        {
            $strRegExp .= '(D|I)';
        }
        elsif ($bDifferential)
        {
            $strRegExp .= 'D';
        }
        else
        {
            $strRegExp .= 'I';
        }

        if ($bFull)
        {
            $strRegExp .= '){0,1}';
        }
    }

    $strRegExp .= "\$";

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
            OP_BACKUP_COMMON_BACKUP_LABEL_FORMAT, \@_,
            {name => 'strType', trace => true},
            {name => 'strBackupLabelLast', required => false, trace => true},
            {name => 'lTimestampStop', trace => true}
        );

    my $strBackupLabel;

    if ($strType eq BACKUP_TYPE_FULL)
    {
        if (defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupPathLast cannot be defined when type = ${strType}");
        }

        $strBackupLabel = timestampFileFormat(undef, $lTimestampStop) . 'F';
    }
    else
    {
        if (!defined($strBackupLabelLast))
        {
            confess &log(ASSERT, "strBackupLabelLast must be defined when type = ${strType}");
        }

        $strBackupLabel = substr($strBackupLabelLast, 0, 16);

        $strBackupLabel .= '_' . timestampFileFormat(undef, $lTimestampStop);

        if ($strType eq BACKUP_TYPE_DIFF)
        {
            $strBackupLabel .= 'D';
        }
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
