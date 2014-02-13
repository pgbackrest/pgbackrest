####################################################################################################################################
# UTILITY MODULE
####################################################################################################################################
package pg_backrest_utility;

use strict;
use warnings;
use Carp;
use IPC::System::Simple qw(capture);

use Exporter qw(import);

our @EXPORT = qw(data_hash_build trim common_prefix date_string_get file_size_format execute log log_file_set
                 TRACE DEBUG ERROR ASSERT WARNING INFO true false);

# Global constants
use constant
{
    true  => 1,
    false => 0
};

my $hLogFile;

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
# FILE_SIZE_FORMAT - Format file sizes in human-readable form
####################################################################################################################################
sub file_size_format
{
    my $lFileSize = shift;

    if ($lFileSize < 1024)
    {
        return $lFileSize . "B";
    }

    if ($lFileSize < (1024 * 1024))
    {
        return int($lFileSize / 1024) . "KB";
    }

    if ($lFileSize < (1024 * 1024 * 1024))
    {
        return int($lFileSize / 1024 / 1024) . "MB";
    }

    return int($lFileSize / 1024 / 1024 / 1024) . "GB";
}

####################################################################################################################################
# DATE_STRING_GET - Get the date and time string
####################################################################################################################################
sub date_string_get
{
    my $strFormat = shift;
    
    if (!defined($strFormat))
    {
        $strFormat = "%4d%02d%02d-%02d%02d%02d";
    }
    
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

    return(sprintf($strFormat, $year+1900, $mon+1, $mday, $hour, $min, $sec));
}

####################################################################################################################################
# LOG_FILE_SET - set the file messages will be logged to
####################################################################################################################################
sub log_file_set
{
    my $strFile = shift;
    
    $strFile .= "-" . date_string_get("%4d%02d%02d") . ".log";
    my $bExists = false;
    
    if (-e $strFile)
    {
        $bExists = true;
    }
    
    open($hLogFile, '>>', $strFile) or confess "unable to open log file ${strFile}";
    
    if ($bExists)
    {
        print $hLogFile "\n";
    }

    print $hLogFile "-------------------PROCESS START-------------------\n";
}

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
use constant 
{
    TRACE   => 'TRACE',
    DEBUG   => 'DEBUG',
    INFO    => 'INFO',
    WARNING => 'WARN',
    ERROR   => 'ERROR',
    ASSERT  => 'ASSERT'
};

sub log
{
    my $strLevel = shift;
    my $strMessage = shift;

    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);

    if (!defined($strMessage))
    {
        $strMessage = "(undefined)";
    }

    if ($strLevel eq "TRACE")
    {
        $strMessage = "        " . $strMessage;
    }
    elsif ($strLevel eq "DEBUG")
    {
        $strMessage = "    " . $strMessage;
    }

    $strMessage = sprintf("%4d-%02d-%02d %02d:%02d:%02d", $year+1900, $mon+1, $mday, $hour, $min, $sec) .
                  (" " x (7 - length($strLevel))) . "${strLevel}: ${strMessage}\n";

    if (defined($hLogFile))
    {
        print $hLogFile $strMessage;
    }
    
    print $strMessage;

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