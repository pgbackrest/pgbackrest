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

use pgBackRest::Common::Exception;
use pgBackRest::Common::Log;

use pgBackRestTest::Common::LogTest;
use pgBackRestTest::Common::DefineTest;

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
# init
#
# Empty init sub in case the ancestor class does not delare one.
####################################################################################################################################
sub init {}

####################################################################################################################################
# final
#
# Empty final sub in case the ancestor class does not delare one.
####################################################################################################################################
sub final {}

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
            {name => 'iVmId'},
            {name => 'strBasePath'},
            {name => 'strTestPath'},
            {name => 'strBackRestExe'},
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

    # Init, run, and end the test(s)
    $self->init();
    $self->run();
    $self->final();
    $self->end();

    # Make sure the correct number of tests ran
    my $hModule = testDefModuleGet($self->{strModule});
    my $hModuleTest = testDefModuleTestGet($hModule, $self->{strModuleTest});

    if ($hModuleTest->{&TESTDEF_TEST_TOTAL} != $self->runCurrent())
    {
        confess &log(ASSERT, "expected $hModuleTest->{&TESTDEF_TEST_TOTAL} tests to run but $self->{iRun} ran");
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
        my $hModule = testDefModuleGet($self->{strModule});
        my $hModuleTest = testDefModuleTestGet($hModule, $self->{strModuleTest});
        $self->{bExpect} =
            defined($hModuleTest->{&TESTDEF_EXPECT}) ?
                ($hModuleTest->{&TESTDEF_EXPECT} ? true : false) :
                (defined($hModule->{&TESTDEF_EXPECT}) ?
                    ($hModule->{&TESTDEF_EXPECT} ? true : false) :
                    false);
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

    # If the module is defined then create a ExpectTest object
    if ($self->doExpect())
    {
        $self->{oExpect} = new pgBackRestTest::Common::LogTest(
            $self->module(), $self->moduleTest(), $self->runCurrent(), $self->doLogForce(), $strDescription, $self->backrestExe(),
            $self->pgBinPath(), $self->testPath());

        &log(INFO, '          expect log: ' . $self->{oExpect}->{strFileName});
    }

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
    my $fnSub = shift;
    my $strExpected = shift;

    my $strActual = $fnSub->();

    if (!defined($strExpected) && defined($strActual) || defined($strExpected) && !defined($strActual) ||
        $strActual ne $strExpected)
    {
        confess
            'expected ' . (defined($strExpected) ? "\"${strExpected}\"" : '[undef]') .
            " but actual was " . (defined($strActual) ? "\"${strActual}\"" : '[undef]');
    }
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

    my $bError = false;
    my $strError = "exception ${iCodeExpected}, \"${strMessageExpected}\" was expected";

    eval
    {
        $fnSub->();
        return true;
    }
    or do
    {
        if (!isException($EVAL_ERROR))
        {
            confess "${strError} but actual was standard Perl exception";
        }

        if (!($EVAL_ERROR->code() == $iCodeExpected && $EVAL_ERROR->message() eq $strMessageExpected))
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
# Getters
####################################################################################################################################
sub backrestExe {return shift->{strBackRestExe}}
sub backrestUser {return shift->{strBackRestUser}}
sub basePath {return shift->{strBasePath}}
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
