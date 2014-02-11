####################################################################################################################################
# UTILITY MODULE
####################################################################################################################################
package pg_backrest_utility;

use strict;
use warnings;
use Carp;
use IPC::System::Simple qw(capture);

use Exporter qw(import);

#our @EXPORT_OK = qw(data_hash_build trim date_string_get);
our @EXPORT = qw(data_hash_build trim common_prefix date_string_get execute log
                 DEBUG ERROR ASSERT WARNING INFO true false);

# Global constants
use constant
{
    true  => 1,
    false => 0
};

####################################################################################################################################
# DATA_HASH_BUILD - Hash a delimited file with header
####################################################################################################################################
sub data_hash_build
{
    my $strData = shift;
    my $strDelimiter = shift;
    my $strUndefinedKey = shift;

    my @stryFile = split("\n", $strData);
    my @stryHeader = split($strDelimiter, $stryFile[0]);
    
    my %oHash;

    for (my $iLineIdx = 1; $iLineIdx < scalar @stryFile; $iLineIdx++)
    {
        my @stryLine = split($strDelimiter, $stryFile[$iLineIdx]);

        if (!defined($stryLine[0]) || $stryLine[0] eq "")
        {
            $stryLine[0] = $strUndefinedKey;
        }

        for (my $iColumnIdx = 1; $iColumnIdx < scalar @stryHeader; $iColumnIdx++)
        {
            if (defined($oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"}))
            {
                confess "the first column must be unique to build the hash";
            }
            
            $oHash{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} = $stryLine[$iColumnIdx];
        }
    }

    return %oHash;
}

####################################################################################################################################
# TRIM - trim whitespace off strings
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

    $strBuffer =~ s/^\s+|\s+$//g;

    return $strBuffer;
}

####################################################################################################################################
# COMMON_PREFIX
####################################################################################################################################
sub common_prefix
{
    my $strString1 = shift;
    my $strString2 = shift;
    
    my $iCommonLen = 0;
    my $iCompareLen = length($strString1) < length($strString2) ? length($strString1) : length($strString2);
    
    for (my $iIndex = 0; $iIndex < $iCompareLen; $iIndex++)
    {
        if (substr($strString1, $iIndex, 1) ne substr($strString2, $iIndex, 1))
        {
            last;
        }
        
        $iCommonLen ++;
    }

    return $iCommonLen;
}

####################################################################################################################################
# DATE_STRING_GET - Get the date and time string
####################################################################################################################################
sub date_string_get
{
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

    return(sprintf("%4d%02d%02d-%02d%02d%02d", $year+1900, $mon+1, $mday, $hour, $min, $sec));
}

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
use constant 
{
    DEBUG   => 'DEBUG',
    INFO    => 'INFO',
    WARNING => 'WARNING',
    ERROR   => 'ERROR',
    ASSERT  => 'ASSERT'
};

sub log
{
    my $strLevel = shift;
    my $strMessage = shift;

    if (!defined($strMessage))
    {
        $strMessage = "(undefined)";
    }

    print date_string_get() . ": ${strLevel}: ${strMessage}\n";

    return $strMessage;
}

####################################################################################################################################
# EXECUTE - execute a command
####################################################################################################################################
sub execute
{
    my $strCommand = shift;
    my $strOutput;

#    print("$strCommand");
    $strOutput = capture($strCommand) or confess &log(ERROR, "unable to execute command ${strCommand}: " . $_);

    return $strOutput;
}

1;