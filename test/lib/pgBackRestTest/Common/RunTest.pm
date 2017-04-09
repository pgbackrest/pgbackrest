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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;
use pgBackRest::Common::Wait;

use pgBackRestTest::Common::DefineTest;
use pgBackRestTest::Common::ExecuteTest;
use pgBackRestTest::Common::LogTest;

####################################################################################################################################
# Constant to use when bogus data is required
####################################################################################################################################
use constant BOGUS =>                                               'bogus';
    push @EXPORT, qw(BOGUS);

####################################################################################################################################
# The current test run that is executung.  Only a single run should ever occur in a process to prevent various cleanup issues from
# affecting the next run.  Of course multiple subtests can be executed in a single run.
####################################################################################################################################
my $oTestRun;

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
# Empty init sub in case the ancestor class does not delare one.
####################################################################################################################################
sub initModule {}

####################################################################################################################################
# initTest
#
# Empty init sub in case the ancestor class does not delare one.
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

    executeTest('sudo rm -rf ' . $self->testPath() . '/*');
}

####################################################################################################################################
# cleanModule
#
# Empty final sub in case the ancestor class does not delare one.
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
        $self->{strVmHost},
        $self->{iVmId},
        $self->{strBasePath},
        $self->{strTestPath},
        $self->{strBackRestExeOriginal},
        $self->{strPgBinPath},
        $self->{strPgVersion},
        $self->{strModule},
        $self->{strModuleTest},
        $self->{iyModuleTestRun},
        $self->{iProcessMax},
        $self->{bOutput},
        $self->{bDryRun},
        $self->{bCleanup},
        $self->{bLogForce},
        $self->{strPgUser},
        $self->{strBackRestUser},
        $self->{strGroup},
    ) =
        logDebugParam
        (
            __PACKAGE__ . '->process', \@_,
            {name => 'strVm'},
            {name => 'strVmHost'},
            {name => 'iVmId'},
            {name => 'strBasePath'},
            {name => 'strTestPath'},
            {name => 'strBackRestExeOriginal'},
            {name => 'strPgBinPath', required => false},
            {name => 'strPgVersion', required => false},
            {name => 'strModule'},
            {name => 'strModuleTest'},
            {name => 'iModuleTestRun', required => false},
            {name => 'iProcessMax'},
            {name => 'bOutput'},
            {name => 'bDryRun'},
            {name => 'bCleanup'},
            {name => 'bLogForce'},
            {name => 'strPgUser'},
            {name => 'strBackRestUser'},
            {name => 'strGroup'},
        );

    # Init will only be run on first test, clean/init on subsequent tests
    $self->{bFirstTest} = true;

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
    if (defined($bExpect))
    {
        $self->{bExpect} = $bExpect;
    }
    # Else get the default expect setting
    else
    {
        $self->{bExpect} = (testDefModuleTest($self->{strModule}, $self->{strModuleTest}))->{&TESTDEF_EXPECT};
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

    my $strExe = $self->backrestExeOriginal();

    # If coverage is requested then prepend the coverage code
    if ($self->coverage())
    {
        $strExe = testRunExe(
            $strExe, dirname($self->testPath()), $self->basePath(), $self->module(), $self->moduleTest(), $self->runCurrent(),
            true);
    }
    # Else if the module is defined then create a ExpectTest object
    elsif ($self->doExpect())
    {
        $self->{oExpect} = new pgBackRestTest::Common::LogTest(
            $self->module(), $self->moduleTest(), $self->runCurrent(), $self->doLogForce(), $strDescription, $strExe,
            $self->pgBinPath(), $self->testPath());

        &log(INFO, '          expect log: ' . $self->{oExpect}->{strFileName});
    }

    $self->{strBackRestExe} = $strExe;

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
    ) =
        logDebugParam
        (
            __PACKAGE__ . '::testResult', \@_,
            {name => 'fnSub', trace => true},
            {name => 'strExpected', required => false, trace => true},
            {name => 'strDescription', trace => true},
            {name => 'iWaitSeconds', optional => true, default => 0, trace => true},
        );

    &log(INFO, '    ' . $strDescription);
    my $strActual;

    my $oWait = waitInit($iWaitSeconds);
    my $bDone = false;

    do
    {
        eval
        {
            logDisable();
            my @stryResult = ref($fnSub) eq 'CODE' ? $fnSub->() : $fnSub;

            if (@stryResult <= 1)
            {
                $strActual = ${logDebugBuild($stryResult[0])};
            }
            else
            {
                $strActual = ${logDebugBuild(\@stryResult)};
            }

            logEnable();
            return true;
        }
        or do
        {
            logEnable();

            if (!isException($EVAL_ERROR))
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

    # Return from function and log return values if any
    return logDebugReturn($strOperation);
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
    my @stryErrorMessage = split('\n', $strMessageExpected);
    &log(INFO, "    [${iCodeExpected}] " . $stryErrorMessage[0]);

    my $bError = false;
    my $strError = "exception ${iCodeExpected}, \"${strMessageExpected}\" was expected";

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

        if (!isException($EVAL_ERROR))
        {
            confess "${strError} but actual was standard Perl exception" . (defined($EVAL_ERROR) ? ": ${EVAL_ERROR}" : '');
        }

        if (!($EVAL_ERROR->code() == $iCodeExpected &&
            ($EVAL_ERROR->message() eq $strMessageExpected || $EVAL_ERROR->message() =~ $strMessageExpected)))
        {
            confess "${strError} but actual was " . $EVAL_ERROR->code() . ", \"" . $EVAL_ERROR->message() . "\"";
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

    my @stryName = split('\-', $strName);
    $strName = undef;

    foreach my $strPart (@stryName)
    {
        $strName .= ucfirst($strPart);
    }

    return $strName;
}

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
        'pgBackRestTest::' . testRunName($strModule) . '::' . testRunName($strModule) . testRunName($strModuleTest) . 'Test';

    $oTestRun = eval(                                                ## no critic (BuiltinFunctions::ProhibitStringyEval)
        "require ${strModuleName}; ${strModuleName}->import(); return new ${strModuleName}();")
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
# testExe
####################################################################################################################################
sub testRunExe
{
    my $strExe = shift;
    my $strCoveragePath = shift;
    my $strBackRestBasePath = shift;
    my $strModule = shift;
    my $strTest = shift;
    my $iRun = shift;
    my $bLog = shift;

    # Limit Perl modules tested to what is defined in the test coverage (if it exists)
    my $hTestCoverage = (testDefModuleTest($strModule, $strTest))->{&TESTDEF_COVERAGE};
    my $strPerlModule;
    my $strPerlModuleLog;

    if (defined($hTestCoverage))
    {
        foreach my $strCoverageModule (sort(keys(%{$hTestCoverage})))
        {
            $strPerlModule .= ',.*/' . $strCoverageModule . '\.p.$';
            $strPerlModuleLog .= (defined($strPerlModuleLog) ? ', ' : '') . $strCoverageModule;
        }
    }

    # Build the exe
    if (defined($strPerlModule))
    {
        $strExe =
            'perl -MDevel::Cover=-silent,1,-dir,' . $strCoveragePath . ',-subs_only,1' .
            ",-select${strPerlModule},+inc," . $strBackRestBasePath .
            ',-coverage,statement,branch,condition,path,subroutine' . " ${strExe}";

        if (defined($bLog) && $bLog)
        {
            &log(INFO, "          coverage: ${strPerlModuleLog}");
        }
    }

    return $strExe;
}

push(@EXPORT, qw(testRunExe));

####################################################################################################################################

####################################################################################################################################
# Getters
####################################################################################################################################
sub backrestExe {return shift->{strBackRestExe}}
sub backrestExeOriginal {return shift->{strBackRestExeOriginal}}
sub backrestUser {return shift->{strBackRestUser}}
sub basePath {return shift->{strBasePath}}
sub coverage {my $self = shift; return $self->{strVm} eq $self->{strVmHost}}
sub dataPath {return shift->basePath() . '/test/data'}
sub doCleanup {return shift->{bCleanup}}
sub doExpect {return shift->{bExpect}}
sub doLogForce {return shift->{bLogForce}}
sub group {return shift->{strGroup}}
sub isDryRun {return shift->{bDryRun}}
sub expect {return shift->{oExpect}}
sub module {return shift->{strModule}}
sub moduleTest {return shift->{strModuleTest}}
sub pgBinPath {return shift->{strPgBinPath}}
sub pgUser {return shift->{strPgUser}}
sub pgVersion {return shift->{strPgVersion}}
sub processMax {return shift->{iProcessMax}}
sub runCurrent {return shift->{iRun}}
sub stanza {return 'db'}
sub testPath {return shift->{strTestPath}}
sub vm {return shift->{strVm}}
sub vmId {return shift->{iVmId}}

1;
