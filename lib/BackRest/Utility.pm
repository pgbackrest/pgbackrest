####################################################################################################################################
# UTILITY MODULE
####################################################################################################################################
package BackRest::Utility;

use threads;
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

use Cwd qw(abs_path);
use Exporter qw(import);
use Fcntl qw(:DEFAULT :flock);
use File::Basename;
use File::Path qw(remove_tree);
use JSON::PP;
use POSIX qw(ceil);
use Time::HiRes qw(gettimeofday usleep);

use lib dirname($0) . '/../lib';
use BackRest::Exception;

our @EXPORT = qw(version_get
                 data_hash_build trim common_prefix file_size_format execute
                 log log_file_set log_level_set test_set test_get test_check
                 hsleep wait_remainder
                 timestamp_string_get timestamp_file_string_get
                 TRACE DEBUG ERROR ASSERT WARN INFO OFF true false
                 TEST TEST_ENCLOSE TEST_MANIFEST_BUILD TEST_BACKUP_RESUME TEST_BACKUP_NORESUME FORMAT);

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

my $strLockPath;
my $hLockFile;

$oLogLevelRank{TRACE}{rank} = 6;
$oLogLevelRank{DEBUG}{rank} = 5;
$oLogLevelRank{INFO}{rank} = 4;
$oLogLevelRank{WARN}{rank} = 3;
$oLogLevelRank{ERROR}{rank} = 2;
$oLogLevelRank{ASSERT}{rank} = 1;
$oLogLevelRank{OFF}{rank} = 0;

# Construct the version file name
my $strVersionFile = abs_path(dirname($0) . '/../VERSION');

####################################################################################################################################
# FORMAT Constant
#
# Identified the format of the manifest and file structure.  The format is used to determine compatability between versions.
####################################################################################################################################
use constant FORMAT => 4;

####################################################################################################################################
# TEST Constants and Variables
####################################################################################################################################
use constant
{
    TEST                    => 'TEST',
    TEST_ENCLOSE            => 'PgBaCkReStTeSt',

    TEST_MANIFEST_BUILD     => 'MANIFEST_BUILD',
    TEST_BACKUP_RESUME      => 'BACKUP_RESUME',
    TEST_BACKUP_NORESUME    => 'BACKUP_NORESUME',
};

# Test global variables
my $bTest = false;
my $fTestDelay;

####################################################################################################################################
# VERSION_GET
####################################################################################################################################
my $strVersion;

sub version_get
{
    my $hVersion;

    # If version is already stored then return it (should never change during execution)
    if (defined($strVersion))
    {
        return $strVersion;
    }

    # Open the file
    if (!open($hVersion, '<', $strVersionFile))
    {
        confess &log(ASSERT, "unable to open VERSION file: ${strVersionFile}");
    }

    # Read version and trim
    if (!($strVersion = readline($hVersion)))
    {
        confess &log(ASSERT, "unable to read VERSION file: ${strVersionFile}");
    }

    $strVersion = trim($strVersion);

    # Close file
    close($hVersion);

    return $strVersion;
}

####################################################################################################################################
# WAIT_REMAINDER - Wait the remainder of the current second
####################################################################################################################################
sub wait_remainder
{
    my $lTimeBegin = gettimeofday();
    my $lSleepMs = ceil(((int($lTimeBegin) + 1.05) - $lTimeBegin) * 1000);

    usleep($lSleepMs * 1000);

    &log(TRACE, "WAIT_REMAINDER: slept ${lSleepMs}ms: begin ${lTimeBegin}, end " . gettimeofday());

    return int($lTimeBegin);
}

####################################################################################################################################
# DATA_HASH_BUILD - Hash a delimited file with header
####################################################################################################################################
sub data_hash_build
{
    my $oHashRef = shift;
    my $strData = shift;
    my $strDelimiter = shift;
    my $strUndefinedKey = shift;

    my @stryFile = split("\n", $strData);
    my @stryHeader = split($strDelimiter, $stryFile[0]);

    for (my $iLineIdx = 1; $iLineIdx < scalar @stryFile; $iLineIdx++)
    {
        my @stryLine = split($strDelimiter, $stryFile[$iLineIdx]);

        if (!defined($stryLine[0]) || $stryLine[0] eq '')
        {
            $stryLine[0] = $strUndefinedKey;
        }

        for (my $iColumnIdx = 1; $iColumnIdx < scalar @stryHeader; $iColumnIdx++)
        {
            if (defined(${$oHashRef}{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"}))
            {
                confess 'the first column must be unique to build the hash';
            }

            if (defined($stryLine[$iColumnIdx]) && $stryLine[$iColumnIdx] ne '')
            {
                ${$oHashRef}{"$stryHeader[0]"}{"$stryLine[0]"}{"$stryHeader[$iColumnIdx]"} = $stryLine[$iColumnIdx];
            }
        }
    }
}

####################################################################################################################################
# TRIM - trim whitespace off strings
####################################################################################################################################
sub trim
{
    my $strBuffer = shift;

    if (!defined($strBuffer))
    {
        return undef;
    }

    $strBuffer =~ s/^\s+|\s+$//g;

    return $strBuffer;
}

####################################################################################################################################
# hsleep - wrapper for usleep that takes seconds in fractions and returns time slept in ms
####################################################################################################################################
sub hsleep
{
    my $fSecond = shift;

    return usleep($fSecond * 1000000);
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
        if (opendir($hDir, $strDir))
        {
            my @stryFile = grep(/$strRegEx/i, readdir $hDir);
            close $hDir;

            if (scalar @stryFile == 1)
            {
                return;
            }
        }

        hsleep(.1);
    }

    confess &log(ERROR, "could not find $strDir/$strRegEx after ${iSeconds} second(s)");
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

        $iCommonLen++;
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
        return $lFileSize . 'B';
    }

    if ($lFileSize < (1024 * 1024))
    {
        return (int($lFileSize / 102.4) / 10) . 'KB';
    }

    if ($lFileSize < (1024 * 1024 * 1024))
    {
        return (int($lFileSize / 1024 / 102.4) / 10) . 'MB';
    }

    return (int($lFileSize / 1024 / 1024 / 102.4) / 10) . 'GB';
}

####################################################################################################################################
# TIMESTAMP_STRING_GET - Get backrest standard timestamp (or formatted as specified
####################################################################################################################################
sub timestamp_string_get
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

    return sprintf($strFormat, $iYear + 1900, $iMonth + 1, $iMonthDay, $iHour, $iMinute, $iSecond);
}

####################################################################################################################################
# TIMESTAMP_FILE_STRING_GET - Get the date and time string formatted for filenames
####################################################################################################################################
sub timestamp_file_string_get
{
    return timestamp_string_get('%4d%02d%02d-%02d%02d%02d');
}

####################################################################################################################################
# LOG_FILE_SET - set the file messages will be logged to
####################################################################################################################################
sub log_file_set
{
    my $strFile = shift;

    unless (-e dirname($strFile))
    {
        mkdir(dirname($strFile)) or die "unable to create directory for log file ${strFile}";
    }

    $strFile .= '-' . timestamp_string_get('%4d%02d%02d') . '.log';
    my $bExists = false;

    if (-e $strFile)
    {
        $bExists = true;
    }

    open($hLogFile, '>>', $strFile) or confess "unable to open log file ${strFile}";

    if ($bExists)
    {
        syswrite($hLogFile, "\n");
    }

    syswrite($hLogFile, "-------------------PROCESS START-------------------\n");
}

####################################################################################################################################
# TEST_SET - set test parameters
####################################################################################################################################
sub test_set
{
    my $bTestParam = shift;
    my $fTestDelayParam = shift;

    # Set defaults
    $bTest = defined($bTestParam) ? $bTestParam : false;
    $fTestDelay = defined($bTestParam) ? $fTestDelayParam : $fTestDelay;

    # Make sure that a delay is specified in test mode
    if ($bTest && !defined($fTestDelay))
    {
        confess &log(ASSERT, 'iTestDelay must be provided when bTest is true');
    }

    # Test delay should be between 1 and 600 seconds
    if (!($fTestDelay >= 0 && $fTestDelay <= 600))
    {
        confess &log(ERROR, 'test-delay must be between 1 and 600 seconds');
    }
}

####################################################################################################################################
# TEST_GET - are we in test mode?
####################################################################################################################################
sub test_get
{
    return $bTest;
}

####################################################################################################################################
# LOG_LEVEL_SET - set the log level for file and console
####################################################################################################################################
sub log_level_set
{
    my $strLevelFileParam = shift;
    my $strLevelConsoleParam = shift;

    if (defined($strLevelFileParam))
    {
        if (!defined($oLogLevelRank{uc($strLevelFileParam)}{rank}))
        {
            confess &log(ERROR, "file log level ${strLevelFileParam} does not exist");
        }

        $strLogLevelFile = uc($strLevelFileParam);
    }

    if (defined($strLevelConsoleParam))
    {
        if (!defined($oLogLevelRank{uc($strLevelConsoleParam)}{rank}))
        {
            confess &log(ERROR, "console log level ${strLevelConsoleParam} does not exist");
        }

        $strLogLevelConsole = uc($strLevelConsoleParam);
    }
}

####################################################################################################################################
# TEST_CHECK - Check for a test message
####################################################################################################################################
sub test_check
{
    my $strLog = shift;
    my $strTest = shift;

    return index($strLog, TEST_ENCLOSE . '-' . $strTest . '-' . TEST_ENCLOSE) != -1;
}

####################################################################################################################################
# logDebug
####################################################################################################################################
use constant DEBUG_CALL                                             => '()';
    push @EXPORT, qw(DEBUG_CALL);
use constant DEBUG_RESULT                                           => '=>';
    push @EXPORT, qw(DEBUG_RESULT);
use constant DEBUG_MISC                                             => '';
    push @EXPORT, qw(DEBUG_MISC);

sub logDebug
{
    my $strFunction = shift;
    my $strType = shift;
    my $strMessage = shift;
    my $oParamHash = shift;
    my $strLevel = shift;

    $strLevel = defined($strLevel) ? $strLevel : DEBUG;

    if ($oLogLevelRank{$strLevel}{rank} <= $oLogLevelRank{$strLogLevelConsole}{rank} ||
        $oLogLevelRank{$strLevel}{rank} <= $oLogLevelRank{$strLogLevelFile}{rank})
    {
        if (defined($oParamHash))
        {
            my $strParamSet;

            foreach my $strParam (sort(keys(%$oParamHash)))
            {
                if (defined($strParamSet))
                {
                    $strParamSet .= ', ';
                }

                $strParamSet .= "${strParam} = " .
                                (defined($$oParamHash{$strParam}) ?
                                    ($strParam =~ /^is/ ? ($$oParamHash{$strParam} ? 'true' : 'false'):
                                    $$oParamHash{$strParam}) : '[undef]');
            }

            if (defined($strMessage))
            {
                $strMessage = "${strMessage}: ${strParamSet}";
            }
            else
            {
                $strMessage = $strParamSet;
            }
        }

        &log($strLevel, "${strFunction}${strType}" . (defined($strMessage) ? ": $strMessage" : ''));
    }
}

push @EXPORT, qw(logDebug);

sub logTrace
{
    my $strFunction = shift;
    my $strType = shift;
    my $strMessage = shift;
    my $oParamHash = shift;

    logDebug($strFunction, $strType, $strMessage, $oParamHash, TRACE);
}

push @EXPORT, qw(logTrace);

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
sub log
{
    my $strLevel = shift;
    my $strMessage = shift;
    my $iCode = shift;
    my $bSuppressLog = shift;

    # Set defaults
    $bSuppressLog = defined($bSuppressLog) ? $bSuppressLog : false;

    # Set operational variables
    my $strMessageFormat = $strMessage;
    my $iLogLevelRank = $oLogLevelRank{"${strLevel}"}{rank};

    # If test message
    if ($strLevel eq TEST)
    {
        $iLogLevelRank = $oLogLevelRank{TRACE}{rank} + 1;
        $strMessageFormat = TEST_ENCLOSE . '-' . $strMessageFormat . '-' . TEST_ENCLOSE;
    }
    # Else level rank must be valid
    elsif (!defined($iLogLevelRank))
    {
        confess &log(ASSERT, "log level ${strLevel} does not exist");
    }

    # If message was undefined then set default message
    if (!defined($strMessageFormat))
    {
        $strMessageFormat = '(undefined)';
    }

    $strMessageFormat = (defined($iCode) ? "[${iCode}]: " : '') . $strMessageFormat;

    # Indent subsequent lines of the message if it has more than one line - makes the log more readable
    $strMessageFormat =~ s/\n/\n                                    /g;

    if ($strLevel eq TRACE || $strLevel eq TEST)
    {
        $strMessageFormat =~ s/\n/\n        /g;
        $strMessageFormat = '        ' . $strMessageFormat;
    }
    elsif ($strLevel eq DEBUG)
    {
        $strMessageFormat =~ s/\n/\n    /g;
        $strMessageFormat = '    ' . $strMessageFormat;
    }
    elsif ($strLevel eq ERROR)
    {
        if (!defined($iCode))
        {
            $iCode = ERROR_UNKNOWN;
        }

        $strMessageFormat =~ s/\n/\n       /g;
    }

    # Format the message text
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);

    $strMessageFormat = timestamp_string_get() . sprintf('.%03d T%02d', (gettimeofday() - int(gettimeofday())) * 1000,
                        threads->tid()) .
                        (' ' x (7 - length($strLevel))) . "${strLevel}: ${strMessageFormat}\n";

    # Output to console depending on log level and test flag
    if ($iLogLevelRank <= $oLogLevelRank{"${strLogLevelConsole}"}{rank} ||
        $bTest && $strLevel eq TEST)
    {
        if (!$bSuppressLog)
        {
            syswrite(*STDOUT, $strMessageFormat);
        }

        # If in test mode and this is a test messsage then delay so the calling process has time to read the message
        if ($bTest && $strLevel eq TEST && $fTestDelay > 0)
        {
            hsleep($fTestDelay);
        }
    }

    # Output to file depending on log level and test flag
    if ($iLogLevelRank <= $oLogLevelRank{"${strLogLevelFile}"}{rank})
    {
        if (defined($hLogFile))
        {
            if (!$bSuppressLog)
            {
                syswrite($hLogFile, $strMessageFormat);

                if ($strLevel eq ASSERT ||
                    ($strLevel eq ERROR && ($strLogLevelFile eq DEBUG || $strLogLevelFile eq TRACE)))
                {
                    my $strStackTrace = longmess() . "\n";
                    $strStackTrace =~ s/\n/\n                                   /g;

                    syswrite($hLogFile, $strStackTrace);
                }
            }
        }
    }

    # Throw a typed exception if code is defined
    if (defined($iCode))
    {
        return new BackRest::Exception($iCode, $strMessage, longmess());
    }

    # Return the message test so it can be used in a confess
    return $strMessage;
}

####################################################################################################################################
####################################################################################################################################
# Wait Functions
####################################################################################################################################
####################################################################################################################################
push @EXPORT, qw(waitInit waitMore waitInterval);

####################################################################################################################################
# waitInit
####################################################################################################################################
sub waitInit
{
    my $fWaitTime = shift;
    my $fSleep = shift;

    # Declare oWait hash
    my $oWait = {};

    # If wait seconds is not defined or 0 then return undef
    if (!defined($fWaitTime) || $fWaitTime == 0)
    {
        return undef;
    }

    # Wait seconds can be a minimum of .1
    if ($fWaitTime < .1)
    {
        confess &log(ASSERT, 'fWaitTime cannot be < .1');
    }

    # If fSleep is not defined set it
    if (!defined($fSleep))
    {
        if ($fWaitTime >= 1)
        {
            $$oWait{sleep} = .1;
        }
        else
        {
            $$oWait{sleep} = $fWaitTime / 10;
        }
    }
    # Else make sure it's not greater than fWaitTime
    else
    {
        # Make sure fsleep is less than fWaitTime
        if ($fSleep >= $fWaitTime)
        {
            confess &log(ASSERT, 'fSleep > fWaitTime - this is useless');
        }
    }

    # Set variables
    $$oWait{wait_time} = $fWaitTime;
    $$oWait{time_begin} = gettimeofday();
    $$oWait{time_end} = $$oWait{time_begin};

    return $oWait;
}

####################################################################################################################################
# waitMore
####################################################################################################################################
sub waitMore
{
    my $oWait = shift;

    # Return if oWait is not defined
    if (!defined($oWait))
    {
        return false;
    }

    # Sleep for fSleep time
    hsleep($$oWait{sleep});

    # Capture the end time
    $$oWait{time_end} = gettimeofday();

    # Calculate the new sleep time
    $$oWait{sleep} = $$oWait{sleep} * 2 < $$oWait{wait_time} - ($$oWait{time_end} - $$oWait{time_begin}) ?
                         $$oWait{sleep} * 2 : ($$oWait{wait_time} - ($$oWait{time_end} - $$oWait{time_begin})) + .001;

    if ((gettimeofday() - $$oWait{time_begin}) < $$oWait{wait_time})
    {
        return true;
    }

    return false;
}


####################################################################################################################################
# waitInterval
####################################################################################################################################
sub waitInterval
{
    my $oWait = shift;

    # Error if oWait is not defined
    if (!defined($oWait))
    {
        confess &log("fWaitTime was not defined in waitInit");
    }

    return int(($$oWait{time_end} - $$oWait{time_begin}) * 1000) / 1000;
}

1;
