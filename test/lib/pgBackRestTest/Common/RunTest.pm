####################################################################################################################################
# RunTest.pm - All tests are inherited from this object
####################################################################################################################################
package pgBackRestTest::Common::RunTest;

####################################################################################################################################
# Perl includes
####################################################################################################################################
use strict;
use warnings FATAL => qw(all);
use Carp qw(confess);
use English '-no_match_vars';

use Exporter qw(import);
    our @EXPORT = qw();
use File::Basename qw(dirname);

use pgBackRest::Version;

use BackRestDoc::Common::Exception;
use BackRestDoc::Common::Log;
use BackRestDoc::Common::String;

use pgBackRestTest::Common::BuildTest;
use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::LogTest;
use pgBackRestTest::Common::Storage;
use pgBackRestTest::Common::StoragePosix;
use pgBackRestTest::Common::VmTest;
use pgBackRestTest::Common::Wait;

####################################################################################################################################
# Constant to use when bogus data is required
####################################################################################################################################
use constant BOGUS =>                                               'bogus';
    push @EXPORT, qw(BOGUS);

####################################################################################################################################
# The current test run that is executing.  Only a single run should ever occur in a process to prevent various cleanup issues from
# affecting the next run.  Of course multiple subtests can be executed in a single run.
####################################################################################################################################
my $oTestRun;
my $oStorage;

####################################################################################################################################
# new
####################################################################################################################################
sub new
{
    my $class = shift;          # Class name

    # Create the class hash
    my $self = {};
    bless $self, $class;

    # Assign function parameters, defaults, and log debug info
    my ($strOperation) = logDebugParam(__PACKAGE__ . '->new');

    # Initialize run counter
    $self->{iRun} = 0;

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# initModule
#
# Empty init sub in case the ancestor class does not declare one.
####################################################################################################################################
sub initModule {}

####################################################################################################################################
# initTest
#
# Empty init sub in case the ancestor class does not declare one.
####################################################################################################################################
sub initTest {}

####################################################################################################################################
# cleanTest
#
# Delete all files in test directory.
####################################################################################################################################
sub cleanTest
{
    my $self = shift;

    executeTest('rm -rf ' . $self->testPath() . '/*');
}

####################################################################################################################################
# cleanModule
#
# Empty final sub in case the ancestor class does not declare one.
####################################################################################################################################
sub cleanModule {}

####################################################################################################################################
# process
####################################################################################################################################
sub process
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    (
        my $strOperation,
        $self->{strVm},
        $self->{iVmId},
        $self->{strBasePath},
        $self->{strTestPath},
        $self->{strBackRestExe},
        $self->{strBackRestExeHelper},
        $self->{strPgBinPath},
        $self->{strPgVersion},
        $self->{strModule},
        $self->{strModuleTest},
        $self->{iyModuleTestRun},
        $self->{bOutput},
        $self->{bDryRun},
        $self->{bCleanup},
        $self->{bLogForce},
        $self->{strLogLevelTestFile},
        $self->{strPgUser},
        $self->{strGroup},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->process', \@_,
            {name => 'strVm'},
            {name => 'iVmId'},
            {name => 'strBasePath'},
            {name => 'strTestPath'},
            {name => 'strBackRestExe'},
            {name => 'strBackRestExeHelper'},
            {name => 'strPgBinPath', required => false},
            {name => 'strPgVersion', required => false},
            {name => 'strModule'},
            {name => 'strModuleTest'},
            {name => 'iModuleTestRun', required => false},
            {name => 'bOutput'},
            {name => 'bDryRun'},
            {name => 'bCleanup'},
            {name => 'bLogForce'},
            {name => 'strLogLevelTestFile'},
            {name => 'strPgUser'},
            {name => 'strGroup'},
        );

    # Init will only be run on first test, clean/init on subsequent tests
    $self->{bFirstTest} = true;

    # Initialize test storage
    $oStorage = new pgBackRestTest::Common::Storage(
        $self->testPath(), new pgBackRestTest::Common::StoragePosix({bFileSync => false, bPathSync => false}));

    projectBinSet($self->{strBackRestExe});

    # Init, run, and end the test(s)
    $self->initModule();
    $self->run();
    $self->end();
    $self->cleanModule();

    # Make sure the correct number of tests ran
    my $hModuleTest = testDefModuleTest($self->{strModule}, $self->{strModuleTest});

    if ($hModuleTest->{&TESTDEF_TOTAL} != $self->runCurrent())
    {
        confess &log(ASSERT, "expected $hModuleTest->{&TESTDEF_TOTAL} tests to run but $self->{iRun} ran");
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'self', value => $self, trace => true}
    );
}

####################################################################################################################################
# begin
####################################################################################################################################
sub begin
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strDescription,
        $bExpect,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->begin', \@_,
            {name => 'strDescription'},
            {name => 'bExpect', required => false},
        );

    # Save the previous expect log
    $self->end();

    # If bExpect is defined then it is an override of the default
    $self->{bExpect} = false;

    if ($self->vm() eq VM_EXPECT)
    {
        if (defined($bExpect))
        {
            $self->{bExpect} = $bExpect;
        }
        # Else get the default expect setting
        else
        {
            $self->{bExpect} = (testDefModuleTest($self->{strModule}, $self->{strModuleTest}))->{&TESTDEF_EXPECT};
        }
    }

    # Increment the run counter;
    $self->{iRun}++;

    # Return if this test should not be run
    if (@{$self->{iyModuleTestRun}} != 0 && !grep(/^$self->{iRun}$/i, @{$self->{iyModuleTestRun}}))
    {
        return false;
    }

    # Output information about test to run
    &log(INFO, 'run ' . sprintf('%03d', $self->runCurrent()) . ' - ' . $strDescription);

    if ($self->isDryRun())
    {
        return false;
    }

    # Create an ExpectTest object
    if ($self->doExpect())
    {
        $self->{oExpect} = new pgBackRestTest::Common::LogTest(
            $self->module(), $self->moduleTest(), $self->runCurrent(), $self->doLogForce(), $strDescription,
            $self->{strBackRestExe}, $self->pgBinPath(), $self->testPath());

        &log(INFO, '          expect log: ' . $self->{oExpect}->{strFileName});
    }


    if (!$self->{bFirstTest})
    {
        $self->cleanTest();
    }

    $self->initTest();
    $self->{bFirstTest} = false;

    return true;
}

####################################################################################################################################
# end
####################################################################################################################################
sub end
{
    my $self = shift;

    # Save the previous test log
    if (defined($self->expect()))
    {
        $self->expect()->logWrite($self->basePath(), $self->testPath());
        delete($self->{oExpect});
    }
}

####################################################################################################################################
# testResult
####################################################################################################################################
sub testResult
{
    my $self = shift;

    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $fnSub,
        $strExpected,
        $strDescription,
        $iWaitSeconds,
        $strLogExpect,
        $strLogLevel,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::testResult', \@_,
            {name => 'fnSub', trace => true},
            {name => 'strExpected', required => false, trace => true},
            {name => 'strDescription', trace => true},
            {name => 'iWaitSeconds', optional => true, default => 0, trace => true},
            {name => 'strLogExpect', optional => true, trace => true},
            {name => 'strLogLevel', optional => true, default => WARN, trace => true},
        );

    &log(INFO, '    ' . $strDescription);
    my $strActual;
    my $bWarnValid = true;

    my $oWait = waitInit($iWaitSeconds);
    my $bDone = false;

    # Save the current log levels and set the file level to strLogLevel, console to off, and timestamp false
    my ($strLogLevelFile, $strLogLevelConsole, $strLogLevelStdErr, $bLogTimestamp) = logLevel();
    logLevelSet($strLogLevel, OFF, undef, false);

    # Clear the cache for this test
    logFileCacheClear();

    my @stryResult;

    do
    {
        eval
        {
            @stryResult = ref($fnSub) eq 'CODE' ? $fnSub->() : $fnSub;

            if (@stryResult <= 1)
            {
                $strActual = ${logDebugBuild($stryResult[0])};
            }
            else
            {
                $strActual = ${logDebugBuild(\@stryResult)};
            }

            # Restore the log level
            logLevelSet($strLogLevelFile, $strLogLevelConsole, $strLogLevelStdErr, $bLogTimestamp);
            return true;
        }
        or do
        {
            # Restore the log level
            logLevelSet($strLogLevelFile, $strLogLevelConsole, $strLogLevelStdErr, $bLogTimestamp);

            if (!isException(\$EVAL_ERROR))
            {
                confess "unexpected standard Perl exception" . (defined($EVAL_ERROR) ? ": ${EVAL_ERROR}" : '');
            }

            confess &logException($EVAL_ERROR);
        };

        if ($strActual ne (defined($strExpected) ? $strExpected : "[undef]"))
        {
            if (!waitMore($oWait))
            {
                confess
                    "expected:\n" . (defined($strExpected) ? "\"${strExpected}\"" : '[undef]') .
                    "\nbut actual was:\n" . (defined($strActual) ? "\"${strActual}\"" : '[undef]');
            }
        }
        else
        {
            $bDone = true;
        }
    } while (!$bDone);

    # If we get here then test any warning message
    if (defined($strLogExpect))
    {
        my $strLogMessage = trim(logFileCache());

        # Strip leading Process marker and whitespace from each line
        $strLogMessage =~ s/^(P[0-9]{2})*\s+//mg;

        # If the expected message does not exactly match the logged message or is not at least contained in it, then error
        if (!($strLogMessage eq $strLogExpect || $strLogMessage =~ $strLogExpect))
        {
            confess &log(ERROR,
                "the log message:\n$strLogMessage\ndoes not match or does not contain the expected message:\n" .
                $strLogExpect);
        }
    }

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'result', value => \@stryResult, trace => true}
    );
}

####################################################################################################################################
# testException
####################################################################################################################################
sub testException
{
    my $self = shift;
    my $fnSub = shift;
    my $iCodeExpected = shift;
    my $strMessageExpected = shift;

    # Output first line of the error message
    &log(INFO,
        "    [${iCodeExpected}] " . (defined($strMessageExpected) ? (split('\n', $strMessageExpected))[0] : 'undef error message'));

    my $bError = false;
    my $strError =
        "exception ${iCodeExpected}, " . (defined($strMessageExpected) ? "'${strMessageExpected}'" : '[UNDEF]') . " was expected";

    eval
    {
        logDisable();
        $fnSub->();
        logEnable();
        return true;
    }
    or do
    {
        logEnable();

        if (!isException(\$EVAL_ERROR))
        {
            confess "${strError} but actual was standard Perl exception" . (defined($EVAL_ERROR) ? ": ${EVAL_ERROR}" : '');
        }

        if (!($EVAL_ERROR->code() == $iCodeExpected &&
            (!defined($strMessageExpected) && !defined($EVAL_ERROR->message()) ||
                (defined($strMessageExpected) && defined($EVAL_ERROR->message()) &&
                    ($EVAL_ERROR->message() eq $strMessageExpected || $EVAL_ERROR->message() =~ "^${strMessageExpected}" ||
                     $EVAL_ERROR->message() =~ "^${strMessageExpected} at ")))))
        {
            confess
                "${strError} but actual was " . $EVAL_ERROR->code() . ', ' .
                (defined($EVAL_ERROR->message()) ? qw{'} . $EVAL_ERROR->message() . qw{'} : '[UNDEF]');
        }

        $bError = true;
    };

    if (!$bError)
    {
        confess "${strError} but no exception was thrown";
    }
}

####################################################################################################################################
# testRunName
#
# Create module/test names by upper-casing the first letter and then inserting capitals after each -.
####################################################################################################################################
sub testRunName
{
    my $strName = shift;
    my $bInitCapFirst = shift;

    $bInitCapFirst = defined($bInitCapFirst) ? $bInitCapFirst : true;
    my $bFirst = true;

    my @stryName = split('\-', $strName);
    $strName = undef;

    foreach my $strPart (@stryName)
    {
        $strName .= ($bFirst && $bInitCapFirst) || !$bFirst ? ucfirst($strPart) : $strPart;
        $bFirst = false;
    }

    return $strName;
}

push @EXPORT, qw(testRunName);

####################################################################################################################################
# testRun
####################################################################################################################################
sub testRun
{
    # Assign function parameters, defaults, and log debug info
    my
    (
        $strOperation,
        $strModule,
        $strModuleTest,
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::testRun', \@_,
            {name => 'strModule', trace => true},
            {name => 'strModuleTest', trace => true},
        );

    # Error if the test run is already defined - only one run per process is allowed
    if (defined($oTestRun))
    {
        confess &log(ASSERT, 'a test run has already been created in this process');
    }

    my $strModuleName =
        'pgBackRestTest::Module::' . testRunName($strModule) . '::' . testRunName($strModule) . testRunName($strModuleTest) .
        'Test';

    $oTestRun = eval("require ${strModuleName}; ${strModuleName}->import(); return new ${strModuleName}();")
        or do {confess $EVAL_ERROR};

    # Return from function and log return values if any
    return logDebugReturn
    (
        $strOperation,
        {name => 'oRun', value => $oTestRun, trace => true}
    );
}

push @EXPORT, qw(testRun);

####################################################################################################################################
# testRunGet
####################################################################################################################################
sub testRunGet
{
    return $oTestRun;
}

push @EXPORT, qw(testRunGet);

####################################################################################################################################
# storageTest - get the storage for the current test
####################################################################################################################################
sub storageTest
{
    return $oStorage;
}

push(@EXPORT, qw(storageTest));

####################################################################################################################################
# Getters
####################################################################################################################################
sub archBits {return vmArchBits(shift->{strVm})}
sub backrestExe {return shift->{strBackRestExe}}
sub backrestExeHelper {return shift->{strBackRestExeHelper}}
sub basePath {return shift->{strBasePath}}
sub dataPath {return shift->basePath() . '/test/data'}
sub doCleanup {return shift->{bCleanup}}
sub doExpect {return shift->{bExpect}}
sub doLogForce {return shift->{bLogForce}}
sub logLevelTestFile {return shift->{strLogLevelTestFile}}
sub group {return shift->{strGroup}}
sub isDryRun {return shift->{bDryRun}}
sub expect {return shift->{oExpect}}
sub module {return shift->{strModule}}
sub moduleTest {return shift->{strModuleTest}}
sub pgBinPath {return shift->{strPgBinPath}}
sub pgUser {return shift->{strPgUser}}
sub pgVersion {return shift->{strPgVersion}}
sub runCurrent {return shift->{iRun}}
sub stanza {return 'db'}
sub testPath {return shift->{strTestPath}}
sub vm {return shift->{strVm}}
sub vmId {return shift->{iVmId}}

1;
