####################################################################################################################################
# BACKUP COMMON MODULE
####################################################################################################################################
package BackRest::BackupCommon;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);

use Exporter qw(import);
use File::Basename;

use lib dirname($0);
use BackRest::Utility;

####################################################################################################################################
# backupRegExpGet - Generate a regexp depending on the backups that need to be found
####################################################################################################################################
our @EXPORT = qw(backupRegExpGet);

sub backupRegExpGet
{
    my $bFull = shift;
    my $bDifferential = shift;
    my $bIncremental = shift;

    if (!$bFull && !$bDifferential && !$bIncremental)
    {
        confess &log(ERROR, 'one parameter must be true');
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

    &log(DEBUG, "BackupCommon::backupRegExpGet:" .
                " full = ${bFull}, differential = ${bDifferential}, incremental = ${bIncremental}: $strRegExp");

    return $strRegExp;
}

1;
