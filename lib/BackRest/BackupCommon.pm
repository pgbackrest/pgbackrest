####################################################################################################################################
# BACKUP COMMON MODULE
####################################################################################################################################
package BackRest::BackupCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename;

use lib dirname($0);
use BackRest::Common::Log;

####################################################################################################################################
# Operation constants
####################################################################################################################################
use constant OP_BACKUP_COMMON                                       => 'BackupCommon';

use constant OP_BACKUP_COMMON_REG_EXP_GET                           => OP_BACKUP_COMMON . '::backupRegExpGet';

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

1;
