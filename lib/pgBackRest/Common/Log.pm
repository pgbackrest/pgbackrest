####################################################################################################################################
# COMMON LOG MODULE
####################################################################################################################################
package pgBackRest::Common::Log;

use strict;
use warnings FATAL => qw(all);
use Carp qw(confess longmess);

use Exporter qw(import);
    our @EXPORT = qw();
use Fcntl qw(:DEFAULT :flock);
use File::Basename qw(dirname);
use Scalar::Util qw(blessed reftype);
use Time::HiRes qw(gettimeofday usleep);

use pgBackRest::Common::Exception;
use pgBackRest::Common::String;

####################################################################################################################################
# Boolean constants
####################################################################################################################################
use constant                                                        true  => 1;
    push @EXPORT, qw(true);
use constant                                                        false => 0;
    push @EXPORT, qw(false);

####################################################################################################################################
# Log level constants
####################################################################################################################################
use constant TRACE                                                  => 'TRACE';
    push @EXPORT, qw(TRACE);
use constant DEBUG                                                  => 'DEBUG';
    push @EXPORT, qw(DEBUG);
use constant DETAIL                                                 => 'DETAIL';
    push @EXPORT, qw(DETAIL);
use constant INFO                                                   => 'INFO';
    push @EXPORT, qw(INFO);
use constant WARN                                                   => 'WARN';
    push @EXPORT, qw(WARN);
use constant PROTOCOL                                               => 'PROTOCOL';
    push @EXPORT, qw(PROTOCOL);
use constant ERROR                                                  => 'ERROR';
    push @EXPORT, qw(ERROR);
use constant ASSERT                                                 => 'ASSERT';
    push @EXPORT, qw(ASSERT);
use constant OFF                                                    => 'OFF';
    push @EXPORT, qw(OFF);

####################################################################################################################################
# Log levels ranked by severity
####################################################################################################################################
my %oLogLevelRank;

$oLogLevelRank{TRACE}{rank} = 8;
$oLogLevelRank{DEBUG}{rank} = 7;
$oLogLevelRank{DETAIL}{rank} = 6;
$oLogLevelRank{INFO}{rank} = 5;
$oLogLevelRank{WARN}{rank} = 4;
$oLogLevelRank{PROTOCOL}{rank} = 3;
$oLogLevelRank{ERROR}{rank} = 2;
$oLogLevelRank{ASSERT}{rank} = 1;
$oLogLevelRank{OFF}{rank} = 0;

####################################################################################################################################
# Module globals
####################################################################################################################################
my $hLogFile = undef;
my $strLogFileCache = undef;

my $strLogLevelFile = OFF;
my $strLogLevelConsole = OFF;
my $strLogLevelStdErr = WARN;
my $bLogTimestamp = true;

# Flags to limit banner printing until there is actual output
my $bLogFileExists;
my $bLogFileFirst;

# Allow log to be globally enabled or disabled with logEnable() and logDisable()
my $bLogDisable = 0;

# Test globals
my $bTest = false;
my $fTestDelay;
my $oTestPoint;

####################################################################################################################################
# Test constants
####################################################################################################################################
use constant TEST                                                   => 'TEST';
    push @EXPORT, qw(TEST);
use constant TEST_ENCLOSE                                           => 'PgBaCkReStTeSt';
    push @EXPORT, qw(TEST_ENCLOSE);

use constant TEST_MANIFEST_BUILD                                    => 'MANIFEST-BUILD';
    push @EXPORT, qw(TEST_MANIFEST_BUILD);
use constant TEST_BACKUP_RESUME                                     => 'BACKUP-RESUME';
    push @EXPORT, qw(TEST_BACKUP_RESUME);
use constant TEST_BACKUP_NORESUME                                   => 'BACKUP-NORESUME';
    push @EXPORT, qw(TEST_BACKUP_NORESUME);
use constant TEST_BACKUP_START                                      => 'BACKUP-START';
    push @EXPORT, qw(TEST_BACKUP_START);
use constant TEST_BACKUP_STOP                                       => 'BACKUP-STOP';
    push @EXPORT, qw(TEST_BACKUP_STOP);
use constant TEST_KEEP_ALIVE                                        => 'KEEP_ALIVE';
    push @EXPORT, qw(TEST_KEEP_ALIVE);
use constant TEST_ARCHIVE_PUSH_ASYNC_START                          => 'ARCHIVE-PUSH-ASYNC-START';
    push @EXPORT, qw(TEST_ARCHIVE_PUSH_ASYNC_START);

####################################################################################################################################
# logFileSet - set the file messages will be logged to
####################################################################################################################################
sub logFileSet
{
    my $strFile = shift;
print "sub logFileSet: LOGLEVELFILENAME $strFile, FILELOGLEVEL $strLogLevelFile\n"; # CSHANG
    # Only open the log file if file logging is enabled
    if ($strLogLevelFile ne OFF)
    {
print "sub logFileSet: DIRNAME ".dirname($strFile)."\n"; # CSHANG
        filePathCreate(dirname($strFile), '0770', true, true);

        $strFile .= '.log';
        $bLogFileExists = -e $strFile ? true : false;
        $bLogFileFirst = true;
print "sub logFileSet: LOGFILEEXISTS $bLogFileExists\n"; # CSHANG
        $hLogFile = fileOpen($strFile, O_WRONLY | O_CREAT | O_APPEND, '0660');
print "sub logFileSet: HLOGFILE ". (defined($hLogFile) ? 'defined' : 'undef') ."\n"; # CSHANG
$strLogFileCache = 'test'; # CSHANG
        # Write out anything that was cached before the file was opened
        if (defined($strLogFileCache))
        {
            logBanner();
            syswrite($hLogFile, $strLogFileCache);
print "sub logFileSet: LOGWRITTEN $hLogFile\n"; # CSHANG
            undef($strLogFileCache);
        }
    }
}

push @EXPORT, qw(logFileSet);


####################################################################################################################################
# logBanner
#
# Output a banner on the first log entry written to a file
####################################################################################################################################
sub logBanner
{
    if ($bLogFileFirst)
    {
        if ($bLogFileExists)
        {
            syswrite($hLogFile, "\n");
        }

        syswrite($hLogFile, "-------------------PROCESS START-------------------\n");
    }

    $bLogFileFirst = false;
}

####################################################################################################################################
# logLevelSet - set the log level for file and console
####################################################################################################################################
sub logLevelSet
{
    my $strLevelFileParam = shift;
    my $strLevelConsoleParam = shift;
    my $strLevelStdErrParam = shift;
    my $bLogTimestampParam = shift;

    # Load FileCommon module
    require pgBackRest::FileCommon;
    pgBackRest::FileCommon->import();
print "sub logLevelSet: FILELOGLEVEL $strLevelFileParam\n"; # CSHANG
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

    if (defined($strLevelStdErrParam))
    {
        if (!defined($oLogLevelRank{uc($strLevelStdErrParam)}{rank}))
        {
            confess &log(ERROR, "stdout log level ${strLevelStdErrParam} does not exist");
        }

        $strLogLevelStdErr = uc($strLevelStdErrParam);
    }

    if (defined($bLogTimestampParam))
    {
        $bLogTimestamp = $bLogTimestampParam;
    }
}

push @EXPORT, qw(logLevelSet);

####################################################################################################################################
# logDisable
####################################################################################################################################
sub logDisable
{
    $bLogDisable++;
}

push @EXPORT, qw(logDisable);

####################################################################################################################################
# logEnable
####################################################################################################################################
sub logEnable
{
    $bLogDisable--;
}

push @EXPORT, qw(logEnable);

####################################################################################################################################
# logDebugParam
#
# Log parameters passed to functions.
####################################################################################################################################
use constant DEBUG_PARAM                                            => '()';

sub logDebugParam
{
    my $strFunction = shift;
    my $oyParamRef = shift;

    return logDebugProcess($strFunction, DEBUG_PARAM, undef, $oyParamRef, @_);
}

push @EXPORT, qw(logDebugParam);

####################################################################################################################################
# logDebugReturn
#
# Log values returned from functions.
####################################################################################################################################
use constant DEBUG_RETURN                                           => '=>';

sub logDebugReturn
{
    my $strFunction = shift;

    return logDebugProcess($strFunction, DEBUG_RETURN, undef, undef, @_);
}

push @EXPORT, qw(logDebugReturn);

####################################################################################################################################
# logDebugMisc
#
# Log misc values and details during execution.
####################################################################################################################################
use constant DEBUG_MISC                                             => '';

sub logDebugMisc
{
    my $strFunction = shift;
    my $strDetail = shift;

    return logDebugProcess($strFunction, DEBUG_MISC, $strDetail, undef, @_);
}

push @EXPORT, qw(logDebugMisc);

####################################################################################################################################
# logDebugProcess
####################################################################################################################################
sub logDebugProcess
{
    my $strFunction = shift;
    my $strType = shift;
    my $strDetail = shift;
    my $oyParamRef = shift;

    my $iIndex = 0;
    my $oParamHash = {};
    my @oyResult;
    my $bLogTrace = true;

    if ($strType eq DEBUG_PARAM)
    {
        push @oyResult, $strFunction;
    }

    # Process each parameter hash
    my $oParam = shift;

    # Strip the package name off strFunction if it's pgBackRest
    $strFunction =~ s/^pgBackRest[^\:]*\:\://;

    while (defined($oParam))
    {
        my $strParamName = $$oParam{name};
        my $oValue;

        # Push the return value into the return value array
        if ($strType eq DEBUG_PARAM)
        {
            if (defined($$oyParamRef[$iIndex]))
            {
                push(@oyResult, $$oyParamRef[$iIndex]);
            }
            else
            {
                push(@oyResult, $${oParam}{default});
                $$oParamHash{$strParamName}{default} = true;
            }

            $oValue = $oyResult[@oyResult - 1];

            if (!defined($oValue) && (!defined($${oParam}{required}) || $${oParam}{required}))
            {
                confess &log(ASSERT, "${strParamName} is required in ${strFunction}");
            }
        }
        else
        {
            if (ref($$oParam{value}) eq 'ARRAY')
            {
                if (defined($$oParam{ref}) && $$oParam{ref})
                {
                    push(@oyResult, $$oParam{value});
                }
                else
                {
                    push(@oyResult, @{$$oParam{value}});
                }
            }
            else
            {
                push(@oyResult, $$oParam{value});
            }

            $oValue = $$oParam{value};
        }

        if (!defined($$oParam{log}) || $$oParam{log})
        {

            # If the parameter is a hash but not blessed then represent it as a string
            # ??? This should go away once the inputs to logDebug can be changed
            if (ref($oValue) eq 'HASH' && !blessed($oValue))
            {
                $$oParamHash{$strParamName}{value} = '[hash]';
            }
            # Else log the parameter value exactly
            else
            {
                $$oParamHash{$strParamName}{value} = $oValue;
            }

            # There are certain return values that it's wasteful to generate debug logging for
            if (!($strParamName eq 'self') &&
                (!defined($$oParam{trace}) || !$$oParam{trace}))
            {
                $bLogTrace = false;
            }
        }

        # Get the next parameter hash
        $oParam = shift;
        $iIndex++;
    }

    if (defined($strDetail) && $iIndex == 0)
    {
        $bLogTrace = false;
    }

    logDebugOut($strFunction, $strType, $strDetail, $oParamHash, $bLogTrace ? TRACE : DEBUG);

    # If there are one or zero return values then just return a scalar (this will be undef if there are no return values)
    if (@oyResult == 1)
    {
        return $oyResult[0];
    }

    # Else return an array containing return values
    return @oyResult;
}

####################################################################################################################################
# logDebugBuild
####################################################################################################################################
sub logDebugBuild
{
    my $strValue = shift;

    my $rResult;

    # Value is undefined
    if (!defined($strValue))
    {
        $rResult = \'[undef]';
    }
    # Value is not a ref, but return it as a ref for efficiency
    elsif (!ref($strValue))
    {
        $rResult = \$strValue;
    }
    # Value is a hash
    elsif (ref($strValue) eq 'HASH')
    {
        my $strValueHash;

        for my $strSubValue (sort(keys(%{$strValue})))
        {
            $strValueHash .=
                (defined($strValueHash) ? ', ' : '{') . "${strSubValue} => " . ${logDebugBuild($strValue->{$strSubValue})};
        }

        $rResult = \(defined($strValueHash) ?  $strValueHash . '}' : '{}');
    }
    # Value is an array
    elsif (ref($strValue) eq 'ARRAY')
    {
        my $strValueArray;

        for my $strSubValue (@{$strValue})
        {
            $strValueArray .= (defined($strValueArray) ? ', ' : '(') . ${logDebugBuild($strSubValue)};
        }

        $rResult = \(defined($strValueArray) ?  $strValueArray . ')' : '()');
    }
    # Else some other type ??? For the moment this is forced to object to not make big log changes
    else
    {
        $rResult = \('[object]');
    }

    return $rResult;
}

push @EXPORT, qw(logDebugBuild);

####################################################################################################################################
# logDebugOut
####################################################################################################################################
use constant DEBUG_STRING_MAX_LEN                                   => 1024;

sub logDebugOut
{
    my $strFunction = shift;
    my $strType = shift;
    my $strMessage = shift;
    my $oParamHash = shift;
    my $strLevel = shift;

    $strLevel = defined($strLevel) ? $strLevel : DEBUG;

    if ($oLogLevelRank{$strLevel}{rank} <= $oLogLevelRank{$strLogLevelConsole}{rank} ||
        $oLogLevelRank{$strLevel}{rank} <= $oLogLevelRank{$strLogLevelFile}{rank} ||
        $oLogLevelRank{$strLevel}{rank} <= $oLogLevelRank{$strLogLevelStdErr}{rank})
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

                my $strValueRef = defined($oParamHash->{$strParam}{value}) ? logDebugBuild($oParamHash->{$strParam}{value}) : undef;
                my $bDefault =
                    defined($$strValueRef) && defined($$oParamHash{$strParam}{default}) ? $$oParamHash{$strParam}{default} : false;

                $strParamSet .= "${strParam} = " .
                                ($bDefault ? '<' : '') .
                                (defined($$strValueRef) ?
                                    ($strParam =~ /^(b|is)/ ? ($$strValueRef ? 'true' : 'false'):
                                    (length($$strValueRef) > DEBUG_STRING_MAX_LEN ?
                                     substr($$strValueRef, 0, DEBUG_STRING_MAX_LEN) . ' ... <truncated>':
                                     $$strValueRef)) : '[undef]') .
                                ($bDefault ? '>' : '');
            }

            if (defined($strMessage))
            {
                $strMessage = $strMessage . (defined($strParamSet) ? ": ${strParamSet}" : '');
            }
            else
            {
                $strMessage = $strParamSet;
            }
        }

        &log($strLevel, "${strFunction}${strType}" . (defined($strMessage) ? ": $strMessage" : ''));
    }
}

####################################################################################################################################
# logException
####################################################################################################################################
sub logException
{
    my $oException = shift;

    return &log($oException->level(), $oException->message(), $oException->code(), undef, undef, undef, $oException->extra());
}

push @EXPORT, qw(logException);

####################################################################################################################################
# logErrorResult
####################################################################################################################################
sub logErrorResult
{
    my $iCode = shift;
    my $strMessage = shift;
    my $strResult = shift;

    confess &log(ERROR, $strMessage . (defined($strResult) ? ": $strResult" : ''), $iCode);
}

push @EXPORT, qw(logErrorResult);

####################################################################################################################################
# LOG - log messages
####################################################################################################################################
sub log
{
    my $strLevel = shift;
    my $strMessage = shift;
    my $iCode = shift;
    my $bSuppressLog = shift;
    my $iIndent = shift;
    my $iProcessId = shift;
    my $rExtra = shift;

    # Set defaults
    $bSuppressLog = defined($bSuppressLog) ? $bSuppressLog : false;

    # Initialize rExtra
    if (!defined($rExtra))
    {
        $rExtra =
        {
            bLogFile => false,
            bLogConsole => false,
        };
    }

    # Set operational variables
    my $strMessageFormat = $strMessage;
    my $iLogLevelRank = $oLogLevelRank{"${strLevel}"}{rank};

    # If test message
    if ($strLevel eq TEST)
    {
        if (!defined($oTestPoint) || !defined($$oTestPoint{lc($strMessage)}))
        {
            return;
        }

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

    # Set the error code
    if ($strLevel eq ASSERT)
    {
        $iCode = ERROR_ASSERT;
    }
    elsif ($strLevel eq ERROR && !defined($iCode))
    {
        $iCode = ERROR_UNKNOWN;
    }

    $strMessageFormat = (defined($iCode) ? "[${iCode}]: " : '') . $strMessageFormat;

    # Indent subsequent lines of the message if it has more than one line - makes the log more readable
    if (defined($iIndent))
    {
        my $strIndent = ' ' x $iIndent;
        $strMessageFormat =~ s/\n/\n${strIndent}/g;
    }
    else
    {
        # Indent subsequent message lines so they align
        $bLogTimestamp ?
            $strMessageFormat =~ s/\n/\n                                        /g :
            $strMessageFormat =~ s/\n/\n            /g
    }

    # Indent TRACE and debug levels so they are distinct from normal messages
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

    # Format the message text
    my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime(time);

    $strMessageFormat =
        ($bLogTimestamp ? timestampFormat() . sprintf('.%03d ', (gettimeofday() - int(gettimeofday())) * 1000) : '') .
        sprintf('P%02d', defined($iProcessId) ? $iProcessId : 0) .
        (' ' x (7 - length($strLevel))) . "${strLevel}: ${strMessageFormat}\n";
print "sub log: LOG DISABLE $bLogDisable\n"; # CSHANG
    # Skip output if disabled
    if (!$bLogDisable)
    {
        # Output to stderr depending on log level
        if (!$rExtra->{bLogConsole} && $iLogLevelRank <= $oLogLevelRank{$strLogLevelStdErr}{rank})
        {
            if ($strLogLevelStdErr ne PROTOCOL)
            {
                syswrite(*STDERR, $strLevel . (defined($iCode) ? " [${iCode}]" : '') . ': ');
            }

            syswrite(*STDERR, "${strMessage}\n");
            $rExtra->{bLogConsole} = true;
        }
        # Else output to stdout depending on log level and test flag
        elsif (!$rExtra->{bLogConsole} && $iLogLevelRank <= $oLogLevelRank{$strLogLevelConsole}{rank} ||
               $bTest && $strLevel eq TEST)
        {
            if (!$bSuppressLog)
            {
                syswrite(*STDOUT, $strMessageFormat);
            }

            # If in test mode and this is a test messsage then delay so the calling process has time to read the message
            if ($bTest && $strLevel eq TEST && $fTestDelay > 0)
            {
                usleep($fTestDelay * 1000000);
            }

            $rExtra->{bLogConsole} = true;
        }
print "sub log: LOGLOGFILE ".(defined($rExtra->{bLogLogFile}) ? $rExtra->{bLogLogFile} : 'undef') .", RANK ". (defined($iLogLevelRank) ? $iLogLevelRank : 'undef'). " LOGLEVELFILERANK ".$oLogLevelRank{$strLogLevelFile}{rank}." HLOGFILE: ".(defined($hLogFile) ? $hLogFile : 'undef')." SUPPRESSLOG $bSuppressLog\n"; # CSHANG
        # Output to file depending on log level and test flag
        if (!$rExtra->{bLogLogFile} && $iLogLevelRank <= $oLogLevelRank{$strLogLevelFile}{rank})
        {
            if (defined($hLogFile) || (defined($strLogLevelFile) && $strLogLevelFile ne OFF))
            {
                if (!$bSuppressLog)
                {
                    if (defined($hLogFile))
                    {
                        logBanner();
                        syswrite($hLogFile, $strMessageFormat);
                    }
                    else
                    {
                        $strLogFileCache .= $strMessageFormat;
print "sub log: LOGFILECACHE $strLogFileCache\n";
                    }

                    if ($strLevel eq ASSERT ||
                        ($strLevel eq ERROR && ($strLogLevelFile eq DEBUG || $strLogLevelFile eq TRACE)))
                    {
                        my $strStackTrace = longmess() . "\n";
                        $strStackTrace =~ s/\n/\n                                   /g;

                        if (defined($hLogFile))
                        {
                            syswrite($hLogFile, $strStackTrace);
                        }
                        else
                        {
                            $strLogFileCache .= $strStackTrace;
                        }
                    }
                }
            }

            $rExtra->{bLogFile} = true;
        }
    }

    # Throw a typed exception if code is defined
    if (defined($iCode))
    {
        return new pgBackRest::Common::Exception($strLevel, $iCode, $strMessage, longmess(), $rExtra);
    }

    # Return the message test so it can be used in a confess
    return $strMessage;
}

push @EXPORT, qw(log);

####################################################################################################################################
# testSet
#
# Set test parameters.
####################################################################################################################################
sub testSet
{
    my $bTestParam = shift;
    my $fTestDelayParam = shift;
    my $oTestPointParam = shift;

    # Set defaults
    $bTest = defined($bTestParam) ? $bTestParam : false;
    $fTestDelay = defined($bTestParam) ? $fTestDelayParam : $fTestDelay;
    $oTestPoint = $oTestPointParam;

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

push @EXPORT, qw(testSet);

####################################################################################################################################
# testCheck - Check for a test message
####################################################################################################################################
sub testCheck
{
    my $strLog = shift;
    my $strTest = shift;

    return index($strLog, TEST_ENCLOSE . '-' . $strTest . '-' . TEST_ENCLOSE) != -1;
}

push @EXPORT, qw(testCheck);

####################################################################################################################################
# logFileCacheClear - Clear the log file cache for testing
####################################################################################################################################
sub logFileCacheClear
{
    undef($strLogFileCache);
}

push @EXPORT, qw(logFileCacheClear);

####################################################################################################################################
# logFileCacheTest - Test for a string in the log file
####################################################################################################################################
sub logFileCacheTest
{
    my $strTest = shift;
print "$strLogFileCache";
    return (($strLogFileCache =~ m/$strTest/) ? true : false);
}

push @EXPORT, qw(logFileCacheTest);

1;
