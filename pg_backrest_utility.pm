####################################################################################################################################
# UTILITY MODULE
####################################################################################################################################
package pg_backrest_utility;

use strict;
use warnings;
use Carp;
use IPC::System::Simple qw(capture);
use Fcntl qw(:DEFAULT :flock);

use Exporter qw(import);

our @EXPORT = qw(data_hash_build trim common_prefix wait_for_file date_string_get file_size_format execute
                 log log_file_set log_level_set
                 lock_file_create lock_file_remove
                 TRACE DEBUG ERROR ASSERT WARN INFO true false);

# Global constants
use constant
{
    true  => 1,
    false => 0
};

use constant
{
    TRACE  => 'TRACE',
    DEBUG  => 'DEBUG',
    INFO   => 'INFO',
    WARN   => 'WARN',
    ERROR  => 'ERROR',
    ASSERT => 'ASSERT',
    OFF    => 'OFF'
};

my $hLogFile;
my $strLogLevelFile = ERROR;
my $strLogLevelConsole = ERROR;
my %oLogLevelRank;

my $strLockFile;
my $hLockFile;

$oLogLevelRank{TRACE}{rank} = 6;
$oLogLevelRank{DEBUG}{rank} = 5;
$oLogLevelRank{INFO}{rank} = 4;
$oLogLevelRank{WARN}{rank} = 3;
$oLogLevelRank{ERROR}{rank} = 2;
$oLogLevelRank{ASSERT}{rank} = 1;
$oLogLevelRank{OFF}{rank} = 0;

####################################################################################################################################
# LOCK_FILE_CREATE
####################################################################################################################################
sub lock_file_create
{
    my $strLockFileParam = shift;
    
    $strLockFile = $strLockFileParam;

    if (defined($hLockFile))
    {
        confess &lock(ASSERT, "${strLockFile} lock is already held, cannot create lock ${strLockFile}");
    }

    sysopen($hLockFile, $strLockFile, O_WRONLY | O_CREAT)
        or confess &log(ERROR, "unable to open lock file ${strLockFile}");

    if (!flock($hLockFile, LOCK_EX | LOCK_NB))
    {
        close($hLockFile);
        return 0;
    }
    
    return $hLockFile;
}

####################################################################################################################################
# LOCK_FILE_REMOVE
####################################################################################################################################
sub lock_file_remove
{
    if (defined($hLockFile))
    {
        close($hLockFile);
        unlink($strLockFile) or confess &log(ERROR, "unable to remove lock file ${strLockFile}");
        
        $hLockFile = undef;
        $strLockFile = undef;
    }
    else
    {
        confess &log(ASSERT, "there is no lock to free");
    }
}

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
# WAIT_FOR_FILE
####################################################################################################################################
sub wait_for_file
{
    my $strDir = shift;
    my $strRegEx = shift;
    my $iSeconds = shift;
    
    my $lTime = time();
    my $hDir;

    while ($lTime > time() - $iSeconds)
    {
        opendir $hDir, $strDir or die "Could not open dir: $!\n";
        my @stryFile = grep(/$strRegEx/i, readdir $hDir);
        close $hDir;

        if (scalar @stryFile == 1)
        {
            return;
        }

        sleep(1);
    }

    confess &log(ERROR, "could not find $strDir/$strRegEx after $iSeconds second(s)");
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
# LOG_LEVEL_SET - set the log level for file and console
####################################################################################################################################
sub log_level_set
{
    my $strLevelFileParam = shift;
    my $strLevelConsoleParam = shift;

    if (!defined($oLogLevelRank{"${strLevelFileParam}"}{rank}))
    {
        confess &log(ERROR, "file log level ${strLevelFileParam} does not exist");
    }

    if (!defined($oLogLevelRank{"${strLevelConsoleParam}"}{rank}))
    {
        confess &log(ERROR, "console log level ${strLevelConsoleParam} does not exist");
    }

    $strLogLevelFile = $strLevelFileParam;
    $strLogLevelConsole = $strLevelConsoleParam;
}

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
sub log
{
    my $strLevel = shift;
    my $strMessage = shift;

    if (!defined($oLogLevelRank{"${strLevel}"}{rank}))
    {
        confess &log(ASSERT, "log level ${strLevel} does not exist");
    }
    
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

    if ($oLogLevelRank{"${strLevel}"}{rank} <= $oLogLevelRank{"${strLogLevelConsole}"}{rank})
    {
        print $strMessage;
    }

    if ($oLogLevelRank{"${strLevel}"}{rank} <= $oLogLevelRank{"${strLogLevelFile}"}{rank})
    {
        if (defined($hLogFile))
        {
            print $hLogFile $strMessage;
        }
    }

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