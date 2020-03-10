####################################################################################################################################
# COMMON STRING MODULE
####################################################################################################################################
package BackRestDoc::Common::String;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

####################################################################################################################################
# trim
#
# Trim whitespace.
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

    if (!defined($strBuffer))
    {
        return;
    }

    $strBuffer =~ s/^\s+|\s+$//g;

    return $strBuffer;
}

push @EXPORT, qw(trim);

####################################################################################################################################
# coalesce - return first defined parameter
####################################################################################################################################
sub coalesce
{
    foreach my $strParam (@_)
    {
        if (defined($strParam))
        {
            return $strParam;
        }
    }

    return;
}

push @EXPORT, qw(coalesce);

####################################################################################################################################
# timestampFormat
#
# Get standard timestamp format (or formatted as specified).
####################################################################################################################################
sub timestampFormat
{
    my $strFormat = shift;
    my $lTime = shift;

    if (!defined($strFormat))
    {
        $strFormat = '%4d-%02d-%02d %02d:%02d:%02d';
    }

    if (!defined($lTime))
    {
        $lTime = time();
    }

    my ($iSecond, $iMinute, $iHour, $iMonthDay, $iMonth, $iYear, $iWeekDay, $iYearDay, $bIsDst) = localtime($lTime);

    if ($strFormat eq "%4d")
    {
        return sprintf($strFormat, $iYear + 1900)
    }
    else
    {
        return sprintf($strFormat, $iYear + 1900, $iMonth + 1, $iMonthDay, $iHour, $iMinute, $iSecond);
    }
}

push @EXPORT, qw(timestampFormat);

####################################################################################################################################
# stringSplit
####################################################################################################################################
sub stringSplit
{
    my $strString = shift;
    my $strChar = shift;
    my $iLength = shift;

    if (length($strString) <= $iLength)
    {
        return $strString, undef;
    }

    my $iPos = index($strString, $strChar);

    if ($iPos == -1)
    {
        return $strString, undef;
    }

    my $iNewPos = $iPos;

    while ($iNewPos != -1 && $iNewPos + 1 < $iLength)
    {
        $iPos = $iNewPos;
        $iNewPos = index($strString, $strChar, $iPos + 1);
    }

    return substr($strString, 0, $iPos + 1), substr($strString, $iPos + 1);
}

push @EXPORT, qw(stringSplit);

1;
