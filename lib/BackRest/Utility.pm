####################################################################################################################################
# UTILITY MODULE
####################################################################################################################################
package BackRest::Utility;

use threads;
use strict;
use warnings;
use Carp;

use Fcntl qw(:DEFAULT :flock);
use File::Path qw(remove_tree);
use File::Basename;
use JSON;

use lib dirname($0) . '/../lib';
use BackRest::Exception;

use Exporter qw(import);

our @EXPORT = qw(version_get
                 data_hash_build trim common_prefix wait_for_file file_size_format execute
                 log log_file_set log_level_set test_set test_check
                 lock_file_create lock_file_remove
                 config_save config_load timestamp_string_get timestamp_file_string_get
                 TRACE DEBUG ERROR ASSERT WARN INFO OFF true false
                 TEST TEST_ENCLOSE TEST_MANIFEST_BUILD);

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

####################################################################################################################################
# TEST Constants and Variables
####################################################################################################################################
use constant
{
    TEST                => 'TEST',
    TEST_ENCLOSE        => 'PgBaCkReStTeSt',
    TEST_MANIFEST_BUILD => 'MANIFEST_BUILD'
};

# Test global variables
my $bTest = false;
my $iTestDelay;

####################################################################################################################################
# VERSION_GET
####################################################################################################################################
my $strVersion;

sub version_get
{
    my $hVersion;
    my $strVersion;

    if (!open($hVersion, '<', dirname($0) . '/../VERSION'))
    {
        confess &log(ASSERT, 'unable to open VERSION file');
    }

    if (!($strVersion = readline($hVersion)))
    {
        confess &log(ASSERT, 'unable to read VERSION file');
    }

    close($hVersion);

    return trim($strVersion);
}

####################################################################################################################################
# LOCK_FILE_CREATE
####################################################################################################################################
sub lock_file_create
{
    my $strLockPathParam = shift;

    my $strLockFile = $strLockPathParam . '/process.lock';

    if (defined($hLockFile))
    {
        confess &lock(ASSERT, "${strLockFile} lock is already held");
    }

    $strLockPath = $strLockPathParam;

    unless (-e $strLockPath)
    {
        if (system("mkdir -p ${strLockPath}") != 0)
        {
            confess &log(ERROR, "Unable to create lock path ${strLockPath}");
        }
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

        remove_tree($strLockPath) or confess &log(ERROR, "unable to delete lock path ${strLockPath}");

        $hLockFile = undef;
        $strLockPath = undef;
    }
    else
    {
        confess &log(ASSERT, 'there is no lock to free');
    }
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
        opendir $hDir, $strDir
            or confess &log(ERROR, "Could not open path ${strDir}: $!\n");

        my @stryFile = grep(/$strRegEx/i, readdir $hDir);
        close $hDir;

        if (scalar @stryFile == 1)
        {
            return;
        }

        sleep(1);
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
        return int($lFileSize / 1024) . 'KB';
    }

    if ($lFileSize < (1024 * 1024 * 1024))
    {
        return int($lFileSize / 1024 / 1024) . 'MB';
    }

    return int($lFileSize / 1024 / 1024 / 1024) . 'GB';
}

####################################################################################################################################
# TIMESTAMP_STRING_GET - Get backrest standard timestamp (or formatted as specified
####################################################################################################################################
sub timestamp_string_get
{
    my $strFormat = shift;

    if (!defined($strFormat))
    {
        $strFormat = '%4d-%02d-%02d %02d:%02d:%02d';
    }

    my ($iSecond, $iMinute, $iHour, $iMonthDay, $iMonth, $iYear, $iWeekDay, $iYearDay, $bIsDst) = localtime(time);

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
        print $hLogFile "\n";
    }

    print $hLogFile "-------------------PROCESS START-------------------\n";
}

####################################################################################################################################
# TEST_SET - set test parameters
####################################################################################################################################
sub test_set
{
    my $bTestParam = shift;
    my $iTestDelayParam = shift;

    # Set defaults
    $bTest = defined($bTestParam) ? $bTestParam : false;
    $iTestDelay = defined($bTestParam) ? $iTestDelayParam : $iTestDelay;

    # Make sure that a delay is specified in test mode
    if ($bTest && !defined($iTestDelay))
    {
        confess &log(ASSERT, 'iTestDelay must be provided when bTest is true');
    }

    # Test delay should be between 1 and 600 seconds
    if (!($iTestDelay >= 1 && $iTestDelay <= 600))
    {
        confess &log(ERROR, 'test-delay must be between 1 and 600 seconds');
    }
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
        if (!defined($oLogLevelRank{"${strLevelFileParam}"}{rank}))
        {
            confess &log(ERROR, "file log level ${strLevelFileParam} does not exist");
        }

        $strLogLevelFile = $strLevelFileParam;
    }

    if (defined($strLevelConsoleParam))
    {
        if (!defined($oLogLevelRank{"${strLevelConsoleParam}"}{rank}))
        {
            confess &log(ERROR, "console log level ${strLevelConsoleParam} does not exist");
        }

        $strLogLevelConsole = $strLevelConsoleParam;
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

    # Indent subsequent lines of the message if it has more than one line - makes the log more readable
    if ($strLevel eq TRACE || $strLevel eq TEST)
    {
        $strMessageFormat =~ s/\n/\n                                           /g;
        $strMessageFormat = '        ' . $strMessageFormat;
    }
    elsif ($strLevel eq DEBUG)
    {
        $strMessageFormat =~ s/\n/\n                                       /g;
        $strMessageFormat = '    ' . $strMessageFormat;
    }
    else
    {
        $strMessageFormat =~ s/\n/\n                                   /g;
    }

    # Format the message text
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);

    $strMessageFormat = timestamp_string_get() . sprintf(' T%02d', threads->tid()) .
                        (' ' x (7 - length($strLevel))) . "${strLevel}: ${strMessageFormat}" .
                        (defined($iCode) ? " (code ${iCode})" : '') . "\n";

    # Output to console depending on log level and test flag
    if ($iLogLevelRank <= $oLogLevelRank{"${strLogLevelConsole}"}{rank} ||
        $bTest && $strLevel eq TEST)
    {
        if (!$bSuppressLog)
        {
            print $strMessageFormat;
        }

        if ($bTest && $strLevel eq TEST)
        {
            *STDOUT->flush();
            sleep($iTestDelay);
        }
    }

    # Output to file depending on log level and test flag
    if ($iLogLevelRank <= $oLogLevelRank{"${strLogLevelFile}"}{rank})
    {
        if (defined($hLogFile))
        {
            if (!$bSuppressLog)
            {
                print $hLogFile $strMessageFormat;
            }
        }
    }

    # Throw a typed exception if code is defined
    if (defined($iCode))
    {
        return new BackRest::Exception($iCode, $strMessage);
    }

    # Return the message test so it can be used in a confess
    return $strMessage;
}

####################################################################################################################################
# CONFIG_LOAD
#
# Load configuration file from standard INI format to a hash.
####################################################################################################################################
sub config_load
{
    my $strFile = shift;    # Full path to config file to load from
    my $oConfig = shift;    # Reference to the hash where config data will be stored

    # Open the config file for reading
    my $hFile;
    my $strSection;

    open($hFile, '<', $strFile)
        or confess &log(ERROR, "unable to open ${strFile}");

    while (my $strLine = readline($hFile))
    {
        $strLine = trim($strLine);

        if ($strLine ne '')
        {
            # Get the section
            if (index($strLine, '[') == 0)
            {
                $strSection = substr($strLine, 1, length($strLine) - 2);
            }
            else
            {
                # Get key and value
                my $iIndex = index($strLine, '=');

                if ($iIndex == -1)
                {
                    confess &log(ERROR, "unable to read from ${strFile}: ${strLine}");
                }

                my $strKey = substr($strLine, 0, $iIndex);
                my $strValue = substr($strLine, $iIndex + 1);

                # Try to store value as JSON
                eval
                {
                    ${$oConfig}{"${strSection}"}{"${strKey}"} = decode_json($strValue);
                };

                # On error store value as a scalar
                if ($@)
                {
                    ${$oConfig}{"${strSection}"}{"${strKey}"} = $strValue;
                }
            }
        }
    }

    close($hFile);
}

####################################################################################################################################
# CONFIG_SAVE
#
# Save configuration file from a hash to standard INI format.
####################################################################################################################################
sub config_save
{
    my $strFile = shift;    # Full path to config file to save to
    my $oConfig = shift;    # Reference to the hash where config data is stored

    # Open the config file for writing
    my $hFile;
    my $bFirst = true;

    open($hFile, '>', $strFile)
        or confess &log(ERROR, "unable to open ${strFile}");

    foreach my $strSection (sort(keys $oConfig))
    {
        if (!$bFirst)
        {
            syswrite($hFile, "\n")
                or confess "unable to write lf: $!";
        }

        syswrite($hFile, "[${strSection}]\n")
            or confess "unable to write section ${strSection}: $!";

        foreach my $strKey (sort(keys ${$oConfig}{"${strSection}"}))
        {
            my $strValue = ${$oConfig}{"${strSection}"}{"${strKey}"};

            if (defined($strValue))
            {
                if (ref($strValue) eq "HASH")
                {
                    syswrite($hFile, "${strKey}=" . encode_json($strValue) . "\n")
                        or confess "unable to write key ${strKey}: $!";
                }
                else
                {
                    syswrite($hFile, "${strKey}=${strValue}\n")
                        or confess "unable to write key ${strKey}: $!";
                }
            }
        }

        $bFirst = false;
    }

    close($hFile);
}

1;
